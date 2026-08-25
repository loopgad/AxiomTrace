cmake_minimum_required(VERSION 3.16)

add_executable(example_minimal example_minimal.c)
target_link_libraries(example_minimal PRIVATE axiomtrace)

add_executable(example_full example_full.c)
target_link_libraries(example_full PRIVATE axiomtrace)

# Compile-only integration template. It intentionally is not linked into the
# Host examples: its strong Port symbols are meant to be copied into a
# firmware target that selects AXIOM_PORT_SOURCE=NONE.
add_library(axiomtrace_custom_port_skeleton OBJECT EXCLUDE_FROM_ALL
    example_custom_port.c)
target_link_libraries(axiomtrace_custom_port_skeleton PRIVATE axiomtrace)
target_compile_features(axiomtrace_custom_port_skeleton PRIVATE c_std_11)
