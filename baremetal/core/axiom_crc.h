#ifndef AXIOM_CRC_H
#define AXIOM_CRC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * CRC-16/CCITT-FALSE
 * poly = 0x1021, init = 0xFFFF, no reflection, final XOR = 0x0000
 * --------------------------------------------------------------------------- */

uint16_t axiom_crc16(const uint8_t *data, size_t len);

/* Lookup table (256 bytes, stored in ROM) */
extern const uint16_t axiom_crc16_table[256];

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_CRC_H */
