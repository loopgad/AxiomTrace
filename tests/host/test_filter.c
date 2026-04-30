#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "axiom_filter.h"

int main(void) {
    axiom_filter_t f;
    axiom_filter_init(&f);

    assert(axiom_filter_check(&f, AXIOM_LEVEL_DEBUG, 0) == true);
    assert(axiom_filter_check(&f, AXIOM_LEVEL_INFO,  0) == true);
    assert(axiom_filter_check(&f, AXIOM_LEVEL_FAULT, 0) == true);

    /* Mask out DEBUG */
    f.level_mask = 0xFFFFFFFEu;
    assert(axiom_filter_check(&f, AXIOM_LEVEL_DEBUG, 0) == false);
    assert(axiom_filter_check(&f, AXIOM_LEVEL_INFO,  0) == true);

    /* Record a drop */
    axiom_filter_drop(&f, 0x03, 0x0500);
    assert(f.drop_count == 1);
    assert(f.drop_pending == true);

    /* Get and clear */
    uint32_t lost = axiom_filter_drop_count_get_and_clear(&f);
    assert(lost == 1);
    assert(f.drop_pending == false);

    printf("test_filter: PASSED\n");
    return 0;
}
