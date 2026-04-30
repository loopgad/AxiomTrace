/* ============================================================================
 * AxiomTrace Host Benchmark
 * Measures encoding, CRC, and ring write cycles (simulated via rdtsc).
 * ============================================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#if defined(__x86_64__) || defined(__i386__)
#include <intrin.h>
#define READ_TSC() __rdtsc()
#else
#define READ_TSC() (uint64_t)clock()
#endif

#include "axiomtrace.h"

static uint64_t measure_encode(void) {
    uint8_t buf[AXIOM_MAX_PAYLOAD_LEN];
    uint8_t pos = 0;

    uint64_t t0 = READ_TSC();
    axiom_enc_u16(buf, &pos, 0x1234);
    axiom_enc_i16(buf, &pos, (int16_t)(-1));
    axiom_enc_f32(buf, &pos, 3.14159f);
    uint64_t t1 = READ_TSC();
    return t1 - t0;
}

static uint64_t measure_crc(void) {
    uint8_t data[64];
    for (int i = 0; i < 64; ++i) data[i] = (uint8_t)i;

    uint64_t t0 = READ_TSC();
    volatile uint16_t crc = axiom_crc16(data, sizeof(data));
    (void)crc;
    uint64_t t1 = READ_TSC();
    return t1 - t0;
}

static uint64_t measure_ring_write(void) {
    uint8_t buf[256];
    axiom_ring_t ring;
    axiom_ring_init(&ring, buf, sizeof(buf));
    uint8_t frame[32] = {0xA5, 0x10, 0x01, 0x03, 0x01, 0x00, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03};

    uint64_t t0 = READ_TSC();
    axiom_ring_write(&ring, frame, sizeof(frame));
    uint64_t t1 = READ_TSC();
    return t1 - t0;
}

int main(void) {
    printf("AxiomTrace Host Benchmark\n");
    printf("=========================\n");

    uint64_t enc = measure_encode();
    printf("Encode (u16+i16+f32): %llu cycles\n", (unsigned long long)enc);

    uint64_t crc = measure_crc();
    printf("CRC-16 (64 bytes):    %llu cycles\n", (unsigned long long)crc);

    uint64_t ring = measure_ring_write();
    printf("Ring write (32 bytes):%llu cycles\n", (unsigned long long)ring);

    return 0;
}
