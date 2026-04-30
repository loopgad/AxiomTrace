/* ============================================================================
 * AxiomTrace Precompiled Library Example
 * ============================================================================
 * Demonstrates how to use AxiomTrace as a precompiled static library.
 * This allows you to:
 *   1. Distribute libaxiomtrace.a without source code
 *   2. Speed up build times for large projects
 *   3. Hide implementation details
 *
 * Build:
 *   cmake -B build \
 *     -DAXIOM_PRECOMPILED_LIB=/path/to/libaxiomtrace.a \
 *     -DAXIOM_INCLUDE_DIR=/path/to/axiomtrace_headers
 *   cmake --build build
 * ============================================================================ */

#include "axiomtrace.h"
#include <stdio.h>

int main(void) {
    axiom_init();

    /* Structured event using new CLI */
    AX_EVT(INFO, 0x01, 0x01, (uint16_t)1234);

    /* Key-Value event */
    AX_KV(INFO, 0x02, 0x01, "build", (uint32_t)42);

    /* Fault event */
    AX_FAULT(0x03, 0x01, (uint8_t)1, (int16_t)(-1));

    printf("Precompiled library example complete.\n");
    return 0;
}