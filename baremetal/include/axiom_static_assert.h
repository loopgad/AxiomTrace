#ifndef AXIOM_STATIC_ASSERT_H
#define AXIOM_STATIC_ASSERT_H

/* ---------------------------------------------------------------------------
 * Compile-time assertions (C11 _Static_assert wrapper)
 * --------------------------------------------------------------------------- */

#define AXIOM_STATIC_ASSERT(expr, msg) _Static_assert((expr), msg)

#define AXIOM_CHECK_SIZE(type, expected) \
    AXIOM_STATIC_ASSERT(sizeof(type) == (expected), #type " size mismatch")

#define AXIOM_CHECK_ALIGN(type, align) \
    AXIOM_STATIC_ASSERT(_Alignof(type) == (align), #type " align mismatch")

#define AXIOM_CHECK_FIELD_OFFSET(type, field, expected) \
    AXIOM_STATIC_ASSERT(__builtin_offsetof(type, field) == (expected), #type "." #field " offset mismatch")

#define AXIOM_CHECK_RANGE(val, min, max) \
    AXIOM_STATIC_ASSERT((val) >= (min) && (val) <= (max), #val " out of range")

#define AXIOM_CHECK_POWER_OF_TWO(val) \
    AXIOM_STATIC_ASSERT(((val) != 0) && (((val) & ((val) - 1)) == 0), #val " must be power of two")

#define AXIOM_CHECK_ARRAY_SIZE(arr, expected) \
    AXIOM_STATIC_ASSERT((sizeof(arr) / sizeof((arr)[0])) == (expected), #arr " size mismatch")

#endif /* AXIOM_STATIC_ASSERT_H */
