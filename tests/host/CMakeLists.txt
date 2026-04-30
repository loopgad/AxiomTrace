cmake_minimum_required(VERSION 3.16)
project(axiom_tests LANGUAGES C)
set(CMAKE_C_STANDARD 11)

# Simple host tests without Unity for now
add_executable(test_ring_buf test_ring.c)
target_link_libraries(test_ring_buf PRIVATE axiomtrace)
add_test(NAME test_ring_buf COMMAND test_ring_buf)

add_executable(test_crc test_crc.c)
target_link_libraries(test_crc PRIVATE axiomtrace)
add_test(NAME test_crc COMMAND test_crc)

add_executable(test_event test_event.c)
target_link_libraries(test_event PRIVATE axiomtrace)
add_test(NAME test_event COMMAND test_event)

add_executable(test_filter test_filter.c)
target_link_libraries(test_filter PRIVATE axiomtrace)
add_test(NAME test_filter COMMAND test_filter)
