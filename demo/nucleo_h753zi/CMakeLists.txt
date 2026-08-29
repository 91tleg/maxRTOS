cmake_minimum_required(VERSION 3.20)

project(maxrtos_demo_nucleo_h753zi C ASM)

set(MAXRTOS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MAXRTOS_BUILD_HARDWARE ON CACHE BOOL "" FORCE)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../maxrtos
    ${CMAKE_CURRENT_BINARY_DIR}/maxrtos
    EXCLUDE_FROM_ALL
)

add_executable(maxrtos_demo_nucleo_h753zi
    main.c
    syscalls.c
    startup.s
)

target_link_libraries(maxrtos_demo_nucleo_h753zi PRIVATE
    maxrtos
)

target_compile_options(maxrtos_demo_nucleo_h753zi PRIVATE
    -mcpu=cortex-m7
    -mthumb
    -Wall
    -Wextra
    -Wpedantic
    -g3
    -O0
)

target_link_options(maxrtos_demo_nucleo_h753zi PRIVATE
    -mcpu=cortex-m7
    -mthumb
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
    --specs=nosys.specs
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/maxrtos_demo_nucleo_h753zi.map
    -Wl,--gc-sections
)
