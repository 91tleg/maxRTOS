add_executable(maxrtos_arch_cortex_m7_test_mpu
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../arch/cortex_m7/src/mpu.c
    test_mpu.c
)

target_include_directories(maxrtos_arch_cortex_m7_test_mpu PRIVATE
    ../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../arch/cortex_m7/include 
)

add_test(NAME arch_cortex_m7_mpu_tests COMMAND maxrtos_arch_cortex_m7_test_mpu)
