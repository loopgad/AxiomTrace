# AxiomTrace Benchmark Baseline

> Version: v1.0 baseline candidate | Update Date: 2026-11-22

This document records the host-side regression benchmark used as the v1.0 release gate. Host timings are not MCU cycle guarantees; they catch regressions in encode, CRC, ring write, filtering, and overflow behavior. Target MCU cycle counts still require platform-specific measurement.

## Current Local Baseline

Command (any working GNU toolchain; MinGW gcc or clang):

```bash
cmake -S . -B build-bench -G Ninja -DAXIOM_BUILD_BENCHMARKS=ON -DAXIOM_PLATFORM=host
cmake --build build-bench
build-bench/tests/host/test_benchmark
```

Numbers below are from gcc 16.1 (MinGW) on the 2026-11-22 tree. Only B1-B3 gate
the FOC budget; the remaining scenarios print
`Budget check: informational scenario (not FOC-gated)` and exist for
regression tracking.

Verified scenarios:

| Scenario | Avg | P99.9 | Gate |
|---|---:|---:|---|
| FOC 30kHz | ~0.06 us | ~0.20 us | FOC-gated: PASS <= 3.3 us |
| FOC 30kHz max load | ~0.06 us | ~0.20 us | FOC-gated: PASS <= 3.3 us |
| FOC 30kHz small ring | ~0.06 us | ~0.10 us | FOC-gated: PASS <= 3.3 us |
| IoT sensor | ~0.05 us | ~0.10 us | informational |
| ISR nesting pressure | ~0.06 us | ~0.10 us | informational |
| Ring overflow stress (5 ring sizes) | ~0.05 us | ~0.10-0.20 us | informational |
| Full encode pipeline | ~0.06 us | ~0.20 us | informational |
| Write+flush drain (capsule eviction live) | ~0.34 us | ~0.70 us | informational |

Standalone checks:

| Check | Avg | P99 | P99.9 |
|---|---:|---:|---:|
| CRC-16, 32B | ~0.15 us | ~0.20 us | ~0.30 us |
| Critical enter+exit | ~0.02 us | ~0.10 us | ~0.10 us |

## Release Gate

- CI must run `tests/host/test_benchmark` and publish the output.
- A regression is release-blocking when FOC P99.9 exceeds the 3.3 us host gate or changes by more than 20% without a documented reason. Only B1-B3 carry the FOC gate; the write+flush drain scenario (B8) reports the per-event capsule pre-window eviction cost that the ring-full steady state hides.
- MCU estimates in benchmark output are advisory. Exact Cortex-M/RISC-V numbers must be measured in a target project before claiming target-specific cycle guarantees.
