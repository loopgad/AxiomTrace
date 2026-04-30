#ifndef AXIOM_FRONTEND_H
#define AXIOM_FRONTEND_H

#include "axiom_event.h"
#include "axiom_encode.h"
#include "axiom_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Profile control
 * --------------------------------------------------------------------------- */
#define AXIOM_PROFILE_DEV    0
#define AXIOM_PROFILE_FIELD  1
#define AXIOM_PROFILE_PROD   2

#ifndef AXIOM_PROFILE
#define AXIOM_PROFILE AXIOM_PROFILE_DEV
#endif

/* ---------------------------------------------------------------------------
 * Variadic argument counting (GCC/Clang/MSVC compatible)
 * --------------------------------------------------------------------------- */
#define _AXIOM_ARG_N(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, N, ...) N
#define _AXIOM_NARG_(...) _AXIOM_ARG_N(__VA_ARGS__)
#define _AXIOM_NARG(...) _AXIOM_NARG_(0, ##__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define _AXIOM_CONCAT2(a, b) a ## b
#define _AXIOM_CONCAT(a, b)  _AXIOM_CONCAT2(a, b)

/* ---------------------------------------------------------------------------
 * Core write declaration (defined in axiom_event.c)
 * --------------------------------------------------------------------------- */
void axiom_write(axiom_level_t level, uint8_t module_id, uint16_t event_id,
                 const uint8_t *payload, uint8_t payload_len);
void axiom_init(void);
void axiom_flush(void);

/* ---------------------------------------------------------------------------
 * FNV-1a 16-bit hash (runtime, for key hashing)
 * --------------------------------------------------------------------------- */
static inline uint16_t _axiom_fnv1a_16(const char *s) {
    uint16_t h = 0x811cu;
    while (*s) {
        h ^= (uint16_t)(unsigned char)(*s++);
        h *= 0x0103u;
    }
    return h;
}

/* ---------------------------------------------------------------------------
 * AX_EVT — Structured Event (always compiled, all profiles)
 * --------------------------------------------------------------------------- */
#define _AXIOM_EVT_0(level, mod, evt) \
    axiom_write((level), (mod), (evt), NULL, 0)

#define _AXIOM_EVT_1(level, mod, evt, a1) \
    do { uint8_t _b[AXIOM_MAX_PAYLOAD_LEN]; uint8_t _p = 0; \
         _AXIOM_ENCODE_ONE(_b, _p, a1); \
         axiom_write((level), (mod), (evt), _b, _p); } while(0)

#define _AXIOM_EVT_2(level, mod, evt, a1, a2) \
    do { uint8_t _b[AXIOM_MAX_PAYLOAD_LEN]; uint8_t _p = 0; \
         _AXIOM_ENCODE_ONE(_b, _p, a1); \
         _AXIOM_ENCODE_ONE(_b, _p, a2); \
         axiom_write((level), (mod), (evt), _b, _p); } while(0)

#define _AXIOM_EVT_3(level, mod, evt, a1, a2, a3) \
    do { uint8_t _b[AXIOM_MAX_PAYLOAD_LEN]; uint8_t _p = 0; \
         _AXIOM_ENCODE_ONE(_b, _p, a1); \
         _AXIOM_ENCODE_ONE(_b, _p, a2); \
         _AXIOM_ENCODE_ONE(_b, _p, a3); \
         axiom_write((level), (mod), (evt), _b, _p); } while(0)

#define _AXIOM_EVT_4(level, mod, evt, a1, a2, a3, a4) \
    do { uint8_t _b[AXIOM_MAX_PAYLOAD_LEN]; uint8_t _p = 0; \
         _AXIOM_ENCODE_ONE(_b, _p, a1); \
         _AXIOM_ENCODE_ONE(_b, _p, a2); \
         _AXIOM_ENCODE_ONE(_b, _p, a3); \
         _AXIOM_ENCODE_ONE(_b, _p, a4); \
         axiom_write((level), (mod), (evt), _b, _p); } while(0)

