# nRF52 integration map (AxiomTrace 1.0 RC)

This directory is a short integration map, not a tested Nordic SDK or SEGGER
RTT implementation. It deliberately does not search SDK paths, add ARM flags,
or ship register/RTT driver sources.

Start from the one compile-checked callback template:

```text
baremetal/examples/example_custom_port.c
```

In the nRF5 SDK application, replace its timer/IRQ/Flash callbacks and the
Backend write/ready/flush callbacks with the SDK and SEGGER RTT calls. Build
the core with the application-provided Port:

```sh
cmake -S . -B build-nrf52 \
  -DAXIOM_PLATFORM=cortex-m -DAXIOM_PORT_SOURCE=NONE \
  -DAXIOM_BUILD_TESTS=OFF -DAXIOM_BUILD_EXAMPLES=OFF \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/arm-toolchain.cmake
cmake --build build-nrf52
```

The integrator supplies the exact nRF52 SKU, clock source, startup/linker
files, `SEGGER_RTT.c`/header and buffer policy, reset-cause mapping, and
target timing measurement. Register the custom Backend before the first event;
the normal callback must not block or allocate. The release library does not
provide a universal RTT backend.
