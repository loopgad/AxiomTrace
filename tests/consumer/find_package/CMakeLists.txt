cmake_minimum_required(VERSION 3.16)
project(AxiomTraceInstalledConsumer LANGUAGES C)

find_package(AxiomTrace 1.0 CONFIG REQUIRED)

if(NOT TARGET AxiomTrace::port_generic)
    message(FATAL_ERROR "Installed AxiomTrace package lacks AxiomTrace::port_generic")
endif()

add_executable(consumer ../main.c)
target_link_libraries(consumer PRIVATE
    AxiomTrace::axiomtrace
    AxiomTrace::port_generic)

add_custom_command(TARGET consumer POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        "-DAXIOMTRACE_CORE_ARCHIVE=$<TARGET_FILE:AxiomTrace::axiomtrace>"
        "-DAXIOMTRACE_NM=${CMAKE_NM}"
        -P "${CMAKE_CURRENT_LIST_DIR}/../check_core_port_free.cmake"
    VERBATIM)
