#ifndef AXIOM_TIMESTAMP_H
#define AXIOM_TIMESTAMP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Compressed relative timestamp
 * Stores delta from previous event to reduce payload size.
 * --------------------------------------------------------------------------- */

typedef struct {
    uint32_t last_raw;
    uint32_t baseline;
} axiom_timestamp_ctx_t;

/* Initialize timestamp context */
void axiom_timestamp_init(axiom_timestamp_ctx_t *ctx);

/* Encode current timestamp into up to 4 bytes. Returns encoded length (1..4). */
uint8_t axiom_timestamp_encode(axiom_timestamp_ctx_t *ctx, uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_TIMESTAMP_H */
