# ============================================================================
# AxiomTrace Examples
# ============================================================================
#
# This directory contains example applications demonstrating AxiomTrace usage.
#
# Structure:
#   example_precompiled/  - Demonstrates precompiled library usage
#   ports/                 - Platform implementation references
#
# Build all examples:
#   cmake -B build -DAXIOM_BUILD_EXAMPLES=ON
#   cmake --build build
#
# Build specific example:
#   cmake -B build -DAXIOM_BUILD_EXAMPLES=ON
#   cmake --build build --target example_precompiled
# ============================================================================

cmake_minimum_required(VERSION 3.16)

project(AxiomTraceExamples LANGUAGES C)

# Enable examples if not already set
if(NOT DEFINED AXIOM_BUILD_EXAMPLES)
    option(AXIOM_BUILD_EXAMPLES "Build example applications" ON)
endif()

if(AXIOM_BUILD_EXAMPLES)
    add_subdirectory(example_precompiled)
endif()