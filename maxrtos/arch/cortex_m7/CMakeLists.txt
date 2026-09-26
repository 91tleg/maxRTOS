add_library(maxrtos_arch_cortex_m7 STATIC
    src/context_switch.c
    src/context_switch.S
    src/fault_handlers.c
    src/fault_dispatch.c
    src/idle.c
    src/privilege.c
    src/mpu.c
    src/mpu_hw.c
    src/systick.c
    src/yield.c
    src/timing.c
    src/svc.c
    src/svc.S
    src/start_scheduler.c
    src/queue_port.c
    src/mutex.c
    src/semaphore.c
    src/buffer.c
)

target_include_directories(maxrtos_arch_cortex_m7
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_link_libraries(maxrtos_arch_cortex_m7 PUBLIC
    maxrtos_kernel
)
