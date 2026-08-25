/* ============================================================================
 * AxiomTrace custom Port + Backend skeleton
 * ============================================================================
 *
 * This translation unit is a compile-checked integration template, not a
 * board driver. Copy it into the firmware target, replace the marked Port
 * callbacks with the platform SDK/assembly, and replace the Backend write
 * callback with the real non-blocking transport. Do not link this object in
 * addition to another Port provider.
 *
 * The local CMake target is an OBJECT target and is excluded from the default
 * build, so it verifies the callback signatures without changing the Host
 * example or pretending to validate hardware.
 * ============================================================================ */

#include "axiomtrace.h"

/* ---------------------------------------------------------------------------
 * Port contract
 * ---------------------------------------------------------------------------
 * The timestamp must be monotonic in microseconds (natural uint32_t wrap is
 * allowed). Critical sections must cover the Core's IRQ-safe operations. The
 * optional diagnostics/Flash hooks may return the documented unavailable
 * values until the firmware supplies them.
 */

uint32_t axiom_port_timestamp(void) {
    /* TODO(integrator): read a monotonic hardware timer and convert to us. */
    return 0u;
}

void axiom_port_critical_enter(void) {
    /* TODO(integrator): save/disable IRQ state; support the chosen nesting. */
}

void axiom_port_critical_exit(void) {
    /* TODO(integrator): restore the IRQ state saved by _enter(). */
}

void axiom_port_string_out(const char *str) {
    /* TODO(integrator): route DEV/FIELD AX_LOG text to UART/RTT if wanted. */
    (void)str;
}

void axiom_port_fault_hook(uint8_t module_id, uint16_t event_id,
                           const uint8_t *payload, uint8_t payload_len) {
    /* TODO(integrator): record an out-of-band fault indication if required. */
    (void)module_id;
    (void)event_id;
    (void)payload;
    (void)payload_len;
}

uint8_t axiom_port_reset_reason(void) {
    /* TODO(integrator): return the platform reset-cause code. */
    return 0u;
}

uint8_t axiom_port_fault_snapshot(uint8_t *buf, uint8_t max_len) {
    /* TODO(integrator): copy a bounded register snapshot into buf. */
    (void)buf;
    (void)max_len;
    return 0u;
}

int axiom_port_flash_erase(uint32_t addr, uint32_t len) {
    /* TODO(integrator): implement only for a non-ISR capsule commit path. */
    (void)addr;
    (void)len;
    return -1;
}

int axiom_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    /* TODO(integrator): honor the target's erase/program/alignment contract. */
    (void)addr;
    (void)data;
    (void)len;
    return -1;
}

int axiom_port_flash_read(uint32_t addr, uint8_t *out, uint32_t len) {
    /* TODO(integrator): read from the configured capsule region. */
    (void)addr;
    (void)out;
    (void)len;
    return -1;
}

/* ---------------------------------------------------------------------------
 * Backend contract
 * ---------------------------------------------------------------------------
 * write() must be bounded and non-blocking in the normal path. Return a
 * negative value while the transport is unavailable; the registry then
 * applies its configured degradation/recovery policy and invokes on_drop.
 */

typedef struct {
    uint32_t dropped;
    void *transport;
} axiom_custom_backend_ctx_t;

axiom_backend_t axiom_custom_backend_init(const char *name,
                                          axiom_custom_backend_ctx_t *ctx);

static int axiom_custom_backend_write(const uint8_t *buf, uint16_t len,
                                      void *opaque) {
    axiom_custom_backend_ctx_t *ctx = (axiom_custom_backend_ctx_t *)opaque;
    /* TODO(integrator): enqueue buf[0..len) into the hardware transport. */
    (void)buf;
    (void)len;
    (void)ctx;
    return -1;
}

static int axiom_custom_backend_ready(void *opaque) {
    /* TODO(integrator): return 1 only when the transport can accept a frame. */
    (void)opaque;
    return 0;
}

static int axiom_custom_backend_flush(void *opaque) {
    /* TODO(integrator): drain the transport from a non-ISR context. */
    (void)opaque;
    return 0;
}

static int axiom_custom_backend_panic_write(const uint8_t *buf, uint16_t len,
                                            void *opaque) {
    /* TODO(integrator): provide a bounded emergency write if supported. */
    return axiom_custom_backend_write(buf, len, opaque);
}

static void axiom_custom_backend_on_drop(uint32_t lost, void *opaque) {
    axiom_custom_backend_ctx_t *ctx = (axiom_custom_backend_ctx_t *)opaque;
    if (ctx != NULL) {
        ctx->dropped += lost;
    }
}

axiom_backend_t axiom_custom_backend_init(const char *name,
                                          axiom_custom_backend_ctx_t *ctx) {
    if (ctx != NULL) {
        ctx->dropped = 0u;
        ctx->transport = NULL;
    }
    return (axiom_backend_t)AXIOM_BACKEND_INIT(
        .name = name,
        .caps = 0u, /* Set the matching AXIOM_BACKEND_CAP_* after integration. */
        .write = axiom_custom_backend_write,
        .ready = axiom_custom_backend_ready,
        .flush = axiom_custom_backend_flush,
        .panic_write = axiom_custom_backend_panic_write,
        .on_drop = axiom_custom_backend_on_drop,
        .ctx = ctx);
}
