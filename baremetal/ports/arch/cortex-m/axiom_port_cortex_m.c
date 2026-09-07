#include "axiom_port.h"

/* The port contract is microseconds, while DWT_CYCCNT is CPU cycles.
 * Define AXIOM_CPU_HZ to the actual core clock for a usable timestamp. */
#ifndef AXIOM_CPU_HZ
#define AXIOM_CPU_HZ 0u
#endif

/* ============================================================================
 * Cortex-M Architecture Port
 * ============================================================================
 *
 * ARM Cortex-M specific implementations using CoreDebug/DWT registers.
 * DWT support is selected explicitly by AXIOM_CORTEX_M_HAS_DWT.  The safe
 * default is no DWT, because some otherwise compatible cores (including
 * Cortex-M23 implementations) may omit the cycle counter.
 *
 * These are weak defaults.  A board port may replace individual functions
 * with strong symbols, or select a complete custom provider in CMake.
 * ============================================================================ */

#ifndef AXIOM_CORTEX_M_HAS_DWT
#define AXIOM_CORTEX_M_HAS_DWT 0
#endif

#if (AXIOM_CORTEX_M_HAS_DWT != 0) && (AXIOM_CORTEX_M_HAS_DWT != 1)
#error "AXIOM_CORTEX_M_HAS_DWT must be 0 or 1"
#endif

#if AXIOM_CORTEX_M_HAS_DWT
/* DWT_CYCCNT 寄存器地址 (Cortex-M7 example, varies by core) */
static volatile uint32_t * const DWT_CTRL = (uint32_t *)0xE0001000;
static volatile uint32_t * const DWT_CYCCNT = (uint32_t *)0xE0001004;
static volatile uint32_t * const SCB_DEMCR = (uint32_t *)0xE000EDFC;

static uint32_t axiom_port_cycles_to_us(uint32_t cycles) {
#if AXIOM_CPU_HZ > 0u
    return (uint32_t)(((uint64_t)cycles * 1000000ull) /
                      (uint64_t)AXIOM_CPU_HZ);
#else
    /* No honest cycle-to-time conversion is possible without the clock. */
    (void)cycles;
    return 0u;
#endif
}
#endif

AXIOM_COMPILER_WEAK uint32_t axiom_port_timestamp(void) {
#if AXIOM_CORTEX_M_HAS_DWT
    /* 确保 DWT_CYCCNT 已启用 */
    if ((*DWT_CTRL & 0x1u) == 0u) {
        /* 启用 trace, 先解锁 DWT */
        *SCB_DEMCR |= 0x01000000u;  /* TRCENA bit */
        *DWT_CTRL |= 0x1u;          /* CYCCNTENA bit */
    }
    return axiom_port_cycles_to_us((uint32_t)*DWT_CYCCNT);
#else
    /* 降级方案: 使用 SysTick 或返回 0 */
    return 0u;
#endif
}

static uint32_t g_critical_nesting;
static uint32_t g_saved_primask;

AXIOM_COMPILER_WEAK void axiom_port_critical_enter(void) {
    uint32_t primask;
    /* Keep the read and mask operation together so the saved state belongs to
     * the outermost critical section. */
    __asm volatile (
        "mrs %0, primask\n"
        "cpsid i"
        : "=r"(primask)
        :
        : "memory");
    if (g_critical_nesting == 0u) {
        g_saved_primask = primask;
    }
    g_critical_nesting++;
}

AXIOM_COMPILER_WEAK void axiom_port_critical_exit(void) {
    if (g_critical_nesting > 0u) {
        g_critical_nesting--;
        if (g_critical_nesting == 0u) {
            __asm volatile ("msr primask, %0" : : "r"(g_saved_primask) : "memory");
        }
    }
}

AXIOM_COMPILER_WEAK void axiom_port_string_out(const char *str) {
    /* 默认空实现 - 由具体 SoC/Board 实现提供 */
    (void)str;
}

AXIOM_COMPILER_WEAK void axiom_port_fault_hook(uint8_t module_id, uint16_t event_id,
                                               const uint8_t *payload, uint8_t payload_len) {
    (void)module_id;
    (void)event_id;
    (void)payload;
    (void)payload_len;
    /* 可在此处添加硬件调试器集成 */
}

AXIOM_COMPILER_WEAK uint8_t axiom_port_reset_reason(void) {
    return 0u;
}

AXIOM_COMPILER_WEAK uint8_t axiom_port_fault_snapshot(uint8_t *buf, uint8_t max_len) {
    (void)buf;
    (void)max_len;
    return 0;
}

AXIOM_COMPILER_WEAK int axiom_port_flash_erase(uint32_t addr, uint32_t len) {
    (void)addr;
    (void)len;
    return -1;  /* 需要 SoC 实现 */
}

AXIOM_COMPILER_WEAK int axiom_port_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    (void)addr;
    (void)data;
    (void)len;
    return -1;  /* 需要 SoC 实现 */
}

AXIOM_COMPILER_WEAK int axiom_port_flash_read(uint32_t addr, uint8_t *out, uint32_t len) {
    (void)addr;
    (void)out;
    (void)len;
    return -1;  /* 需要 SoC 实现 */
}
