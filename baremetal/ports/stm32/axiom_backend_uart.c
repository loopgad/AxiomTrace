/**
 * @file axiom_backend_uart.c
 * @brief STM32 UART DMA Backend with COBS Framing
 *
 * Provides non-blocking UART transmission using DMA.
 * Uses COBS (Consistent Overhead Byte Stuffing) encoding with 0x00 frame delimiter.
 *
 * Features:
 * - Non-blocking DMA-based transmission
 * - COBS framing for reliable frame detection
 * - Panic write support for fault events
 * - Zero malloc, ISR-safe
 */

#include "axiom_backend.h"
#include <string.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------------
 * Configuration - User must customize for their hardware
 * --------------------------------------------------------------------------- */
#ifndef AXIOM_UART_TX_BUFFER_SIZE
#define AXIOM_UART_TX_BUFFER_SIZE  256
#endif

#ifndef AXIOM_UART_TX_COBS_BUFFER_SIZE
/* COBS adds overhead: max 1 byte per 254 bytes + framing */
#define AXIOM_UART_TX_COBS_BUFFER_SIZE  (AXIOM_UART_TX_BUFFER_SIZE + 16)
#endif

/* ---------------------------------------------------------------------------
 * STM32F4 UART/USART Register Definitions (CMSIS-style)
 * --------------------------------------------------------------------------- */
typedef struct {
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t BRR;
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t CR3;
    volatile uint32_t GTPR;
} UART_TypeDef;

typedef struct {
    volatile uint32_t CCR;
    volatile uint32_t CNDTR;
    volatile uint32_t CPAR;
    volatile uint32_t CMAR;
} DMA_Stream_TypeDef;

/* UART base addresses (example for STM32F4 - adjust for your chip) */
#define UART1_BASE               0x40011000UL
#define UART1                    ((UART_TypeDef *) UART1_BASE)

#define DMA2_BASE                0x40026400UL
#define DMA2_STREAM7_BASE        0x40026438UL
#define DMA2_STREAM7             ((DMA_Stream_TypeDef *) DMA2_STREAM7_BASE)

/* UART SR bits */
#define UART_SR_TXE              (1UL << 7)
#define UART_SR_TC               (1UL << 6)

/* UART CR1 bits */
#define UART_CR1_UE              (1UL << 13)
#define UART_CR1_TE              (1UL << 3)
#define UART_CR1_RE              (1UL << 2)

/* DMA SxCR bits */
#define DMA_SxCR_EN              (1UL << 0)
#define DMA_SxCR_TCIE            (1UL << 4)
#define DMA_SxCR_DIR             (1UL << 6)
#define DMA_SxCR_DIR_MEM_TO_PER  (1UL << 7)

/* ---------------------------------------------------------------------------
 * Context Structure
 * --------------------------------------------------------------------------- */
typedef struct {
    UART_TypeDef *uart;
    DMA_Stream_TypeDef *dma_stream;
    uint32_t uart_clock;

    uint8_t tx_buf[AXIOM_UART_TX_BUFFER_SIZE];
    uint8_t cobs_buf[AXIOM_UART_TX_COBS_BUFFER_SIZE];

    volatile bool dma_busy;
    volatile bool panic_mode;
} axiom_uart_backend_ctx_t;

/* Default context - user can provide their own */
static axiom_uart_backend_ctx_t g_uart_ctx;

/* ---------------------------------------------------------------------------
 * COBS (Consistent Overhead Byte Stuffing) Encoding
 *
 * Encodes data so that 0x00 can be used as frame delimiter.
 * Max code byte value is 0xFF, indicating no 0x00 in the block.
 * --------------------------------------------------------------------------- */
