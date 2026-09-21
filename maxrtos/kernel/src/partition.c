/**
 * @file partition.c
 * @brief Implementation of partition scheduling and dispatch services.
 *
 * Provides initialization and management of the partition table,
 * per-partition scheduler contexts, current-process state, and
 * partition dispatch operations.
 *
 * Each partition has an independent scheduler context. Processes are
 * associated with the scheduler context identified by their partition
 * ID. Dispatch operations select the next process within the
 * specified partition and update the corresponding current-process
 * identifier.
 */

#include <stdbool.h>
#include <stddef.h>

#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/dispatch.h"
#include "maxrtos/kernel/process.h"

maxrtos_status_t maxrtos_partition_table_init(
    maxrtos_partition_table_t * table )
{
    maxrtos_status_t status;
 
    status = MAXRTOS_ERR_INVALID_ARG;

    if( table != NULL )
    {
        size_t partition_id;

        status = MAXRTOS_OK;

        for( partition_id = 0U;
            ( partition_id < MAXRTOS_MAX_PARTITIONS ) && ( status == MAXRTOS_OK );
            partition_id++ )
        {
            status = maxrtos_scheduler_init(
                &table->scheduler_ctx[ partition_id ] );
        }

        if( status == MAXRTOS_OK )
        {
            for( partition_id = 0U;
                 partition_id < MAXRTOS_MAX_PARTITIONS;
                 partition_id++ )
            {
                table->current_id[ partition_id ] =
                    MAXRTOS_INVALID_PROCESS_ID;
                table->halted[ partition_id ] = false;
            }
        }
    }

    return status;
}

maxrtos_status_t maxrtos_partition_add_process(
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t id )
{
    maxrtos_status_t status;
    maxrtos_process_control_block_t * pcb;

    status = MAXRTOS_ERR_INVALID_ARG;
    pcb = NULL;

    if( table != NULL )
    {
        pcb = maxrtos_process_get( id ); 

        if( pcb == NULL )
        {
            status = MAXRTOS_ERR_INVALID_ID;
        }
        else if( pcb->partition_id >= MAXRTOS_MAX_PARTITIONS )
        {
            status = MAXRTOS_ERR_INVALID_ARG;
        }
        else
        {
            status = maxrtos_scheduler_add_process(
                &table->scheduler_ctx[ pcb->partition_id ],
                id );
        }
    }

    return status;
}

maxrtos_status_t maxrtos_partition_dispatch(
    maxrtos_partition_table_t * table,
    maxrtos_partition_id_t partition_id,
    maxrtos_process_id_t * out_next_id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( table != NULL ) &&
        ( out_next_id != NULL ) &&
        ( partition_id < MAXRTOS_MAX_PARTITIONS ) )
    {
        if( table->halted[ partition_id ] )
        {
            status = MAXRTOS_ERR_PARTITION_HALTED;
        }
        else
        {
            status = maxrtos_kernel_dispatch(
                        &table->scheduler_ctx[ partition_id ],
                        table->current_id[ partition_id ],
                        out_next_id );
 
            if( status == MAXRTOS_OK )
            {
                table->current_id[ partition_id ] = *out_next_id;
            }
        }
    }

    return status;
}

maxrtos_status_t maxrtos_partition_ready_process(
    maxrtos_partition_table_t * table,
    maxrtos_partition_id_t partition_id,
    maxrtos_process_id_t id )
{
    maxrtos_status_t status;
 
    status = MAXRTOS_ERR_INVALID_ARG;
 
    if( ( table != NULL ) && ( partition_id < MAXRTOS_MAX_PARTITIONS ) )
    {
        status = maxrtos_process_set_state(
            id, MAXRTOS_PROCESS_STATE_READY );
 
        if( status == MAXRTOS_OK )
        {
            status = maxrtos_scheduler_add_process(
                &table->scheduler_ctx[ partition_id ], id );
        }
    }
 
    return status;
}

maxrtos_status_t maxrtos_partition_block_and_dispatch(
    maxrtos_partition_table_t * table,
    maxrtos_partition_id_t partition_id,
    maxrtos_process_id_t blocking_id,
    maxrtos_process_id_t * out_next_id )
{
    maxrtos_status_t status;
 
    status = MAXRTOS_ERR_INVALID_ARG;
 
    if( ( table != NULL ) &&
        ( out_next_id != NULL ) &&
        ( partition_id < MAXRTOS_MAX_PARTITIONS ) )
    {
        if( table->halted[ partition_id ] )
        {
            status = MAXRTOS_ERR_PARTITION_HALTED;
        }
        else
        {
            status = maxrtos_kernel_block_and_dispatch(
                        &table->scheduler_ctx[ partition_id ],
                        blocking_id,
                        out_next_id );
 
            if( status == MAXRTOS_OK )
            {
                table->current_id[ partition_id ] = *out_next_id;
            }
        }
    }
 
    return status;
}
