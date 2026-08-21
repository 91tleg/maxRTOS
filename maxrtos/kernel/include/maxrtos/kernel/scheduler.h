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

#include "maxrtos/status.h"
#include "maxrtos/config.h"
#include "maxrtos/kernel/process.h"

/**
 * @brief FIFO ready queue for a single priority level.
 *
 * Stores process IDs currently eligible for execution at the
 * associated priority.
 */
typedef struct
{
    maxrtos_process_id_t items[ MAXRTOS_MAX_READY_PER_PRIORITY ];
    size_t head;
    size_t count;
} maxrtos_ready_queue_t;

/**
 * @brief Scheduler state for one scheduling domain.
 *
 * Contains one FIFO ready queue for each supported process priority.
 *
 * The scheduler does not own process control blocks. Process storage
 * is maintained by the process-management subsystem.
 */
typedef struct
{
    maxrtos_ready_queue_t queues[ MAXRTOS_MAX_PRIORITY + 1U ];
} maxrtos_scheduler_context_t;

/**
 * @brief Initialize a scheduler context.
 *
 * Clears all ready queues in the specified scheduler context.
 * Process control blocks are not modified.
 *
 * @param[out] ctx Scheduler context to initialize.
 *
 * @return
 *     MAXRTOS_OK if the context was initialized successfully.
 *     MAXRTOS_ERR_INVALID_ARG if ctx is NULL.
 *
 * @pre The process pool shall have been initialized.
 */
maxrtos_status_t maxrtos_scheduler_init(
    maxrtos_scheduler_context_t * ctx );

/**
 * @brief Add a READY process to a scheduler context.
 *
 * Adds the specified process ID to the ready queue corresponding to
 * the process priority.
 *
 * @param[in,out] ctx
 *     Scheduler context.
 * @param[in] id
 *     Process identifier.
 *
 * @return
 *     MAXRTOS_OK if the process was added successfully.
 *     MAXRTOS_ERR_INVALID_ARG if ctx is NULL.
 *     MAXRTOS_ERR_INVALID_ID if id does not identify an allocated
 *     process.
 *     MAXRTOS_ERR_INVALID_STATE if the process is not READY.
 *     MAXRTOS_ERR_QUEUE_FULL if the ready queue is full.
 *
 * The process shall be in the READY state. This function does not
 * modify the process state.
 */
maxrtos_status_t maxrtos_scheduler_add_process(
    maxrtos_scheduler_context_t * ctx,
    maxrtos_process_id_t id );

/**
 * @brief Remove a process from a scheduler context.
 *
 * Removes the specified process ID from its priority ready queue.
 *
 * @param[in,out] ctx
 *     Scheduler context.
 * @param[in] id
 *     Process identifier.
 *
 * @return
 *     MAXRTOS_OK if the process was removed successfully.
 *     MAXRTOS_ERR_INVALID_ARG if ctx is NULL.
 *     MAXRTOS_ERR_INVALID_ID if id does not identify an allocated
 *     process.
 *     MAXRTOS_ERR_INVALID_STATE if the process is not READY.
 *     MAXRTOS_ERR_NOT_FOUND if the process is not present in the
 *     ready queue.
 *
 * The process shall be in the READY state when this function is
 * called. This function does not modify the process state.
 */
maxrtos_status_t maxrtos_scheduler_remove_process(
    maxrtos_scheduler_context_t * ctx,
    maxrtos_process_id_t id );

/**
 * @brief Select the highest-priority READY process.
 *
 * Selects the READY process with the highest priority. If multiple
 * READY processes have the same priority, the process at the front
 * of the corresponding ready queue is selected.
 *
 * @param[in] ctx
 *     Scheduler context.
 * @param[out] out_id
 *     Output location for the selected process identifier.
 *
 * @return
 *     MAXRTOS_OK if a READY process was selected.
 *     MAXRTOS_ERR_INVALID_ARG if ctx or out_id is NULL.
 *     MAXRTOS_ERR_QUEUE_EMPTY if no READY process is available.
 *
 * This function does not modify ready-queue membership, process
 * state, or processor execution state.
 */
maxrtos_status_t maxrtos_scheduler_next(
    maxrtos_scheduler_context_t const * ctx,
    maxrtos_process_id_t * out_id );

/**
 * @brief Return the number of schedulable processes.
 *
 * @param[in] ctx
 *     Scheduler context.
 *
 * @return
 *     Number of process IDs currently present in the scheduler
 *     ready queues.
 *
 * This function does not modify scheduler state or process state.
 */
size_t maxrtos_scheduler_process_count(
    maxrtos_scheduler_context_t const * ctx );

#endif /* MAXRTOS_KERNEL_SCHEDULER_H */
