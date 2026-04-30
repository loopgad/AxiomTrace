# ============================================================================
# RISC-V Architecture Port
# ============================================================================
#
# RISC-V 架构特定实现
# 提供:
#   - 机器模式定时器 (mtime)
#   - 中断使能/禁用 (mie/mstatus)
#   - 默认平台配置
# ============================================================================

cmake_minimum_required(VERSION 3.16)

set(AXIOM_PORT_ARCH_NAME "riscv")

# RISC-V port 源文件
set(AXIOM_PORT_RISCV_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/axiom_port_riscv.c
    PARENT_SCOPE
)
