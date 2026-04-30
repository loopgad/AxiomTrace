# ============================================================================
# Cortex-M Architecture Port
# ============================================================================
#
# ARM Cortex-M 架构特定实现
# 提供:
#   - DWT_CYCCNT 精确定时
#   - PRIMASK/BASEPRI 临界区
#   - 默认 NVIC 配置
# ============================================================================

cmake_minimum_required(VERSION 3.16)

set(AXIOM_PORT_ARCH_NAME "cortex-m")

# Cortex-M port 源文件
set(AXIOM_PORT_CORTEX_M_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/axiom_port_cortex_m.c
    PARENT_SCOPE
)

# 可选: CMSE (TrustZone) 支持
option(AXIOM_CORTEX_M_CMSE "Enable ARM TrustZone/CMSE support" OFF)
