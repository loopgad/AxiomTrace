cmake_minimum_required(VERSION 3.16)

add_executable(example_minimal example_minimal.c)
target_link_libraries(example_minimal PRIVATE axiomtrace)

add_executable(example_full example_full.c)
target_link_libraries(example_full PRIVATE axiomtrace)
