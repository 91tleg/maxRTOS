cmake_minimum_required(VERSION 3.20)

project(maxrtos_demo_nucleo_h753zi C ASM)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/c_standard.cmake)

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

# Size the kernel's tables to this module (cmake/maxrtos_limits.cmake). The
# directory is generated with `maxrtos-codegen module.json --out-dir generated
# --emit-limits` before configuring.
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/maxrtos_limits.cmake)
maxrtos_use_generated_limits(${CMAKE_CURRENT_SOURCE_DIR}/generated/maxrtos_limits.h)

add_executable(maxrtos_demo_nucleo_h753zi
    main.c
    generated/maxrtos_config.c
    syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../maxrtos/arch/cortex_m7/examples/startup.S
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
)

target_link_options(maxrtos_demo_nucleo_h753zi PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/generated/link.ld
    --specs=nosys.specs
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/maxrtos_demo_nucleo_h753zi.map
    -Wl,--gc-sections
)
