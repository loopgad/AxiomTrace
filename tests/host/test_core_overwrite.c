#include "test_helpers.h"

#include "axiom_diagnostics.h"
#include "axiom_frame.h"
#include "axiom_frontend.h"
#include "axiom_backend.h"
#include "axiom_ring.h"

static void test_complete_frame_overwrite(void) {
    uint8_t capture[2048] = {0};
    axiom_memory_backend_ctx_t memory_context;
    axiom_backend_t memory = axiom_backend_memory(
        "overwrite", capture, sizeof(capture), &memory_context);
    axiom_init();
    CHECK("overwrite: memory registered",
          axiom_backend_register(&memory) == AXIOM_BACKEND_OK);

    for (uint16_t i = 0u; i < 40u; ++i) {
        axiom_write(AXIOM_LEVEL_INFO, 1u, (uint16_t)(0x400u + i), NULL, 0u);
    }
    axiom_diagnostics_t diagnostics;
    axiom_diagnostics_get(&diagnostics);
    CHECK("overwrite: complete old frames counted", diagnostics.ring_full > 0u);
    axiom_flush();

    uint32_t offset = 0u;
    uint32_t frames = 0u;
    while (offset < memory_context.head) {
        uint16_t frame_len = 0u;
        CHECK("overwrite: every retained record validates",
              axiom_frame_validate(capture + offset,
                                   (uint16_t)(memory_context.head - offset),
                                   &frame_len));
        if (frame_len == 0u) {
            break;
        }
        offset += frame_len;
        frames++;
    }
    CHECK("overwrite: capture contains frames", frames > 0u);
    CHECK("overwrite: no partial trailing bytes", offset == memory_context.head);
}

static void test_peek_consume_snapshot(void) {
    uint8_t storage[8] = {0u};
    uint8_t first[] = {1u, 2u, 3u, 4u};
    uint8_t overwrite[] = {5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u};
    uint8_t out[4] = {0u};
    uint32_t tail_snapshot = 0u;
    axiom_ring_t ring;

    axiom_ring_init(&ring, storage, sizeof(storage));
    CHECK("snapshot: initial write", axiom_ring_write(&ring, first, sizeof(first)));
    CHECK("snapshot: peek succeeds",
          axiom_ring_peek_snapshot(&ring, out, sizeof(out), &tail_snapshot) == sizeof(out));
    CHECK("snapshot: bytes copied", out[0] == 1u && out[3] == 4u);

    CHECK("snapshot: overwrite write", axiom_ring_write(&ring, overwrite, sizeof(overwrite)));
    CHECK("snapshot: stale consume rejected",
          !axiom_ring_consume_if(&ring, tail_snapshot, sizeof(out)));
    CHECK("snapshot: current tail preserved", ring.tail == sizeof(first));

    CHECK("snapshot: refresh succeeds",
          axiom_ring_peek_snapshot(&ring, out, sizeof(out), &tail_snapshot) == sizeof(out));
    CHECK("snapshot: current bytes copied", out[0] == 5u && out[3] == 8u);
    CHECK("snapshot: matching consume succeeds",
          axiom_ring_consume_if(&ring, tail_snapshot, sizeof(out)));
}

int main(void) {
    test_complete_frame_overwrite();
    test_peek_consume_snapshot();
    TEST_RESULT("test_core_overwrite", failures);
    return failures;
}
