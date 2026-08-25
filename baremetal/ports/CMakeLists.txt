# ============================================================================
# AxiomTrace architecture-level port selection
# ============================================================================
#
# This file is included by the top-level CMake project. SDK-dependent vendor
# packages stay under baremetal/ports/{esp32,nrf52,stm32} and are consumed by
# firmware projects separately.
# ============================================================================

set(AXIOM_PORTS_DIR "${CMAKE_CURRENT_LIST_DIR}")

# The core library deliberately has no port object.  Keep the selected port
# as an independent provider so consumers must choose the hardware contract
# explicitly at their final link step.
unset(AXIOM_PORT_SOURCES)
unset(AXIOM_PORT_SOURCES CACHE)
unset(AXIOM_PORT_PROVIDER_SOURCES)
unset(AXIOM_PORT_PROVIDER_TARGET)

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
    elseif(AXIOM_PLATFORM STREQUAL "riscv")
        set(_AXIOM_DEFAULT_PORT_SOURCE
            "${AXIOM_PORTS_DIR}/arch/riscv/axiom_port_riscv.c")
    else()
        set(_AXIOM_DEFAULT_PORT_SOURCE
            "${AXIOM_PORTS_DIR}/generic/axiom_port_generic.c")
    endif()
    set(AXIOM_PORT_PROVIDER_SOURCES "${_AXIOM_DEFAULT_PORT_SOURCE}")
else()
    set(AXIOM_PORT_PROVIDER_SOURCES ${AXIOM_PORT_SOURCE})
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