static uint16_t cobs_encode(const uint8_t *input, uint16_t input_len,
                            uint8_t *output, uint16_t output_size) {
    if (input_len == 0 || output_size < input_len + 2) {
        return 0;
    }

    uint16_t code_idx = 0;
    uint16_t data_idx = 1;
    uint16_t input_idx = 0;
    uint8_t run_len = 1;

    output[code_idx] = 0xFF; /* Placeholder for code byte */

    while (input_idx < input_len) {
        if (data_idx >= output_size - 1) {
            break; /* Buffer full */
        }

        if (input[input_idx] == 0x00) {
            /* Found 0x00 - write code byte with distance to next 0x00 or end */
            output[code_idx] = run_len;
            code_idx = data_idx;
            output[++data_idx] = 0xFF; /* Placeholder */
            run_len = 1;
        } else {
            output[data_idx] = input[input_idx];
            data_idx++;
            run_len++;
        }

        /* Handle run length overflow (shouldn't happen with proper framing) */
        if (run_len == 0xFF) {
            output[code_idx] = run_len;
            code_idx = data_idx;
            output[++data_idx] = 0xFF;
            run_len = 1;
        }

        input_idx++;
    }

    /* Write final code byte */
    output[code_idx] = run_len;

    return data_idx + 1; /* Include final delimiter */
}

/* ---------------------------------------------------------------------------
 * UART/DMA Transmission (non-blocking)
 * --------------------------------------------------------------------------- */
static void uart_dma_start(const uint8_t *data, uint16_t len) {
    axiom_uart_backend_ctx_t *ctx = &g_uart_ctx;

    if (ctx->dma_busy) {
        return; /* Should not happen - caller should check ready() */
    }

    /* Copy data to DMA source (in case original buffer is stack) */
    if (data != ctx->tx_buf) {
        uint16_t copy_len = (len < AXIOM_UART_TX_BUFFER_SIZE) ? len : AXIOM_UART_TX_BUFFER_SIZE;
        memcpy(ctx->tx_buf, data, copy_len);
    }

    /* Configure DMA stream */
    ctx->dma_stream->CCR = 0;
    ctx->dma_stream->CNDTR = len;
    ctx->dma_stream->CPAR = (uint32_t)&ctx->uart->DR;
    ctx->dma_stream->CMAR = (uint32_t)ctx->tx_buf;

    /* Enable DMA: memory->UART, 8-bit, no increment for periph */
    ctx->dma_stream->CCR = DMA_SxCR_DIR_MEM_TO_PER |
                          (0UL << 4) |  /* No TCIE initially */
                          (2UL << 8) |  /* 8-bit mem */
                          (2UL << 10) | /* 8-bit periph */
                          (1UL << 8);   /* Mem increment */

    ctx->dma_stream->CCR |= DMA_SxCR_EN;

    ctx->dma_busy = true;
}

/* ---------------------------------------------------------------------------
 * UART Backend Implementation
 * --------------------------------------------------------------------------- */
static int uart_backend_write(const uint8_t *frame, uint16_t len, void *ctx) {
    axiom_uart_backend_ctx_t *uart_ctx = (axiom_uart_backend_ctx_t *)ctx;
    if (!uart_ctx || !frame || len == 0) return -1;

    /* Check if DMA is available */
    if (uart_ctx->dma_busy) {
        return -1; /* Busy, drop frame */
    }

    /* Encode frame with COBS */
    uint16_t encoded_len = cobs_encode(frame, len,
                                        uart_ctx->cobs_buf,
                                        AXIOM_UART_TX_COBS_BUFFER_SIZE);
    if (encoded_len == 0) return -1;

    /* Add frame delimiter (0x00) */
    if (encoded_len < AXIOM_UART_TX_COBS_BUFFER_SIZE) {
        uart_ctx->cobs_buf[encoded_len++] = 0x00;
    }

    /* Start DMA transfer */
    uart_dma_start(uart_ctx->cobs_buf, encoded_len);

    return 0;
}

static int uart_backend_ready(void *ctx) {
    axiom_uart_backend_ctx_t *uart_ctx = (axiom_uart_backend_ctx_t *)ctx;
    if (!uart_ctx) return 0;
    return !uart_ctx->dma_busy;
}

