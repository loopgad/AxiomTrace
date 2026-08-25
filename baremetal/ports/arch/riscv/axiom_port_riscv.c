#include "axiom_port.h"

/* The port contract is microseconds, while mcycle is CPU cycles.
 * Define AXIOM_CPU_HZ to the actual core clock for a usable timestamp. */
#ifndef AXIOM_CPU_HZ
#define AXIOM_CPU_HZ 0u
#endif

/* The critical-section state below is process-global. This port is therefore
 * intentionally limited to one machine-mode hart until a per-hart backend is
 * supplied. */
#ifndef AXIOM_RISCV_SINGLE_HART
#define AXIOM_RISCV_SINGLE_HART 1
#endif

#if defined(__riscv) && !AXIOM_RISCV_SINGLE_HART
#error "axiom_port_riscv requires AXIOM_RISCV_SINGLE_HART=1"
#endif

/* ============================================================================
 * RISC-V Architecture Port
 * ============================================================================
 *
 * RISC-V specific implementations using machine-mode registers.
 * Works on RV32I/RV64I cores with standard mtime/mcycle.
 *
 * Default weak implementations - override by providing strong symbols.
 * ============================================================================ */

#if defined(__riscv)

/* RISC-V CSR 地址 */
#define CSR_MSTATUS 0x300
#define CSR_MIE     0x304
#define CSR_MTVEC   0x305

/* 读取 mcycle CSR */
static inline uint64_t __riscv_read_mcycle(void) {
#if __riscv_xlen == 32
    uint32_t high_before;
    uint32_t low;
    uint32_t high_after;
    do {
        __asm volatile ("csrr %0, mcycleh" : "=r"(high_before));
        __asm volatile ("csrr %0, mcycle" : "=r"(low));
        __asm volatile ("csrr %0, mcycleh" : "=r"(high_after));
    } while (high_before != high_after);
    return ((uint64_t)high_after << 32u) | low;
#else
    uint64_t val;
    __asm volatile ("csrr %0, mcycle" : "=r"(val));
    return val;
#endif
}

static uint32_t axiom_port_cycles_to_us(uint64_t cycles) {
#if AXIOM_CPU_HZ > 0u
    const uint64_t hz = (uint64_t)AXIOM_CPU_HZ;
    const uint64_t seconds = cycles / hz;
    const uint64_t remainder = cycles % hz;
    return (uint32_t)(seconds * 1000000ull +
                      (remainder * 1000000ull) / hz);
#else
    /* No honest cycle-to-time conversion is possible without the clock. */
    (void)cycles;
    return 0u;
#endif
}

#endif

uint32_t axiom_port_timestamp(void) {
#if defined(__riscv)
    return axiom_port_cycles_to_us(__riscv_read_mcycle());
#else
    return 0u;
#endif
}

#define MSTATUS_MIE 0x8u

static uint32_t g_critical_nesting = 0u;
static uint32_t g_mstatus_state = 0u;

void axiom_port_critical_enter(void) {
#if defined(__riscv)
    /* Atomically save mstatus and clear MIE before touching nesting state. */
    uint32_t mstatus;
    const uint32_t mie = MSTATUS_MIE;
    __asm volatile ("csrrc %0, mstatus, %1" : "=r"(mstatus) : "r"(mie) : "memory");
    if (g_critical_nesting == 0u) {
        g_mstatus_state = mstatus;
    }
    g_critical_nesting++;
#endif
}

void axiom_port_critical_exit(void) {
#if defined(__riscv)
    if (g_critical_nesting > 0u) {
        g_critical_nesting--;
        if (g_critical_nesting == 0u && (g_mstatus_state & MSTATUS_MIE) != 0u) {
            const uint32_t mie = MSTATUS_MIE;
            __asm volatile ("csrs mstatus, %0" : : "r"(mie) : "memory");
        }
    }
#endif
}

void axiom_port_string_out(const char *str) {
    (void)str;
    /* 默认空实现 - 由具体 SoC/Board 实现提供 */
}

void axiom_port_fault_hook(uint8_t module_id, uint16_t event_id,
                           const uint8_t *payload, uint8_t payload_len) {
    (void)module_id;
    (void)event_id;
    (void)payload;
    (void)payload_len;
}

uint8_t axiom_port_reset_reason(void) {
    return 0u;
}

uint8_t axiom_port_fault_snapshot(uint8_t *buf, uint8_t max_len) {
    (void)buf;
    (void)max_len;
    return 0;
}

int axiom_port_flash_erase(uint32_t addr, uint32_t len) {
    (void)addr;
    (void)len;
    return -1;  /* 需要 SoC 实现 */
}

int axiom_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    (void)addr;
    (void)data;
    (void)len;
    return -1;  /* 需要 SoC 实现 */
}

int axiom_port_flash_read(uint32_t addr, uint8_t *out, uint32_t len) {
    (void)addr;
    (void)out;
    (void)len;
    return -1;  /* 需要 SoC 实现 */
}
