cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../cmake/x7-mips-toolchain.cmake"
            CACHE FILEPATH "X7 MIPS O32 cross toolchain")
    endif()
    project(x7_app_probe LANGUAGES C ASM)
    include(../cmake/X7Probe.cmake)
endif()
set(APP_BASE "0x69800000" CACHE STRING "Fixed application load base")
if(NOT APP_BASE MATCHES "^0x[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]$")
    message(FATAL_ERROR "APP_BASE must be an eight-digit hexadecimal address")
endif()
add_executable(sysprobe crt.c probe.c app_stubs.S)
set_target_properties(sysprobe PROPERTIES SUFFIX ".app")
x7_probe_target(sysprobe "${CMAKE_CURRENT_SOURCE_DIR}/app.xn")
set_property(TARGET sysprobe APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py")
target_link_options(sysprobe PRIVATE "-Wl,--defsym=APP_BASE=${APP_BASE}")
add_custom_command(TARGET sysprobe POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py"
            "$<TARGET_FILE:sysprobe>"
    VERBATIM)

add_executable(sysprobe-ui crt.c probe.c app_stubs.S ui_probe.c ui_stubs.S)
target_compile_definitions(sysprobe-ui PRIVATE PROBE_UI=1)
set_target_properties(sysprobe-ui PROPERTIES SUFFIX ".app")
x7_probe_target(sysprobe-ui "${CMAKE_CURRENT_SOURCE_DIR}/app.xn")
set_property(TARGET sysprobe-ui APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py")
target_link_options(sysprobe-ui PRIVATE "-Wl,--defsym=APP_BASE=${APP_BASE}")
add_custom_command(TARGET sysprobe-ui POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py" "$<TARGET_FILE:sysprobe-ui>"
    VERBATIM)

add_executable(sysprobe-font crt.c probe.c app_stubs.S ui_probe.c ui_stubs.S)
target_compile_definitions(sysprobe-font PRIVATE PROBE_UI=1 PROBE_FONT=1)
set_target_properties(sysprobe-font PROPERTIES SUFFIX ".app")
x7_probe_target(sysprobe-font "${CMAKE_CURRENT_SOURCE_DIR}/app.xn")
set_property(TARGET sysprobe-font APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py")
target_link_options(sysprobe-font PRIVATE "-Wl,--defsym=APP_BASE=${APP_BASE}")
add_custom_command(TARGET sysprobe-font POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py" "$<TARGET_FILE:sysprobe-font>"
    VERBATIM)

set(discovery_index 0)
foreach(candidate IN ITEMS commonui fusion style apconfig all)
    math(EXPR discovery_index "${discovery_index} + 1")
    set(target "sysprobe-default-${candidate}")
    add_executable(${target} crt.c probe.c app_stubs.S ui_stubs.S)
    target_compile_definitions(${target} PRIVATE PROBE_UI=1 PROBE_DISCOVERY=${discovery_index})
    set_target_properties(${target} PROPERTIES SUFFIX ".app")
    x7_probe_target(${target} "${CMAKE_CURRENT_SOURCE_DIR}/app.xn")
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py")
    target_link_options(${target} PRIVATE "-Wl,--defsym=APP_BASE=${APP_BASE}")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py" "$<TARGET_FILE:${target}>"
        VERBATIM)
endforeach()

add_executable(sysprobe-hsv crt.c probe.c app_stubs.S ui_probe.c ui_stubs.S hsv.c)
target_compile_definitions(sysprobe-hsv PRIVATE PROBE_UI=1 PROBE_HSV=1)
set_target_properties(sysprobe-hsv PROPERTIES SUFFIX ".app")
x7_probe_target(sysprobe-hsv "${CMAKE_CURRENT_SOURCE_DIR}/app.xn")
set_property(TARGET sysprobe-hsv APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py")
target_link_options(sysprobe-hsv PRIVATE "-Wl,--defsym=APP_BASE=${APP_BASE}")
add_custom_command(TARGET sysprobe-hsv POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py" "$<TARGET_FILE:sysprobe-hsv>"
    VERBATIM)

add_executable(sysprobe-bitmap crt.c probe.c app_stubs.S ui_probe.c ui_stubs.S hsv.c)
target_compile_definitions(sysprobe-bitmap PRIVATE PROBE_UI=1 PROBE_HSV=1 PROBE_BITMAP=1)
set_target_properties(sysprobe-bitmap PROPERTIES SUFFIX ".app")
x7_probe_target(sysprobe-bitmap "${CMAKE_CURRENT_SOURCE_DIR}/app.xn")
set_property(TARGET sysprobe-bitmap APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py")
target_link_options(sysprobe-bitmap PRIVATE "-Wl,--defsym=APP_BASE=${APP_BASE}")
add_custom_command(TARGET sysprobe-bitmap POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py" "$<TARGET_FILE:sysprobe-bitmap>"
    VERBATIM)

add_executable(sysprobe-dodeca crt.c probe.c app_stubs.S dodeca_demo.c dodeca_render.c ui_stubs.S hsv.c)
target_compile_definitions(sysprobe-dodeca PRIVATE PROBE_UI=1 PROBE_DODECA=1)
set_target_properties(sysprobe-dodeca PROPERTIES SUFFIX ".app")
x7_probe_target(sysprobe-dodeca "${CMAKE_CURRENT_SOURCE_DIR}/app.xn")
set_property(TARGET sysprobe-dodeca APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py")
target_link_options(sysprobe-dodeca PRIVATE "-Wl,--defsym=APP_BASE=${APP_BASE}")
add_custom_command(TARGET sysprobe-dodeca POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py" "$<TARGET_FILE:sysprobe-dodeca>"
    VERBATIM)

add_executable(sysprobe-dodeca-fb crt.c probe.c app_stubs.S fb_video.c dodeca_demo.c dodeca_render.c ui_stubs.S hsv.c)
target_compile_definitions(sysprobe-dodeca-fb PRIVATE PROBE_UI=1 PROBE_DODECA=1 PROBE_FB=1)
set_target_properties(sysprobe-dodeca-fb PROPERTIES SUFFIX ".app")
x7_probe_target(sysprobe-dodeca-fb "${CMAKE_CURRENT_SOURCE_DIR}/app.xn")
set_property(TARGET sysprobe-dodeca-fb APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py")
target_link_options(sysprobe-dodeca-fb PRIVATE "-Wl,--defsym=APP_BASE=${APP_BASE}")
add_custom_command(TARGET sysprobe-dodeca-fb POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py" "$<TARGET_FILE:sysprobe-dodeca-fb>"
    VERBATIM)

add_executable(sysprobe-dodeca-fast crt.c probe.c app_stubs.S fb_video.c dodeca_demo.c dodeca_render.c ui_stubs.S hsv.c)
target_compile_definitions(sysprobe-dodeca-fast PRIVATE PROBE_UI=1 PROBE_DODECA=1 PROBE_FB=1 PROBE_FAST=1)
set_target_properties(sysprobe-dodeca-fast PROPERTIES SUFFIX ".app")
x7_probe_target(sysprobe-dodeca-fast "${CMAKE_CURRENT_SOURCE_DIR}/app.xn")
set_property(TARGET sysprobe-dodeca-fast APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py")
target_link_options(sysprobe-dodeca-fast PRIVATE "-Wl,--defsym=APP_BASE=${APP_BASE}")
add_custom_command(TARGET sysprobe-dodeca-fast POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_app.py" "$<TARGET_FILE:sysprobe-dodeca-fast>"
    VERBATIM)