static int uart_backend_flush(void *ctx) {
    axiom_uart_backend_ctx_t *uart_ctx = (axiom_uart_backend_ctx_t *)ctx;
    if (!uart_ctx) return -1;

    /* Wait for DMA completion (blocking - use with caution) */
    while (uart_ctx->dma_busy) {
        /* Could add timeout here */
    }

    return 0;
}

static int uart_backend_panic_write(const uint8_t *frame, uint16_t len, void *ctx) {
    axiom_uart_backend_ctx_t *uart_ctx = (axiom_uart_backend_ctx_t *)ctx;
    if (!uart_ctx || !frame || len == 0) return -1;

    uart_ctx->panic_mode = true;

    /* For panic writes, we might want blocking transmission */
    /* Encode and send directly via polling */
    uint16_t encoded_len = cobs_encode(frame, len,
                                        uart_ctx->cobs_buf,
                                        AXIOM_UART_TX_COBS_BUFFER_SIZE);
    if (encoded_len == 0) return -1;

    uart_ctx->cobs_buf[encoded_len++] = 0x00;

    /* Polling transmission (blocking but fast for small frames) */
    for (uint16_t i = 0; i < encoded_len; i++) {
        /* Wait for TX buffer empty */
        while (!(uart_ctx->uart->SR & UART_SR_TXE));
        uart_ctx->uart->DR = uart_ctx->cobs_buf[i];
    }

    /* Wait for transmission complete */
    while (!(uart_ctx->uart->SR & UART_SR_TC));

    uart_ctx->panic_mode = false;

    return 0;
}

static void uart_backend_on_drop(uint32_t lost, void *ctx) {
    (void)ctx;
    (void)lost;
    /* Could add LED blink or error counter here */
}

/* ---------------------------------------------------------------------------
 * UART Backend Initialization
 *
 * Call once at startup after clock configuration.
 * --------------------------------------------------------------------------- */
void axiom_backend_uart_init(UART_TypeDef *uart, DMA_Stream_TypeDef *dma_stream,
                             uint32_t uart_clock, uint32_t baudrate) {
    axiom_uart_backend_ctx_t *ctx = &g_uart_ctx;

    ctx->uart = uart;
    ctx->dma_stream = dma_stream;
    ctx->uart_clock = uart_clock;
    ctx->dma_busy = false;
    ctx->panic_mode = false;

    /* Configure UART: 8N1, enable transmitter */
    uint32_t brr = uart_clock / baudrate;
    ctx->uart->BRR = brr;
    ctx->uart->CR1 = UART_CR1_UE | UART_CR1_TE;
    ctx->uart->CR2 = 0;
    ctx->uart->CR3 = 0;
}

/* ---------------------------------------------------------------------------
 * DMA Transfer Complete Callback
 *
 * Call this from DMA Stream IRQ handler:
 *   if (DMA2_Stream7->LISR & DMA_LISR_TCIF7) {
 *       DMA2_Stream7->LISR = DMA_LISR_TCIF7;
 *       axiom_backend_uart_dma_complete();
 *   }
 * --------------------------------------------------------------------------- */
void axiom_backend_uart_dma_complete(void) {
    axiom_uart_backend_ctx_t *ctx = &g_uart_ctx;
    ctx->dma_busy = false;
}

/* ---------------------------------------------------------------------------
 * Get UART Backend Descriptor
 *
 * Returns backend descriptor for axiom_backend_register().
 * --------------------------------------------------------------------------- */
axiom_backend_t axiom_backend_uart(const char *name,
                                   axiom_uart_backend_ctx_t *ctx) {
    if (!ctx) {
        ctx = &g_uart_ctx;
    }

    return (axiom_backend_t){
        .name = name ? name : "uart",
        .caps = AXIOM_BACKEND_CAP_UART,
        .write = uart_backend_write,
        .ready = uart_backend_ready,
        .flush = uart_backend_flush,
        .panic_write = uart_backend_panic_write,
        .on_drop = uart_backend_on_drop,
        .ctx = ctx,
    };
}