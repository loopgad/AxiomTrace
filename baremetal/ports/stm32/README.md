# STM32 integration map (AxiomTrace 1.0 RC)

This directory is a short integration map, not a tested STM32 driver. It does
not select a CPU, ABI, clock, UART, DMA stream, SDK, or linker script.

Use the single callback template first:

```text
baremetal/examples/example_custom_port.c
```

Copy that file into the STM32 firmware, replace the timestamp, IRQ critical
section, Flash, and transport TODOs with the project's CMSIS/HAL code, and
compile the core with an external Port:

```sh
cmake -S . -B build-stm32 \
  -DAXIOM_PLATFORM=cortex-m -DAXIOM_PORT_SOURCE=NONE \
  -DAXIOM_BUILD_TESTS=OFF -DAXIOM_BUILD_EXAMPLES=OFF \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/arm-toolchain.cmake
cmake --build build-stm32
```

The integrator owns the STM32 family/SKU, CMSIS or HAL version, clock source,
startup and linker files, UART/DMA framing, IRQ handoff, and target timing
measurement. Register a custom `axiom_backend_t` before the first event; the
normal backend callback must be bounded and non-blocking. The release library
ships Memory and Deferred backends only.
