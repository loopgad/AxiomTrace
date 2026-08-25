#ifndef AXIOM_COBS_H
#define AXIOM_COBS_H

#include <stdint.h>

/* Encode one non-empty frame. The returned length excludes the 0x00
 * delimiter, and output_size must leave room for the caller to append it. */
static inline uint16_t axiom_cobs_encode(const uint8_t *input, uint16_t input_len, uint8_t *output,
                                         uint16_t output_size) {
    if (!input || !output || input_len == 0u || output_size < 2u) {
        return 0u;
    }

    uint32_t code_index = 0u;
    uint32_t output_index = 1u;
    uint8_t code = 1u;

    for (uint32_t input_index = 0u; input_index < input_len; ++input_index) {
        uint8_t byte = input[input_index];
        if (byte == 0u || code == 0xFFu) {
            if (output_index >= output_size) {
                return 0u;
            }
            output[code_index] = code;
            code_index = output_index++;
            code = 1u;
            if (byte == 0u) {
                continue;
            }
        }

        if (output_index >= output_size) {
            return 0u;
        }
        output[output_index++] = byte;
        ++code;
    }

    if (output_index >= output_size) {
        return 0u;
    }
    output[code_index] = code;
    return (uint16_t)output_index;
}

#endif /* AXIOM_COBS_H */
