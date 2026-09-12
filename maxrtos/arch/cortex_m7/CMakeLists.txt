add_library(maxrtos_arch_cortex_m7 STATIC
    src/context_switch.c
    src/context_switch.S
    src/fault_handlers.c
    src/mpu.c
    src/mpu_hw.c
    src/systick.c
    src/yield.c
)

target_include_directories(maxrtos_arch_cortex_m7
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/internal
)

target_link_libraries(maxrtos_arch_cortex_m7 PUBLIC
    maxrtos_kernel
)
