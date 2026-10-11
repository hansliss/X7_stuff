cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../cmake/x7-mips-toolchain.cmake" CACHE FILEPATH "X7 toolchain")
    endif()
    project(doom_x7 LANGUAGES C ASM)
    include(../cmake/X7Probe.cmake)
endif()
set(APP_BASE "0x69800000" CACHE STRING "Application load base")
set(DOOM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../DOOM/linuxdoom-1.10")
if(NOT EXISTS "${DOOM_SOURCE}/d_main.c")
    message(FATAL_ERROR "Clone the original DOOM repository under DOOM first")
endif()
set(GENERATED "${CMAKE_CURRENT_BINARY_DIR}/engine")
file(GLOB DOOM_INPUTS CONFIGURE_DEPENDS "${DOOM_SOURCE}/*.c" "${DOOM_SOURCE}/*.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${DOOM_INPUTS} "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py" "${DOOM_SOURCE}" "${GENERATED}" COMMAND_ERROR_IS_FATAL ANY)
# The compiler provides stddef/stdarg; all other required runtime headers are local.
set(HEADERS "${CMAKE_CURRENT_BINARY_DIR}/headers")
file(MAKE_DIRECTORY "${HEADERS}/sys")
foreach(name stdio.h stdlib.h string.h ctype.h values.h fcntl.h unistd.h malloc.h sys/stat.h sys/types.h)
    file(WRITE "${HEADERS}/${name}" "#include \"x7_runtime.h\"\n")
endforeach()
file(WRITE "${HEADERS}/math.h" "/* Engine uses compiled integer trig tables. */\n")
file(WRITE "${HEADERS}/alloca.h" "#define alloca(n) __builtin_alloca(n)\n")
file(GLOB ENGINE "${GENERATED}/*.c")
list(FILTER ENGINE EXCLUDE REGEX "/(i_main|i_system|i_video|i_sound|i_net|m_fixed|s_sound)\\.c$")
option(X7_DOOM_DEBUG_LOG "Write playable diagnostics to the SD card" OFF)
add_executable(doom ${ENGINE} "${GENERATED}/s_sound.c" platform.c runtime.c fixed.c audio.c input.c display.c settings.c music.c vendor/rp2040-opl/emu8950.c input_stubs.S
 ../sysprobe/crt.c ../sysprobe/app_stubs.S ../sysprobe/ui_stubs.S ../sysprobe/fb_video.c ../sysprobe/audio_stubs.S)
target_compile_definitions(doom PRIVATE X7_REALTIME_AUDIO=1 X7_PLAYABLE=1 X7_DOOM_DEBUG_LOG=$<BOOL:${X7_DOOM_DEBUG_LOG}>)
set_target_properties(doom PROPERTIES SUFFIX ".app")
target_include_directories(doom PRIVATE "${HEADERS}" include "${GENERATED}")
target_compile_definitions(doom PRIVATE NORMALUNIX=1 LINUX=1)
x7_probe_target(doom "${CMAKE_CURRENT_SOURCE_DIR}/../sysprobe/app.xn")
# Old engine warnings are preserved in the build log; port sources are checked separately.
target_compile_options(doom PRIVATE -fwrapv -fno-strict-aliasing
    "$<$<COMPILE_LANGUAGE:C>:-include;${CMAKE_CURRENT_SOURCE_DIR}/include/x7_runtime.h>")
set_source_files_properties(${ENGINE} "${GENERATED}/s_sound.c"
    PROPERTIES COMPILE_OPTIONS "-Wno-error")
target_link_options(doom PRIVATE "-Wl,--defsym=APP_BASE=${APP_BASE}")
add_custom_command(TARGET doom POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../sysprobe/check_app.py" "$<TARGET_FILE:doom>"
    VERBATIM)

# The RP2040 OPL source uses an anonymous named struct supported by GCC.
set_source_files_properties(music.c vendor/rp2040-opl/emu8950.c
    PROPERTIES COMPILE_OPTIONS "-fms-extensions")
set_source_files_properties(vendor/rp2040-opl/emu8950.c
    PROPERTIES COMPILE_OPTIONS "-fms-extensions;-Wno-unused-parameter")
