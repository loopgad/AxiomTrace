"""AxiomTrace event dictionary code generation."""

from __future__ import annotations

import json
from pathlib import Path
import re
from typing import Any

from axiomtrace_tools.dictionary import TYPE_TAG_BY_NAME, VALID_LEVELS, WIRE_SIZE_BY_NAME, parse_int
from axiomtrace_tools.metadata_id import location_contract, metadata_id_for_data
from axiomtrace_tools.source_map import generate_source_map


def load_event_source(path: str | Path) -> dict[str, Any]:
    """Load events.json or events.yaml style input."""
    source_path = Path(path)
    text = source_path.read_text(encoding="utf-8")
    suffix = source_path.suffix.lower()
    if suffix == ".json":
        try:
            data = json.loads(text)
        except json.JSONDecodeError as exc:
            raise ValueError(f"invalid event source JSON: {path}: {exc.msg}") from exc
        if not isinstance(data, dict):
            raise ValueError(f"event source root must be an object: {path}")
        return data
    if suffix in (".yaml", ".yml"):
        try:
            import yaml
            try:
                data = yaml.safe_load(text) or {}
            except yaml.YAMLError as exc:
                raise ValueError(f"invalid event source YAML: {path}: {exc}") from exc
            if not isinstance(data, dict):
                raise ValueError(f"event source root must be an object: {path}")
            return data
        except ImportError as exc:
            raise RuntimeError(
                "Detected YAML format dictionary. Please install PyYAML by running 'pip install pyyaml' "
                "or convert your dictionary to standard JSON format."
            ) from exc
    raise RuntimeError(f"Unsupported dictionary format (must be .json, .yaml, or .yml): {path}")


def normalize_event_source(data: dict[str, Any]) -> list[dict[str, Any]]:
    """Normalize supported event-source schemas to a module list."""
    if not isinstance(data, dict):
        raise ValueError("event source root must be an object")
    dictionary = data.get("dictionary", data)
    if not isinstance(dictionary, dict):
        raise ValueError("event source dictionary must be an object")
    modules = dictionary.get("modules", [])
    if isinstance(modules, dict):
        normalized = []
        for module_key, module in modules.items():
            if not isinstance(module, dict):
                raise ValueError(f"module {module_key!r} must be an object")
            module = dict(module)
            module.setdefault("id", module_key)
            events = module.get("events", {})
            if isinstance(events, dict):
                normalized_events = []
                for event_key, event in events.items():
                    if not isinstance(event, dict):
                        raise ValueError(f"event {event_key!r} in module {module_key!r} must be an object")
                    normalized_event = dict(event)
                    normalized_event.setdefault("id", event_key)
                    normalized_events.append(normalized_event)
                module["events"] = normalized_events
            elif not isinstance(events, list):
                raise ValueError(f"events in module {module_key!r} must be an object or array")
            normalized.append(module)
        modules = normalized
    if not isinstance(modules, list):
        raise ValueError("event source modules must be an object or array")
    if any(not isinstance(module, dict) for module in modules):
        raise ValueError("every event source module must be an object")
    return modules


