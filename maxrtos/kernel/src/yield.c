/**
 * @file yield.c
 * @brief Implementation of kernel-level voluntary process yield control logic.
 *
 * Implements architecture-independent logic for voluntary yield processing.
 * Handles invokes partition dispatch selection, and calculates context
 * switch flags.
 */

#include "maxrtos/kernel/yield.h"
#include "maxrtos/kernel/partition.h"

maxrtos_status_t maxrtos_kernel_yield(
    maxrtos_partition_table_t * table,
    maxrtos_partition_id_t partition_id,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id,
    bool * out_switch_needed )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( table != NULL ) &&
        ( out_next_id != NULL ) &&
        ( out_switch_needed != NULL ) )
    {
        maxrtos_process_id_t next_id;

        status = maxrtos_partition_dispatch(
            table,
            partition_id,
            &next_id );

        if( status == MAXRTOS_OK )
        {
            *out_next_id = next_id;
            *out_switch_needed = ( next_id != current_id );
        }
    }

    return status;
}
