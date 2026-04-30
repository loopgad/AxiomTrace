# ============================================================================
# nRF52 SoC Port
# ============================================================================
#
# Nordic nRF52 系列芯片特定实现
# 提供:
#   - RTC 定时器 (低功耗)
#   - RADIO/ECB 加密 (可选)
#   - RTT backend 集成 (SEGGER)
# ============================================================================

cmake_minimum_required(VERSION 3.16)

set(AXIOM_PORT_SOC_NAME "nrf52")

# nRF52 特定配置
set(NRF52_RTC_FREQ 32768)  # 32.768 kHz crystal

# nRF52 port 源文件 (可选)
# set(AXIOM_PORT_NRF52_SOURCES
#     ${CMAKE_CURRENT_SOURCE_DIR}/axiom_port_nrf52.c
#     PARENT_SCOPE
# )
