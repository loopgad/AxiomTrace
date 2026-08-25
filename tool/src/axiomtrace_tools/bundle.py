"""Standard AxiomTrace metadata bundle support."""

from __future__ import annotations

import json
import platform
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from axiomtrace_tools.codegen import generate_assets
from axiomtrace_tools.dictionary import EventDictionary, load_dictionary
from axiomtrace_tools.metadata_id import WIRE_VERSION, hash_file, location_contract, metadata_id_for_data
from axiomtrace_tools.source_map import generate_source_map, load_source_map


BUNDLE_SCHEMA = "axiomtrace.bundle.v1"
BUNDLE_SEARCH_DIRS = [
    Path("build") / "axiomtrace-bundle",
    Path("axiomtrace-bundle"),
    Path(".axiom") / "bundle",
]


@dataclass
class AxiomBundle:
    """Loaded AxiomTrace metadata bundle."""

    root: Path
    manifest: dict[str, Any]

    @property
    def metadata_id(self) -> str | None:
        return self.manifest.get("metadata", {}).get("id")

    def artifact_path(self, key: str) -> Path | None:
        artifacts = self.manifest.get("artifacts", {})
        if not isinstance(artifacts, dict):
            raise ValueError("bundle artifacts must be an object")
        value = artifacts.get(key)
        if not value:
            return None
        if not isinstance(value, str):
            raise ValueError(f"bundle artifact path must be a string: {key}")
        return (self.root / value).resolve()

    def load_dictionary(self) -> EventDictionary | None:
        return load_dictionary(self.artifact_path("dictionary"))

    def load_source_map(self) -> dict[str, Any] | None:
        return load_source_map(self.artifact_path("source_map"))


