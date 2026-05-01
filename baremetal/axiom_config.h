#ifndef AXIOM_CONFIG_H
#define AXIOM_CONFIG_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Core Configuration
 * --------------------------------------------------------------------------- */

/* Main ring buffer size. MUST be a power of two. */
#ifndef AXIOM_RING_BUFFER_SIZE
#define AXIOM_RING_BUFFER_SIZE 4096u
#endif

/* DROP: Discard new events when full. 
 * OVERWRITE: Overwrite oldest events when full (maintains O(1)). */
#define AXIOM_RING_BUFFER_POLICY_DROP      0
#define AXIOM_RING_BUFFER_POLICY_OVERWRITE 1

#ifndef AXIOM_RING_BUFFER_POLICY
#define AXIOM_RING_BUFFER_POLICY AXIOM_RING_BUFFER_POLICY_DROP
#endif

/* Maximum payload size per event in bytes. */
#ifndef AXIOM_MAX_PAYLOAD_LEN
#define AXIOM_MAX_PAYLOAD_LEN 128u
#endif

/* ---------------------------------------------------------------------------
 * Observability Extensions
 * --------------------------------------------------------------------------- */

/* If enabled, every AX_EVT will automatically append __LINE__ and a hash of __FILE__.
 * Adds approximately 4 bytes of payload per event. */
#ifndef AXIOM_CFG_USE_LOCATION
#define AXIOM_CFG_USE_LOCATION 0
#endif

/* If enabled, the core will periodically (or manually) emit sync events 
 * to map local counters to host-provided Unix time. */
#ifndef AXIOM_CFG_TIME_SYNC_ENABLED
#define AXIOM_CFG_TIME_SYNC_ENABLED 1
#endif

#endif /* AXIOM_CONFIG_H */
