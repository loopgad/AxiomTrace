#ifndef AXIOM_FRAME_H
#define AXIOM_FRAME_H

#include "axiom_crc.h"
#include "axiom_event.h"
#include "axiom_timestamp.h"

#include <stdbool.h>
#include <stdint.h>

/* Validate one complete, contiguous frame copied from a ring. */
static inline bool axiom_frame_validate(const uint8_t *frame, uint16_t available,
                                        uint16_t *frame_len) {
    if (!frame || available < 12u || frame[0] != AXIOM_SYNC_BYTE ||
        frame[1] != AXIOM_WIRE_VERSION || frame[2] >= AXIOM_LEVEL_MAX) {
        return false;
    }

    uint8_t ts_len = axiom_timestamp_decode_len(frame[AXIOM_HEADER_LEN]);
    uint16_t payload_offset = (uint16_t)(AXIOM_HEADER_LEN + ts_len);
    if (payload_offset >= available) {
        return false;
    }

    uint16_t payload_len = frame[payload_offset];
    uint16_t total = (uint16_t)(payload_offset + 1u + payload_len + AXIOM_CRC_LEN);
    if (total > AXIOM_MAX_FRAME_LEN || total > available) {
        return false;
    }

    uint16_t expected_crc = (uint16_t)frame[total - 2u] |
        (uint16_t)((uint16_t)frame[total - 1u] << 8u);
    if (axiom_crc16(frame, total - AXIOM_CRC_LEN) != expected_crc) {
        return false;
    }

    if (frame_len) {
        *frame_len = total;
    }
    return true;
}

/* Bytes needed to parse a frame's total length: 8-byte header plus the
 * longest variable timestamp plus the 1-byte payload length. */
#define AXIOM_FRAME_PREFIX_LEN (AXIOM_HEADER_LEN + AXIOM_MAX_TIMESTAMP_LEN + 1u)

/* Parse the total frame length from a prefix without copying or CRC-checking
 * the payload. Eviction paths use this when the oldest frame is about to be
 * discarded: the write side already covered the frame with CRC and the flush
 * path re-validates on drain, so re-running the table CRC per eviction inside
 * the write critical section only adds latency. `prefix` holds prefix_len
 * bytes peeked from the head; `available` is the true number of buffered
 * bytes the frame may claim. Header validation mirrors axiom_frame_validate;
 * parse failure keeps the caller's existing reset semantics. */
static inline bool axiom_frame_prefix_len(const uint8_t *prefix, uint16_t prefix_len,
                                          uint16_t available, uint16_t *frame_len) {
    if (!prefix || available < 12u || prefix_len < (uint16_t)(AXIOM_HEADER_LEN + 1u) ||
        prefix[0] != AXIOM_SYNC_BYTE || prefix[1] != AXIOM_WIRE_VERSION ||
        prefix[2] >= AXIOM_LEVEL_MAX) {
        return false;
    }
    uint8_t ts_len = axiom_timestamp_decode_len(prefix[AXIOM_HEADER_LEN]);
    uint16_t payload_offset = (uint16_t)(AXIOM_HEADER_LEN + ts_len);
    if (payload_offset >= prefix_len || payload_offset >= available) {
        return false;
    }
    uint16_t payload_len = prefix[payload_offset];
    uint16_t total = (uint16_t)(payload_offset + 1u + payload_len + AXIOM_CRC_LEN);
    if (total > AXIOM_MAX_FRAME_LEN || total > available) {
        return false;
    }
    if (frame_len) {
        *frame_len = total;
    }
    return true;
}

#endif /* AXIOM_FRAME_H */
