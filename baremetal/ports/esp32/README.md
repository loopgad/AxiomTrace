# ESP32 integration map (AxiomTrace 1.0 RC)

This directory is a short ESP-IDF mapping note, not a universal ESP32
component. ESP-IDF owns target selection, UART routing, FreeRTOS, Kconfig,
startup, and linker configuration; this directory contains no component or
hardware driver sources.

Copy the one compile-checked callback template into the consuming component:

```text
baremetal/examples/example_custom_port.c
```

Replace its timer/critical/Flash callbacks with ESP-IDF APIs and implement the
Backend callbacks with the selected `uart_driver_*`, USB, or other transport.
Register those copied sources from the application component, for example:

```cmake
idf_component_register(
    SRCS "axiomtrace_impl.c" "axiomtrace_custom_port.c"
    INCLUDE_DIRS "."
    REQUIRES driver
)
```

Build the core with no implicit architecture provider when the application
owns the complete Port API. Use the ESP-IDF toolchain and `idf.py`; do not add
this reference directory as a normal Host-CMake subproject:

```sh
idf.py set-target esp32   # or esp32c3/esp32s3 as selected by the project
idf.py build
```

The integrator owns the exact chip/board pin map, clock source, FreeRTOS
context rules, transport framing, reset-cause mapping, Flash partition, and
target timing measurement. Register a custom `axiom_backend_t` before the
first event; normal writes must be bounded and non-blocking. The release
library ships Memory and Deferred backends only.
