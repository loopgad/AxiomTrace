#include "axiomtrace.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int failures;

#define CHECK(msg, cond) \
    do { \
        if (!(cond)) { \
            printf("  FAIL: %s\n", msg); \
            failures++; \
        } \
    } while (0)

static uint16_t read_u16_le(const uint8_t *data) {
    uint16_t value = data[0];
    value = (uint16_t)(value | (uint16_t)((uint16_t)data[1] << 8u));
    return value;
}

static uint32_t read_u32_le(const uint8_t *data) {
    return (uint32_t)data[0] |
        ((uint32_t)data[1] << 8u) |
        ((uint32_t)data[2] << 16u) |
        ((uint32_t)data[3] << 24u);
}

static uint32_t crc32_host(const uint8_t *data, uint32_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8u; ++bit) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static bool capsule_crc_ok(const uint8_t *data, uint16_t len) {
    uint32_t expected = read_u32_le(data + len - 4u);
    return crc32_host(data, (uint32_t)len - 4u) == expected;
}

static uint8_t count_capsule_frames(const uint8_t *capsule, uint16_t len) {
    uint8_t snapshot_len = capsule[20];
    uint16_t pos = (uint16_t)(21u + snapshot_len);
    uint16_t end = (uint16_t)(len - 4u);
    uint8_t frames = 0;
    while (pos + 12u <= end && capsule[pos] == AXIOM_SYNC_BYTE) {
        uint8_t ts_len = axiom_timestamp_decode_len(capsule[pos + 8u]);
        uint16_t payload_len_pos = (uint16_t)(pos + 8u + ts_len);
        if (payload_len_pos >= end) {
            break;
        }
        uint16_t payload_len = capsule[payload_len_pos];
        uint16_t frame_len = (uint16_t)(8u + ts_len + 1u + payload_len + 2u);
        if (pos + frame_len > end) {
            break;
        }
        pos = (uint16_t)(pos + frame_len);
        frames++;
    }
    return frames;
}

static uint16_t capsule_event_id_at(const uint8_t *capsule, uint16_t len,
                                    uint8_t wanted_index) {
    uint16_t pos = (uint16_t)(21u + capsule[20]);
    uint16_t end = (uint16_t)(len - 4u);
    uint8_t index = 0u;
    while (pos + 12u <= end && capsule[pos] == AXIOM_SYNC_BYTE) {
        uint8_t ts_len = axiom_timestamp_decode_len(capsule[pos + 8u]);
        uint16_t payload_len_pos = (uint16_t)(pos + 8u + ts_len);
        if (payload_len_pos >= end) {
            break;
        }
        uint16_t frame_len = (uint16_t)(8u + ts_len + 1u +
                                        capsule[payload_len_pos] + 2u);
        if (pos + frame_len > end) {
            break;
        }
        if (index == wanted_index) {
            return read_u16_le(capsule + pos + 4u);
        }
        pos = (uint16_t)(pos + frame_len);
        index++;
    }
    return 0xFFFFu;
}

static void emit_pre_events(uint8_t count) {
    for (uint8_t i = 0; i < count; ++i) {
        AX_EVT(INFO, 0x10u, (uint16_t)(0x0100u + i), i);
    }
}

#if AXIOM_CAPSULE_FLASH_SIZE < AXIOM_CAPSULE_MAX_IMAGE_SIZE
static void emit_large_pre_events(uint8_t count) {
    uint8_t payload[AXIOM_MAX_PAYLOAD_LEN];
    memset(payload, 0x5Au, sizeof(payload));
    for (uint8_t i = 0; i < count; ++i) {
        axiom_write(AXIOM_LEVEL_INFO, 0x10u, (uint16_t)(0x0600u + i),
                    payload, (uint8_t)sizeof(payload));
    }
}
#endif

static uint16_t read_capsule(uint8_t *buffer, uint16_t size) {
    uint32_t n = axiom_capsule_read(buffer, size);
    CHECK("read returns complete capsule", n > 0u && n <= size);
    return (uint16_t)n;
}

static void prepare_host(void) {
    axiom_port_host_flash_reset();
    g_axiom_port_reset_reason = 0x7Au;
    g_axiom_port_fault_snapshot_len = 8u;
    g_axiom_port_fault_snapshot_data[0] = 0x11u;
    g_axiom_port_fault_snapshot_data[1] = 0x22u;
    g_axiom_port_fault_snapshot_data[2] = 0x33u;
    g_axiom_port_fault_snapshot_data[3] = 0x44u;
    g_axiom_port_fault_snapshot_data[4] = 0x55u;
    g_axiom_port_fault_snapshot_data[5] = 0x66u;
    g_axiom_port_fault_snapshot_data[6] = 0x77u;
    g_axiom_port_fault_snapshot_data[7] = 0x88u;
    axiom_init();
}

