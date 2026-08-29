/**
 * @file process.c
 * @brief Process control block management and process lifecycle operations.
 *
 * Implements static allocation, state management, and lookup operations for
 * process control blocks.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "maxrtos/config.h"
#include "maxrtos/kernel/process.h"

static maxrtos_process_control_block_t
    s_process_pool[ MAXRTOS_MAX_PROCESSES ];

static maxrtos_status_t slot_index_from_id(
    maxrtos_process_id_t id,
    size_t * out_index )
{
    maxrtos_status_t status;

    if( ( id == MAXRTOS_INVALID_PROCESS_ID ) ||
        ( id >= ( maxrtos_process_id_t ) MAXRTOS_MAX_PROCESSES ) ||
        ( out_index == NULL ) )
    {
        status = MAXRTOS_ERR_INVALID_ID;
    }
    else
    {
        *out_index = ( size_t ) id;
        status = MAXRTOS_OK;
    }

    return status;
}

void maxrtos_process_pool_init( void )
{
    size_t i;

    for( i = 0U; i < MAXRTOS_MAX_PROCESSES; i++ )
    {
        s_process_pool[ i ].state = MAXRTOS_PROCESS_STATE_UNUSED;
        s_process_pool[ i ].id = MAXRTOS_INVALID_PROCESS_ID;
        s_process_pool[ i ].stack_pointer = NULL;
        s_process_pool[ i ].stack_base = NULL;
        s_process_pool[ i ].stack_size = 0U;
        s_process_pool[ i ].priority = 0U;
        s_process_pool[ i ].entry = NULL;
        s_process_pool[ i ].entry_arg = NULL;
        s_process_pool[ i ].partition_id = MAXRTOS_INVALID_PARTITION_ID;
    }
}

maxrtos_status_t maxrtos_process_create(
    uint8_t * stack_base,
    size_t stack_size,
    maxrtos_partition_id_t partition_id,
    uint8_t priority,
    maxrtos_process_entry_t entry,
    void * entry_arg,
    maxrtos_process_id_t * out_id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_POOL_FULL;

    if( ( stack_base == NULL ) ||
        ( stack_size == 0U ) ||
        ( partition_id >= MAXRTOS_MAX_PARTITIONS ) ||
        ( priority > MAXRTOS_MAX_PRIORITY ) ||
        ( entry == NULL ) ||
        ( out_id == NULL ) )
    {
        status = MAXRTOS_ERR_INVALID_ARG;
    }
    else
    {
        size_t i;

        for( i = 0U;
             ( i < MAXRTOS_MAX_PROCESSES ) && ( status != MAXRTOS_OK );
             i++ )
        {
            if( s_process_pool[ i ].state == MAXRTOS_PROCESS_STATE_UNUSED )
            {
                maxrtos_process_control_block_t * pcb;

                pcb = &s_process_pool[ i ];

                pcb->id = ( maxrtos_process_id_t ) i;
                pcb->partition_id = partition_id;
                pcb->stack_base = stack_base;
                pcb->stack_size = stack_size;
                pcb->stack_pointer = &stack_base[ stack_size ];
                pcb->priority = priority;
                pcb->entry = entry;
                pcb->entry_arg = entry_arg;
                pcb->state = MAXRTOS_PROCESS_STATE_READY;

                *out_id = pcb->id;

                status = MAXRTOS_OK;
            }
        }
    }

    return status;
}

maxrtos_process_control_block_t *
maxrtos_process_get( maxrtos_process_id_t id )
{
    maxrtos_process_control_block_t * result;
    size_t index;
    maxrtos_status_t status;

    result = NULL;
    index = 0U;

    status = slot_index_from_id( id, &index );

    if( ( status == MAXRTOS_OK ) &&
        ( s_process_pool[ index ].state !=
          MAXRTOS_PROCESS_STATE_UNUSED ) )
    {
        result = &s_process_pool[ index ];
    }

    return result;
}

maxrtos_status_t maxrtos_process_set_state(
    maxrtos_process_id_t id,
    maxrtos_process_state_t new_state )
{
    maxrtos_status_t status;
    size_t index;

    index = 0U;

    status = slot_index_from_id( id, &index );

    if( ( status == MAXRTOS_OK ) &&
        ( s_process_pool[ index ].state !=
          MAXRTOS_PROCESS_STATE_UNUSED ) )
    {
        s_process_pool[ index ].state = new_state;
        status = MAXRTOS_OK;
    }
    else
    {
        status = MAXRTOS_ERR_INVALID_ID;
    }

    return status;
}
