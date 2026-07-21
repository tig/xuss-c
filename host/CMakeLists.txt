cmake_minimum_required(VERSION 3.16)
project(xuss_c_host C)

set(CMAKE_C_STANDARD 11)
enable_testing()

add_library(xuss_core
  ../src/edge.c
  ../src/config.c
  ../src/protocol.c
)
target_include_directories(xuss_core PUBLIC ../include)

add_executable(test_edge test_edge.c)
target_link_libraries(test_edge xuss_core)
add_test(NAME edge COMMAND test_edge)

add_executable(test_config test_config.c)
target_link_libraries(test_config xuss_core)
add_test(NAME config COMMAND test_config)

add_executable(test_protocol test_protocol.c)
target_link_libraries(test_protocol xuss_core)
add_test(NAME protocol COMMAND test_protocol)
