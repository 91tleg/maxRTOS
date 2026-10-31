cmake_minimum_required(VERSION 3.20)

# Hardware-in-the-loop tests for the NUCLEO-H753ZI.
#
# Each test is one self-checking firmware. runner.py builds them, flashes
# them one at a time and reads the verdict from the board; run it instead of
# calling CMake by hand:
#
#   python3 maxrtos/tests/hil/runner.py
#
# To build only:
#   cmake -S maxrtos/tests/hil -B build/hil -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/arm-none-eabi.cmake
#   cmake --build build/hil
# Requires: arm-none-eabi-gcc and python3 with jinja2 (for the code generator).

project(maxrtos_hil C ASM)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/c_standard.cmake)

set(MAXRTOS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set(QEMU_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/../qemu)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_subdirectory(
    ${MAXRTOS_ROOT}/maxrtos
    ${CMAKE_CURRENT_BINARY_DIR}/maxrtos
    EXCLUDE_FROM_ALL
)
add_subdirectory(
    ${MAXRTOS_ROOT}/maxrtos/arch/cortex_m7
    ${CMAKE_CURRENT_BINARY_DIR}/arch
    EXCLUDE_FROM_ALL
)

file(GLOB CODEGEN_SOURCES CONFIGURE_DEPENDS
    ${MAXRTOS_ROOT}/tools/maxrtos_codegen/maxrtos_codegen/*.py
    ${MAXRTOS_ROOT}/tools/maxrtos_codegen/maxrtos_codegen/generators/*.py
    ${MAXRTOS_ROOT}/tools/maxrtos_codegen/maxrtos_codegen/templates/*.j2
)

set(HIL_MANIFEST ${CMAKE_CURRENT_BINARY_DIR}/hil_tests.txt)
file(WRITE ${HIL_MANIFEST} "")

# add_hil_test(<name>
#     SOURCE <file> [ASM <file>...]
#     [CONFIG <module.json>]       default: the QEMU tests' module.json
#     [REGION <spec>...]           extra background MPU regions, see
#                                  retarget_module.py
#     [DEFINES <def>...])
#
# Builds hil_<name> from the test source and a maxrtos_config generated from
# the module JSON retargeted to the board's memory map, and adds it to the
# manifest that runner.py reads.
function(add_hil_test name)
    cmake_parse_arguments(ARG "" "SOURCE;CONFIG" "ASM;REGION;DEFINES" ${ARGN})

    if(NOT ARG_CONFIG)
        set(ARG_CONFIG ${QEMU_TESTS}/module.json)
    endif()

    set(gen ${CMAKE_CURRENT_BINARY_DIR}/gen_${name})
    set(target hil_${name})

    set(region_args "")
    foreach(spec ${ARG_REGION})
        list(APPEND region_args --region ${spec})
    endforeach()

    add_custom_command(
        OUTPUT
            ${gen}/maxrtos_config.c
            ${gen}/maxrtos_config.h
            ${gen}/link.ld
        COMMAND ${CMAKE_COMMAND} -E make_directory ${gen}
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/retarget_module.py
            ${ARG_CONFIG} ${gen}/module.json ${region_args}
        COMMAND ${CMAKE_COMMAND} -E env
            PYTHONPATH=${MAXRTOS_ROOT}/tools/maxrtos_codegen
            ${Python3_EXECUTABLE} -m maxrtos_codegen.cli
            ${gen}/module.json --out-dir ${gen}
        DEPENDS
            ${ARG_CONFIG}
            ${CMAKE_CURRENT_SOURCE_DIR}/retarget_module.py
            ${CODEGEN_SOURCES}
        COMMENT "Generating RTOS configuration for ${name}"
    )

    add_executable(${target}
        ${ARG_SOURCE}
        ${ARG_ASM}
        ${CMAKE_CURRENT_SOURCE_DIR}/harness.c
        ${gen}/maxrtos_config.c
        ${MAXRTOS_ROOT}/maxrtos/arch/cortex_m7/examples/startup.S
    )

    # The QEMU directory comes last: it supplies harness.h, the interface the
    # tests are written against. The HIL directory supplies hil.h.
    target_include_directories(${target} PRIVATE
        ${gen}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${QEMU_TESTS}
    )

    target_link_libraries(${target} PRIVATE
        maxrtos
        maxrtos_arch_cortex_m7
    )

    target_compile_options(${target} PRIVATE -Wall -Wextra -g)
    target_compile_definitions(${target} PRIVATE
        EMU_RESULTS_BASE=0x24040000UL
        ${ARG_DEFINES})

    target_link_options(${target} PRIVATE
        -T${gen}/link.ld
        -nostartfiles
        --specs=nosys.specs
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${target}.map
    )
    set_target_properties(${target} PROPERTIES LINK_DEPENDS ${gen}/link.ld)

    file(APPEND ${HIL_MANIFEST} "${name}\n")
endfunction()

# Tests that only mean something on silicon.
add_hil_test(timebase
    SOURCE tests/test_timebase.c
    DEFINES HIL_TICK_LIMIT=8000)

add_hil_test(mpu_regs
    SOURCE tests/test_mpu_regs.c)

add_hil_test(mpu_isolation
    SOURCE tests/test_mpu_isolation.c)

# 0x00100000 is inside the code region but past the 64 KiB of ITCM: the
# region lets the access through the MPU so it reaches the bus.
add_hil_test(fault_status
    SOURCE tests/test_fault_status.c
    REGION 4:0x00100000:4K:read_write)

# FPU instructions, with the soft-float calling convention the kernel uses.
# Not through link-time optimization: the LTO link step builds every unit with
# the target's options, which have no FPU, and cannot assemble these.
set_source_files_properties(
    ${QEMU_TESTS}/test_fpu_context.c ${QEMU_TESTS}/test_fpu_more.c
    ${QEMU_TESTS}/test_fpu_startup.c ${QEMU_TESTS}/test_stack_integrity.c
    ${QEMU_TESTS}/test_stack_halt.c
    PROPERTIES
    COMPILE_OPTIONS "-mfpu=fpv5-d16;-mfloat-abi=softfp;-fno-lto")
add_hil_test(fpu_context
    SOURCE ${QEMU_TESTS}/test_fpu_context.c
    CONFIG ${QEMU_TESTS}/module_fpu.json)
add_hil_test(fpu_more
    SOURCE ${QEMU_TESTS}/test_fpu_more.c
    CONFIG ${QEMU_TESTS}/module_fpu_system.json)
add_hil_test(fpu_startup
    SOURCE ${QEMU_TESTS}/test_fpu_startup.c)
add_hil_test(stack_halt
    SOURCE ${QEMU_TESTS}/test_stack_halt.c
    CONFIG ${QEMU_TESTS}/module_halt.json)
add_hil_test(stack_integrity
    SOURCE ${QEMU_TESTS}/test_stack_integrity.c
    CONFIG ${QEMU_TESTS}/module_fpu.json)

add_hil_test(context_integrity
    SOURCE tests/test_context_integrity.c
    ASM tests/context_integrity.S
    CONFIG ${QEMU_TESTS}/module_priority.json)

# The QEMU tests, unchanged, on the real chip.
add_hil_test(yield              SOURCE ${QEMU_TESTS}/test_yield.c)
add_hil_test(queue_port_ipc     SOURCE ${QEMU_TESTS}/test_queue_port_ipc.c)
add_hil_test(sampling_port      SOURCE ${QEMU_TESTS}/test_sampling_port.c)
add_hil_test(hm_restart_process SOURCE ${QEMU_TESTS}/test_hm_restart_process.c)
add_hil_test(hm_halt_partition  SOURCE ${QEMU_TESTS}/test_hm_halt_partition.c
    CONFIG ${QEMU_TESTS}/module_halt.json)
add_hil_test(deadline_supervision SOURCE ${QEMU_TESTS}/test_deadline_supervision.c)
add_hil_test(partition_privilege SOURCE ${QEMU_TESTS}/test_partition_privilege.c
    CONFIG ${QEMU_TESTS}/module_system.json)
add_hil_test(hardfault_escalation SOURCE ${QEMU_TESTS}/test_hardfault_escalation.c)
add_hil_test(mutex              SOURCE ${QEMU_TESTS}/test_mutex.c)
add_hil_test(mutex_release_on_restart
    SOURCE ${QEMU_TESTS}/test_mutex_release_on_restart.c)
add_hil_test(semaphore          SOURCE ${QEMU_TESTS}/test_semaphore.c)
add_hil_test(timed_wait         SOURCE ${QEMU_TESTS}/test_timed_wait.c)
add_hil_test(priority           SOURCE ${QEMU_TESTS}/test_priority.c
    CONFIG ${QEMU_TESTS}/module_priority.json)
