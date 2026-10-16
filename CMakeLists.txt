cmake_minimum_required(VERSION 3.20)
project(RadSlice LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# The golden model. RadSlice links riscv_core and never modifies it.
# EXCLUDE_FROM_ALL: build only the library, not riscv-emu's own tests/CLI.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/riscv-emu/CMakeLists.txt")
  message(FATAL_ERROR
    "extern/riscv-emu is empty. Run: git submodule update --init")
endif()
add_subdirectory(extern/riscv-emu EXCLUDE_FROM_ALL)

# The hardware model: sliced ALU and register file under the golden core.
add_library(radslice_hw STATIC src/sliced_alu.cpp src/sliced_regfile.cpp)
target_include_directories(radslice_hw PUBLIC include)
target_link_libraries(radslice_hw PUBLIC riscv_core)
target_compile_options(radslice_hw PRIVATE -Wall -Wextra)

enable_testing()
add_executable(radslice_run src/radslice_run.cpp)
target_link_libraries(radslice_run PRIVATE radslice_hw)
target_compile_options(radslice_run PRIVATE -Wall -Wextra)

foreach(t test_sliced_alu test_hw_lockstep)
  add_executable(${t} tests/${t}.cpp)
  target_link_libraries(${t} PRIVATE radslice_hw)
  target_compile_options(${t} PRIVATE -Wall -Wextra)
  add_test(NAME radslice_${t} COMMAND ${t})
endforeach()
