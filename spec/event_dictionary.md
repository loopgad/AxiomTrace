> [English](event_dictionary.md) | [简体中文](event_dictionary_zh.md)

# AxiomTrace Event Dictionary Specification

## 1. Purpose

The event dictionary is the authoritative mapping from `(module_id, event_id)` to human-readable metadata. It lives on the host (build machine / PC) and is never flashed to the target as text.

## 2. YAML Schema

```yaml
version: "1.0"
dictionary:
  <module_id_hex>:
    name: "MODULE_NAME"
    description: "Human readable module description"
    events:
      <event_id_hex>:
        level: "INFO|WARN|ERROR|FAULT|DEBUG"
        name: "EVENT_NAME"
        text: "Human readable template with {type} placeholders"
        args:
          - name: "arg0_name"
            type: "u8|i8|u16|i16|u32|i32|f32|ts|bytes"
          - name: "arg1_name"
            type: "..."
```

## 3. Placeholder Syntax

The `text` field uses `{name:type}` placeholders. Supported types:

| Placeholder | Payload Type Tag | Description            |
|-------------|------------------|------------------------|
| `{name:u8}`  | 0x01             | Unsigned 8-bit         |
| `{name:i8}`  | 0x02             | Signed 8-bit           |
| `{name:u16}` | 0x03             | Unsigned 16-bit        |
| `{name:i16}` | 0x04             | Signed 16-bit          |
| `{name:u32}` | 0x05             | Unsigned 32-bit        |
| `{name:i32}` | 0x06             | Signed 32-bit          |
| `{name:f32}` | 0x07             | IEEE-754 float         |
| `{name:ts}`  | 0x08             | Compressed timestamp   |
| `{name:bytes}`| 0x09            | Byte array             |

## 4. Code Generation

The `codegen` tool reads the YAML dictionary and emits:

1. `axiom_events_generated.h` — C macros mapping `MODULE_EVENT_NAME` to `(module_id, event_id)`.
2. `axiom_modules_generated.h` — Module ID enum.
3. `dictionary.json` — Compact JSON for decoder consumption.

## 5. Validation Rules

- Every `(module_id, event_id)` must be unique.
- `level` must match the dictionary entry; the firmware encoder does not enforce this, but the validator warns on mismatch.
- `args` type sequence must match the order of encoded payload fields.
- `text` placeholders must reference exactly the declared `args`.