static void test_commit_read_and_clear(void) {
    uint8_t capsule[AXIOM_CAPSULE_MAX_IMAGE_SIZE];
    prepare_host();

    emit_pre_events(3u);
    CHECK("normal operation does not erase flash", g_axiom_port_flash_erase_calls == 0u);
    CHECK("normal operation does not write flash", g_axiom_port_flash_write_calls == 0u);

    AX_FAULT(0x44u, 0x0400u, (uint8_t)9u);
    AX_EVT(INFO, 0x45u, 0x0401u, (uint8_t)1u);
    AX_EVT(INFO, 0x45u, 0x0402u, (uint8_t)2u);
    CHECK("fault capture still does not write flash before commit", g_axiom_port_flash_write_calls == 0u);

    CHECK("commit succeeds", axiom_capsule_commit());
    CHECK("commit erases once", g_axiom_port_flash_erase_calls == 1u);
    CHECK("commit uses bounded segmented writes", g_axiom_port_flash_write_calls > 1u);
    CHECK("present after commit", axiom_capsule_present());

    uint16_t len = read_capsule(capsule, sizeof(capsule));
    CHECK("magic", memcmp(capsule, "AXCP", 4) == 0);
    CHECK("version", capsule[4] == 1u);
    CHECK("encoded length", read_u16_le(capsule + 5) == len);
    CHECK("reset reason", capsule[7] == 0x7Au);
    CHECK("fault type records triggering module", capsule[8] == 0x44u);
    CHECK("pre count", capsule[17] == 3u);
    CHECK("post count includes fault event", capsule[18] == 3u);
    CHECK("snapshot id", capsule[19] == AXIOM_CAPSULE_SNAPSHOT_ID);
    CHECK("snapshot len", capsule[20] == 8u);
    CHECK("snapshot bytes", capsule[21] == 0x11u && capsule[28] == 0x88u);
    CHECK("capsule crc", capsule_crc_ok(capsule, len));
    CHECK("frame count", count_capsule_frames(capsule, len) == 6u);
    CHECK("frame order: first pre", capsule_event_id_at(capsule, len, 0u) == 0x0100u);
    CHECK("frame order: last pre", capsule_event_id_at(capsule, len, 2u) == 0x0102u);
    CHECK("frame order: fault", capsule_event_id_at(capsule, len, 3u) == 0x0400u);
    CHECK("frame order: first post", capsule_event_id_at(capsule, len, 4u) == 0x0401u);
    CHECK("frame order: last post", capsule_event_id_at(capsule, len, 5u) == 0x0402u);

    axiom_capsule_clear();
    CHECK("clear removes present capsule", !axiom_capsule_present());
}

static void test_pre_window_cap(void) {
    uint8_t capsule[AXIOM_CAPSULE_MAX_IMAGE_SIZE];
    prepare_host();
    emit_pre_events((uint8_t)(AXIOM_CAPSULE_PRE_EVENTS + 4u));
    AX_FAULT(0x22u, 0x0200u);
    CHECK("commit succeeds after capped pre-window", axiom_capsule_commit());
    uint16_t len = read_capsule(capsule, sizeof(capsule));
    CHECK("pre window capped", capsule[17] == AXIOM_CAPSULE_PRE_EVENTS);
    CHECK("post window has fault", capsule[18] == 1u);
    CHECK("capped frame count", count_capsule_frames(capsule, len) == (uint8_t)(AXIOM_CAPSULE_PRE_EVENTS + 1u));
    CHECK("oldest complete pre frames evicted",
          capsule_event_id_at(capsule, len, 0u) == 0x0104u);
    CHECK("fault remains after eviction",
          capsule_event_id_at(capsule, len, AXIOM_CAPSULE_PRE_EVENTS) == 0x0200u);
}

static void test_fault_bypasses_filters(void) {
    uint8_t capsule[AXIOM_CAPSULE_MAX_IMAGE_SIZE];
    prepare_host();
    axiom_level_mask_set(0u);
    axiom_module_mask_set(0u);

    AX_FAULT(0x23u, 0x0230u, (uint8_t)1u);

    CHECK("fault bypasses filters: capsule present", axiom_capsule_present());
    uint16_t len = read_capsule(capsule, sizeof(capsule));
    CHECK("fault bypasses filters: one post frame", capsule[18] == 1u);
    CHECK("fault bypasses filters: fault frame retained",
          capsule_event_id_at(capsule, len, 0u) == 0x0230u);
    axiom_capsule_clear();
}

