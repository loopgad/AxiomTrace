> [English](CHANGELOG.md) | [简体中文](CHANGELOG_zh.md)

# Changelog

All notable changes to AxiomTrace will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- **BREAKING**: Complete v1.0 architecture refactor. The project pivots from v2.0 "text-first, Flash-default" design to v1.0 "Event Record as the single source of truth" micro-kernel model.
- Replaced all v2.0 placeholder implementations with v1.0 five-plane architecture (Semantic / Frontend / Core / Backend / Tool).
- Renamed and restructured `baremetal/` directory to align with v1.0 planes.

### Added
- `RULES.md`: Enforced development rules including hot-path prohibitions (no malloc, no printf, no Flash erase), drop-summary requirements, and mandatory issue→design→spec→golden→impl→decoder→tests→benchmark→docs→changelog workflow.
- `PLAN.md` v1.0: Frozen target architecture, release gates, and design philosophy.
- `ROUTE.md` v1.0: Development stages from v0.1-core through v0.9-rc to v1.0-stable.
- `spec/api_reference.md`: Frontend API reference for `AX_LOG`, `AX_EVT`, `AX_PROBE`, `AX_FAULT`, `AX_KV`.
- `spec/decoder_protocol.md`: Decoder input/output protocol and dictionary format specification.

### Removed
- v2.0 `axiom_storage_t` unified storage abstraction (replaced by Backend Contract).
- v2.0 `AXIOM_LOG("fmt", ...)` printf-like runtime string hashing (replaced by structured Event Record macros).
- v2.0 linker-section auto-registration backend system (replaced by explicit `axiom_backend_register()`).

## [0.1.0] - TBD

### Added
- Core Plane: Lock-free IRQ-safe RAM Ring buffer with compile-time configurable size and policy (DROP / OVERWRITE).
- Core Plane: Binary Event Record encoder with C11 `_Generic` type-safe payload encoding and self-describing type tags.
- Core Plane: CRC-16/CCITT-FALSE with 256-byte ROM lookup table.
- Core Plane: Compressed relative timestamp (delta encoding).
- Core Plane: Runtime level/module filter and drop statistics with DROP_SUMMARY generation.
- Frontend Plane: `AX_LOG`, `AX_EVT`, `AX_PROBE`, `AX_FAULT`, `AX_KV` macros with DEV/FIELD/PROD Profile compile-time trimming.
- Backend Plane: Unified `axiom_backend_t` contract with `write/ready/flush/panic_write/on_drop`.
- Backend Plane: Memory, UART, USB CDC, RTT, SWO/ITM, CAN-FD backend templates.
- Fault Capsule: Pre-window freeze, post-window capture, register snapshot, firmware hash, capsule CRC, flash commit.
- Tool Plane: Python decoder, text renderer, JSON exporter, capsule reporter, golden frame manager, benchmark tool.
- Host unit tests and Python regression tests.
- Single-file library generator (`tool/scripts/amalgamate.py`).
