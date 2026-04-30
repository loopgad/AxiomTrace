# ============================================================================
# AxiomTrace Port Layer - CMake Unified Management
# ============================================================================
#
# 三层架构:
#   arch     - 架构层: cortex-m, riscv
#   soc      - 芯片层: stm32f4, nrf52, esp32
#   board    - 板级层: 特定开发板配置
#
# CMake 选项:
#   AXIOM_PLATFORM  - 目标平台 (cortex-m, riscv, host)
#   AXIOM_SOC       - 芯片型号 (stm32f4, nrf52, esp32)
#   AXIOM_BOARD     - 开发板型号 (可选, 覆盖 soc 默认配置)
#
# 使用方式:
#   cmake -B build -DAXIOM_PLATFORM=cortex-m -DAXIOM_SOC=stm32f4
# ============================================================================

# ---------------------------------------------------------------------------
# Port Source Selection
# ---------------------------------------------------------------------------
# Note: When using include(), CMAKE_CURRENT_LIST_DIR gives the directory
#       of the included file, while CMAKE_CURRENT_SOURCE_DIR gives the
#       directory of the main CMakeLists.txt
# ---------------------------------------------------------------------------
set(AXIOM_PORTS_DIR "${CMAKE_CURRENT_LIST_DIR}")

# 1. Generic port (默认实现, 弱符号可被覆盖)
set(AXIOM_PORT_GENERIC "${AXIOM_PORTS_DIR}/generic/axiom_port_generic.c")

# 2. Architecture-specific port
if(AXIOM_PLATFORM STREQUAL "cortex-m")
    set(AXIOM_PORT_ARCH_SOURCES "${AXIOM_PORTS_DIR}/arch/cortex-m/axiom_port_cortex_m.c")
elseif(AXIOM_PLATFORM STREQUAL "riscv")
    set(AXIOM_PORT_ARCH_SOURCES "${AXIOM_PORTS_DIR}/arch/riscv/axiom_port_riscv.c")
endif()

# 3. SoC-specific port (optional, only if defined)
if(AXIOM_SOC STREQUAL "stm32f4")
    set(AXIOM_PORT_SOC_SOURCES "${AXIOM_PORTS_DIR}/soc/stm32f4/axiom_port_stm32f4.c")
elseif(AXIOM_SOC STREQUAL "nrf52")
    set(AXIOM_PORT_SOC_SOURCES "${AXIOM_PORTS_DIR}/soc/nrf52/axiom_port_nrf52.c")
elseif(AXIOM_SOC STREQUAL "esp32")
    set(AXIOM_PORT_SOC_SOURCES "${AXIOM_PORTS_DIR}/soc/esp32/axiom_port_esp32.c")
endif()

# 4. Board-specific port (optional, only if defined)
if(NOT AXIOM_BOARD STREQUAL "generic")
    set(AXIOM_PORT_BOARD_SOURCES "${AXIOM_PORTS_DIR}/board/${AXIOM_BOARD}/axiom_port_${AXIOM_BOARD}.c")
endif()

# ---------------------------------------------------------------------------
# Build Port Source List
# ---------------------------------------------------------------------------
# 收集所有可用的 port 源文件
# 优先级: board > soc > arch > generic (后面的覆盖前面的弱符号)

set(AXIOM_ALL_PORT_SOURCES ${AXIOM_PORT_GENERIC})

if(AXIOM_PORT_ARCH_SOURCES AND EXISTS "${AXIOM_PORT_ARCH_SOURCES}")
    list(APPEND AXIOM_ALL_PORT_SOURCES ${AXIOM_PORT_ARCH_SOURCES})
endif()

if(AXIOM_PORT_SOC_SOURCES AND EXISTS "${AXIOM_PORT_SOC_SOURCES}")
    list(APPEND AXIOM_ALL_PORT_SOURCES ${AXIOM_PORT_SOC_SOURCES})
endif()

if(AXIOM_PORT_BOARD_SOURCES AND EXISTS "${AXIOM_PORT_BOARD_SOURCES}")
    list(APPEND AXIOM_ALL_PORT_SOURCES ${AXIOM_PORT_BOARD_SOURCES})
endif()

# ---------------------------------------------------------------------------
# Export Variables for Parent CMake
# Note: When using include(), variables are set in current scope
# ---------------------------------------------------------------------------
set(AXIOM_PORT_SOURCES ${AXIOM_ALL_PORT_SOURCES} CACHE INTERNAL "")
set(AXIOM_PORT_INCLUDE_DIRS "${AXIOM_PORTS_DIR}" CACHE INTERNAL "")

# ---------------------------------------------------------------------------
# Configuration Summary
# ---------------------------------------------------------------------------
message(STATUS "=== AxiomTrace Port Configuration ===")
message(STATUS "  AXIOM_PLATFORM: ${AXIOM_PLATFORM}")
message(STATUS "  AXIOM_SOC:      ${AXIOM_SOC}")
message(STATUS "  AXIOM_BOARD:    ${AXIOM_BOARD}")
message(STATUS "  Port sources:")
foreach(src ${AXIOM_ALL_PORT_SOURCES})
    message(STATUS "    - ${src}")
endforeach()
message(STATUS "===================================")
