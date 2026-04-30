# ============================================================================
# AxiomTrace Precompiled Library Example
# ============================================================================
#
# This CMakeLists.txt demonstrates how to use AxiomTrace as a precompiled
# static library instead of building from source.
#
# Usage:
#   cmake -B build \
#     -DAXIOM_PRECOMPILED_LIB=/path/to/libaxiomtrace.a \
#     -DAXIOM_INCLUDE_DIR=/path/to/axiomtrace_headers
#   cmake --build build
#
# Or let CMake find the library automatically if installed:
#   cmake -B build -DAXIOM_USE_PRECOMPILED=ON
# ============================================================================

cmake_minimum_required(VERSION 3.16)

project(AxiomTracePrecompiledExample LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Precompiled Library Configuration
# ---------------------------------------------------------------------------

# Option 1: Use precompiled library
option(AXIOM_USE_PRECOMPILED "Use precompiled library instead of building from source" OFF)

# Path to precompiled library (set by user or CMake)
set(AXIOM_PRECOMPILED_LIB "" CACHE FILEPATH "Path to libaxiomtrace.a")
set(AXIOM_INCLUDE_DIR "" CACHE PATH "Path to axiomtrace headers")

if(AXIOM_USE_PRECOMPILED)

    # If precompiled library is enabled but paths not provided, try to find them
    if(AXIOM_PRECOMPILED_LIB STREQUAL "" OR AXIOM_INCLUDE_DIR STREQUAL "")
        find_library(AXIOM_LIB axiomtrace REQUIRED)
        find_path(AXIOM_INCLUDE axiomtrace.h REQUIRED)
        message(STATUS "Found AxiomTrace library: ${AXIOM_LIB}")
        message(STATUS "Found AxiomTrace include: ${AXIOM_INCLUDE}")
    else()
        set(AXIOM_LIB ${AXIOM_PRECOMPILED_LIB})
        set(AXIOM_INCLUDE ${AXIOM_INCLUDE_DIR})
    endif()

    add_executable(example_precompiled example_precompiled.c)
    target_include_directories(example_precompiled PRIVATE ${AXIOM_INCLUDE})
    target_link_libraries(example_precompiled PRIVATE ${AXIOM_LIB})

    message(STATUS "=== AxiomTrace Precompiled Example Configuration ===")
    message(STATUS "  Library: ${AXIOM_LIB}")
    message(STATUS "  Include: ${AXIOM_INCLUDE}")
    message(STATUS "====================================================")

else()

    # Option 2: Build from source (fallback)
    # This requires the full AxiomTrace source tree
    #
    # To enable: cmake -B build -DAXIOM_USE_PRECOMPILED=OFF
    # Or simply don't set AXIOM_USE_PRECOMPILED

    message(WARNING "Precompiled library not enabled. This example requires:")
    message(WARNING "  cmake -B build -DAXIOM_USE_PRECOMPILED=ON")
    message(WARNING "Or provide paths explicitly:")
    message(WARNING "  cmake -B build -DAXIOM_PRECOMPILED_LIB=/path/to/libaxiomtrace.a -DAXIOM_INCLUDE_DIR=/path/to/headers")

    # For demonstration purposes, we still create the target but it won't link properly
    # without the source library in this fallback mode
    add_executable(example_precompiled example_precompiled.c)
    target_include_directories(example_precompiled PRIVATE
        ${CMAKE_SOURCE_DIR}/baremetal/include
        ${CMAKE_SOURCE_DIR}/baremetal/port
        ${CMAKE_SOURCE_DIR}/baremetal/core
        ${CMAKE_SOURCE_DIR}/baremetal/frontend
        ${CMAKE_SOURCE_DIR}/baremetal/backend
    )
    # In real use, you would link against the actual precompiled library

endif()