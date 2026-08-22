/**
 * @file partition.h
 * @brief Partition scheduler state and dispatch interface.
 *
 * Provides independent scheduler state for each configured partition.
 * Each process is associated with exactly one partition through its
 * process control block.
 */

#ifndef MAXRTOS_KERNEL_PARTITION_H
#define MAXRTOS_KERNEL_PARTITION_H

#include <stdbool.h>

#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/scheduler.h"
#include "maxrtos/status.h"
#include "maxrtos/config.h"

/**
 * @brief Partition scheduler state.
 *
 * Maintains the scheduler context, current process, and halt state
 * for each configured partition.
 *
 * Partition state is indexed by partition ID. A process shall only
 * be added to the scheduler context associated with the partition ID
 * stored in its process control block.
 *
 * @field scheduler_ctx
 *     Scheduler context for each configured partition.
 *
 * @field current_id
 *     ID of the process that was most recently RUNNING in each
 *     partition.
 *     MAXRTOS_INVALID_PROCESS_ID indicates that no current process
 *     is established for the partition. This value is valid during
 *     initial dispatch and after a current process has been cleared
 *     for re-dispatch.
 *
 * @field halted
 *     Indicates whether dispatch is permitted for each partition.
 *     false indicates that dispatch is permitted.
 *     true indicates that dispatch shall be rejected.
 *     A partition is placed in the halted state by the fault recovery
 *     mechanism in response to MAXRTOS_HM_ACTION_HALT_PARTITION.
 */
typedef struct
{
    maxrtos_scheduler_context_t scheduler_ctx[ MAXRTOS_MAX_PARTITIONS ];
    maxrtos_process_id_t current_id[ MAXRTOS_MAX_PARTITIONS ];
    bool halted[ MAXRTOS_MAX_PARTITIONS ];
} maxrtos_partition_table_t;

/**
 * @brief Initialize the partition scheduler state.
 *
 * Initializes all configured partition scheduler contexts. Each
 * partition is initialized with no current process and with dispatch
 * enabled.
 *
 * @param[out] table
 *     Partition table to initialize.
 *
 * @return
 *     MAXRTOS_OK if all partition scheduler contexts are initialized.
 *     MAXRTOS_ERR_INVALID_ARG if table is NULL.
 *     MAXRTOS_ERR_INVALID_STATE if a scheduler context cannot be
 *     initialized.
 *
 * @post
 *     For each configured partition p:
 *     - scheduler_ctx[p] is initialized.
 *     - current_id[p] == MAXRTOS_INVALID_PROCESS_ID.
 *     - halted[p] == false.
 */
maxrtos_status_t maxrtos_partition_table_init(
    maxrtos_partition_table_t * table );

/**
 * @brief Add a READY process to its partition scheduler.
 *
 * Obtains the process control block identified by id and adds the
 * process to the scheduler context corresponding to its partition ID.
 *
 * The process shall be in the READY state before this function is
 * called.
 *
 * @param[in,out] table
 *     Partition table to modify.
 *
 * @param[in] id
 *     ID of the process to add.
 *
 * @return
 *     MAXRTOS_OK if the process is added successfully.
 *     MAXRTOS_ERR_INVALID_ARG if table is NULL or the process has an
 *     invalid partition ID.
 *     MAXRTOS_ERR_INVALID_ID if id does not identify an allocated
 *     process.
 *     MAXRTOS_ERR_INVALID_STATE if the process is not READY.
 *     MAXRTOS_ERR_QUEUE_FULL if the partition scheduler queue is full.
 *
 * @post
 *     If MAXRTOS_OK is returned, the process is present in the
 *     scheduler context associated with its partition ID.
 *
 * @post
 *     If an error is returned, the partition scheduler state is not
 *     modified by this function.
 */
maxrtos_status_t maxrtos_partition_add_process(
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t id );

/**
 * @brief Select the next process for execution within a partition.
 *
 * Dispatches the next eligible READY process from the specified
 * partition scheduler.
 *
 * If current_id[partition_id] is
 * MAXRTOS_INVALID_PROCESS_ID, the partition is treated as having no
 * current process and selection begins directly from its READY
 * processes.
 *
 * If a valid current process exists, it shall be in the RUNNING state
 * before dispatch is performed.
 *
 * A halted partition shall not be dispatched.
 *
 * @param[in,out] table
 *     Partition table to update.
 *
 * @param[in] partition_id
 *     Partition to dispatch.
 *
 * @param[out] out_next_id
 *     Receives the ID of the selected process.
 *
 * @return
 *     MAXRTOS_OK if a process is selected successfully.
 *     MAXRTOS_ERR_INVALID_ARG if table or out_next_id is NULL, or
 *     partition_id is outside the configured partition range.
 *     MAXRTOS_ERR_INVALID_STATE if the current process exists but is
 *     not in the RUNNING state.
 *     MAXRTOS_ERR_QUEUE_EMPTY if no READY process is available.
 *     MAXRTOS_ERR_PARTITION_HALTED if the partition is halted.
 *
 * @post
 *     If MAXRTOS_OK is returned, out_next_id contains the ID of the
 *     selected process and current_id[partition_id] contains the same
 *     process ID.
 *
 * @post
 *     If MAXRTOS_ERR_PARTITION_HALTED is returned, partition scheduler
 *     state is unchanged.
 */
maxrtos_status_t maxrtos_partition_dispatch(
    maxrtos_partition_table_t * table,
    maxrtos_partition_id_t partition_id,
    maxrtos_process_id_t * out_next_id );

#endif /* MAXRTOS_KERNEL_PARTITION_H */
