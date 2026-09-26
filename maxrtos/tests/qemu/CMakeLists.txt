cmake_minimum_required(VERSION 3.20)

# QEMU emulation tests: the real kernel and Cortex-M7 architecture code,
# cross-compiled and run on an emulated Cortex-M7 (QEMU mps2-an500).
#
# Configure with the repository's cross toolchain file:
#   cmake -S maxrtos/tests/qemu -B build/qemu \
#         -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/arm-none-eabi.cmake
# Requires: arm-none-eabi-gcc, qemu-system-arm, python3 with jinja2.

project(maxrtos_qemu_tests C ASM)

enable_testing()

set(MAXRTOS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(QEMU_SYSTEM_ARM qemu-system-arm REQUIRED)

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

# add_emu_test(<name> <source> <module.json>)
#
# Builds one self-checking firmware from the given source and a
# maxrtos_config generated from the given module JSON, and registers a
# CTest that runs it under QEMU. The firmware ends QEMU through
# semihosting, so the QEMU exit status is the test verdict.
function(add_emu_test name source config)
    set(gen ${CMAKE_CURRENT_BINARY_DIR}/gen_${name})
    set(target emu_${name})

    add_custom_command(
        OUTPUT
            ${gen}/maxrtos_config.c
            ${gen}/maxrtos_config.h
            ${gen}/link.ld
        COMMAND ${CMAKE_COMMAND} -E env
            PYTHONPATH=${MAXRTOS_ROOT}/tools/maxrtos_codegen
            ${Python3_EXECUTABLE} -m maxrtos_codegen.cli
            ${CMAKE_CURRENT_SOURCE_DIR}/${config}
            --out-dir ${gen}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${config} ${CODEGEN_SOURCES}
        COMMENT "Generating RTOS configuration for ${name}"
    )

    add_executable(${target}
        ${source}
        harness.c
        ${gen}/maxrtos_config.c
        ${MAXRTOS_ROOT}/maxrtos/arch/cortex_m7/examples/startup.S
    )

    target_include_directories(${target} PRIVATE
        ${gen}
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(${target} PRIVATE
        maxrtos
        maxrtos_arch_cortex_m7
    )

    target_compile_options(${target} PRIVATE -Wall -Wextra -g -O0)

    target_link_options(${target} PRIVATE
        -T${gen}/link.ld
        -nostartfiles
        --specs=nosys.specs
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${target}.map
    )
    set_target_properties(${target} PROPERTIES LINK_DEPENDS ${gen}/link.ld)

    # -icount makes virtual time follow executed instructions, so tick
    # timing does not depend on how loaded the host is.
    add_test(
        NAME emu_${name}
        COMMAND ${QEMU_SYSTEM_ARM}
            -M mps2-an500 -cpu cortex-m7
            -nographic -monitor none -serial none
            -icount shift=0
            -semihosting-config enable=on,target=native
            -kernel $<TARGET_FILE:${target}>
    )
    set_tests_properties(emu_${name} PROPERTIES
        TIMEOUT 60
        LABELS emu
        PASS_REGULAR_EXPRESSION "EMU PASS"
        FAIL_REGULAR_EXPRESSION "EMU FAIL"
    )
endfunction()

add_emu_test(yield              test_yield.c              module.json)
add_emu_test(queue_port_ipc     test_queue_port_ipc.c     module.json)
add_emu_test(hm_restart_process test_hm_restart_process.c module.json)
add_emu_test(hm_halt_partition  test_hm_halt_partition.c  module_halt.json)
add_emu_test(deadline_supervision test_deadline_supervision.c module.json)
add_emu_test(partition_privilege test_partition_privilege.c module_system.json)
add_emu_test(hardfault_escalation test_hardfault_escalation.c module.json)
add_emu_test(mutex               test_mutex.c               module.json)
add_emu_test(mutex_release_on_restart test_mutex_release_on_restart.c module.json)
add_emu_test(semaphore           test_semaphore.c           module.json)
