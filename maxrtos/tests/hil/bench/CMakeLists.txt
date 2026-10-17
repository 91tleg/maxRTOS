cmake_minimum_required(VERSION 3.20)

# Context-switch latency benchmark for the NUCLEO-H753ZI.
#
#   cmake -S maxrtos/tests/hil/bench -B build/bench -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/arm-none-eabi.cmake \
#         [-DBENCH_CACHES=ON]
#   cmake --build build/bench
#
# Run it with run_bench.py, which flashes the board and reads the results.

project(maxrtos_bench_context_switch C ASM)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/c_standard.cmake)

option(BENCH_480MHZ "Run the core at 480 MHz (PLL, VOS0, flash wait states)" OFF)
option(BENCH_CACHES "Enable the Cortex-M7 I- and D-caches before the run" OFF)

set(MAXRTOS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
set(DEMO ${MAXRTOS_ROOT}/demo/nucleo_h753zi)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_subdirectory(${MAXRTOS_ROOT}/maxrtos
                 ${CMAKE_CURRENT_BINARY_DIR}/maxrtos EXCLUDE_FROM_ALL)
add_subdirectory(${MAXRTOS_ROOT}/maxrtos/arch/cortex_m7
                 ${CMAKE_CURRENT_BINARY_DIR}/arch EXCLUDE_FROM_ALL)

file(GLOB CODEGEN_SOURCES CONFIGURE_DEPENDS
    ${MAXRTOS_ROOT}/tools/maxrtos_codegen/maxrtos_codegen/*.py
    ${MAXRTOS_ROOT}/tools/maxrtos_codegen/maxrtos_codegen/generators/*.py
    ${MAXRTOS_ROOT}/tools/maxrtos_codegen/maxrtos_codegen/templates/*.j2
)

set(gen ${CMAKE_CURRENT_BINARY_DIR}/generated)

add_custom_command(
    OUTPUT ${gen}/maxrtos_config.c ${gen}/maxrtos_config.h ${gen}/link.ld
    COMMAND ${CMAKE_COMMAND} -E env
        PYTHONPATH=${MAXRTOS_ROOT}/tools/maxrtos_codegen
        ${Python3_EXECUTABLE} -m maxrtos_codegen.cli
        ${CMAKE_CURRENT_SOURCE_DIR}/module.json --out-dir ${gen}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/module.json ${CODEGEN_SOURCES}
)

add_executable(bench_context_switch
    bench_context_switch.c
    ${gen}/maxrtos_config.c
    ${DEMO}/syscalls.c
    ${MAXRTOS_ROOT}/maxrtos/arch/cortex_m7/examples/startup.S
)

target_include_directories(bench_context_switch PRIVATE ${gen})
target_link_libraries(bench_context_switch PRIVATE maxrtos maxrtos_arch_cortex_m7)
target_compile_options(bench_context_switch PRIVATE -Wall -Wextra -g3)

if(BENCH_480MHZ)
    target_compile_definitions(bench_context_switch PRIVATE BENCH_480MHZ=1)
endif()

if(BENCH_CACHES)
    target_compile_definitions(bench_context_switch PRIVATE BENCH_CACHES=1)
endif()

target_link_options(bench_context_switch PRIVATE
    -T${gen}/link.ld
    --specs=nosys.specs
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/bench_context_switch.map
    -Wl,--gc-sections
)
set_target_properties(bench_context_switch PROPERTIES LINK_DEPENDS ${gen}/link.ld)
