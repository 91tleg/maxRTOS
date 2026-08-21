/**
 * @file scheduler.h
 * @brief Fixed-priority process scheduler interface.
 *
 * Scheduling uses strict fixed-priority ordering. Lower numeric
 * priority values represent higher priority. Processes with equal
 * priority are selected in FIFO order based on ready-queue insertion
 * order.
 *
 * The scheduler does not perform context switching or directly modify
 * process state. It determines which READY process shall be selected
 * for execution and maintains ready-queue membership.
 */

#ifndef MAXRTOS_KERNEL_SCHEDULER_H
#define MAXRTOS_KERNEL_SCHEDULER_H

#include <stddef.h>

#include "maxrtos/kernel/process.h"
#include "maxrtos/status.h"

/**
 * Initialize scheduler state.
 *
 * Clears all scheduler ready queues. Process control blocks are not
 * modified.
 *
 * This function shall be called after maxrtos_process_pool_init()
 * and before any other scheduler operation.
 */
void maxrtos_scheduler_init( void );

/**
 * @brief Add a process to the scheduler.
 *
 * Adds the specified process ID to the ready queue corresponding to
 * the process priority.
 *
 * @param[in] id
 *     Process identifier.
 *
 * @return
 *     MAXRTOS_OK if the process was added successfully.
 *     MAXRTOS_ERR_INVALID_ID if id does not identify an allocated
 *     process.
 *     MAXRTOS_ERR_INVALID_STATE if the process is not in the READY
 *     state.
 *     MAXRTOS_ERR_QUEUE_FULL if the ready queue is full.
 *
 * The process shall be in the READY state. This function does not
 * modify the process state.
 */
maxrtos_status_t maxrtos_scheduler_add_process(
    maxrtos_process_id_t id );

/**
 * @brief Remove a process from the scheduler.
 *
 * Removes the specified process ID from its priority ready queue.
 *
 * @param[in] id
 *     Process identifier.
 *
 * @return
 *     MAXRTOS_OK if the process was removed successfully.
 *     MAXRTOS_ERR_INVALID_ID if id does not identify an allocated
 *     process.
 *     MAXRTOS_ERR_INVALID_STATE if the process is not in the READY
 *     state.
 *     MAXRTOS_ERR_NOT_FOUND if the process is not present in the
 *     ready queue.
 *
 * The process shall be in the READY state when this function is
 * called. This function does not modify the process state.
 */
maxrtos_status_t maxrtos_scheduler_remove_process(
    maxrtos_process_id_t id );

/**
 * @brief Determine the next process to execute.
 *
 * Selects the READY process with the highest priority. If multiple
 * READY processes have the same priority, the process at the front
 * of the corresponding ready queue is selected.
 *
 * @param[out] out_id
 *     Output location for the selected process identifier.
 *
 * @return
 *     MAXRTOS_OK if a READY process was selected.
 *     MAXRTOS_ERR_INVALID_ARG if out_id is NULL.
 *     MAXRTOS_ERR_QUEUE_EMPTY if no READY process is available.
 *
 * This function does not modify ready-queue membership, process
 * state, or processor execution state.
 */
maxrtos_status_t maxrtos_scheduler_next(
    maxrtos_process_id_t * out_id );

/**
 * @brief Return the number of schedulable processes.
 *
 * @return
 *     Number of process IDs currently present in the scheduler
 *     ready queues.
 *
 * This function does not modify scheduler state or process state.
 */
size_t maxrtos_scheduler_process_count( void );

#endif /* MAXRTOS_KERNEL_SCHEDULER_H */
