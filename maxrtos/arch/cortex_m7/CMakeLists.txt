add_library(maxrtos_arch_cortex_m7 STATIC
    src/context_switch.c
    src/context_switch.S
)

target_include_directories(maxrtos_arch_cortex_m7 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

target_link_libraries(maxrtos_arch_cortex_m7 PUBLIC
    maxrtos_kernel
)

target_compile_options(maxrtos_arch_cortex_m7 PUBLIC
    -mcpu=cortex-m7
    -mthumb
)

target_compile_options(maxrtos_arch_cortex_m7 PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
