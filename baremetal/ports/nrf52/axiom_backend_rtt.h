/* ============================================================================
 * SEGGER RTT Backend for AxiomTrace - Public API
 * ============================================================================
 */

#ifndef AXIOM_BACKEND_RTT_H
#define AXIOM_BACKEND_RTT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * RTT Backend API
 * ============================================================================ */

/* Initialize RTT backend and register with AxiomTrace
 * Call this from your initialization code before axiom_init() */
int axiom_backend_rtt_init(void);

/* Get the RTT backend instance for manual registration */
const struct axiom_backend *axiom_backend_rtt_get(void);

/* Get drop count since initialization */
uint32_t axiom_backend_rtt_get_dropped(void);

/* Check if RTT is connected (host is listening) */
int axiom_backend_rtt_is_connected(void);

/* String output helper (used by axiom_port_string_out) */
void axiom_rtt_write_string(const char *str);

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */
#define AXIOM_RTT_BUFFER_INDEX    1  /* RTT channel 1 (0 often used for logs) */
#define AXIOM_RTT_BUFFER_SIZE     1024  /* Up buffer size in bytes */

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_BACKEND_RTT_H */
