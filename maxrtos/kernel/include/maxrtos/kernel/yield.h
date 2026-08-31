/**
 * @file yield.h
 * @brief Interface for kernel-level process yield operations. 
 */

#include <stdbool.h>

#include "maxrtos/status.h"
#include "maxrtos/kernel/partition.h"

/**
 * @brief Processes a voluntary yield request for the active partition.
 *
 * @param[in] table
 *     Pointer to the initialized partition table instance.
 *
 * @param[in] partition_id
 *     Identifier of the requesting process's partition.
 *
 * @param[in] current_id
 *     Identifier of the currently executing process.
 *
 * @param[out] out_next_id
 *     Pointer to store the identifier of the scheduled process.
 *
 * @param[out] out_switch_needed
 *     Pointer to store the boolean context-switch requirement flag.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if required pointer arguments are NULL.
 *     MAXRTOS_ERR_* Partition dispatch error returned by maxrtos_partition_dispatch().
 */
maxrtos_status_t maxrtos_kernel_yield(
    maxrtos_partition_table_t * table,
    maxrtos_partition_id_t partition_id,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id,
    bool * out_switch_needed );
