cmake_minimum_required(VERSION 3.16)
project(gcu_c_host C)

set(CMAKE_C_STANDARD 11)
enable_testing()

add_library(gcu_core
  ../src/defaults.c
  ../src/domain.c
  ../src/proto.c
  ../src/ui.c
  ../src/audio.c
  ../src/input.c
)
target_include_directories(gcu_core PUBLIC ../include)

add_executable(test_defaults test_defaults.c)
target_link_libraries(test_defaults gcu_core)
add_test(NAME defaults COMMAND test_defaults)

add_executable(test_ui test_ui.c)
target_link_libraries(test_ui gcu_core)
add_test(NAME ui COMMAND test_ui)

add_executable(test_proto test_proto.c)
target_link_libraries(test_proto gcu_core)
add_test(NAME proto COMMAND test_proto)

add_executable(test_audio test_audio.c)
target_link_libraries(test_audio gcu_core)
add_test(NAME audio COMMAND test_audio)

add_executable(test_input test_input.c)
target_link_libraries(test_input gcu_core)
add_test(NAME input COMMAND test_input)

# Convenience: cmake --build build/host --target host_test runs ctest.
# Do not name this target "test" — CMake reserves that name when enable_testing() is on.
add_custom_target(host_test
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  DEPENDS test_defaults test_ui test_proto test_audio test_input
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
