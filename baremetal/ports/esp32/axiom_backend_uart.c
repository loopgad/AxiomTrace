/**
 * @file axiom_backend_uart.c
 * @brief ESP32 UART Backend with COBS Framing
 *
 * Provides non-blocking UART transmission using ESP-IDF's UART driver.
 * Uses COBS (Consistent Overhead Byte Stuffing) encoding with 0x00 frame delimiter.
 *
 * Features:
 * - Non-blocking transmission via ESP-IDF UART driver
 * - COBS framing for reliable frame detection
 * - Panic write support for fault events
 * - Zero malloc, ISR-safe
 * - Configurable UART pins via Kconfig or parameters
 *
 * Note: UART GPIO pins are NOT hardcoded - they must be configured via
 * menuconfig (idf.py menuconfig) or passed during initialization.
 */

#include "axiom_backend.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* ESP-IDF UART driver */
#include "driver/uart.h"

/* ---------------------------------------------------------------------------
 * Configuration
 * --------------------------------------------------------------------------- */
#ifndef AXIOM_UART_TX_BUFFER_SIZE
#define AXIOM_UART_TX_BUFFER_SIZE  256
#endif

#ifndef AXIOM_UART_TX_COBS_BUFFER_SIZE
/* COBS adds overhead: max 1 byte per 254 bytes + framing */
#define AXIOM_UART_TX_COBS_BUFFER_SIZE  (AXIOM_UART_TX_BUFFER_SIZE + 16)
#endif

/* Default UART configuration - can be overridden via menuconfig */
#ifndef AXIOM_ESP32_UART_NUM
#define AXIOM_ESP32_UART_NUM    UART_NUM_1
#endif

#ifndef AXIOM_ESP32_UART_BAUD
#define AXIOM_ESP32_UART_BAUD    921600
#endif

/* Default TX GPIO - MUST be configured via menuconfig or axiom_backend_uart_init() */
/* These are placeholders - actual pins should be set via Kconfig */
#ifndef AXIOM_ESP32_UART_TX_GPIO
#define AXIOM_ESP32_UART_TX_GPIO  10  /* Default: GPIO10 (UART1 default without matrix) */
#endif

#ifndef AXIOM_ESP32_UART_RX_GPIO
#define AXIOM_ESP32_UART_RX_GPIO  9   /* Default: GPIO9 (UART1 default without matrix) */
#endif

/* ---------------------------------------------------------------------------
 * Context Structure
 * --------------------------------------------------------------------------- */
typedef struct {
    uart_port_t uart_num;
    int tx_gpio;
    int rx_gpio;

    uint8_t tx_buf[AXIOM_UART_TX_BUFFER_SIZE];
    uint8_t cobs_buf[AXIOM_UART_TX_COBS_BUFFER_SIZE];

    volatile bool tx_busy;
    volatile bool panic_mode;
} axiom_esp32_uart_ctx_t;

/* Default context - user can provide their own */
static axiom_esp32_uart_ctx_t g_uart_ctx;

/* UART event queue handle for non-blocking reads (if needed) */
static QueueHandle_t g_uart_event_queue = NULL;

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
 * UART Transmission (non-blocking via ESP-IDF driver)
 * --------------------------------------------------------------------------- */
static int uart_esp32_write(const uint8_t *data, uint16_t len) {
    if (g_uart_ctx.tx_busy) {
        return -1; /* Busy, should not happen with proper flow control */
    }

    g_uart_ctx.tx_busy = true;

    /* uart_write_bytes with UART_NOSYNC flushes immediately for small data */
    int written = uart_write_bytes(g_uart_ctx.uart_num, (const char *)data, len);

    if (written < 0) {
        g_uart_ctx.tx_busy = false;
        return -1;
    }

    /* For non-blocking, we rely on UART driver's internal buffer */
    /* tx_busy will be cleared when TX FIFO is empty (via event callback) */
    g_uart_ctx.tx_busy = false;

    return 0;
}

/* ---------------------------------------------------------------------------
 * UART Backend Implementation
 * --------------------------------------------------------------------------- */
static int uart_backend_write(const uint8_t *frame, uint16_t len, void *ctx) {
    axiom_esp32_uart_ctx_t *uart_ctx = (axiom_esp32_uart_ctx_t *)ctx;
    if (!uart_ctx || !frame || len == 0) return -1;

    /* Encode frame with COBS */
    uint16_t encoded_len = cobs_encode(frame, len,
                                       uart_ctx->cobs_buf,
                                       AXIOM_UART_TX_COBS_BUFFER_SIZE);
    if (encoded_len == 0) return -1;

    /* Add frame delimiter (0x00) */
    if (encoded_len < AXIOM_UART_TX_COBS_BUFFER_SIZE) {
        uart_ctx->cobs_buf[encoded_len++] = 0x00;
    }

    /* Start transmission */
    if (uart_esp32_write(uart_ctx->cobs_buf, encoded_len) < 0) {
        return -1;
    }

    return 0;
}

static int uart_backend_ready(void *ctx) {
    axiom_esp32_uart_ctx_t *uart_ctx = (axiom_esp32_uart_ctx_t *)ctx;
    if (!uart_ctx) return 0;

    /* Check if UART TX FIFO has space */
    return !uart_ctx->tx_busy;
}

