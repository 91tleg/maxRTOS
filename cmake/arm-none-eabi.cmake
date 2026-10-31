set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(MCU_FLAGS
    -mcpu=cortex-m7
    -mthumb
    -mfloat-abi=soft
)

# Optimization level for every target built with this toolchain.
set(MAXRTOS_OPT_LEVEL "-O0" CACHE STRING "Compiler optimization flag (-O0, -O2, -Os)")

# Link-time optimization lets the compiler inline across the kernel's
# translation units (the yield path crosses six of them). On for optimized
# builds, off at -O0 where it only slows the build and hides nothing.
if(MAXRTOS_OPT_LEVEL STREQUAL "-O0")
    set(MAXRTOS_LTO_DEFAULT OFF)
else()
    set(MAXRTOS_LTO_DEFAULT ON)
endif()
option(MAXRTOS_LTO "Build with link-time optimization" ${MAXRTOS_LTO_DEFAULT})
if(MAXRTOS_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

add_compile_options(${MCU_FLAGS} ${MAXRTOS_OPT_LEVEL})
add_link_options(${MCU_FLAGS})
