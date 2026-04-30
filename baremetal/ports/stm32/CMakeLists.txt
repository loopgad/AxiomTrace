# ---------------------------------------------------------------------------
# STM32F4 Port - CMake Configuration
# ---------------------------------------------------------------------------
#
# This file provides CMake configuration for AxiomTrace STM32F4 port.
# Include from your project's CMakeLists.txt after axiomtrace is built.
#
# Usage:
#   add_subdirectory(path/to/axiomtrace/baremetal/ports/stm32 axiom_trace_stm32)
#
# Options:
#   AXIOM_UART_TX_BUFFER_SIZE - UART TX buffer size (default: 256)
#   AXIOM_UART_TX_COBS_BUFFER_SIZE - COBS encoding buffer size (default: auto)
#   AXIOM_STM32_UART_INSTANCE - UART peripheral base address (default: UART1)
#   AXIOM_STM32_DMA_STREAM - DMA stream base address (default: DMA2_STREAM7)
#   AXIOM_STM32_CLOCK_FREQ - UART clock frequency in Hz (default: 84000000)
#   AXIOM_STM32_BAUD_RATE - UART baud rate (default: 115200)
#
# Example:
#   set(AXIOM_STM32_CLOCK_FREQ 84000000)
#   set(AXIOM_STM32_BAUD_RATE 921600)
#   set(AXIOM_UART_TX_BUFFER_SIZE 512)
#   add_subdirectory(vendor/axiomtrace/baremetal/ports/stm32 axiom_trace_stm32)
#
# ---------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.16)

project(AxiomTrace_STM32 C)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
if(NOT DEFINED AXIOM_UART_TX_BUFFER_SIZE)
    set(AXIOM_UART_TX_BUFFER_SIZE 256)
endif()

if(NOT DEFINED AXIOM_STM32_UART_INSTANCE)
    set(AXIOM_STM32_UART_INSTANCE "UART1")
endif()

if(NOT DEFINED AXIOM_STM32_DMA_STREAM)
    set(AXIOM_STM32_DMA_STREAM "DMA2_STREAM7")
endif()

if(NOT DEFINED AXIOM_STM32_CLOCK_FREQ)
    set(AXIOM_STM32_CLOCK_FREQ 84000000)
endif()

if(NOT DEFINED AXIOM_STM32_BAUD_RATE)
    set(AXIOM_STM32_BAUD_RATE 115200)
endif()

# ---------------------------------------------------------------------------
# Compiler flags for ARM Cortex-M4
# ---------------------------------------------------------------------------
if(CMAKE_C_COMPILER_ID MATCHES "GNU")
    target_compile_options(AxiomTrace_STM32 PRIVATE
        -mcpu=cortex-m4
        -mthumb
        -mfloat-abi=hard
        -mfpu=fpv4-sp-d16
        -Wall
        -Wextra
    )
endif()

# ---------------------------------------------------------------------------
# Source files for STM32 port
# ---------------------------------------------------------------------------
set(AXIOM_STM32_PORT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/axiom_port_stm32.c
    ${CMAKE_CURRENT_SOURCE_DIR}/axiom_backend_uart.c
)

# ---------------------------------------------------------------------------
# Create library
# ---------------------------------------------------------------------------
add_library(axiom_trace_stm32 STATIC ${AXIOM_STM32_PORT_SOURCES})

target_include_directories(axiom_trace_stm32 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/baremetal/include
    ${CMAKE_SOURCE_DIR}/baremetal/port
    ${CMAKE_SOURCE_DIR}/baremetal/core
    ${CMAKE_SOURCE_DIR}/baremetal/frontend
    ${CMAKE_SOURCE_DIR}/baremetal/backend
)

target_compile_definitions(axiom_trace_stm32 PUBLIC
    AXIOM_UART_TX_BUFFER_SIZE=${AXIOM_UART_TX_BUFFER_SIZE}
    AXIOM_STM32_CLOCK_FREQ=${AXIOM_STM32_CLOCK_FREQ}
    AXIOM_STM32_BAUD_RATE=${AXIOM_STM32_BAUD_RATE}
)

# ---------------------------------------------------------------------------
# Linker script (optional - provide your own for final linking)
# ---------------------------------------------------------------------------
# target_link_libraries(axiom_trace_stm32
#     your_chip_startup
#     your_linker_script
# )

# ---------------------------------------------------------------------------
# Integration Notes
# ---------------------------------------------------------------------------
# 1. Add this directory to your CMake project BEFORE axiomtrace is linked
# 2. Link axiom_trace_stm32 library to your application
# 3. Call axiom_port_init() in your main() before axiom_init()
# 4. Call axiom_backend_uart_init() with your UART/DMA peripheral addresses
# 5. Add DMA IRQ handler for your DMA stream
# 6. Connect UART TX to your host (e.g., via ST-Link virtual COM port)