static int uart_backend_flush(void *ctx) {
    axiom_esp32_uart_ctx_t *uart_ctx = (axiom_esp32_uart_ctx_t *)ctx;
    if (!uart_ctx) return -1;

    /* Wait for TX to complete */
    uart_wait_tx_done(uart_ctx->uart_num, portMAX_DELAY);

    return 0;
}

static int uart_backend_panic_write(const uint8_t *frame, uint16_t len, void *ctx) {
    axiom_esp32_uart_ctx_t *uart_ctx = (axiom_esp32_uart_ctx_t *)ctx;
    if (!uart_ctx || !frame || len == 0) return -1;

    uart_ctx->panic_mode = true;

    /* Encode frame with COBS */
    uint16_t encoded_len = cobs_encode(frame, len,
                                       uart_ctx->cobs_buf,
                                       AXIOM_UART_TX_COBS_BUFFER_SIZE);
    if (encoded_len == 0) {
        uart_ctx->panic_mode = false;
        return -1;
    }

    uart_ctx->cobs_buf[encoded_len++] = 0x00;

    /* Blocking transmission for panic - ensure data is sent */
    int written = uart_write_bytes(uart_ctx->uart_num,
                                   (const char *)uart_ctx->cobs_buf,
                                   encoded_len);

    /* Wait for TX to complete */
    uart_wait_tx_done(uart_ctx->uart_num, portMAX_DELAY);

    uart_ctx->panic_mode = false;

    return (written == encoded_len) ? 0 : -1;
}

static void uart_backend_on_drop(uint32_t lost, void *ctx) {
    (void)ctx;
    (void)lost;
    /* Could add LED blink or error counter here */
}

/* ---------------------------------------------------------------------------
 * UART Event Handler
 *
 * Optional: Handle UART events for advanced flow control
 * --------------------------------------------------------------------------- */
static void uart_event_handler(void *arg, uart_event_t *event) {
    (void)arg;

    switch (event->type) {
        case UART_TX_DONE:
            g_uart_ctx.tx_busy = false;
            break;
        case UART_FRAME_ERR:
        case UART_PARITY_ERR:
        case UART_BUFFER_FULL:
            /* Handle errors - could log or reset */
            break;
        default:
            break;
    }
}

/* ---------------------------------------------------------------------------
 * UART Backend Initialization
 *
 * Call once at startup after UART pins are configured.
 *
 * Parameters:
 *   uart_num - UART port number (UART_NUM_0, UART_NUM_1, UART_NUM_2)
 *   tx_gpio  - GPIO pin for TX (e.g., GPIO10 for UART1)
 *   rx_gpio  - GPIO pin for RX (e.g., GPIO9 for UART1)
 *   baud     - UART baud rate (e.g., 921600)
 *
 * Note: GPIO pins MUST be configured based on your board.
 *       Default pins may conflict with SPI Flash or other peripherals.
 * --------------------------------------------------------------------------- */
void axiom_backend_uart_init(uart_port_t uart_num,
                             int tx_gpio, int rx_gpio,
                             uint32_t baud) {
    axiom_esp32_uart_ctx_t *ctx = &g_uart_ctx;

    ctx->uart_num = uart_num;
    ctx->tx_gpio = tx_gpio;
    ctx->rx_gpio = rx_gpio;
    ctx->tx_busy = false;
    ctx->panic_mode = false;

    /* UART configuration */
    uart_config_t uart_config = {
        .baud_rate = (int)baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .use_ref_tick = false,
    };

    /* Configure UART parameters */
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

    /* Set UART pins - MUST be called after uart_param_config */
    ESP_ERROR_CHECK(uart_set_pin(uart_num, tx_gpio, rx_gpio,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    /* Install UART driver with internal TX buffer */
    /* Queue depth of 0 since we don't use RX events */
    ESP_ERROR_CHECK(uart_driver_install(uart_num,
                                        AXIOM_UART_TX_BUFFER_SIZE,  /* TX buffer */
                                        0,                           /* RX buffer */
                                        0,                           /* Queue depth */
                                        &g_uart_event_queue,
                                        0));                         /* Flags */
}

/* ---------------------------------------------------------------------------
 * UART Backend Deinitialization
 *
 * Call on shutdown to release UART driver.
 * --------------------------------------------------------------------------- */
void axiom_backend_uart_deinit(void) {
    if (g_uart_event_queue) {
        vQueueDelete(g_uart_event_queue);
        g_uart_event_queue = NULL;
    }

    ESP_ERROR_CHECK(uart_driver_delete(g_uart_ctx.uart_num));
}

/* ---------------------------------------------------------------------------
 * Get UART Backend Descriptor
 *
 * Returns backend descriptor for axiom_backend_register().
 * --------------------------------------------------------------------------- */
axiom_backend_t axiom_backend_uart(const char *name,
                                   axiom_esp32_uart_ctx_t *ctx) {
    if (!ctx) {
        ctx = &g_uart_ctx;
    }

    return (axiom_backend_t){
        .name = name ? name : "esp32-uart",
        .caps = AXIOM_BACKEND_CAP_UART,
        .write = uart_backend_write,
        .ready = uart_backend_ready,
        .flush = uart_backend_flush,
        .panic_write = uart_backend_panic_write,
        .on_drop = uart_backend_on_drop,
        .ctx = ctx,
    };
}