def validate_event_source(
    data: dict[str, Any],
    location_mode: str = "none",
    max_payload_len: int = 128,
) -> list[dict[str, Any]]:
    """Validate and normalize an event source, raising ValueError on invalid data."""
    modules = normalize_event_source(data)
    if not modules:
        raise ValueError("event source must define at least one module")

    seen_modules: set[int] = set()
    seen_module_names: set[str] = set()
    seen_pairs: set[tuple[int, int]] = set()
    for module in modules:
        try:
            module_id = parse_int(module.get("id"))
        except (TypeError, ValueError) as exc:
            raise ValueError(f"module id must be an integer: {module.get('id')!r}") from exc
        if not 0 <= module_id <= 0xFF:
            raise ValueError(f"module id out of range: {module_id}")
        if module_id in seen_modules:
            raise ValueError(f"duplicate module id: {module_id}")
        seen_modules.add(module_id)
        if not module.get("name"):
            raise ValueError(f"module 0x{module_id:02X} is missing name")
        module_name = str(module["name"])
        if not re.fullmatch(r"[A-Z][A-Z0-9_]{0,31}", module_name):
            raise ValueError(f"invalid module name: {module_name}")
        if (
            "description" in module
            and module["description"] is not None
            and not isinstance(module["description"], str)
        ):
            raise ValueError(f"description for {module_name} must be a string")
        if module_name in seen_module_names:
            raise ValueError(f"duplicate module name: {module_name}")
        seen_module_names.add(module_name)
        seen_events: set[int] = set()
        seen_event_names: set[str] = set()
        events = module.get("events", [])
        if not isinstance(events, list) or not events:
            raise ValueError(f"module {module['name']} must define at least one event")
        for event in events:
            if not isinstance(event, dict):
                raise ValueError(f"events in {module_name} must be objects")
            try:
                event_id = parse_int(event.get("id"))
            except (TypeError, ValueError) as exc:
                raise ValueError(f"event id in {module_name} must be an integer: {event.get('id')!r}") from exc
            if not 0 <= event_id <= 0xFFFF:
                raise ValueError(f"event id out of range: {event_id}")
            if event_id in seen_events:
                raise ValueError(f"duplicate event id in {module['name']}: {event_id}")
            seen_events.add(event_id)
            pair = (module_id, event_id)
            if pair in seen_pairs:
                raise ValueError(f"duplicate module/event pair: {pair}")
            seen_pairs.add(pair)
            if not event.get("name"):
                raise ValueError(f"event 0x{event_id:04X} in {module['name']} is missing name")
            event_name = str(event["name"])
            if not re.fullmatch(r"[A-Z][A-Z0-9_]{0,63}", event_name):
                raise ValueError(f"invalid event name: {event_name}")
            if event_name in seen_event_names:
                raise ValueError(f"duplicate event name in {module_name}: {event_name}")
            seen_event_names.add(event_name)
            if (
                "description" in event
                and event["description"] is not None
                and not isinstance(event["description"], str)
            ):
                raise ValueError(f"description for {module_name}.{event_name} must be a string")
            if "text" in event and not isinstance(event["text"], str):
                raise ValueError(f"text for {module_name}.{event_name} must be a string")
            placeholders = re.findall(r"\{([A-Za-z_][A-Za-z0-9_]*):([A-Za-z0-9_]+)\}", event.get("text", ""))
            args = event.get("args", [])
            if not isinstance(args, list):
                raise ValueError(f"args for {module_name}.{event_name} must be an array")
            if any(not isinstance(arg, dict) for arg in args):
                raise ValueError(f"args for {module_name}.{event_name} must contain objects")
            for index, arg in enumerate(args):
                if "name" not in arg or not isinstance(arg["name"], str):
                    raise ValueError(f"argument {index} for {module_name}.{event_name} must have a string name")
            expected = [(str(arg.get("name")), str(arg.get("type"))) for arg in args]
            if placeholders != expected:
                raise ValueError(f"placeholder/args mismatch for {module_name}.{event_name}")
            for arg in args:
                arg_type = arg.get("type")
                if arg_type not in TYPE_TAG_BY_NAME:
                    raise ValueError(f"unsupported arg type for {module['name']}.{event['name']}: {arg_type}")
            level = str(event.get("level", "INFO")).upper()
            if level not in VALID_LEVELS:
                raise ValueError(f"invalid event level for {module_name}.{event_name}: {level}")
            metadata_overhead = {"none": 0, "file_id": 6, "hash": 8}.get(location_mode)
            if metadata_overhead is None:
                raise ValueError(f"unsupported location mode: {location_mode}")
            if _payload_size(_normalized_args(args)) + metadata_overhead > max_payload_len:
                raise ValueError(f"payload exceeds AXIOM_MAX_PAYLOAD_LEN for {module_name}.{event_name}")
    return modules


def generate_assets(
    events_path: str | Path,
    output_dir: str | Path,
    check_only: bool = False,
    source_id_map: str | Path | None = None,
    location_mode: str = "none",
    location_function: bool = False,
    source_map: dict[str, Any] | None = None,
) -> dict[str, Path]:
    """Generate C headers and dictionary JSON from an event source."""
    data = load_event_source(events_path)
    modules = validate_event_source(data, location_mode=location_mode)
    effective_source_map = source_map or generate_source_map(
        None,
        Path(events_path).resolve().parent,
        source_id_map,
        location_mode,
    )
    out_dir = Path(output_dir)
    if check_only:
        return {}
    out_dir.mkdir(parents=True, exist_ok=True)

    dictionary = _emit_dictionary(data, modules)
    dictionary_path = out_dir / "dictionary.json"
    dictionary_path.write_text(json.dumps(dictionary, indent=2), encoding="utf-8")
    source_map_path = out_dir / "source_map.json"
    source_map_path.write_text(json.dumps(effective_source_map, indent=2), encoding="utf-8")
    metadata_id = metadata_id_for_data(
        dictionary,
        effective_source_map,
        location_contract(location_mode, location_function),
    )

    modules_header = out_dir / "axiom_modules_generated.h"
    events_header = out_dir / "axiom_events_generated.h"
    metadata_id_header = out_dir / "axiom_metadata_id_generated.h"
    modules_header.write_text(_emit_modules_header(modules), encoding="utf-8")
    events_header.write_text(_emit_events_header(modules), encoding="utf-8")
    metadata_id_header.write_text(_emit_metadata_id_header(metadata_id), encoding="utf-8")
    dep_path = out_dir / ".axiom_codegen.d"
    dep_path.write_text(
        f"{events_header.as_posix()} {modules_header.as_posix()} {metadata_id_header.as_posix()} {dictionary_path.as_posix()} {source_map_path.as_posix()}: {Path(events_path).as_posix()}\n",
        encoding="utf-8",
    )
    return {
        "dictionary": dictionary_path,
        "source_map": source_map_path,
        "modules_header": modules_header,
        "events_header": events_header,
        "metadata_id_header": metadata_id_header,
        "depfile": dep_path,
    }


