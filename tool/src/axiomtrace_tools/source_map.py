"""Source map generation and loading for host-side source location lookup."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any


def generate_source_map(
    compile_db: str | Path | None,
    project_root: str | Path | None = None,
    source_id_map: str | Path | None = None,
    location_mode: str | None = None,
) -> dict[str, Any]:
    """Generate a deterministic file-id source map from compile_commands.json."""
    root = Path(project_root or ".").resolve()
    files: dict[str, dict[str, Any]] = {}
    if source_id_map and Path(source_id_map).is_file():
        with Path(source_id_map).open("r", encoding="utf-8") as handle:
            source_ids = json.load(handle)
        source_ids = _validate_source_id_map(source_ids)
        for entry in source_ids["files"]:
            # Firmware HASH mode hashes the compiler's exact __FILE__ spelling.
            hash_input = entry["path"]
            path = _normalize_path(hash_input, root)
            file_id = str(_parse_uint(entry["id"], "source id", 0xFFFF))
            files[file_id] = {"path": path, "hash16": f"0x{_fnv1a16(hash_input):04X}"}
    elif compile_db:
        db_path = Path(compile_db)
        if db_path.is_file():
            with db_path.open("r", encoding="utf-8") as handle:
                entries = json.load(handle)
            if not isinstance(entries, list):
                raise ValueError("compile database root must be an array")
            for index, entry in enumerate(entries):
                if not isinstance(entry, dict):
                    raise ValueError(f"compile database entry {index} must be an object")
                if not isinstance(entry.get("file"), str) or not entry["file"]:
                    raise ValueError(f"compile database entry {index} file must be a non-empty string")
            source_paths = sorted(
                {
                    (_normalize_path(entry["file"], root), entry["file"])
                    for entry in entries
                }
            )
            for idx, (path, hash_input) in enumerate(source_paths, start=1):
                files[str(idx)] = {"path": path, "hash16": f"0x{_fnv1a16(hash_input):04X}"}

    inferred_mode = "file_id" if files else "none"
    selected_mode = location_mode or inferred_mode
    if selected_mode not in {"none", "hash", "file_id"}:
        raise ValueError(f"unsupported location mode: {selected_mode}")
    if selected_mode == "file_id" and not files:
        raise ValueError("file_id location mode requires a source id map or compile database")
    hash_index = _build_hash_index(files)
    return {
        "schema": "axiomtrace.source_map.v1",
        "location_mode": selected_mode,
        "path_base": "project",
        "files": files,
        "hash_index": hash_index,
        "hash_collisions": {key: ids for key, ids in hash_index.items() if len(ids) > 1},
        "functions": [],
    }


def resolve_location(location: dict[str, Any] | None, source_map: dict[str, Any] | None) -> dict[str, Any] | None:
    """Enrich decoded location metadata from a loaded source map."""
    if location is None:
        return None
    if not isinstance(location, dict):
        raise ValueError("location must be an object")
    if not location:
        return None
    resolved = dict(location)
    if source_map is None:
        return resolved
    if not isinstance(source_map, dict):
        raise ValueError("source map root must be an object")
    if not source_map:
        return resolved
    files = source_map.get("files", {})
    if not isinstance(files, dict):
        raise ValueError("source map files must be an object")
    match: dict[str, Any] | None = None
    if location.get("mode") == "file_id":
        file_id = str(_parse_uint(location.get("file_id"), "file id", 0xFFFF))
        _parse_uint(location.get("line"), "source line", 0xFFFF)
        if file_id in files and not isinstance(files[file_id], dict):
            raise ValueError("source map file entry must be an object")
        match = files.get(file_id)
    elif location.get("mode") == "hash":
        file_hash = _parse_uint(location.get("file_hash", 0), "file hash", 0xFFFF)
        _parse_uint(location.get("line"), "source line", 0xFFFF)
        _parse_uint(location.get("function_hash"), "function hash", 0xFFFF)
        expected = f"0x{file_hash:04X}".upper()
        hash_index = source_map.get("hash_index")
        if hash_index is not None and not isinstance(hash_index, dict):
            raise ValueError("source map hash_index must be an object")
        if not isinstance(hash_index, dict):
            hash_index = _build_hash_index(files)
        candidate_ids = hash_index.get(expected, hash_index.get(expected.lower(), []))
        if candidate_ids and any(str(file_id) not in files for file_id in candidate_ids):
            candidate_ids = []
        if not candidate_ids:
            candidate_ids = _build_hash_index(files).get(expected, [])
        if not isinstance(candidate_ids, list) or any(not isinstance(file_id, str) for file_id in candidate_ids):
            raise ValueError("source map hash_index entries must be arrays of file IDs")
        if len(candidate_ids) == 1:
            candidate = files.get(str(candidate_ids[0]))
            if candidate is not None and not isinstance(candidate, dict):
                raise ValueError("source map file entry must be an object")
            match = candidate
        elif len(candidate_ids) > 1:
            resolved["warning"] = "AMBIGUOUS_FILE_HASH"
            resolved["candidates"] = [
                files[str(file_id)].get("path")
                for file_id in candidate_ids
                if str(file_id) in files and isinstance(files[str(file_id)], dict)
            ]
    if match:
        resolved["file"] = match.get("path")
    return resolved


def load_source_map(path: str | Path | None) -> dict[str, Any] | None:
    """Load source_map.json if available."""
    if path is None:
        return None
    source_path = Path(path)
    if not source_path.is_file():
        return None
    with source_path.open("r", encoding="utf-8") as handle:
        return _validate_source_map(json.load(handle))


def _normalize_path(value: str | None, root: Path) -> str:
    if not value:
        return ""
    path = Path(value)
    if not path.is_absolute():
        path = (root / path).resolve()
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return path.as_posix()


def _fnv1a16(text: str) -> int:
    h = 0x811C
    for byte in text.encode("utf-8"):
        h ^= byte
        h = (h * 0x0103) & 0xFFFF
    return h


def _parse_uint(value: Any, field: str, maximum: int) -> int:
    if isinstance(value, bool):
        raise ValueError(f"{field} must be an integer")
    try:
        if isinstance(value, str):
            parsed = int(value, 0)
        elif isinstance(value, int):
            parsed = value
        else:
            raise TypeError
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{field} must be an integer") from exc
    if parsed < 0 or parsed > maximum:
        raise ValueError(f"{field} out of range: {parsed}")
    return parsed


def _validate_source_id_map(data: Any) -> dict[str, Any]:
    if not isinstance(data, dict):
        raise ValueError("source id map root must be an object")
    entries = data.get("files")
    if not isinstance(entries, list):
        raise ValueError("source id map files must be an array")
    seen_ids: set[int] = set()
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            raise ValueError(f"source id map entry {index} must be an object")
        if "id" not in entry:
            raise ValueError(f"source id map entry {index} is missing id")
        file_id = _parse_uint(entry["id"], "source id", 0xFFFF)
        if file_id in seen_ids:
            raise ValueError(f"duplicate source id: {file_id}")
        seen_ids.add(file_id)
        if not isinstance(entry.get("path"), str) or not entry["path"]:
            raise ValueError(f"source id map entry {index} path must be a non-empty string")
    return data


def _validate_source_map(data: Any) -> dict[str, Any]:
    if not isinstance(data, dict):
        raise ValueError("source map root must be an object")
    files = data.get("files", {})
    if not isinstance(files, dict):
        raise ValueError("source map files must be an object")
    for file_id, candidate in files.items():
        if not isinstance(file_id, str):
            raise ValueError("source map file IDs must be strings")
        if not isinstance(candidate, dict):
            raise ValueError(f"source map file {file_id!r} must be an object")
        if "path" in candidate and not isinstance(candidate["path"], str):
            raise ValueError(f"source map file {file_id!r} path must be a string")
        if "hash16" in candidate and not isinstance(candidate["hash16"], str):
            raise ValueError(f"source map file {file_id!r} hash16 must be a string")
    hash_index = data.get("hash_index")
    if hash_index is not None:
        if not isinstance(hash_index, dict):
            raise ValueError("source map hash_index must be an object")
        for hash_key, file_ids in hash_index.items():
            if not isinstance(hash_key, str) or not isinstance(file_ids, list):
                raise ValueError("source map hash_index must map strings to file ID arrays")
            if any(not isinstance(file_id, str) for file_id in file_ids):
                raise ValueError("source map hash_index entries must be strings")
            if any(file_id not in files for file_id in file_ids):
                raise ValueError("source map hash_index references an unknown file ID")
    collisions = data.get("hash_collisions")
    if collisions is not None:
        if not isinstance(collisions, dict):
            raise ValueError("source map hash_collisions must be an object")
        for hash_key, file_ids in collisions.items():
            if not isinstance(hash_key, str) or not isinstance(file_ids, list):
                raise ValueError("source map hash_collisions must map strings to file ID arrays")
            if any(not isinstance(file_id, str) for file_id in file_ids):
                raise ValueError("source map hash_collisions entries must be strings")
            if len(file_ids) < 2 or any(file_id not in files for file_id in file_ids):
                raise ValueError("source map hash_collisions references invalid file IDs")
    if "location_mode" in data and data["location_mode"] not in {"none", "hash", "file_id"}:
        raise ValueError(f"unsupported location mode: {data['location_mode']}")
    if "functions" in data and not isinstance(data["functions"], list):
        raise ValueError("source map functions must be an array")
    return data


def _build_hash_index(files: dict[str, Any]) -> dict[str, list[str]]:
    if not isinstance(files, dict):
        raise ValueError("source map files must be an object")
    index: dict[str, list[str]] = {}
    for file_id, candidate in files.items():
        if not isinstance(candidate, dict):
            continue
        hash16 = str(candidate.get("hash16", "")).upper()
        if not hash16:
            continue
        index.setdefault(hash16, []).append(str(file_id))
    return index
