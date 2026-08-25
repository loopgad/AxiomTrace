"""Dictionary loading and semantic lookup for AxiomTrace tools."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any


TYPE_TAG_BY_NAME = {
    "bool": 0x00,
    "u8": 0x01,
    "i8": 0x02,
    "u16": 0x03,
    "i16": 0x04,
    "u32": 0x05,
    "i32": 0x06,
    "f32": 0x07,
    "ts": 0x08,
    "timestamp": 0x08,
    "bytes": 0x09,
}

VALID_LEVELS = frozenset({"DEBUG", "INFO", "WARN", "ERROR", "FAULT"})

WIRE_SIZE_BY_NAME = {
    "bool": 1,
    "u8": 1,
    "i8": 1,
    "u16": 2,
    "i16": 2,
    "u32": 4,
    "i32": 4,
    "f32": 4,
    "ts": 4,
    "timestamp": 4,
    "bytes": None,
}


@dataclass(frozen=True)
class EventMetadata:
    """Semantic metadata for a decoded event."""

    module_id: int
    event_id: int
    module_name: str
    event_name: str
    level: str | None
    text: str | None
    args: list[dict[str, Any]]
    raw: dict[str, Any]


def parse_int(value: Any) -> int:
    """Parse decimal or hex integer values from YAML/JSON metadata."""
    if isinstance(value, bool):
        raise TypeError("expected integer-like value, got bool")
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        return int(value, 0)
    raise TypeError(f"expected integer-like value, got {type(value).__name__}")


def event_key(module_id: int, event_id: int) -> str:
    """Return the canonical dictionary key for a module/event tuple."""
    return f"0x{module_id:02X}:0x{event_id:04X}"


class EventDictionary:
    """Normalized metadata dictionary supporting legacy and current schemas."""

    def __init__(self, data: dict[str, Any], path: Path | None = None):
        if not isinstance(data, dict):
            raise ValueError("dictionary root must be an object")
        self.data = data
        self.path = path
        self.events = self._normalize(data)

    def find_event(self, module_id: int, event_id: int) -> EventMetadata | None:
        """Return metadata for a module/event pair, or None if unknown."""
        return self.events.get((module_id, event_id))

    @staticmethod
    def _normalize(data: dict[str, Any]) -> dict[tuple[int, int], EventMetadata]:
        if "modules" in data:
            return _normalize_modules(data["modules"])
        if "dictionary" in data:
            if not isinstance(data["dictionary"], dict):
                raise ValueError("dictionary field must be an object")
            dictionary = data["dictionary"]
            if "modules" in dictionary:
                return _normalize_modules(dictionary["modules"])
            return _normalize_nested_dictionary(dictionary)
        if "events" in data:
            return _normalize_flat_events(data["events"])
        return {}


def _normalize_modules(modules: Any) -> dict[tuple[int, int], EventMetadata]:
    events: dict[tuple[int, int], EventMetadata] = {}
    if not isinstance(modules, (dict, list)):
        raise ValueError("dictionary modules must be an object or array")
    iterable = modules.items() if isinstance(modules, dict) else enumerate(modules)
    for module_key, module in iterable:
        if not isinstance(module, dict):
            raise ValueError(f"dictionary module {module_key!r} must be an object")
        module_id = _parse_id(module.get("id", module_key), "module")
        if "name" in module and not isinstance(module["name"], str):
            raise ValueError(f"module {module_id} name must be a string")
        if (
            "description" in module
            and module["description"] is not None
            and not isinstance(module["description"], str)
        ):
            raise ValueError(f"module {module_id} description must be a string")
        module_name = module.get("name", f"MODULE_{module_id:02X}")
        if not module_name:
            raise ValueError(f"module {module_id} is missing name")
        _validate_level(module.get("level"), f"module {module_id}")
        module_events = module.get("events", {})
        if not isinstance(module_events, (dict, list)):
            raise ValueError(f"module {module_name} events must be an object or array")
        event_iter = module_events.items() if isinstance(module_events, dict) else enumerate(module_events)
        for event_key_value, event in event_iter:
            if not isinstance(event, dict):
                raise ValueError(f"event {event_key_value!r} in {module_name} must be an object")
            event_id = _parse_id(event.get("id", event_key_value), "event")
            _validate_event(event, f"{module_name}.{event_id}")
            if (module_id, event_id) in events:
                raise ValueError(f"duplicate module/event pair: {(module_id, event_id)}")
            args = _normalize_args(event["args"] if "args" in event else [])
            events[(module_id, event_id)] = EventMetadata(
                module_id=module_id,
                event_id=event_id,
                module_name=module_name,
                event_name=str(event.get("name", f"EVENT_{event_id:04X}")),
                level=_upper_or_none(event.get("level")),
                text=event.get("text"),
                args=args,
                raw=event,
            )
    return events


def _normalize_nested_dictionary(dictionary: dict[str, Any]) -> dict[tuple[int, int], EventMetadata]:
    events: dict[tuple[int, int], EventMetadata] = {}
    for module_key, module in dictionary.items():
        if not isinstance(module, dict):
            raise ValueError(f"dictionary module {module_key!r} must be an object")
        module_id = _parse_id(module_key, "module")
        if "name" in module and not isinstance(module["name"], str):
            raise ValueError(f"module {module_id} name must be a string")
        if (
            "description" in module
            and module["description"] is not None
            and not isinstance(module["description"], str)
        ):
            raise ValueError(f"module {module_id} description must be a string")
        module_name = module.get("name", f"MODULE_{module_id:02X}")
        _validate_level(module.get("level"), f"module {module_name}")
        module_events = module.get("events", {})
        if not isinstance(module_events, dict):
            raise ValueError(f"module {module_name} events must be an object")
        for event_key_value, event in module_events.items():
            if not isinstance(event, dict):
                raise ValueError(f"event {event_key_value!r} in {module_name} must be an object")
            event_id = _parse_id(event_key_value, "event")
            _validate_event(event, f"{module_name}.{event_id}")
            if (module_id, event_id) in events:
                raise ValueError(f"duplicate module/event pair: {(module_id, event_id)}")
            events[(module_id, event_id)] = EventMetadata(
                module_id=module_id,
                event_id=event_id,
                module_name=module_name,
                event_name=str(event.get("name", f"EVENT_{event_id:04X}")),
                level=_upper_or_none(event.get("level")),
                text=event.get("text"),
                args=_normalize_args(event["args"] if "args" in event else []),
                raw=event,
            )
    return events


def _normalize_flat_events(flat_events: dict[str, Any]) -> dict[tuple[int, int], EventMetadata]:
    events: dict[tuple[int, int], EventMetadata] = {}
    if not isinstance(flat_events, dict):
        raise ValueError("dictionary events must be an object")
    for key, event in flat_events.items():
        if not isinstance(key, str) or ":" not in key:
            raise ValueError(f"invalid flat event key: {key!r}")
        if not isinstance(event, dict):
            raise ValueError(f"flat event {key!r} must be an object")
        module_text, event_text = key.split(":", 1)
        module_id = _parse_id(module_text, "module")
        event_id = _parse_id(event_text, "event")
        _validate_event(event, key)
        if (module_id, event_id) in events:
            raise ValueError(f"duplicate module/event pair: {(module_id, event_id)}")
        if "module" in event and not isinstance(event["module"], str):
            raise ValueError(f"module name for {key} must be a string")
        module_name = event.get("module", f"MODULE_{module_id:02X}")
        events[(module_id, event_id)] = EventMetadata(
            module_id=module_id,
            event_id=event_id,
            module_name=module_name,
            event_name=str(event.get("name", f"EVENT_{event_id:04X}")),
            level=_upper_or_none(event.get("level")),
            text=event.get("text") if event.get("text") is not None else event.get("description"),
            args=_normalize_args(event["args"] if "args" in event else []),
            raw=event,
        )
    return events


def _normalize_args(args: Any) -> list[dict[str, Any]]:
    normalized: list[dict[str, Any]] = []
    if not isinstance(args, list):
        raise ValueError("event args must be an array")
    for idx, arg in enumerate(args):
        if not isinstance(arg, dict):
            raise ValueError(f"event argument {idx} must be an object")
        if "name" in arg and not isinstance(arg["name"], str):
            raise ValueError(f"event argument {idx} name must be a string")
        arg_type = arg.get("type", "unknown")
        if not isinstance(arg_type, str):
            raise ValueError(f"event argument {idx} type must be a string")
        if arg_type not in TYPE_TAG_BY_NAME:
            raise ValueError(f"unsupported event argument type: {arg_type}")
        expected_tag = TYPE_TAG_BY_NAME[arg_type]
        if "type_tag" in arg:
            if (
                isinstance(arg["type_tag"], bool)
                or not isinstance(arg["type_tag"], int)
                or arg["type_tag"] != expected_tag
            ):
                raise ValueError(f"event argument {idx} has an invalid type tag")
        expected_size = WIRE_SIZE_BY_NAME[arg_type]
        if "wire_size" in arg:
            wire_size = arg["wire_size"]
            if expected_size is None:
                valid_wire_size = wire_size is None
            else:
                valid_wire_size = (
                    isinstance(wire_size, int)
                    and not isinstance(wire_size, bool)
                    and wire_size == expected_size
                )
            if not valid_wire_size:
                raise ValueError(f"event argument {idx} has an invalid wire size")
        normalized.append(
            {
                **arg,
                "name": str(arg.get("name", f"arg{idx}")),
                "type": arg_type,
                "type_tag": arg.get("type_tag", TYPE_TAG_BY_NAME.get(arg_type)),
                "wire_size": arg.get("wire_size", WIRE_SIZE_BY_NAME.get(arg_type)),
            }
        )
    return normalized


def _parse_id(value: Any, kind: str) -> int:
    try:
        parsed = parse_int(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{kind} id must be an integer: {value!r}") from exc
    limit = 0xFF if kind == "module" else 0xFFFF
    if not 0 <= parsed <= limit:
        raise ValueError(f"{kind} id out of range: {parsed}")
    return parsed


def _validate_level(value: Any, context: str) -> None:
    if value is None:
        return
    if not isinstance(value, str):
        raise ValueError(f"level for {context} must be a string")
    level = value.upper()
    if level not in VALID_LEVELS:
        raise ValueError(f"invalid level for {context}: {value!r}")


def _validate_event(event: dict[str, Any], context: str) -> None:
    if "name" in event and not isinstance(event["name"], str):
        raise ValueError(f"event name for {context} must be a string")
    if "text" in event and event["text"] is not None and not isinstance(event["text"], str):
        raise ValueError(f"event text for {context} must be a string")
    if "description" in event and event["description"] is not None and not isinstance(event["description"], str):
        raise ValueError(f"event description for {context} must be a string")
    _validate_level(event.get("level"), context)
    _normalize_args(event["args"] if "args" in event else [])


def _upper_or_none(value: Any) -> str | None:
    if value is None:
        return None
    return str(value).upper()


def load_dictionary(path: str | Path | None) -> EventDictionary | None:
    """Load a JSON dictionary file, returning None when no path is provided."""
    if path is None:
        return None
    dict_path = Path(path)
    with dict_path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    return EventDictionary(data, dict_path)
