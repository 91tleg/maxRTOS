/**
 * @file dispatch.h
 * @brief Process dispatch and execution-state transition interface.
 *
 * Provides the kernel-level operation that converts a scheduler
 * decision into a process state transition.
 *
 * The dispatch layer operates above the scheduler and process
 * management layers. It does not perform an architecture-specific
 * context switch.
 */

#ifndef MAXRTOS_KERNEL_DISPATCH_H
#define MAXRTOS_KERNEL_DISPATCH_H

#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/scheduler.h"
#include "maxrtos/status.h"

/**
 * @brief Select and dispatch the next READY process.
 *
 * Obtains the highest-priority process selected by the scheduler.
 * If the selected process differs from the currently running process,
 * the scheduler ready queues are updated before either process state
 * is changed.
 *
 * On successful completion, the previously running process is placed
 * in the READY state and the selected process is placed in the RUNNING
 * state. If the scheduler operation fails, the current process remains
 * RUNNING and no process-state transition is performed.
 *
 * If current_id is MAXRTOS_INVALID_PROCESS_ID, the call is treated as
 * the initial dispatch and no current process state is modified.
 *
 * @param[in] ctx 
 *    the scheduler context (one partition's ready queues) to
 *    dispatch within. Must not be NULL.
 *
 * @param[in] current_id
 *     Process ID that is currently RUNNING. May be
 *     MAXRTOS_INVALID_PROCESS_ID when no process is currently running.
 *
 * @param[out] out_next_id
 *     Receives the process ID selected for execution. Must not be NULL.
 *
 * @return
 *     MAXRTOS_OK if a process was successfully selected and the
 *     required state transition was completed.
 *     MAXRTOS_ERR_INVALID_ARG if out_next_id is NULL.
 *     MAXRTOS_ERR_QUEUE_EMPTY if no READY process is available.
 *     MAXRTOS_ERR_INVALID_ID if a referenced process ID is invalid.
 *     MAXRTOS_ERR_INVALID_STATE if a referenced process is not in the
 *     expected state.
 *     MAXRTOS_ERR_QUEUE_FULL if the current process cannot be returned
 *     to its ready queue.
 */
maxrtos_status_t maxrtos_kernel_dispatch(
    maxrtos_scheduler_context_t * ctx,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id );

#endif /* MAXRTOS_KERNEL_DISPATCH_H */
