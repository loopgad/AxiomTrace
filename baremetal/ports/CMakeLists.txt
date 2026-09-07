# ============================================================================
# AxiomTrace architecture-level port selection
# ============================================================================
#
# This file is included by the top-level CMake project. SDK-dependent vendor
# packages stay under baremetal/ports/{esp32,nrf52,stm32} and are consumed by
# firmware projects separately.
# ============================================================================

set(AXIOM_PORTS_DIR "${CMAKE_CURRENT_LIST_DIR}")

# DWT/CYCCNT is not guaranteed by the Cortex-M architecture macro alone.
# Keep the default safe (timestamp returns zero) and require the board build
# to opt in after confirming the concrete core/SoC implements the counter.
if(AXIOM_PLATFORM STREQUAL "cortex-m")
    set(AXIOM_CORTEX_M_HAS_DWT "" CACHE STRING
        "Set to 1 only when the selected Cortex-M core exposes DWT_CYCCNT")
    if(AXIOM_CORTEX_M_HAS_DWT AND
       NOT AXIOM_CORTEX_M_HAS_DWT MATCHES "^[01]$")
        message(FATAL_ERROR
            "AXIOM_CORTEX_M_HAS_DWT must be 0 or 1 when provided")
    endif()
endif()

# Backend recovery compares a monotonic microsecond timestamp.  Track whether
# it is enabled now; the selected provider is validated below once CMake knows
# whether it is a built-in architecture port or a custom/external provider.
set(_AXIOM_BACKEND_DEGRADATION_ENABLED FALSE)
if(DEFINED AXIOM_BACKEND_DEGRADATION AND AXIOM_BACKEND_DEGRADATION)
    set(_AXIOM_BACKEND_DEGRADATION_ENABLED TRUE)
endif()
string(TOLOWER "${AXIOM_PRESET}" _AXIOM_PORT_PRESET)
if(DEFINED AXIOM_PRESET_${_AXIOM_PORT_PRESET}_BACKEND_DEGRADATION AND
   AXIOM_PRESET_${_AXIOM_PORT_PRESET}_BACKEND_DEGRADATION)
    set(_AXIOM_BACKEND_DEGRADATION_ENABLED TRUE)
endif()
# The core library deliberately has no port object.  Keep the selected port
# as an independent provider so consumers must choose the hardware contract
# explicitly at their final link step.
unset(AXIOM_PORT_SOURCES)
unset(AXIOM_PORT_SOURCES CACHE)
unset(AXIOM_PORT_PROVIDER_SOURCES)
unset(AXIOM_PORT_PROVIDER_TARGET)
set(_AXIOM_USING_DEFAULT_ARCH_PORT FALSE)

set(AXIOM_PORT_GENERIC_SOURCE
    "${AXIOM_PORTS_DIR}/generic/axiom_port_generic.c")
if(NOT EXISTS "${AXIOM_PORT_GENERIC_SOURCE}")
    message(FATAL_ERROR
        "Generic AxiomTrace port source is missing: ${AXIOM_PORT_GENERIC_SOURCE}")
endif()

# The generic provider is always available for host/default integrations and
# is exported separately from the port selected below.  It is not linked to
# AxiomTrace itself.
add_library(axiomtrace_port_generic STATIC "${AXIOM_PORT_GENERIC_SOURCE}")
set_target_properties(axiomtrace_port_generic PROPERTIES EXPORT_NAME port_generic)
add_library(AxiomTrace::port_generic ALIAS axiomtrace_port_generic)
target_compile_features(axiomtrace_port_generic PUBLIC c_std_11)
target_include_directories(axiomtrace_port_generic
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/baremetal>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/baremetal/include>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/baremetal/port>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/axiomtrace>
)
if(AXIOM_BUILD_TESTS AND AXIOM_PLATFORM STREQUAL "host")
    target_compile_definitions(axiomtrace_port_generic PUBLIC
        $<BUILD_INTERFACE:AXIOM_HOST_TESTING=1>)
endif()

string(TOUPPER "${AXIOM_PORT_SOURCE}" _AXIOM_PORT_SOURCE_NORMALIZED)
if(AXIOM_EXTERNAL_PORT)
    message(WARNING
        "AXIOM_EXTERNAL_PORT is deprecated; use AXIOM_PORT_SOURCE=NONE instead")
endif()

if(AXIOM_EXTERNAL_PORT OR _AXIOM_PORT_SOURCE_NORMALIZED STREQUAL "NONE")
    if(AXIOM_PORT_SOURCE AND
       NOT _AXIOM_PORT_SOURCE_NORMALIZED STREQUAL "NONE")
        message(FATAL_ERROR
            "AXIOM_EXTERNAL_PORT=ON cannot be combined with AXIOM_PORT_SOURCE; "
            "choose an external provider or custom port sources")
    endif()
    set(AXIOM_PORT_PROVIDER_SOURCES)
