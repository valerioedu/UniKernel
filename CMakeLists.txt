cmake_minimum_required(VERSION 3.16)

if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin")
    set(CMAKE_SYSTEM_NAME Generic)
    set(CMAKE_OSX_SYSROOT "")
endif()

set(CMAKE_OSX_ARCHITECTURES "")

project(valerioedx ASM C CXX)

set(ARCH "AArch64")
        
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
set(CMAKE_LINKER ld)

set(CMAKE_C_FLAGS "-march=armv8-a -ffreestanding -fno-pie -fno-builtin")
set(CMAKE_ASM_FLAGS "-march=armv8-a")

set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_C_COMPILER> <LINK_FLAGS> <OBJECTS> -o <TARGET>")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

file(GLOB_RECURSE KERNEL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.S
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_executable(kernel.elf ${KERNEL_SOURCES})

target_compile_definitions(kernel.elf PRIVATE MAX_CPUS=2)

set_target_properties(kernel.elf PROPERTIES
    LINK_FLAGS "-T ${LINKER_SCRIPT} -nostdlib"
    LINK_DEPENDS ${LINKER_SCRIPT}
)