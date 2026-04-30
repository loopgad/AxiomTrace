/* ============================================================================
 * nRF52 Platform Port - Public Declarations
 * ============================================================================
 *
 * This header declares nRF52-specific functions that are not part of the
 * standard axiom_port interface.
 *
 * ============================================================================ */

#ifndef AXIOM_PORT_NRF52_H
#define AXIOM_PORT_NRF52_H

#include <stdint.h>

/* Forward declaration - actual definition in axiom_backend.h */
struct axiom_backend;

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * RTC Initialization
 * Call this from your system initialization to start the timestamp counter.
 * ============================================================================ */
void axiom_port_rtc_init(void);

/* ============================================================================
 * RTT Backend API
 * ============================================================================ */

/* Initialize RTT backend and register with AxiomTrace */
int axiom_backend_rtt_init(void);

/* Get the RTT backend instance for manual registration */
const struct axiom_backend *axiom_backend_rtt_get(void);

/* Get drop count */
uint32_t axiom_backend_rtt_get_dropped(void);

/* Check if RTT is connected (host is listening) */
int axiom_backend_rtt_is_connected(void);

/* String output helper (used by axiom_port_string_out) */
void axiom_rtt_write_string(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_PORT_NRF52_H */
