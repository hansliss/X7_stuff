cmake_minimum_required(VERSION 3.20)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/x7-mips-toolchain.cmake"
        CACHE FILEPATH "X7 MIPS O32 cross toolchain")
endif()
project(x7_probes LANGUAGES C ASM)
include(cmake/X7Probe.cmake)
add_subdirectory(libg1_probe)
add_subdirectory(sysprobe)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/DOOM/linuxdoom-1.10/d_main.c")
    add_subdirectory(doom_x7)
endif()

add_subdirectory(omega_x7)
