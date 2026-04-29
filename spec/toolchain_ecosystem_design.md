> [English](toolchain_ecosystem_design.md) | [简体中文](toolchain_ecosystem_design_zh.md)

# AxiomTrace Toolchain and Ecosystem Design Document

> **Version**: v0.4-draft  
> **Author**: toolchain-ecosystem-designer  
> **Goal**: Define the Python toolchain, Golden Frame validation process, and ecosystem integration standards to ensure "one-click readability restoration" on the host side.

---

## Table of Contents

1. [YAML Dictionary Schema](#1-yaml-dictionary-schema)
2. [Codegen Tool Design](#2-codegen-tool-design)
3. [Decoder Tool Design](#3-decoder-tool-design)
4. [Validator Tool Design](#4-validator-tool-design)
5. [Benchmark Tool Design](#5-benchmark-tool-design)
6. [RTOS / Linux Integration Documentation Framework](#6-rtos--linux-integration-documentation-framework)
7. [One-Click Workflow Experience Summary](#7-one-click-workflow-experience-summary)

---

## 1. YAML Dictionary Schema

### 1.1 Top-Level Structure

```yaml
version: "1.0"                 # Required, dictionary format version, currently only "1.0" supported
dictionary:
  modules: []                  # Required, list of modules, must contain at least one module
  metadata: {}                 # Optional, build-level metadata
```

### 1.2 Field Definitions (Type and Constraints)

| Path | Type | Required | Constraints / Description |
|------|------|----------|---------------------------|
| `version` | `string` | Yes | Enum `"1.0"` |
| `dictionary` | `object` | Yes | Root container |
| `dictionary.modules` | `array[module]` | Yes | Length `1..256` |
| `module.id` | `uint8` (YAML supports `0x03` or `3`) | Yes | Unique, range `0x00..0xFF` |
| `module.name` | `string` | Yes | `^[A-Z][A-Z0-9_]{0,31}$`, uppercase with underscores |
| `module.description` | `string` | No | Length `0..256` |
| `module.events` | `array[event]` | Yes | Length `1..65535` (practically limited by ROM) |
| `event.id` | `uint16` | Yes | Unique within module, range `0x0000..0xFFFF` |
| `event.name` | `string` | Yes | `^[A-Z][A-Z0-9_]{0,63}$` |
| `event.level` | `string` | Yes | Enum: `DEBUG`, `INFO`, `WARN`, `ERROR`, `FAULT` |
| `event.text` | `string` | Yes | Human-readable template with `{name:type}` placeholders |
| `event.args` | `array[arg]` | No | Required if `text` has placeholders; order must match |
| `arg.name` | `string` | Yes | Must strictly match placeholder name in `text` |
| `arg.type` | `string` | Yes | See table below |

### 1.3 Supported Argument Types

| `arg.type` | Payload Type Tag | C Type | Wire Size | Description |
|------------|------------------|--------|-----------|-------------|
| `bool` | `0x00` | `bool` | 1 | `0` or `1` |
| `u8` | `0x01` | `uint8_t` | 1 | Unsigned 8-bit |
| `i8` | `0x02` | `int8_t` | 1 | Signed 8-bit |
| `u16` | `0x03` | `uint16_t` | 2 | Little-endian |
| `i16` | `0x04` | `int16_t` | 2 | Little-endian two's complement |
| `u32` | `0x05` | `uint32_t` | 4 | Little-endian |
| `i32` | `0x06` | `int32_t` | 4 | Little-endian two's complement |
| `f32` | `0x07` | `float` | 4 | IEEE-754 single-precision LE |
| `ts` | `0x08` | `uint32_t` | 4 | Compressed relative timestamp |
| `bytes` | `0x09` | `uint8_t[]` | `1+N` | 1-byte length prefix + raw bytes |

### 1.4 Validation Rules

1. **Global Uniqueness**: The `(module.id, event.id)` tuple must be unique across the entire dictionary.
2. **Naming Uniqueness**: `module.name` is globally unique; `event.name` is unique within the module.
3. **Placeholder Consistency**: All `{name:type}` in `event.text` must satisfy:
   - `name` exists in `event.args`;
   - `type` strictly matches the corresponding `arg.type`;
   - Order of appearance matches the `args` array order.
4. **Level Matching**: `event.level` must match the event's semantics (Validator warns on mismatch, Codegen generates corresponding macros).
5. **Payload Limit**: The maximum payload length calculated from `args` must not exceed `255` bytes.

### 1.5 YAML → JSON Collateral Conversion Rules

Codegen reads YAML and outputs `dictionary.json` (Decoder's collateral). Conversion rules:

- All key names maintain snake_case.
- `module.id` and `event.id` are output as decimal integers (easier for Decoder indexing).
- `level` is converted to lowercase strings.
- Placeholders in `text` are extracted as a structured array `placeholders`; original `text` is preserved for human reading.
- `max_payload_size` is calculated and attached to each event.

---

## 2. Codegen Tool Design

### 2.1 Command Line Interface

```bash
python -m axiom_codegen \
    --input events.yaml \
    --output-dir gen/ \
    [--lang c] \
    [--check] \
    [--watch]
```

| Option | Description |
|--------|-------------|
| `--input` / `-i` | Path to input YAML dictionary (Required) |
| `--output-dir` / `-o` | Output directory (default `gen/`) |
| `--lang` | Target language, currently only `c` (default) |
| `--check` | Execute validation only, no file generation; non-zero exit code on failure |
| `--watch` | Watch YAML changes and auto-regenerate (Dev mode) |

### 2.2 Generated Assets List

| Asset Name | Description | Consumer |
|------------|-------------|----------|
| `axiom_events_generated.h` | C Macros: `MODULE_EVENT_NAME` → `(module_id, event_id)` | Firmware Compilation |
| `axiom_modules_generated.h` | Module ID Enums / Constants | Firmware Compilation |
| `dictionary.json` | Compact JSON Collateral | Decoder / Validator |
| `.axiom_codegen.d` | Dependency file (records input timestamps) | Incremental Build |

### 2.3 `axiom_events_generated.h` Generation Template

```c
/* Automatically generated, do not edit manually */
#ifndef AXIOM_EVENTS_GENERATED_H
#define AXIOM_EVENTS_GENERATED_H

#define _AXIOM_EVENTS_GENERATED_H 1

/* ---------------------------------------------------------------------------
 * Module IDs
 * --------------------------------------------------------------------------- */
#define _AXIOM_MODULE_ID_MOTOR   0x03u
#define _AXIOM_MODULE_ID_SENSOR  0x05u
/* ... */

/* ---------------------------------------------------------------------------
 * Event IDs & Convenience Macros
 * --------------------------------------------------------------------------- */
#define _AXIOM_EVENT_ID_MOTOR_START   0x0001u
#define _AXIOM_EVENT_ID_MOTOR_STOP    0x0002u

/* Meta-info for argument count (used for static assertions) */
#define _AXIOM_EVENT_ARGC_MOTOR_START 2

/* Pre-compiled level mask (optional optimization) */
#define _AXIOM_EVENT_LEVEL_MOTOR_START AXIOM_LEVEL_INFO

/* Helper: Convert MODULE_EVENT to string (for debugging) */
#define _AXIOM_EVENT_NAME_MOTOR_START "MOTOR_START"

#endif /* AXIOM_EVENTS_GENERATED_H */
```

### 2.4 Incremental Build Support

Codegen writes a `.axiom_codegen.d` dependency file in the output directory to support incremental builds in CMake via `add_custom_command`.

---

## 3. Decoder Tool Design

### 3.1 Input Sources

- **Raw Binary**: `.bin` files dumped from serial/emulator.
- **Serial Stream**: Real-time UART / USB CDC input.
- **Hex Dump**: Text-based hex (e.g., `xxd` output).

### 3.2 Processing Pipeline

```
Input (bin / serial / hex)
    │
    ▼
[ COBS Decoding ]  ──► Extract frames by 0x00 delimiter (Skip for Memory/SWO)
    │
    ▼
[ CRC-16/CCITT Check ] ──► Mark CORRUPT on failure, continue syncing
    │
    ▼
[ Header Parsing ] ──► 8-byte packed struct → Field extraction
    │
    ▼
[ Payload Deserialization ] ──► Parse arguments one by one by type tag
    │
    ▼
[ Dictionary Lookup ] ──► Retrieve metadata from dictionary.json by (module_id, event_id)
    │
    ▼
[ Formatted Output ]
```

### 3.3 Output Formats

- **Human-Readable Text** (Default, `dmesg`-like).
- **JSON Lines** (`--format jsonl`): Each line is a JSON object.
- **Chrome Trace Event Format**: For timeline visualization.

---

## 4. Validator Tool Design

Validator performs **Golden Frame Validation** by comparing byte-level binary frames (`frame.bin`) against expected metadata and decoder outputs.

### 4.1 Consistency Check Rules

- **CRC Validation**: Recompute CRC-16 and compare with frame tail.
- **Endianness**: Verify multi-byte fields are little-endian.
- **Alignment**: Ensure Header is 8-byte packed and Payload is tightly arranged.
- **Type Matching**: Payload type tags must match dictionary `args`.
- **Level Matching**: Header `level` must match dictionary `event.level`.

---

## 5. Benchmark Tool Design

Measures Cycles per log, RAM/ROM footprint, and Backend throughput. Supports regression detection via baseline comparison and automatic alerting in CI.

---

## 6. RTOS / Linux Integration Documentation Framework

Standardized integration guides for Zephyr, FreeRTOS, systemd journald, and OpenTelemetry.

---

## 7. One-Click Workflow Experience Summary

The ultimate goal of the AxiomTrace toolchain: **A developer executes a single command on the host and gains a complete path from firmware binary to human-readable logs.**

1. **Single Source of Truth**: `events.yaml` defines everything.
2. **Host-side Readability**: Minimize firmware ROM/RAM by offloading strings.
3. **Collateral Mechanism**: Shared `dictionary.json` for all tools.
4. **Pipeline Friendly**: JSONL output for downstream integration.
5. **CI Native**: Machine-readable reports (JUnit XML / JSON).
