# ============================================================================
# STM32F4 SoC Port
# ============================================================================
#
# STM32F4 系列芯片特定实现
# 提供:
#   - DWT_CYCCNT 精确定时 (已由 arch 层提供)
#   - Flash 操作 (HAL 风格但无依赖)
#   - UART backend 集成
# ============================================================================

cmake_minimum_required(VERSION 3.16)

set(AXIOM_PORT_SOC_NAME "stm32f4")

# STM32F4 特定配置
set(STM32F4_FLASH_BASE 0x08000000)
set(STM32F4_FLASH_PAGE_SIZE 0x4000)  # 16KB sectors for F4

# STM32F4 port 源文件 (可选, 由 arch 层提供基础实现)
# set(AXIOM_PORT_STM32F4_SOURCES
#     ${CMAKE_CURRENT_SOURCE_DIR}/axiom_port_stm32f4.c
#     PARENT_SCOPE
# )
