#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "axiom_ring.h"

static uint8_t buf[256];
static axiom_ring_t ring;

int main(void) {
    axiom_ring_init(&ring, buf, sizeof(buf));
    assert(axiom_ring_used(&ring) == 0);
    assert(axiom_ring_free(&ring) == 256);

    const uint8_t data1[] = {1, 2, 3, 4, 5};
    assert(axiom_ring_write(&ring, data1, sizeof(data1)) == true);
    assert(axiom_ring_used(&ring) == 5);

    uint8_t out[8] = {0};
    uint16_t n = axiom_ring_read(&ring, out, sizeof(out));
    assert(n == 5);
    assert(memcmp(out, data1, 5) == 0);
    assert(axiom_ring_used(&ring) == 0);

    printf("test_ring: PASSED\n");
    return 0;
}
