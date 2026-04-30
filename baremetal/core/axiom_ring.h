#ifndef AXIOM_RING_H
#define AXIOM_RING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Lock-free IRQ-safe RAM Ring Buffer
 * Single-producer (ISR or main loop), single-consumer (backend drain).
 * --------------------------------------------------------------------------- */

typedef struct {
    volatile uint32_t head;
    volatile uint32_t tail;
    uint32_t capacity;
    uintptr_t reserved;
} axiom_ring_t;

/* Initialize ring buffer in the provided memory block */
void axiom_ring_init(axiom_ring_t *ring, uint8_t *buf, uint32_t size);

/* Write data to ring. Returns true on success, false if dropped. */
bool axiom_ring_write(axiom_ring_t *ring, const uint8_t *data, uint16_t len);

/* Read up to max_len bytes from ring. Returns bytes read. */
uint16_t axiom_ring_read(axiom_ring_t *ring, uint8_t *out, uint16_t max_len);

/* Peek at data without consuming. Returns bytes available. */
uint16_t axiom_ring_peek(const axiom_ring_t *ring, uint8_t *out, uint16_t max_len);

/* Bytes currently stored */
static inline uint32_t axiom_ring_used(const axiom_ring_t *ring) {
    return ring->head - ring->tail;
}

/* Free space */
static inline uint32_t axiom_ring_free(const axiom_ring_t *ring) {
    return ring->capacity - axiom_ring_used(ring);
}

/* Reset ring to empty */
void axiom_ring_reset(axiom_ring_t *ring);

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_RING_H */
