#include "axiom_cobs.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void test_zero_split(void) {
    static const uint8_t input[] = {0x11u, 0x00u, 0x22u};
    static const uint8_t expected[] = {0x02u, 0x11u, 0x02u, 0x22u};
    uint8_t output[sizeof(expected) + 1u] = {0u};
    uint16_t length = axiom_cobs_encode(input, sizeof(input), output, sizeof(output));
    assert(length == sizeof(expected));
    assert(memcmp(output, expected, sizeof(expected)) == 0);
}

static void test_full_block(void) {
    uint8_t input[254];
    uint8_t output[256] = {0u};
    memset(input, 0xA5, sizeof(input));
    uint16_t length = axiom_cobs_encode(input, sizeof(input), output, sizeof(output));
    assert(length == 255u);
    assert(output[0] == 0xFFu);
    assert(memcmp(output + 1u, input, sizeof(input)) == 0);
}

static void test_capacity_and_invalid_inputs(void) {
    static const uint8_t input[] = {0x11u};
    uint8_t output[3] = {0xA5u, 0xA5u, 0xA5u};
    assert(axiom_cobs_encode(input, sizeof(input), output, 2u) == 0u);
    assert(output[2] == 0xA5u);
    assert(axiom_cobs_encode(NULL, sizeof(input), output, sizeof(output)) == 0u);
    assert(axiom_cobs_encode(input, 0u, output, sizeof(output)) == 0u);
}

int main(void) {
    test_zero_split();
    test_full_block();
    test_capacity_and_invalid_inputs();
    return 0;
}
