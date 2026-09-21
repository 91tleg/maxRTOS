add_library(maxrtos_kernel STATIC
    src/scheduler.c
    src/process.c
    src/frame.c
    src/dispatch.c
    src/partition.c
    src/queue_port.c
    src/ipc_block.c
    src/fault_recovery.c
    src/health_monitor.c
    src/tick.c
    src/yield.c
    src/start_scheduler.c
    src/internal/fifo.c
    src/waitlist.c
)

target_include_directories(maxrtos_kernel
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/internal
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/internal
)
