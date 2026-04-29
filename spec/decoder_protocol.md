> [English](decoder_protocol.md) | [简体中文](decoder_protocol_zh.md)

# AxiomTrace Decoder Protocol Specification

> Version: v1.0 | Status: **Building (v0.7-toolchain finalized)**

---

## 1. Overview

The AxiomTrace decoder toolchain runs on the host (Linux/macOS/Windows) and converts binary Event Records / Fault Capsules into human-readable forms.

**Components**:

- `axiom_decoder.py` — Parses binary frames into structured objects
- `axiom_render.py` — Renders structured objects to text or JSON
- `dict_loader.py` — Loads and validates event dictionaries

---

## 2. Input Formats

### 2.1 Raw Binary Stream

A concatenation of complete v1.0 frames. For UART/USB transports, the stream is COBS-encoded with `0x00` delimiters.

```bash
python -m axiom_decoder --input trace.bin --format binary --dict dictionary.json
```

### 2.2 Memory Dump

A raw memory dump containing the ring buffer metadata + raw ring data:

```
┌─────────────┬─────────────┬─────────────┬─────────────────────────────┐
│ head (4B LE)│ tail (4B LE)│ cap (4B LE) │ raw ring data (cap bytes)   │
└─────────────┴─────────────┴─────────────┴─────────────────────────────┘
```

```bash
python -m axiom_decoder --input trace_ram.bin --format memory --dict dictionary.json
```

### 2.3 Capsule Binary

A fault capsule committed to Flash. See `spec/fault_capsule.md` for layout.

```bash
python -m axiom_decoder --input capsule.bin --format capsule --dict dictionary.json
```

---

## 3. Output Formats

### 3.1 Text Render

Default output. Uses dictionary templates to fill placeholders:

```
[00:00:01.234] [INFO] [MOTOR] START: rpm=3200
[00:00:05.678] [WARN] [MOTOR] CURRENT_OVER_LIMIT: phase=2, current=1520, limit=1200
```

Template syntax in dictionary:

```json
{
  "text": "motor current over limit: phase={u8}, current={i16}, limit={i16}"
}
```

Type hints (`{u8}`, `{i16}`, `{f32}`) must match the decoded payload type tags. Mismatch triggers a validator warning.

### 3.2 JSON Export

```json
[
  {
    "timestamp": 1.234,
    "level": "INFO",
    "module": "MOTOR",
    "event": "START",
    "seq": 42,
    "payload": {
      "rpm": 3200
    }
  }
]
```

### 3.3 Capsule Report

Markdown or HTML report containing:

- Fault summary (time, reset reason, fault type)
- Register snapshot (pc, lr, sp, xpsr, etc.)
- Pre-fault event sequence
- Post-fault event sequence
- Firmware hash
- Drop statistics

---

## 4. Dictionary Format

### 4.1 JSON Dictionary

```json
{
  "version": "1.0",
  "modules": {
    "0x03": {
      "name": "MOTOR",
      "events": {
        "0x01": {
          "name": "START",
          "level": "INFO",
          "text": "motor started: rpm={u16}",
          "args": [
            { "name": "rpm", "type": "u16" }
          ]
        }
      }
    }
  }
}
```

### 4.2 Validation Rules

- Every `(module_id, event_id)` referenced in the binary must exist in the dictionary, or the decoder marks it as `UNKNOWN_EVENT`.
- `text` template type hints must match the payload's type tag sequence. Mismatch = `TYPE_MISMATCH` warning.
- `level` in dictionary should match the frame's `level` field. Mismatch = `LEVEL_MISMATCH` warning.

---

## 5. CLI Interface

```bash
# Decode binary to text
python -m axiom_decoder trace.bin --dict dict.json --output text

# Decode binary to JSON
python -m axiom_decoder trace.bin --dict dict.json --output json > trace.json

# Decode capsule to report
python -m axiom_decoder capsule.bin --dict dict.json --output report --format capsule

# Validate dictionary against golden frames
python -m axiom_decoder --validate-dict dict.json --golden golden/

# Update golden frames from encoder
python tool/golden/update_golden.py --output golden/
```

---

## 6. Error Handling

| Error Code | Description | Decoder Action |
|------------|-------------|----------------|
| `FRAME_INVALID` | CRC mismatch, bad sync, or bad version | Skip to next delimiter/sync, log warning |
| `UNKNOWN_EVENT` | (module_id, event_id) not in dictionary | Output raw IDs and hex payload, continue |
| `TYPE_MISMATCH` | Dictionary template type != payload type tag | Output with `[!]` warning, continue |
| `TRUNCATED_PAYLOAD` | payload_len > actual bytes available | Mark as truncated, skip frame |
| `UNKNOWN_TYPE_TAG` | Type tag in reserved/user range not recognized | Skip field or mark as unknown, continue |

**Critical rule**: The decoder must never crash or enter undefined state, regardless of input corruption.