def load_bundle(path: str | Path) -> AxiomBundle:
    """Load a bundle from a directory or manifest path."""
    bundle_path = Path(path)
    manifest_path = bundle_path / "manifest.json" if bundle_path.is_dir() else bundle_path
    with manifest_path.open("r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    _validate_manifest(manifest)
    return AxiomBundle(manifest_path.parent.resolve(), manifest)


def find_bundle(start_path: str | Path | None = None) -> Path | None:
    """Find a local bundle by walking up from start_path."""
    current = Path(start_path or ".").resolve()
    while True:
        for relative in BUNDLE_SEARCH_DIRS:
            manifest = current / relative / "manifest.json"
            if manifest.is_file():
                return manifest.parent
        if current.parent == current:
            return None
        current = current.parent


def find_bundle_in_store(store: str | Path, metadata_id: str) -> Path | None:
    """Find a bundle in a bundle-store by trace metadata identity."""
    store_path = Path(store)
    candidates = [store_path / "manifest.json", *store_path.glob("*/manifest.json")]
    for manifest in candidates:
        if not manifest.is_file():
            continue
        try:
            bundle = load_bundle(manifest)
        except (OSError, ValueError, json.JSONDecodeError):
            continue
        if bundle.metadata_id == metadata_id:
            return bundle.root
    return None


def generate_bundle(
    events: str | Path,
    out: str | Path,
    elf: str | Path | None = None,
    compile_db: str | Path | None = None,
    source_id_map: str | Path | None = None,
    map_file: str | Path | None = None,
    firmware_name: str | None = None,
    firmware_version: str | None = None,
    location_mode: str | None = None,
    location_function: bool = False,
) -> dict[str, Path]:
    """Generate a standard metadata bundle."""
    out_dir = Path(out)
    out_dir.mkdir(parents=True, exist_ok=True)
    generated_dir = out_dir / "generated"
    source_map = generate_source_map(
        compile_db,
        Path(events).resolve().parent,
        source_id_map,
        location_mode,
    )
    selected_location_mode = str(source_map["location_mode"])
    location = location_contract(selected_location_mode, location_function)
    assets = generate_assets(
        events,
        generated_dir,
        source_id_map=source_id_map,
        location_mode=selected_location_mode,
        location_function=location_function,
        source_map=source_map,
    )

    dictionary_path = out_dir / "dictionary.json"
    shutil.copy2(assets["dictionary"], dictionary_path)
    source_map_path = out_dir / "source_map.json"
    source_map_path.write_text(json.dumps(source_map, indent=2), encoding="utf-8")

    copied_elf = _copy_optional(elf, out_dir)
    copied_map = _copy_optional(map_file, out_dir)
    with dictionary_path.open("r", encoding="utf-8") as handle:
        dictionary = json.load(handle)
    metadata_id = metadata_id_for_data(dictionary, source_map, location)
    build_info_path = out_dir / "build_info.json"
    build_info_path.write_text(
        json.dumps(
            _build_info(metadata_id, firmware_name, firmware_version, copied_elf, copied_map),
            indent=2,
        ),
        encoding="utf-8",
    )

    manifest = {
        "schema": BUNDLE_SCHEMA,
        "bundle_version": 1,
        "metadata": {
            "id": metadata_id,
            "wire_version": WIRE_VERSION,
            "identity_basis": ["wire_version", "location", "dictionary", "source_map"],
        },
        "firmware": {
            "name": firmware_name or "firmware",
            "version": firmware_version or "0.0.0",
        },
        "artifacts": {
            "dictionary": "dictionary.json",
            "source_map": "source_map.json",
            "build_info": "build_info.json",
            "elf": copied_elf.name if copied_elf else None,
            "map": copied_map.name if copied_map else None,
        },
        "location": location,
        "decoder": {
            "default_format": "text",
            "require_metadata_id_match": True,
        },
        "features": {
            "source_location": selected_location_mode != "none",
            "metadata_identity_event": True,
            "metadata_type_tags": ["0x0A", "0x0B"],
        },
    }
    manifest_path = out_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    return {
        "manifest": manifest_path,
        "dictionary": dictionary_path,
        "source_map": source_map_path,
        "build_info": build_info_path,
        "generated": generated_dir,
    }


def _copy_optional(path: str | Path | None, out_dir: Path) -> Path | None:
    if path is None:
        return None
    source = Path(path)
    if not source.is_file():
        raise FileNotFoundError(f"artifact not found: {source}")
    destination = out_dir / source.name
    if source.resolve() != destination.resolve():
        shutil.copy2(source, destination)
    return destination


def _validate_manifest(manifest: Any) -> dict[str, Any]:
    if not isinstance(manifest, dict):
        raise ValueError("bundle manifest root must be an object")
    if manifest.get("schema") != BUNDLE_SCHEMA:
        raise ValueError(f"unsupported bundle schema: {manifest.get('schema')}")
    if not isinstance(manifest.get("bundle_version"), int) or isinstance(manifest["bundle_version"], bool):
        raise ValueError("bundle_version must be an integer")
    metadata = manifest.get("metadata")
    if not isinstance(metadata, dict):
        raise ValueError("bundle metadata must be an object")
    if not isinstance(metadata.get("id"), str):
        raise ValueError("bundle metadata id must be a string")
    if not isinstance(metadata.get("wire_version"), str):
        raise ValueError("bundle metadata wire_version must be a string")
    if "identity_basis" in metadata:
        _validate_string_array(metadata["identity_basis"], "bundle metadata identity_basis")
    artifacts = manifest.get("artifacts")
    if not isinstance(artifacts, dict):
        raise ValueError("bundle artifacts must be an object")
    for key, value in artifacts.items():
        if value is not None and not isinstance(value, str):
            raise ValueError(f"bundle artifact path must be a string: {key}")
    for key in ("firmware", "location", "decoder", "features"):
        if key in manifest and not isinstance(manifest[key], dict):
            raise ValueError(f"bundle {key} must be an object")
    location = manifest.get("location", {})
    if "mode" in location and location["mode"] not in {"none", "hash", "file_id"}:
        raise ValueError(f"unsupported location mode: {location['mode']}")
    return manifest


def _validate_string_array(value: Any, field: str) -> None:
    if not isinstance(value, list) or any(not isinstance(item, str) for item in value):
        raise ValueError(f"{field} must be an array of strings")


def _build_info(
    metadata_id: str,
    firmware_name: str | None,
    firmware_version: str | None,
    elf: Path | None,
    map_file: Path | None,
) -> dict[str, Any]:
    return {
        "schema": "axiomtrace.build_info.v1",
        "metadata_id": metadata_id,
        "firmware_name": firmware_name or "firmware",
        "firmware_version": firmware_version or "0.0.0",
        "wire_version": WIRE_VERSION,
        "identity_basis": ["wire_version", "location", "dictionary", "source_map"],
        "diagnostic_artifact_sha256": {
            "elf": hash_file(elf) if elf else None,
            "map": hash_file(map_file) if map_file else None,
        },
        "host": {
            "python": sys.version.split()[0],
            "platform": platform.platform(),
            "cmake": _tool_version("cmake"),
            "ninja": _tool_version("ninja"),
            "git": _tool_version("git"),
            "gcc": _tool_version("gcc"),
            "clang": _tool_version("clang"),
            "arm_none_eabi_gcc": _tool_version("arm-none-eabi-gcc"),
        },
    }


def _tool_version(command: str) -> str | None:
    if shutil.which(command) is None:
        return None
    try:
        result = subprocess.run(
            [command, "--version"],
            check=False,
            capture_output=True,
            text=True,
            timeout=5,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    first_line = (result.stdout or result.stderr).splitlines()
    return first_line[0] if first_line else None
