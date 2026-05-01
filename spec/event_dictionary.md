> [English](event_dictionary.md) | [简体中文](event_dictionary_zh.md)

# AxiomTrace Event Dictionary Specification

## 1. Purpose

The event dictionary is the authoritative mapping from `(module_id, event_id)` to human-readable metadata. It lives on the host (build machine / PC) and is never flashed to the target as text.

## 2. YAML Schema

```yaml
version: "1.0"
enums:
  ENUM_NAME:
    <value>: "LABEL"
    <value>: "LABEL"
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
            enum: "ENUM_NAME" # Optional: reference to a defined enum
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

## 4. Enum Mapping

Enums provide a way to translate raw integer values into descriptive strings during decoding.

- **Definition**: Defined globally under the `enums` key. Key is the enum name, values are mappings of integers (as keys) to strings.
- **Mapping**: In the firmware, enums are passed as standard integer types (`u8`, `u32`, etc.).
- **Resolution**: The decoder uses the `enum` field in the argument definition to find the appropriate mapping. If a value is received that is not defined in the enum, the decoder should fall back to displaying the raw integer.

## 5. Code Generation

The `codegen` tool reads the YAML dictionary and emits:

1. `axiom_events_generated.h` — C macros mapping `MODULE_EVENT_NAME` to `(module_id, event_id)`.
2. `axiom_modules_generated.h` — Module ID enum.
3. `dictionary.json` — Compact JSON for decoder consumption.

## 6. Validation Rules

- Every `(module_id, event_id)` must be unique.
- `level` must match the dictionary entry; the firmware encoder does not enforce this, but the validator warns on mismatch.
- `args` type sequence must match the order of encoded payload fields.
- `text` placeholders must reference exactly the declared `args`.
- Referenced `enum` names must exist in the `enums` section.

