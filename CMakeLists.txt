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

# M2 onward adds the hardware-model library here and links riscv_core.