def _emit_dictionary(data: dict[str, Any], modules: list[dict[str, Any]]) -> dict[str, Any]:
    output: dict[str, Any] = {
        "version": str(data.get("version", "1.0")),
        "modules": {},
    }
    for module in modules:
        module_id = parse_int(module["id"])
        module_entry = {
            "name": str(module["name"]),
            "description": str(module.get("description", "")),
            "events": {},
        }
        for event in module.get("events", []):
            event_id = parse_int(event["id"])
            args = []
            for arg in event.get("args", []) or []:
                arg_type = str(arg["type"])
                args.append(
                    {
                        "name": str(arg["name"]),
                        "type": arg_type,
                        "type_tag": TYPE_TAG_BY_NAME[arg_type],
                        "wire_size": WIRE_SIZE_BY_NAME[arg_type],
                    }
                )
            module_entry["events"][str(event_id)] = {
                "name": str(event["name"]),
                "level": str(event.get("level", "INFO")).upper(),
                "text": str(event.get("text", "")),
                "args": args,
                "max_payload_size": _payload_size(args),
            }
        output["modules"][str(module_id)] = module_entry
    return output


def _emit_modules_header(modules: list[dict[str, Any]]) -> str:
    lines = [
        "/* Automatically generated by axiom-codegen. Do not edit. */",
        "#ifndef AXIOM_MODULES_GENERATED_H",
        "#define AXIOM_MODULES_GENERATED_H",
        "",
    ]
    for module in modules:
        name = _macro_name(str(module["name"]))
        module_id = parse_int(module["id"])
        lines.append(f"#define AXIOM_MODULE_{name} 0x{module_id:02X}u")
        lines.append(f"#define _AXIOM_MODULE_ID_{name} 0x{module_id:02X}u")
    lines.extend(["", "#endif /* AXIOM_MODULES_GENERATED_H */", ""])
    return "\n".join(lines)


def _emit_events_header(modules: list[dict[str, Any]]) -> str:
    lines = [
        "/* Automatically generated by axiom-codegen. Do not edit. */",
        "#ifndef AXIOM_EVENTS_GENERATED_H",
        "#define AXIOM_EVENTS_GENERATED_H",
        "",
    ]
    for module in modules:
        module_name = _macro_name(str(module["name"]))
        for event in module.get("events", []):
            event_name = _macro_name(str(event["name"]))
            event_id = parse_int(event["id"])
            argc = len(event.get("args", []) or [])
            level = str(event.get("level", "INFO")).upper()
            lines.append(f"#define AXIOM_EVENT_{module_name}_{event_name} 0x{event_id:04X}u")
            lines.append(f"#define _AXIOM_EVENT_ID_{module_name}_{event_name} 0x{event_id:04X}u")
            lines.append(f"#define _AXIOM_EVENT_ARGC_{module_name}_{event_name} {argc}")
            lines.append(f"#define _AXIOM_EVENT_LEVEL_{module_name}_{event_name} AXIOM_LEVEL_{level}")
            lines.append("")
    lines.extend(["#endif /* AXIOM_EVENTS_GENERATED_H */", ""])
    return "\n".join(lines)


def _emit_metadata_id_header(metadata_id: str) -> str:
    values = ", ".join(f"0x{metadata_id[index:index + 2].upper()}u" for index in range(0, 16, 2))
    return "\n".join(
        [
            "/* Automatically generated by axiom-codegen. Do not edit. */",
            "#ifndef AXIOM_METADATA_ID_GENERATED_H",
            "#define AXIOM_METADATA_ID_GENERATED_H",
            "",
            f"#define AXIOM_GENERATED_METADATA_ID_HEX \"{metadata_id}\"",
            f"static const uint8_t AXIOM_GENERATED_METADATA_ID[AXIOM_METADATA_ID_LEN] = {{{values}}};",
            "#define AXIOM_EMIT_METADATA_ID() axiom_emit_metadata_id(AXIOM_GENERATED_METADATA_ID)",
            "",
            "#endif /* AXIOM_METADATA_ID_GENERATED_H */",
            "",
        ]
    )


def _normalized_args(args: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return [
        {
            "wire_size": WIRE_SIZE_BY_NAME[str(arg["type"])],
        }
        for arg in args
    ]


def _payload_size(args: list[dict[str, Any]]) -> int:
    size = 0
    for arg in args:
        wire_size = arg.get("wire_size")
        if wire_size is None:
            continue
        size += int(wire_size)
    return size


def _macro_name(value: str) -> str:
    return "".join(ch if ch.isalnum() else "_" for ch in value.upper())
