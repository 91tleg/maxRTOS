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

/**
 * @brief Transition the currently-running process to BLOCKED and
 *        dispatch a replacement, WITHOUT requeuing the blocking
 *        process into the ready queue.
 *
 * This is distinct from maxrtos_kernel_dispatch(), which always
 * transitions "current" back to READY and re-adds it to the
 * scheduler -- that is correct for cooperative yield and tick-driven
 * preemption, where the outgoing process remains eligible to run
 * again immediately. It is NOT correct for blocking on a contended
 * resource (e.g. a mutex): a blocked process must not be selected by
 * maxrtos_scheduler_next() again until something else (e.g. a mutex
 * unlock) explicitly makes it READY.
 *
 * @param[in,out] ctx
 *     Scheduler context for the blocking process's partition.
 *
 * @param[in] blocking_id
 *     Process to block. Must currently be in the RUNNING state --
 *     this function performs the RUNNING -> BLOCKED transition
 *     itself; the caller must NOT have already changed its state
 *     before calling (mirrors maxrtos_kernel_dispatch()'s existing
 *     convention for the outgoing "current" process).
 *
 * @param[out] out_next_id
 *     Process selected to run in blocking_id's place. Must not be
 *     NULL.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if ctx or out_next_id is NULL, or
 *     blocking_id is MAXRTOS_INVALID_PROCESS_ID.
 *     MAXRTOS_ERR_INVALID_ID if blocking_id does not refer to an
 *     allocated process.
 *     MAXRTOS_ERR_INVALID_STATE if blocking_id's process is not
 *     currently RUNNING.
 *     MAXRTOS_ERR_QUEUE_EMPTY if no other process in this partition
 *     is READY. blocking_id's process has NOT been transitioned to
 *     BLOCKED in this case (there is nothing to run in its place) --
 *     the caller must not proceed as though blocking succeeded.
 */
maxrtos_status_t maxrtos_kernel_block_and_dispatch(
    maxrtos_scheduler_context_t * ctx,
    maxrtos_process_id_t blocking_id,
    maxrtos_process_id_t * out_next_id );

#endif /* MAXRTOS_KERNEL_DISPATCH_H */
