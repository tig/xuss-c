cmake_minimum_required(VERSION 3.16)
project(gcu_c_host C)

set(CMAKE_C_STANDARD 11)
enable_testing()

add_library(gcu_core
  ../src/defaults.c
  ../src/domain.c
  ../src/themes.c
  ../src/music.c
  ../src/face.c
  ../src/app.c
  ../src/protocol.c
  ../src/details.c
  ../src/render.c
  ../src/input.c
)
target_include_directories(gcu_core PUBLIC ../include)

# One test executable + ctest case per domain area (spec L0 coverage).
set(GCU_HOST_TESTS defaults themes music face app protocol details render input)
foreach(t IN LISTS GCU_HOST_TESTS)
  add_executable(test_${t} test_${t}.c)
  target_link_libraries(test_${t} gcu_core)
  add_test(NAME ${t} COMMAND test_${t})
endforeach()

# Convenience: cmake --build build/host --target host_test runs ctest.
# Do not name this target "test" — CMake reserves that name when enable_testing() is on.
add_custom_target(host_test
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
foreach(t IN LISTS GCU_HOST_TESTS)
  add_dependencies(host_test test_${t})
endforeach()
