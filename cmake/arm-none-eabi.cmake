set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(MCU_FLAGS
    -mcpu=cortex-m7
    -mthumb
    # Soft-float: the kernel does not save or restore FPU state on a context
    # switch, so no code may use the FPU until that support exists.
    -mfloat-abi=soft
)

# Optimization level for every target built with this toolchain.
set(MAXRTOS_OPT_LEVEL "-O0" CACHE STRING "Compiler optimization flag (-O0, -O2, -Os)")

add_compile_options(${MCU_FLAGS} ${MAXRTOS_OPT_LEVEL})
add_link_options(${MCU_FLAGS})
