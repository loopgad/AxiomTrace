/* ============================================================================
 * SEGGER RTT Backend for AxiomTrace
 * ============================================================================
 *
 * Provides high-speed real-time transfer via J-Link debug probes.
 * Zero-copy design with static RTT buffer configuration.
 *
 * Requirements:
 * - SEGGER J-Link firmware (V6.30 or later recommended)
 * - RTT Viewer tool from J-Link software package
 * - No Nordic SDK dependency - uses SEGGER's public RTT API
 *
 * ============================================================================ */

#include "axiom_backend.h"
#include <stdint.h>
#include <string.h>

/* ============================================================================
 * SEGGER RTT API Declarations
 * These are from SEGGER's public RTT header (RTT.h)
 * ============================================================================ */
#ifdef __cplusplus
extern "C" {
#endif

/* RTT control block - must be placed in memory by linker */
typedef struct {
    char        acID[16];
    int32_t     MaxNumUpBuffers;
    int32_t     MaxNumDownBuffers;
} SEGGER_RTT_CB;

/* Buffer descriptor */
typedef struct {
    volatile int32_t WrOff;         /* Write offset */
    volatile int32_t RdOff;         /* Read offset (volatile) */
    int32_t         Flags;
    char           *pBuffer;       /* Pointer to buffer */
    int32_t         SizeOfBuffer;   /* Size of buffer */
    int32_t         Next;           /* Index of next buffer */
} SEGGER_RTT_BUFFER;

/* RTT control block structure */
typedef struct {
    SEGGER_RTT_BUFFER Up[3];        /* Up buffers (MCU->Host) */
    SEGGER_RTT_BUFFER Down[3];      /* Down buffers (Host->MCU) */
} SEGGER_RTT_RING_BUFFER;

extern SEGGER_RTT_RING_BUFFER _SEGGER_RTT;

/* Core RTT API */
int SEGGER_RTT_Write(unsigned BufferIndex, const void *pData, unsigned NumBytes);
int SEGGER_RTT_WriteString(unsigned BufferIndex, const char *pStr);
int SEGGER_RTT_GetBytesInBuffer(unsigned BufferIndex);

/* RTT buffer configuration */
int SEGGER_RTT_ConfigUpBuffer(unsigned BufferIndex, const char *pName,
                               char *pBuffer, int BufferSize, unsigned Flags);
int SEGGER_RTT_SetTerminal(unsigned char TerminalId);
unsigned SEGGER_RTT_GetTerminal(void);

#ifdef __cplusplus
}
#endif

/* ============================================================================
 * RTT Backend Configuration
 * ============================================================================ */

/* Default RTT buffer size - 1KB up buffer, static allocation */
#define AXIOM_RTT_BUFFER_SIZE     1024
#define AXIOM_RTT_BUFFER_INDEX    1  /* Use channel 1 (0 is often used for logs) */
#define AXIOM_RTT_FLAGS           0  /* Default flags */

/* Static buffers - no malloc */
static uint8_t s_rtt_up_buffer[AXIOM_RTT_BUFFER_SIZE];

/* Backend context */
typedef struct {
    uint32_t dropped_frames;
    uint8_t  initialized;
} axiom_rtt_backend_ctx_t;

static axiom_rtt_backend_ctx_t s_rtt_ctx = {
    .dropped_frames = 0,
    .initialized = 0
};

/* ============================================================================
 * RTT Backend Callbacks
 * ============================================================================ */

/* Write callback - called by axiom_backend_dispatch() */
static int rtt_write(const uint8_t *buf, uint16_t len, void *ctx) {
    (void)ctx;

    if (!s_rtt_ctx.initialized) {
        return -1;
    }

    /* SEGGER_RTT_Write returns number of bytes written, or negative on error */
    int written = SEGGER_RTT_Write(AXIOM_RTT_BUFFER_INDEX, buf, len);

    if (written < 0) {
        /* RTT not connected or error */
        return -1;
    }

    if ((uint16_t)written < len) {
        /* Partial write - bytes were dropped */
        s_rtt_ctx.dropped_frames++;
    }

    return 0;
}

/* Ready callback - RTT is always ready if initialized */
static int rtt_ready(void *ctx) {
    (void)ctx;
    return s_rtt_ctx.initialized ? 1 : 0;
}

/* Flush callback - RTT is already flushed (no local buffering) */
static int rtt_flush(void *ctx) {
    (void)ctx;
    return 0;  /* SEGGER_RTT_Write is immediate, no flush needed */
}

/* Panic write - for fault conditions, use blocking write */
static int rtt_panic_write(const uint8_t *buf, uint16_t len, void *ctx) {
    (void)ctx;

    if (!s_rtt_ctx.initialized) {
        return -1;
    }

    /* Try to write, but don't wait if buffer is full */
    int written = SEGGER_RTT_Write(AXIOM_RTT_BUFFER_INDEX, buf, len);

    if (written < 0) {
        s_rtt_ctx.dropped_frames++;
        return -1;
    }

    return 0;
}

/* Drop callback - called when frames are dropped */
static void rtt_on_drop(uint32_t lost, void *ctx) {
    axiom_rtt_backend_ctx_t *c = (axiom_rtt_backend_ctx_t *)ctx;
    (void)lost;
    c->dropped_frames++;
}

/* ============================================================================
 * Backend Instance
 * ============================================================================ */
static axiom_backend_t s_rtt_backend = {
    .name = "RTT",
    .caps = AXIOM_BACKEND_CAP_RTT,
    .write = rtt_write,
    .ready = rtt_ready,
    .flush = rtt_flush,
    .panic_write = rtt_panic_write,
    .on_drop = rtt_on_drop,
    .ctx = &s_rtt_ctx,
};

/* ============================================================================
 * Public API
 * ============================================================================ */

/* Initialize RTT backend and register with AxiomTrace
 * Call this from your initialization code before axiom_init() */
int axiom_backend_rtt_init(void) {
    /* Configure up buffer - this is safe to call multiple times */
    int result = SEGGER_RTT_ConfigUpBuffer(
        AXIOM_RTT_BUFFER_INDEX,
        "AxiomTrace",
        (char *)s_rtt_up_buffer,
        AXIOM_RTT_BUFFER_SIZE,
        AXIOM_RTT_FLAGS
    );

    if (result < 0) {
        return -1;  /* Failed to configure RTT buffer */
    }

    s_rtt_ctx.initialized = 1;

    /* Register with axiom backend system */
    return axiom_backend_register(&s_rtt_backend);
}

/* Get the RTT backend instance for manual registration */
const axiom_backend_t *axiom_backend_rtt_get(void) {
    return &s_rtt_backend;
}

/* Get drop count */
uint32_t axiom_backend_rtt_get_dropped(void) {
    return s_rtt_ctx.dropped_frames;
}

/* Check if RTT is connected (bytes available to read means host connected) */
int axiom_backend_rtt_is_connected(void) {
    return SEGGER_RTT_GetBytesInBuffer(AXIOM_RTT_BUFFER_INDEX) >= 0;
}

/* ============================================================================
 * String Output Helper (used by axiom_port_string_out)
 * ============================================================================ */
void axiom_rtt_write_string(const char *str) {
    if (s_rtt_ctx.initialized && str != NULL) {
        SEGGER_RTT_WriteString(AXIOM_RTT_BUFFER_INDEX, str);
    }
}
