/**
 * @file ipc_block.h
 * @brief Generic IPC blocking, wake, timeout, and resolution primitives.
 *
 * Provides the shared blocking and wakeup mechanism used by IPC
 * primitives such as queue ports, mutexes, semaphores, and sampling
 * ports.
 *
 * This module is architecture-independent. It does not identify the
 * currently executing process itself and does not perform a context
 * switch. The caller supplies the current process ID and the scheduler
 * context required to perform the state transition.
 */

#ifndef MAXRTOS_KERNEL_IPC_BLOCK_H
#define MAXRTOS_KERNEL_IPC_BLOCK_H

#include "maxrtos/status.h"
#include "maxrtos/types.h"
#include "maxrtos/kernel/waitlist.h"
#include "maxrtos/kernel/ipc_operation.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/scheduler.h"
#include "maxrtos/kernel/process.h"

/**
 * @brief Block a process on an IPC wait list.
 *
 * Records the IPC operation in the process control block, removes the
 * process from the scheduler's ready queue, marks it BLOCKED, and adds
 * it to waitlist.
 *
 * Does not perform a context switch. On success, out_next_id contains
 * the process selected to run next.
 *
 * The process is blocked and its replacement dispatched through
 * maxrtos_partition_block_and_dispatch(), so the partition's current
 * process stays consistent with the replacement.
 *
 * @param[in,out] table
 *     Partition table. The process's own partition is used.
 *
 * @param[in] current_id
 *     ID of the process being blocked.
 *
 * @param[in] waitlist
 *     Wait list on which the process shall block.
 *
 * @param[in] wake_tick
 *     Absolute timeout tick, or MAXRTOS_TICK_NONE for no timeout.
 *
 * @param[in] op
 *     IPC operation to record in the process control block.
 *
 * @param[out] out_next_id
 *     ID of the next process selected by the scheduler.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid.
 *     MAXRTOS_ERR_INVALID_ID if current_id is invalid.
 *     Otherwise, a status returned by the scheduler.
 */
maxrtos_status_t maxrtos_ipc_block_current(
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id,
    maxrtos_waitlist_t * waitlist,
    maxrtos_tick_t wake_tick,
    maxrtos_ipc_operation_t const * op,
    maxrtos_process_id_t * out_next_id );

/**
 * @brief Wake the first process waiting on an IPC wait list.
 *
 * Removes the first process from waitlist and snapshots its IPC
 * operation into out_op. The process remains BLOCKED until the caller
 * resolves it with maxrtos_ipc_resolve().
 *
 * @param[in] waitlist
 *     Wait list to pop.
 *
 * @param[out] out_id
 *     ID of the process that was woken.
 *
 * @param[out] out_op
 *     Snapshot of the process's IPC operation.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid.
 *     MAXRTOS_ERR_QUEUE_EMPTY if the wait list is empty.
 *     MAXRTOS_ERR_INVALID_ID if the process no longer exists.
 */
maxrtos_status_t maxrtos_ipc_wake_one(
    maxrtos_waitlist_t * waitlist,
    maxrtos_process_id_t * out_id,
    maxrtos_ipc_operation_t * out_op );

/**
 * @brief Remove a timed-out process from an IPC wait list.
 *
 * Removes the specified process from waitlist and snapshots its IPC
 * operation into out_op. The process remains BLOCKED until the caller
 * resolves it with maxrtos_ipc_resolve().
 *
 * @param[in] waitlist
 *     Wait list containing the timed-out process.
 *
 * @param[in] id
 *     ID of the timed-out process.
 *
 * @param[out] out_op
 *     Snapshot of the process's IPC operation.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid.
 *     MAXRTOS_ERR_NOT_FOUND if the process is not in waitlist.
 *     MAXRTOS_ERR_INVALID_ID if the process no longer exists.
 */
maxrtos_status_t maxrtos_ipc_timeout_current(
    maxrtos_waitlist_t * waitlist,
    maxrtos_process_id_t id,
    maxrtos_ipc_operation_t * out_op );

/**
 * @brief Complete a blocked operation and make its process READY.
 *
 * The process must already have been removed from its wait list (by
 * maxrtos_ipc_wake_one() or maxrtos_ipc_timeout_current()). Records
 * result for delivery as the process's system-call return value and
 * returns the process to the ready queue of its OWN partition, which is
 * not necessarily the partition of the caller.
 *
 * @param[in,out] table
 *     Partition table.
 *
 * @param[in] id
 *     Process to resume. Must be BLOCKED.
 *
 * @param[in] result
 *     Status the resumed operation returns to the process.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if table is NULL.
 *     MAXRTOS_ERR_INVALID_ID if id is invalid.
 *     MAXRTOS_ERR_INVALID_STATE if the process is not BLOCKED.
 *     MAXRTOS_ERR_QUEUE_FULL if the ready queue cannot take it.
 */
maxrtos_status_t maxrtos_ipc_resolve(
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t id,
    maxrtos_status_t result );

/**
 * @brief Time out every blocked process whose wait has expired.
 *
 * For each BLOCKED process with wake_tick <= now: removes it from its
 * wait list and resumes it with MAXRTOS_ERR_TIMEOUT.
 *
 * @param[in,out] table
 *     Partition table.
 *
 * @param[in] now
 *     Current kernel tick.
 *
 * @return
 *     Number of processes resumed.
 */
size_t maxrtos_ipc_expire_timeouts(
    maxrtos_partition_table_t * table,
    maxrtos_tick_t now );

#endif /* MAXRTOS_KERNEL_IPC_BLOCK_H */
