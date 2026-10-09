include_guard(GLOBAL)
get_filename_component(X7_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "mips")
    message(FATAL_ERROR "Use cmake/x7-mips-toolchain.cmake to build X7 probes")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(X7_READELF NAMES "${X7_TOOLCHAIN_PREFIX}-readelf" REQUIRED)
find_program(X7_OBJDUMP NAMES "${X7_TOOLCHAIN_PREFIX}-objdump" REQUIRED)
add_library(x7_probe_options INTERFACE)
target_compile_options(x7_probe_options INTERFACE
    -EL -mabi=32 -march=24kec -O2 -G0 -mno-abicalls -fno-pic
    "$<$<COMPILE_LANGUAGE:C>:-ffreestanding;-fno-builtin;-fno-stack-protector;-fno-unwind-tables;-fno-asynchronous-unwind-tables;-std=c11;-Wall;-Wextra;-Werror>")
target_link_options(x7_probe_options INTERFACE
    -EL -mabi=32 -march=24kec -nostdlib -no-pie
    "-Wl,--build-id=none,-n")
add_library(x7_syscalls OBJECT "${X7_ROOT}/syscalls.c")
target_link_libraries(x7_syscalls PRIVATE x7_probe_options)
add_custom_target(x7_check_syscalls
    COMMAND "${Python3_EXECUTABLE}" "${X7_ROOT}/tools/check_syscalls.py"
            --object $<TARGET_OBJECTS:x7_syscalls>
    DEPENDS x7_syscalls
    COMMENT "Checking all 243 firmware syscall stubs" VERBATIM)

function(x7_probe_target target linker_script)
    target_link_libraries(${target} PRIVATE x7_probe_options x7_syscalls)
    add_dependencies(${target} x7_check_syscalls)
    target_link_options(${target} PRIVATE "-T${linker_script}")
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${linker_script}")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            "-DX7_ELF=$<TARGET_FILE:${target}>"
            "-DX7_REPORT=${CMAKE_CURRENT_BINARY_DIR}/$<TARGET_FILE_NAME:${target}>"
            "-DX7_READELF=${X7_READELF}" "-DX7_OBJDUMP=${X7_OBJDUMP}"
            -P "${X7_ROOT}/cmake/InspectElf.cmake"
        VERBATIM)
endfunction()
