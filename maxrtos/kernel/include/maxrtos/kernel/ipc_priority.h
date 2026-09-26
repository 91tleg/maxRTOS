/**
 * @file ipc_priority.h
 * @brief Priority-ordered waiter selection for blocking IPC primitives.
 *
 * The generic wait list is FIFO. Primitives that serve waiters by
 * priority (mutexes, semaphores) use this to pick the next one.
 */

#ifndef MAXRTOS_KERNEL_IPC_PRIORITY_H
#define MAXRTOS_KERNEL_IPC_PRIORITY_H

#include "maxrtos/status.h"
#include "maxrtos/types.h"
#include "maxrtos/kernel/waitlist.h"
#include "maxrtos/kernel/ipc_operation.h"

/**
 * @brief Remove the best waiter from a wait list, with its IPC operation.
 *
 * The best waiter is the one with the highest priority (lowest priority
 * number); among equals, the one that has waited longest. The process
 * leaves the list and its blocked state is cleared, but it stays
 * BLOCKED until the caller resolves it with maxrtos_ipc_resolve().
 *
 * @param[in,out] waitlist
 *     Wait list to take from.
 *
 * @param[out] out_id
 *     Receives the chosen process.
 *
 * @param[out] out_op
 *     Receives a snapshot of the chosen process's IPC operation.
 *
 * @return
 *     MAXRTOS_OK if a waiter was removed.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is NULL.
 *     MAXRTOS_ERR_QUEUE_EMPTY if nobody is waiting.
 */
maxrtos_status_t maxrtos_ipc_take_best_waiter_op(
    maxrtos_waitlist_t * waitlist,
    maxrtos_process_id_t * out_id,
    maxrtos_ipc_operation_t * out_op );

/**
 * @brief Remove the best waiter from a wait list.
 *
 * Equivalent to maxrtos_ipc_take_best_waiter_op() for callers that do not
 * need the woken process's IPC operation.
 *
 * @param[in,out] waitlist
 *     Wait list to take from.
 *
 * @param[out] out_id
 *     Receives the chosen process.
 *
 * @return
 *     MAXRTOS_OK if a waiter was removed.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is NULL.
 *     MAXRTOS_ERR_QUEUE_EMPTY if nobody is waiting.
 */
maxrtos_status_t maxrtos_ipc_take_best_waiter(
    maxrtos_waitlist_t * waitlist,
    maxrtos_process_id_t * out_id );

#endif /* MAXRTOS_KERNEL_IPC_PRIORITY_H */
