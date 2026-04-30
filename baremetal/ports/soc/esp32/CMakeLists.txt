# ============================================================================
# ESP32 SoC Port
# ============================================================================
#
# Espressif ESP32 芯片特定实现
# 提供:
#   - esp_timer 高精度定时
#   - UART backend 集成
#   - WiFi/BT 不在热路径使用
# ============================================================================

cmake_minimum_required(VERSION 3.16)

set(AXIOM_PORT_SOC_NAME "esp32")

# ESP32 特定配置
set(ESP32_UART_NUM 0)
set(ESP32_UART_BAUD 115200)

# ESP32 port 源文件 (可选)
# set(AXIOM_PORT_ESP32_SOURCES
#     ${CMAKE_CURRENT_SOURCE_DIR}/axiom_port_esp32.c
#     PARENT_SCOPE
# )
