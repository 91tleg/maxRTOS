# Software-in-the-loop (SIL) tests.
#
# The production kernel and the hardware-independent part of the Cortex-M7
# architecture layer are compiled unchanged for the host and run on a virtual
# Cortex-M7 (sim/). See README.md.

find_package(Threads REQUIRED)

set(MAXRTOS_ARCH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../arch/cortex_m7)
set(SIL_SIM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sim)

# Architecture sources with no direct hardware access once the port hooks
# (arch/cortex_m7/port.h) are redirected to the simulator.
set(SIL_ARCH_SOURCES
    ${MAXRTOS_ARCH_DIR}/src/svc.c
    ${MAXRTOS_ARCH_DIR}/src/systick.c
    ${MAXRTOS_ARCH_DIR}/src/start_scheduler.c
    ${MAXRTOS_ARCH_DIR}/src/idle.c
    ${MAXRTOS_ARCH_DIR}/src/mpu.c
    ${MAXRTOS_ARCH_DIR}/src/mpu_hw.c
    ${MAXRTOS_ARCH_DIR}/src/fault_dispatch.c
)

add_library(maxrtos_sil_target STATIC
    ${SIL_ARCH_SOURCES}
    sim/sim_target.c
    sim/sim_mpu.c
    sim/sil_system.c
    sim/sil_test.c
)

set_source_files_properties(${SIL_ARCH_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-include;${SIL_SIM_DIR}/sim_port_overrides.h"
)

target_include_directories(maxrtos_sil_target PUBLIC
    ${SIL_SIM_DIR}
    ${MAXRTOS_ARCH_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_link_libraries(maxrtos_sil_target PUBLIC maxrtos_kernel Threads::Threads)

target_compile_options(maxrtos_sil_target PRIVATE -Wall -Wextra -Wno-unused-parameter)

# add_sil_test(<name> <source>): one suite executable, one CTest entry, and
# a JUnit XML report per suite under <build>/sil-reports/.
function(add_sil_test name source)
    add_executable(sil_${name} ${source})
    target_link_libraries(sil_${name} PRIVATE maxrtos_sil_target)
    target_compile_options(sil_${name} PRIVATE -Wall -Wextra)

    add_test(
        NAME sil_${name}
        COMMAND sil_${name} --junit ${CMAKE_BINARY_DIR}/sil-reports/${name}.xml
    )
    set_tests_properties(sil_${name} PROPERTIES LABELS sil TIMEOUT 120)
endfunction()

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/sil-reports)

add_sil_test(simulator_selfcheck  test_simulator_selfcheck.c)
add_sil_test(temporal_partitioning test_temporal_partitioning.c)
add_sil_test(scheduler_startup   test_scheduler_startup.c)
add_sil_test(process_yield      test_process_yield.c)
add_sil_test(ipc_queue_port      test_ipc_queue_port.c)
add_sil_test(spatial_partitioning test_spatial_partitioning.c)
add_sil_test(fault_containment   test_fault_containment.c)
add_sil_test(fault_recovery_soak test_fault_recovery_soak.c)