#define _AXIOM_EVT_DISPATCH(level, mod, evt, ...) \
    _AXIOM_CONCAT(_AXIOM_EVT_, _AXIOM_NARG(__VA_ARGS__))(level, mod, evt, ##__VA_ARGS__)

#define AX_EVT(level, mod, evt, ...) \
    _AXIOM_EVT_DISPATCH(AXIOM_LEVEL_##level, mod, evt, ##__VA_ARGS__)

#define AX_FAULT(mod, evt, ...) \
    _AXIOM_EVT_DISPATCH(AXIOM_LEVEL_FAULT, mod, evt, ##__VA_ARGS__)

/* ---------------------------------------------------------------------------
 * AX_LOG — Development logging (PROD profile = no-op)
 * Outputs plain text via axiom_port_string_out(). Does NOT enter binary protocol.
 * --------------------------------------------------------------------------- */
#if (AXIOM_PROFILE == AXIOM_PROFILE_DEV) || (AXIOM_PROFILE == AXIOM_PROFILE_FIELD)

#define AX_LOG(msg)         do { axiom_port_string_out(msg); axiom_port_string_out("\n"); } while(0)
#define AX_LOG_DEBUG(msg)   do { axiom_port_string_out("[DEBUG] "); axiom_port_string_out(msg); axiom_port_string_out("\n"); } while(0)
#define AX_LOG_INFO(msg)    do { axiom_port_string_out("[INFO] ");  axiom_port_string_out(msg); axiom_port_string_out("\n"); } while(0)
#define AX_LOG_WARN(msg)    do { axiom_port_string_out("[WARN] ");  axiom_port_string_out(msg); axiom_port_string_out("\n"); } while(0)
#define AX_LOG_ERROR(msg)   do { axiom_port_string_out("[ERROR] "); axiom_port_string_out(msg); axiom_port_string_out("\n"); } while(0)

#else /* PROD */

#define AX_LOG(msg)         ((void)0)
#define AX_LOG_DEBUG(msg)   ((void)0)
#define AX_LOG_INFO(msg)    ((void)0)
#define AX_LOG_WARN(msg)    ((void)0)
#define AX_LOG_ERROR(msg)   ((void)0)

#endif

/* ---------------------------------------------------------------------------
 * AX_PROBE — Timing probe (PROD profile = no-op)
 * Writes minimal structured event with a 16-bit tag hash + value.
 * --------------------------------------------------------------------------- */
#if (AXIOM_PROFILE == AXIOM_PROFILE_DEV) || (AXIOM_PROFILE == AXIOM_PROFILE_FIELD)

#define AX_PROBE(tag, value) \
    do { uint8_t _b[AXIOM_MAX_PAYLOAD_LEN]; uint8_t _p = 0; \
         uint16_t _th = _axiom_fnv1a_16(tag); \
         axiom_enc_u16(_b, &_p, _th); \
         _AXIOM_ENCODE_ONE(_b, _p, value); \
         axiom_write(AXIOM_LEVEL_DEBUG, 0, 0, _b, _p); } while(0)

#else /* PROD */

#define AX_PROBE(tag, value) ((void)0)

#endif

/* ---------------------------------------------------------------------------
 * AX_KV — Key-Value event (always compiled)
 * Keys are hashed at compile-time to 16-bit IDs via FNV-1a.
 * --------------------------------------------------------------------------- */
#define _AXIOM_KV_KEYVAL(buf, pos, key, val) \
    do { uint16_t _kh = _axiom_fnv1a_16(key); \
         axiom_enc_u16(buf, &(pos), _kh); \
         _AXIOM_ENCODE_ONE(buf, pos, val); \
    } while(0)

#define _AXIOM_KV_2(level, mod, evt, k1, v1) \
    do { uint8_t _b[AXIOM_MAX_PAYLOAD_LEN]; uint8_t _p = 0; \
         _AXIOM_KV_KEYVAL(_b, _p, k1, v1); \
         axiom_write((level), (mod), (evt), _b, _p); } while(0)

#define _AXIOM_KV_4(level, mod, evt, k1, v1, k2, v2) \
    do { uint8_t _b[AXIOM_MAX_PAYLOAD_LEN]; uint8_t _p = 0; \
         _AXIOM_KV_KEYVAL(_b, _p, k1, v1); \
         _AXIOM_KV_KEYVAL(_b, _p, k2, v2); \
         axiom_write((level), (mod), (evt), _b, _p); } while(0)

#define AX_KV(level, mod, evt, ...) \
    _AXIOM_CONCAT(_AXIOM_KV_, _AXIOM_NARG(__VA_ARGS__))(AXIOM_LEVEL_##level, mod, evt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* AXIOM_FRONTEND_H */
