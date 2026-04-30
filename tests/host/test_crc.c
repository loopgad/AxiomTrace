#include <stdio.h>
#include <stdint.h>
#include "axiom_crc.h"

int main(void) {
    /* CRC-16/CCITT-FALSE known vector: "123456789" */
    const uint8_t data1[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    uint16_t crc1 = axiom_crc16(data1, sizeof(data1));
    if (crc1 != 0x29B1u) {
        printf("test_crc: FAILED expected 0x29B1 got 0x%04X\n", crc1);
        return 1;
    }

    /* Empty data */
    uint16_t crc2 = axiom_crc16(NULL, 0);
    if (crc2 != 0xFFFFu) {
        printf("test_crc: FAILED empty data expected 0xFFFF got 0x%04X\n", crc2);
        return 1;
    }

    printf("test_crc: PASSED\n");
    return 0;
}
