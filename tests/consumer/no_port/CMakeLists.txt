cmake_minimum_required(VERSION 3.16)
project(AxiomTraceNoPortConsumer LANGUAGES C)

if(NOT AXIOMTRACE_SOURCE)
    message(FATAL_ERROR "AXIOMTRACE_SOURCE is required")
endif()

set(AXIOM_PORT_SOURCE NONE CACHE STRING "" FORCE)
add_subdirectory(${AXIOMTRACE_SOURCE} axiomtrace-build)

add_executable(consumer_without_port ../main.c)
target_link_libraries(consumer_without_port PRIVATE AxiomTrace::axiomtrace)
