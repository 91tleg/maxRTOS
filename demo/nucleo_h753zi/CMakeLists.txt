cmake_minimum_required(VERSION 3.20)

project(maxrtos_demo_nucleo_h753zi C ASM)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../maxrtos
    ${CMAKE_CURRENT_BINARY_DIR}/maxrtos
    EXCLUDE_FROM_ALL
)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../maxrtos/arch/cortex_m7
    ${CMAKE_CURRENT_BINARY_DIR}/maxrtos_arch_cortex_m7
    EXCLUDE_FROM_ALL
)

add_executable(maxrtos_demo_nucleo_h753zi
    main.c
    syscalls.c
    startup.s
)

target_link_libraries(maxrtos_demo_nucleo_h753zi
    PRIVATE
        maxrtos
        maxrtos_arch_cortex_m7
)

target_compile_options(maxrtos_demo_nucleo_h753zi PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -g3
    -O0
)

target_link_options(maxrtos_demo_nucleo_h753zi PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
    --specs=nosys.specs
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/maxrtos_demo_nucleo_h753zi.map
    -Wl,--gc-sections
)
