#include "axiom_event.h"
#include "axiom_encode.h"
#include "axiom_crc.h"
#include "axiom_ring.h"
#include "axiom_filter.h"
#include "axiom_timestamp.h"
#include "axiom_backend.h"
#include "axiom_frontend.h"
#include "axiom_port.h"
#include <string.h>

#ifndef AXIOM_RING_BUFFER_SIZE
#define AXIOM_RING_BUFFER_SIZE 4096u
#endif

static uint8_t s_ring_buf[AXIOM_RING_BUFFER_SIZE];
static axiom_ring_t s_ring;
static uint16_t s_seq;
static axiom_filter_t s_filter;
static axiom_timestamp_ctx_t s_ts_ctx;

void axiom_init(void) {
    axiom_ring_init(&s_ring, s_ring_buf, AXIOM_RING_BUFFER_SIZE);
    s_seq = 0;
    axiom_filter_init(&s_filter);
    axiom_timestamp_init(&s_ts_ctx);
}

void axiom_flush(void) {
    uint8_t frame[AXIOM_MAX_FRAME_LEN];
    uint16_t n;
    while ((n = axiom_ring_peek(&s_ring, frame, sizeof(frame))) > 0) {
        /* Minimal frame is 11 bytes (8 header + 1 len + 0 payload + 2 crc) */
        if (n < 11) break;
        /* Determine actual frame length from payload_len */
        uint16_t payload_len = frame[8];
        uint16_t frame_len = 9 + payload_len + 2;
        if (n < frame_len) break;
        /* Consume from ring */
        (void)axiom_ring_read(&s_ring, frame, frame_len);
        axiom_backend_dispatch(frame, frame_len);
    }
}

void axiom_write(axiom_level_t level, uint8_t module_id, uint16_t event_id,
                 const uint8_t *payload, uint8_t payload_len) {
    if (level >= AXIOM_LEVEL_MAX) return;

    if (!axiom_filter_check(&s_filter, level, module_id)) {
        axiom_filter_drop(&s_filter, module_id, event_id);
        return;
    }

    if (level == AXIOM_LEVEL_FAULT) {
        axiom_port_fault_hook(module_id, event_id, payload, payload_len);
    }

    uint8_t frame[AXIOM_MAX_FRAME_LEN];
    uint8_t p = 0;

    /* Header */
    frame[p++] = 0xA5;
    frame[p++] = AXIOM_WIRE_VERSION;
    frame[p++] = (uint8_t)level;
    frame[p++] = module_id;
    frame[p++] = (uint8_t)(event_id & 0xFFu);
    frame[p++] = (uint8_t)(event_id >> 8);
    frame[p++] = (uint8_t)(s_seq & 0xFFu);
    frame[p++] = (uint8_t)(s_seq >> 8);
    s_seq++;

    /* Payload length */
    frame[p++] = payload_len;

    /* Payload */
    for (uint8_t i = 0; i < payload_len; ++i) {
        frame[p++] = payload[i];
    }

    /* CRC-16 */
    uint16_t crc = axiom_crc16(frame, p);
    frame[p++] = (uint8_t)(crc & 0xFFu);
    frame[p++] = (uint8_t)(crc >> 8);

    /* Write to ring and dispatch to backends */
    if (axiom_ring_write(&s_ring, frame, p)) {
        axiom_backend_dispatch(frame, p);
    }

    /* Emit DROP_SUMMARY if drops occurred */
    if (axiom_filter_drop_summary_ready(&s_filter)) {
        uint32_t lost = axiom_filter_drop_count_get_and_clear(&s_filter);
        uint8_t summary[8];
        uint8_t sp = 0;
        summary[sp++] = AXIOM_TYPE_U32;
        summary[sp++] = (uint8_t)(lost & 0xFFu);
        summary[sp++] = (uint8_t)((lost >> 8) & 0xFFu);
        summary[sp++] = (uint8_t)((lost >> 16) & 0xFFu);
        summary[sp++] = (uint8_t)(lost >> 24);
        summary[sp++] = AXIOM_TYPE_U8;
        summary[sp++] = s_filter.drop_module;
        summary[sp++] = AXIOM_TYPE_U16;
        summary[sp++] = (uint8_t)(s_filter.drop_event & 0xFFu);
        summary[sp++] = (uint8_t)(s_filter.drop_event >> 8);
        axiom_write(AXIOM_LEVEL_WARN, 0x00, 0x0001, summary, sp);
    }
}
