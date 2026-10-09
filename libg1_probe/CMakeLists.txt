cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../cmake/x7-mips-toolchain.cmake"
            CACHE FILEPATH "X7 MIPS O32 cross toolchain")
    endif()
    project(x7_libg1_probe LANGUAGES C ASM)
    include(../cmake/X7Probe.cmake)
endif()
option(PROBE_YIELD "Yield one OS tick between libg1 checkpoints" ON)
set(probe_mode 0)
foreach(variant fs metadata directory watchdog)
    set(target "libg1-${variant}")
    add_executable(${target} libg1.c)
    set_target_properties(${target} PROPERTIES SUFFIX ".so")
    target_compile_definitions(${target} PRIVATE
        PROBE_MODE=${probe_mode} PROBE_YIELD=$<BOOL:${PROBE_YIELD}>)
    x7_probe_target(${target} "${CMAKE_CURRENT_SOURCE_DIR}/so.xn")
    target_link_options(${target} PRIVATE "-Wl,--section-start=.init=0x51400000")
    math(EXPR probe_mode "${probe_mode}+1")
endforeach()
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/libg1.so"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:libg1-fs>" "${CMAKE_CURRENT_BINARY_DIR}/libg1.so"
    DEPENDS libg1-fs VERBATIM)
add_custom_target(libg1_default ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libg1.so")
