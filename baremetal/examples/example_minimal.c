/* ============================================================================
 * AxiomTrace Minimal Example
 * ============================================================================
 * Zero-config, 3 lines to first structured log.
 *
 * Build with CMake (recommended):
 *   cmake -B build -DAXIOM_PLATFORM=host
 *   cmake --build build --target example_minimal
 *
 * Build with Make (from baremetal directory):
 *   make example_minimal
 * ============================================================================ */

#include "axiomtrace.h"
#include <stdio.h>

int main(void) {
    axiom_init();

    /* Structured event: module=0x03, event=0x01, payload=one uint16_t */
    AX_EVT(INFO, 0x03, 0x01, (uint16_t)3200);

    /* Fault event */
    AX_FAULT(0x03, 0xFF, (uint8_t)1, (int16_t)(-42));

    printf("Minimal example complete.\n");
    return 0;
}