elseif(NOT AXIOM_PORT_SOURCE)
    if(AXIOM_PLATFORM STREQUAL "cortex-m")
        set(_AXIOM_DEFAULT_PORT_SOURCE
            "${AXIOM_PORTS_DIR}/arch/cortex-m/axiom_port_cortex_m.c")
        set(_AXIOM_USING_DEFAULT_ARCH_PORT TRUE)
    elseif(AXIOM_PLATFORM STREQUAL "riscv")
        set(_AXIOM_DEFAULT_PORT_SOURCE
            "${AXIOM_PORTS_DIR}/arch/riscv/axiom_port_riscv.c")
        set(_AXIOM_USING_DEFAULT_ARCH_PORT TRUE)
    else()
        set(_AXIOM_DEFAULT_PORT_SOURCE
            "${AXIOM_PORTS_DIR}/generic/axiom_port_generic.c")
    endif()
    set(AXIOM_PORT_PROVIDER_SOURCES "${_AXIOM_DEFAULT_PORT_SOURCE}")
else()
    set(AXIOM_PORT_PROVIDER_SOURCES ${AXIOM_PORT_SOURCE})
endif()

# The built-in architecture providers need their hardware counter configured
# for degradation recovery.  Custom and external providers are exempt because
# they may supply an independent monotonic timestamp implementation.
if(_AXIOM_BACKEND_DEGRADATION_ENABLED AND _AXIOM_USING_DEFAULT_ARCH_PORT)
    if(AXIOM_CPU_HZ STREQUAL "0")
        message(FATAL_ERROR
            "AXIOM_BACKEND_DEGRADATION with the built-in ${AXIOM_PLATFORM} "
            "port requires a non-zero AXIOM_CPU_HZ")
    endif()
    if(AXIOM_PLATFORM STREQUAL "cortex-m" AND
       NOT AXIOM_CORTEX_M_HAS_DWT STREQUAL "1")
        message(FATAL_ERROR
            "AXIOM_BACKEND_DEGRADATION with the built-in Cortex-M port "
            "requires AXIOM_CORTEX_M_HAS_DWT=1, or a custom/external port "
            "with a monotonic timestamp")
    endif()
endif()

# Validate and materialize the optional provider target.  A single generic
# source reuses the installed generic target; custom and architecture source
# lists get their own provider target.
foreach(AXIOM_PORT_SOURCE_ENTRY IN LISTS AXIOM_PORT_PROVIDER_SOURCES)
    if(NOT EXISTS "${AXIOM_PORT_SOURCE_ENTRY}")
        message(FATAL_ERROR
            "Selected port source is missing: ${AXIOM_PORT_SOURCE_ENTRY}")
    endif()
endforeach()

if(AXIOM_PORT_PROVIDER_SOURCES)
    list(LENGTH AXIOM_PORT_PROVIDER_SOURCES AXIOM_PORT_PROVIDER_SOURCE_COUNT)
    if(AXIOM_PORT_PROVIDER_SOURCE_COUNT EQUAL 1 AND
       "${AXIOM_PORT_PROVIDER_SOURCES}" STREQUAL "${AXIOM_PORT_GENERIC_SOURCE}")
        add_library(axiomtrace_port INTERFACE)
        target_link_libraries(axiomtrace_port INTERFACE AxiomTrace::port_generic)
    else()
        add_library(axiomtrace_port STATIC ${AXIOM_PORT_PROVIDER_SOURCES})
        target_compile_features(axiomtrace_port PUBLIC c_std_11)
        target_include_directories(axiomtrace_port
            PUBLIC
                $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/baremetal>
                $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/baremetal/include>
                $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/baremetal/port>
                $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/axiomtrace>
        )
        if(AXIOM_CPU_HZ MATCHES "^[0-9]+$" AND NOT AXIOM_CPU_HZ STREQUAL "0")
            target_compile_definitions(axiomtrace_port PRIVATE
                AXIOM_CPU_HZ=${AXIOM_CPU_HZ})
        endif()
        if(AXIOM_PLATFORM STREQUAL "cortex-m" AND
           AXIOM_CORTEX_M_HAS_DWT MATCHES "^[01]$")
            target_compile_definitions(axiomtrace_port PRIVATE
                AXIOM_CORTEX_M_HAS_DWT=${AXIOM_CORTEX_M_HAS_DWT})
        endif()
    endif()
    set_target_properties(axiomtrace_port PROPERTIES EXPORT_NAME port)
    add_library(AxiomTrace::port ALIAS axiomtrace_port)
    set(AXIOM_PORT_PROVIDER_TARGET axiomtrace_port)
endif()

message(STATUS "=== AxiomTrace Port Configuration ===")
message(STATUS "  AXIOM_PLATFORM: ${AXIOM_PLATFORM}")
if(AXIOM_PORT_PROVIDER_SOURCES)
    message(STATUS "  Port provider:  ${AXIOM_PORT_PROVIDER_SOURCES}")
else()
    message(STATUS "  Port provider:  external (application-provided)")
endif()
message(STATUS "===================================")

unset(AXIOM_PORTS_DIR)
unset(AXIOM_PORT_GENERIC_SOURCE)
unset(AXIOM_PORT_PROVIDER_SOURCES)
unset(AXIOM_PORT_PROVIDER_TARGET)
unset(_AXIOM_BACKEND_DEGRADATION_ENABLED)
unset(_AXIOM_PORT_PRESET)
unset(_AXIOM_USING_DEFAULT_ARCH_PORT)
