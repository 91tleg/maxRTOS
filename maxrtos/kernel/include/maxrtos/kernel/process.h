/**
 * @file process.h
 * @brief Process control block and process lifecycle management.
 *
 * Defines process states, the process control block, and the
 * process-management interface.
 *
 * This interface does not perform scheduler operations unless
 * explicitly stated by the individual function contract.
 */

#ifndef MAXRTOS_KERNEL_PROCESS_H
#define MAXRTOS_KERNEL_PROCESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "maxrtos/status.h"
#include "maxrtos/types.h"
#include "maxrtos/kernel/ipc_operation.h"
#include "maxrtos/kernel/waitlist.h"

/**
 * @brief Process execution state.
 */
typedef enum
{
    MAXRTOS_PROCESS_STATE_UNUSED = 0U, /* Process slot is available. */
    MAXRTOS_PROCESS_STATE_READY,       /* Process is eligible for execution. */
    MAXRTOS_PROCESS_STATE_RUNNING,     /* Process is currently executing. */
    MAXRTOS_PROCESS_STATE_BLOCKED,     /* Process is waiting for an event or resource. */
    MAXRTOS_PROCESS_STATE_SUSPENDED    /* Process is administratively stopped. */
} maxrtos_process_state_t;

/**
 * @brief Process entry function type.
 *
 * @param[in] arg
 *     Application-defined process argument.
 */
typedef void ( * maxrtos_process_entry_t ) ( void * arg );

/**
 * @brief Process control block.
 *
 * Contains the execution context, scheduling attributes, process
 * state, and blocking-operation information for one process.
 *
 * @warning
 * stack_pointer shall remain the first member of this structure.
 * The architecture-specific context-switch implementation accesses
 * this member at offset zero.
 */
typedef struct process_control_block_s
{
    void * stack_pointer;
    uint8_t * stack_base;
    size_t stack_size;

    maxrtos_process_id_t id;
    maxrtos_partition_id_t partition_id;
    uint8_t priority;
    maxrtos_process_state_t state;

    /**
     * @brief Absolute tick at which the current timed wait expires.
     *
     * MAXRTOS_TICK_NONE indicates that no timed wait is active.
     * The value is not meaningful when it is MAXRTOS_TICK_NONE.
     */
    maxrtos_tick_t wake_tick;

    /**
     * @brief Wait list containing this process while it is blocked.
     *
     * NULL indicates that the process is not currently associated
     * with a wait list.
     *
     * The owning IPC primitive shall set this field when the process
     * is blocked and clear it when the process is removed from the
     * wait list.
     */
    maxrtos_waitlist_t * waitlist;

    /**
     * @brief IPC operation associated with the current blocked state.
     *
     * kind shall be MAXRTOS_IPC_OP_NONE when no IPC operation is
     * pending. The payload is valid only for the operation identified
     * by kind.
     *
     * The IPC primitive that owns the associated wait list is
     * responsible for interpreting the active payload member.
     */
    maxrtos_ipc_operation_t ipc_operation;

    /**
     * @brief Result of the most recently resolved blocking IPC operation.
     *
     * Set before the process transitions from BLOCKED to READY.
     * The process-resume path shall consume this value when preparing
     * the process's system-call return value.
     *
     * The value is not meaningful unless the process has been made
     * READY by the IPC resolution path.
     */
    maxrtos_status_t ipc_result;

    /**
     * @brief True while ipc_result has not yet been delivered to the
     *        process.
     *
     * Set by the kernel when it completes a blocked operation. The
     * architecture resume path shall write ipc_result into the process's
     * system-call return value the next time the process is restored,
     * and then clear this flag.
     */
    bool ipc_result_pending;

    /**
     * @brief Periodic release and deadline supervision (ARINC 653 PERIOD
     *        and TIME_CAPACITY). See maxrtos/kernel/timing.h.
     *
     * period is the release interval in ticks; 0 makes the process
     * aperiodic. time_capacity is the deadline in ticks, measured from the
     * current release: the process must complete the release (call
     * maxrtos_periodic_wait()) by release_time + time_capacity, in wall
     * clock time, however much CPU it received. 0 means no deadline.
     *
     * deadline_time is the absolute tick at which the armed deadline
     * expires, or MAXRTOS_TICK_NONE when none is armed. periodic_waiting
     * is true while the process is blocked until its next release.
     * deadline_misses counts misses since the process was created.
     */
    maxrtos_tick_t period;
    maxrtos_tick_t time_capacity;
    maxrtos_tick_t release_time;
    maxrtos_tick_t deadline_time;
    bool periodic_waiting;
    uint32_t deadline_misses;

    maxrtos_process_entry_t entry;
    void * entry_arg;
} maxrtos_process_control_block_t;

/**
 * @brief Initialize the process pool.
 *
 * Marks all process slots as UNUSED.
 *
 * This function shall be called before any other process-management
 * operation.
 */
void maxrtos_process_pool_init( void );

/**
 * @brief Create and initialize a process.
 *
 * @param[in] stack_base
 *     Caller-owned statically allocated stack memory.
 *
 * @param[in] stack_size
 *     Size of the stack in bytes.
 *
 * @param[in] partition_id
 *     Partition assigned to the process. The process runs with the
 *     privilege level of its partition, which is fixed by the
 *     configuration (application partitions are unprivileged); a
 *     process cannot choose its own.
 *
 * @param[in] priority
 *     Process priority. Valid values are 0 through
 *     MAXRTOS_MAX_PRIORITY, where 0 is the highest priority.
 *
 * @param[in] entry
 *     Process entry function. Shall not be NULL.
 *
 * @param[in] entry_arg
 *     Argument passed to the process entry function.
 *
 * @param[out] out_id
 *     Location receiving the allocated process identifier.
 *     Shall not be NULL.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if a required argument is invalid.
 *     MAXRTOS_ERR_POOL_FULL if no process slot is available.
 *
 * @post
 *     The process is initialized in the READY state.
 * @post
 *     The process is not added to the scheduler ready queue.
 */
maxrtos_status_t maxrtos_process_create(
    uint8_t * stack_base,
    size_t stack_size,
    maxrtos_partition_id_t partition_id,
    uint8_t priority,
    maxrtos_process_entry_t entry,
    void * entry_arg,
    maxrtos_process_id_t * out_id );

/**
 * @brief Retrieve a process control block by process identifier.
 *
 * @param[in] id
 *     Process identifier to look up.
 *
 * @return
 *     Pointer to the process control block if id identifies an
 *     allocated process; otherwise NULL.
 *
 * The returned pointer refers to storage owned by the process pool.
 * The caller shall not release or replace the returned storage.
 */
maxrtos_process_control_block_t * maxrtos_process_get(
    maxrtos_process_id_t id );

/**
 * @brief Set the state of a process.
 *
 * @param[in] id
 *     Process identifier of the process to update.
 *
 * @param[in] new_state
 *     State to assign to the process.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ID if id is invalid or identifies an
 *     UNUSED process slot.
 *
 * This function modifies the process state only. It does not modify
 * scheduler membership, request rescheduling, or perform a context
 * switch.
 */
maxrtos_status_t maxrtos_process_set_state(
    maxrtos_process_id_t id,
    maxrtos_process_state_t new_state );

#endif /* MAXRTOS_KERNEL_PROCESS_H */
