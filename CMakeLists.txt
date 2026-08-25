cmake_minimum_required(VERSION 3.16)
project(AxiomTrace VERSION 1.0.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(AXIOM_BUILD_TESTS "Build host unit tests" OFF)
option(AXIOM_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(AXIOM_BUILD_EXAMPLES "Build examples" OFF)
option(AXIOM_ENABLE_CLANG_TIDY
    "Run low-noise clang-tidy checks while compiling AxiomTrace" OFF)
set(AXIOM_CLANG_TIDY_CHECKS
    "clang-analyzer-core.*"
    CACHE STRING "Comma-separated clang-tidy checks for the optional gate")
option(AXIOM_EXTERNAL_PORT
    "Do not compile an architecture port; the consuming application supplies it" OFF)
set(AXIOM_CPU_HZ "0" CACHE STRING
    "Target CPU clock used by architecture ports to report microsecond timestamps")
set(AXIOM_PRESET "dev" CACHE STRING "Resource preset: custom, tiny, prod, field, dev")
set_property(CACHE AXIOM_PRESET PROPERTY STRINGS custom tiny prod field dev)
set(AXIOM_PORT_SOURCE "" CACHE STRING
    "Override port source(s); use NONE with an external port; semicolon-separated paths are accepted")

# Windows Clang links ASan through a runtime DLL that is installed beside the
# compiler resources rather than on PATH. Let CTest provide that directory to
# test processes so sanitizer builds run without manual environment setup.
set(AXIOM_TEST_RUNTIME_DIR "")
if(WIN32 AND CMAKE_C_COMPILER_ID MATCHES "Clang" AND
   CMAKE_C_FLAGS MATCHES "sanitize=address")
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} --print-runtime-dir
        OUTPUT_VARIABLE AXIOM_TEST_RUNTIME_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()

function(axiom_configure_test_runtime test_name)
    if(AXIOM_TEST_RUNTIME_DIR AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.22)
        set_tests_properties(${test_name} PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${AXIOM_TEST_RUNTIME_DIR}")
    endif()
endfunction()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(AxiomTraceTools OPTIONAL)
include(AxiomTracePresets)

# ---------------------------------------------------------------------------
# Platform options
# ---------------------------------------------------------------------------
# The top-level project selects only the architecture-level port. Vendor
# integrations under baremetal/ports/<vendor>/ are separate packages because
# they require an external SDK and board-specific link setup.
unset(AXIOM_PLATFORM_OPTIONS CACHE)
set(AXIOM_PLATFORM_OPTIONS
    cortex-m
    riscv
    host
)

# Platform auto-detection (if not explicitly set)
if(NOT DEFINED AXIOM_PLATFORM)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|cortex")
        set(AXIOM_PLATFORM "cortex-m" CACHE STRING "Target architecture platform" FORCE)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv|rv")
        set(AXIOM_PLATFORM "riscv" CACHE STRING "Target architecture platform" FORCE)
    else()
        set(AXIOM_PLATFORM "host" CACHE STRING "Target architecture platform" FORCE)
    endif()
    message(STATUS "AXIOM_PLATFORM auto-detected: ${AXIOM_PLATFORM}")
endif()

# Validate platform selection
if(NOT AXIOM_PLATFORM IN_LIST AXIOM_PLATFORM_OPTIONS)
    message(FATAL_ERROR "Invalid AXIOM_PLATFORM: '${AXIOM_PLATFORM}'. Valid: ${AXIOM_PLATFORM_OPTIONS}")
endif()

# Include unified ports management
include(baremetal/ports/CMakeLists.txt)

# ---------------------------------------------------------------------------
# Optional low-noise static analysis
# ---------------------------------------------------------------------------
if(AXIOM_ENABLE_CLANG_TIDY)
    find_program(AXIOM_CLANG_TIDY_EXE NAMES clang-tidy)
    if(NOT AXIOM_CLANG_TIDY_EXE)
        message(FATAL_ERROR "AXIOM_ENABLE_CLANG_TIDY=ON but clang-tidy was not found")
    endif()
    set(AXIOM_C_CLANG_TIDY
        "${AXIOM_CLANG_TIDY_EXE};--checks=-*,${AXIOM_CLANG_TIDY_CHECKS};--warnings-as-errors=${AXIOM_CLANG_TIDY_CHECKS}")
    message(STATUS "clang-tidy enabled: ${AXIOM_CLANG_TIDY_CHECKS}")
endif()

# ---------------------------------------------------------------------------
# AxiomTrace core library
# ---------------------------------------------------------------------------
set(AXIOM_CORE_HEADERS
    ${PROJECT_SOURCE_DIR}/baremetal/include/axiom_static_assert.h
    ${PROJECT_SOURCE_DIR}/baremetal/port/axiom_port.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_event.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_diagnostics.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_frame.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_encode.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_crc.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_ring.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_timestamp.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_filter.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_capsule.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_selftest.h
    ${PROJECT_SOURCE_DIR}/baremetal/frontend/axiom_frontend.h
    ${PROJECT_SOURCE_DIR}/baremetal/backend/axiom_backend.h
)

set(AXIOM_CORE_SOURCES
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_event.c
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_diagnostics.c
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_crc.c
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_ring.c
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_timestamp.c
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_filter.c
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_capsule.c
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_selftest.c
    ${PROJECT_SOURCE_DIR}/baremetal/backend/axiom_backend.c
    ${PROJECT_SOURCE_DIR}/baremetal/backend/axiom_backend_memory.c
    ${PROJECT_SOURCE_DIR}/baremetal/backend/axiom_backend_deferred.c
)

# Keep the current install set for compatibility. These headers are included
# in the modular public tree today, but are maintained as INTERNAL headers so
# a future major release can narrow the boundary without silently removing
# files that downstream projects may include directly.
set(AXIOM_PUBLIC_HEADERS
    ${PROJECT_SOURCE_DIR}/baremetal/axiomtrace.h
    ${PROJECT_SOURCE_DIR}/baremetal/axiom_config.h
    ${PROJECT_SOURCE_DIR}/baremetal/port/axiom_port.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_event.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_diagnostics.h
    ${PROJECT_SOURCE_DIR}/baremetal/frontend/axiom_frontend.h
    ${PROJECT_SOURCE_DIR}/baremetal/backend/axiom_backend.h
)
set(AXIOM_INTERNAL_HEADERS
    ${PROJECT_SOURCE_DIR}/baremetal/include/axiom_static_assert.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_frame.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_encode.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_crc.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_ring.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_timestamp.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_filter.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_capsule.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_selftest.h
    ${PROJECT_SOURCE_DIR}/baremetal/backend/axiom_backend_deferred.h
    ${PROJECT_SOURCE_DIR}/baremetal/backend/axiom_cobs.h
)
set(AXIOM_INSTALL_HEADERS ${AXIOM_PUBLIC_HEADERS} ${AXIOM_INTERNAL_HEADERS})
source_group("Public Headers" FILES ${AXIOM_PUBLIC_HEADERS})
source_group("INTERNAL Headers" FILES ${AXIOM_INTERNAL_HEADERS})

add_library(axiomtrace STATIC ${AXIOM_CORE_SOURCES} ${AXIOM_CORE_HEADERS})
add_library(AxiomTrace::axiomtrace ALIAS axiomtrace)
target_compile_features(axiomtrace PUBLIC c_std_11)
set_target_properties(axiomtrace PROPERTIES C_EXTENSIONS ON)
if(NOT AXIOM_CPU_HZ MATCHES "^[0-9]+$")
    message(FATAL_ERROR "AXIOM_CPU_HZ must be a non-negative integer")
endif()
if(AXIOM_ENABLE_CLANG_TIDY)
    set_property(TARGET axiomtrace PROPERTY C_CLANG_TIDY "${AXIOM_C_CLANG_TIDY}")
endif()
target_include_directories(axiomtrace
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/baremetal>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/baremetal/include>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/baremetal/port>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/baremetal/core>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/baremetal/frontend>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/baremetal/backend>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/axiomtrace>
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(axiomtrace PRIVATE
        -Wall -Wextra -Wshadow -Wcast-align -Wstrict-prototypes
        -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls
        -Wbad-function-cast -Wconversion -Wsign-conversion -Wdouble-promotion
        -Wfloat-equal -Wundef -Wunused -Werror=implicit-function-declaration
        -Werror=return-type -Werror=pointer-arith -pedantic
    )
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(axiomtrace PRIVATE
        -Wno-gnu-zero-variadic-macro-arguments
        -Wno-variadic-macro-arguments-omitted
    )
endif()

# Port providers are deliberately not mixed into this target.  The selected
# provider target from baremetal/ports/CMakeLists.txt is consumed explicitly
# by the application at the final link step.

# ---------------------------------------------------------------------------
# Optional feature toggles (forwarded as -D preprocessor macros)
# Must come after add_library(axiomtrace ...)
# ---------------------------------------------------------------------------
option(AXIOM_SELFTEST "Enable runtime self-test at boot" OFF)
option(AXIOM_DEBUG "Enable debug assertions and diagnostics" OFF)

string(TOLOWER "${AXIOM_PRESET}" AXIOM_PRESET_NORMALIZED)

# Validate preset
if(NOT AXIOM_PRESET_NORMALIZED MATCHES "^(custom|tiny|prod|field|dev)$")
    message(FATAL_ERROR "Invalid AXIOM_PRESET='${AXIOM_PRESET}'. Valid values: custom, tiny, prod, field, dev")
endif()

# Map preset name to numeric value (for backward compatibility with axiom_config.h)
if(AXIOM_PRESET_NORMALIZED STREQUAL "custom")
    set(AXIOM_PRESET_VALUE 0)
elseif(AXIOM_PRESET_NORMALIZED STREQUAL "tiny")
    set(AXIOM_PRESET_VALUE 1)
elseif(AXIOM_PRESET_NORMALIZED STREQUAL "prod")
    set(AXIOM_PRESET_VALUE 2)
elseif(AXIOM_PRESET_NORMALIZED STREQUAL "field")
    set(AXIOM_PRESET_VALUE 3)
elseif(AXIOM_PRESET_NORMALIZED STREQUAL "dev")
    set(AXIOM_PRESET_VALUE 4)
endif()

message(STATUS "AXIOM_PRESET: ${AXIOM_PRESET_NORMALIZED}")

# Apply preset values via centralized preset system
axiom_apply_preset(axiomtrace ${AXIOM_PRESET_NORMALIZED})

# Always set the preset numeric value (for axiom_config.h validation)
target_compile_definitions(axiomtrace PUBLIC AXIOM_PRESET=${AXIOM_PRESET_VALUE})

if(AXIOM_SELFTEST)
    target_compile_definitions(axiomtrace PUBLIC AXIOM_SELFTEST=1)
endif()
if(AXIOM_DEBUG)
    target_compile_definitions(axiomtrace PUBLIC AXIOM_DEBUG=1)
endif()
if(AXIOM_BUILD_TESTS AND AXIOM_PLATFORM STREQUAL "host")
    # Expose a controllable generic-port timestamp only to host test builds.
    target_compile_definitions(axiomtrace PUBLIC $<BUILD_INTERFACE:AXIOM_HOST_TESTING=1>)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(AXIOM_BUILD_TESTS)
    if(NOT AXIOM_PLATFORM STREQUAL "host")
        message(FATAL_ERROR
            "AXIOM_BUILD_TESTS requires AXIOM_PLATFORM=host; "
            "cross targets are compile-only")
    endif()
    enable_testing()
    add_subdirectory(tests/host)

    # Host tests exercise the modular core and therefore need an explicit
    # provider.  Single-header tests embed their own default/custom provider
    # and must remain independent of the modular generic target.
    if(AXIOM_PLATFORM STREQUAL "host" AND TARGET AxiomTrace::port_generic)
        get_property(AXIOM_HOST_TARGETS
            DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests/host"
            PROPERTY BUILDSYSTEM_TARGETS)
        foreach(AXIOM_HOST_TARGET IN LISTS AXIOM_HOST_TARGETS)
            set(AXIOM_HOST_NEEDS_GENERIC_PORT FALSE)
            if(AXIOM_HOST_TARGET MATCHES "^test_" AND
               NOT AXIOM_HOST_TARGET MATCHES "^test_single_header")
                set(AXIOM_HOST_NEEDS_GENERIC_PORT TRUE)
            elseif(AXIOM_HOST_TARGET STREQUAL "generate_golden" OR
                   AXIOM_HOST_TARGET STREQUAL "axiomtrace_overwrite" OR
                   AXIOM_HOST_TARGET STREQUAL "axiomtrace_small_payload")
                set(AXIOM_HOST_NEEDS_GENERIC_PORT TRUE)
            endif()
            if(AXIOM_HOST_NEEDS_GENERIC_PORT)
                target_link_libraries(${AXIOM_HOST_TARGET} PRIVATE
                    AxiomTrace::port_generic)
            endif()
        endforeach()
    endif()
elseif(AXIOM_BUILD_BENCHMARKS)
    if(NOT AXIOM_PLATFORM STREQUAL "host")
        message(FATAL_ERROR
            "AXIOM_BUILD_BENCHMARKS requires AXIOM_PLATFORM=host")
    endif()
    add_subdirectory(tests/host)
endif()

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
if(AXIOM_BUILD_EXAMPLES)
    add_subdirectory(baremetal/examples)
    if(AXIOM_PLATFORM STREQUAL "host" AND TARGET AxiomTrace::port_generic)
        target_link_libraries(example_minimal PRIVATE AxiomTrace::port_generic)
        target_link_libraries(example_full PRIVATE AxiomTrace::port_generic)
    endif()
endif()
if(AXIOM_BUILD_TESTS AND AXIOM_BUILD_EXAMPLES)
    find_package(Python3 3.12 COMPONENTS Interpreter REQUIRED)
    add_test(NAME test_example_e2e
        COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/host/test_example_e2e.py
            $<TARGET_FILE:example_minimal>)
    axiom_configure_test_runtime(test_example_e2e)
endif()

# ---------------------------------------------------------------------------
# Installable CMake package
# ---------------------------------------------------------------------------
install(TARGETS axiomtrace axiomtrace_port_generic EXPORT AxiomTraceTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/axiomtrace)
if(TARGET axiomtrace_port)
    install(TARGETS axiomtrace_port EXPORT AxiomTraceTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/axiomtrace)
endif()
install(FILES
    ${AXIOM_INSTALL_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/axiomtrace)
install(EXPORT AxiomTraceTargets
    FILE AxiomTraceTargets.cmake
    NAMESPACE AxiomTrace::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AxiomTrace)
configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/AxiomTraceConfig.cmake.in
    ${PROJECT_BINARY_DIR}/AxiomTraceConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AxiomTrace)
write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/AxiomTraceConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${PROJECT_BINARY_DIR}/AxiomTraceConfig.cmake
    ${PROJECT_BINARY_DIR}/AxiomTraceConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AxiomTrace)