static void test_fault_survives_full_drop_ring(void) {
#if AXIOM_RING_BUFFER_POLICY == AXIOM_RING_BUFFER_POLICY_DROP
    uint8_t capsule[AXIOM_CAPSULE_MAX_IMAGE_SIZE];
    uint8_t payload[AXIOM_MAX_PAYLOAD_LEN] = {0};
    const uint32_t min_full_frame = AXIOM_HEADER_LEN + 1u + 1u +
        AXIOM_MAX_PAYLOAD_LEN + AXIOM_CRC_LEN;
    const uint32_t fill_count = AXIOM_RING_BUFFER_SIZE / min_full_frame + 2u;

    prepare_host();
    for (uint32_t i = 0u; i < fill_count; ++i) {
        axiom_write(AXIOM_LEVEL_INFO, 0x30u, (uint16_t)(0x3000u + i),
                    payload, (uint8_t)sizeof(payload));
    }
    AX_FAULT(0x31u, 0x0310u, (uint8_t)2u);

    CHECK("fault survives full ring: capsule present", axiom_capsule_present());
    uint16_t len = read_capsule(capsule, sizeof(capsule));
    CHECK("fault survives full ring: fault frame retained",
          capsule_event_id_at(capsule, len, capsule[17]) == 0x0310u);
    axiom_capsule_clear();
#endif
}

static void test_commit_failure_keeps_ram_capsule(void) {
    uint8_t capsule[AXIOM_CAPSULE_MAX_IMAGE_SIZE];
    prepare_host();
    emit_pre_events(1u);
    AX_FAULT(0x33u, 0x0300u);
    g_axiom_port_flash_write_limit = 8u;

    CHECK("commit reports short-write failure", !axiom_capsule_commit());
    CHECK("write attempted once", g_axiom_port_flash_write_calls == 1u);
    CHECK("RAM capsule remains present", axiom_capsule_present());
    uint16_t len = read_capsule(capsule, sizeof(capsule));
    CHECK("RAM capsule remains valid", capsule_crc_ok(capsule, len));
}

static void test_flash_crc_mismatch_after_reboot(void) {
    prepare_host();
    emit_pre_events(1u);
    AX_FAULT(0x34u, 0x0301u);
    CHECK("commit succeeds before corruption", axiom_capsule_commit());
    g_axiom_port_flash_storage[30] ^= 0x5Au;

    axiom_init();
    CHECK("corrupt flash capsule is not present", !axiom_capsule_present());
}

static void test_power_loss_at_each_write_stage(void) {
    for (uint32_t stage = 1u; stage <= 4u; ++stage) {
        prepare_host();
        emit_pre_events(1u);
        AX_FAULT(0x35u, (uint16_t)(0x0310u + stage));
        g_axiom_port_flash_fail_write_call = stage;
        CHECK("power stage: commit fails", !axiom_capsule_commit());
        CHECK("power stage: volatile capsule readable", axiom_capsule_present());

        axiom_init();
        CHECK("power stage: partial flash rejected after reboot", !axiom_capsule_present());
    }
}

#if AXIOM_CAPSULE_FLASH_SIZE < AXIOM_CAPSULE_MAX_IMAGE_SIZE
static void test_flash_capacity_guard(void) {
    prepare_host();
    g_axiom_port_flash_storage[0] = 0x3Cu;
    emit_large_pre_events(16u);
    AX_FAULT(0x36u, 0x0360u);

    CHECK("small flash rejects oversized capsule", !axiom_capsule_commit());
    CHECK("small flash does not erase before rejection",
          g_axiom_port_flash_erase_calls == 0u);
    CHECK("small flash does not write before rejection",
          g_axiom_port_flash_write_calls == 0u);
    CHECK("small flash preserves existing bytes",
          g_axiom_port_flash_storage[0] == 0x3Cu);
}
#endif

#if defined(AXIOM_CAPSULE_SMALL_TEST)
int main(void) {
    test_flash_capacity_guard();

    if (failures == 0) {
        printf("test_capsule: PASSED\n");
    } else {
        printf("test_capsule: FAILED (%d failures)\n", failures);
    }
    return failures;
}
#else
int main(void) {
    test_commit_read_and_clear();
    test_pre_window_cap();
    test_fault_bypasses_filters();
    test_fault_survives_full_drop_ring();
    test_commit_failure_keeps_ram_capsule();
    test_flash_crc_mismatch_after_reboot();
    test_power_loss_at_each_write_stage();
#if AXIOM_CAPSULE_FLASH_SIZE < AXIOM_CAPSULE_MAX_IMAGE_SIZE
    test_flash_capacity_guard();
#endif

    if (failures == 0) {
        printf("test_capsule: PASSED\n");
    } else {
        printf("test_capsule: FAILED (%d failures)\n", failures);
    }
    return failures;
}
#endif
