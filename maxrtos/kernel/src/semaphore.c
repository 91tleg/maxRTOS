/**
 * @file semaphore.c
 * @brief Kernel implementation of intra-partition counting semaphores.
 *
 * Owns the semaphore pool, the count and the hand-off of units to
 * waiters. Blocking, timeouts and resumption go through the shared IPC
 * services; a context switch is left to the architecture layer.
 */

#include <stdbool.h>
#include <stddef.h>

#include "maxrtos/config.h"

#include "maxrtos/kernel/semaphore.h"
#include "maxrtos/kernel/ipc_block.h"
#include "maxrtos/kernel/ipc_priority.h"
#include "maxrtos/kernel/tick.h"
#include "maxrtos/kernel/waitlist.h"

typedef struct
{
    bool in_use;
    maxrtos_partition_id_t partition_id;
    uint32_t count;
    uint32_t max;
    maxrtos_waitlist_t waiters;
} maxrtos_semaphore_t;

static maxrtos_semaphore_t s_semaphore_pool[ MAXRTOS_MAX_SEMAPHORES ];

void maxrtos_semaphore_pool_init( void )
{
    size_t i;

    for( i = 0U; i < MAXRTOS_MAX_SEMAPHORES; i++ )
    {
        s_semaphore_pool[ i ].in_use = false;
        s_semaphore_pool[ i ].partition_id = MAXRTOS_INVALID_PARTITION_ID;
        s_semaphore_pool[ i ].count = 0U;
        s_semaphore_pool[ i ].max = 0U;
        ( void ) maxrtos_waitlist_init( &s_semaphore_pool[ i ].waiters );
    }
}

static maxrtos_semaphore_t * maxrtos_semaphore_get(
    maxrtos_semaphore_id_t id )
{
    maxrtos_semaphore_t * semaphore;

    semaphore = NULL;

    if( ( id < ( maxrtos_semaphore_id_t ) MAXRTOS_MAX_SEMAPHORES ) &&
        ( s_semaphore_pool[ id ].in_use == true ) )
    {
        semaphore = &s_semaphore_pool[ id ];
    }

    return semaphore;
}

/* The semaphore, if it exists and belongs to the caller's partition. */
static maxrtos_semaphore_t * maxrtos_semaphore_for_process(
    maxrtos_semaphore_id_t id,
    maxrtos_process_id_t process_id )
{
    maxrtos_semaphore_t * semaphore;
    maxrtos_process_control_block_t const * pcb;

    semaphore = maxrtos_semaphore_get( id );
    pcb = maxrtos_process_get( process_id );

    if( ( pcb == NULL ) ||
        ( ( semaphore != NULL ) &&
          ( semaphore->partition_id != pcb->partition_id ) ) )
    {
        semaphore = NULL;
    }

    return semaphore;
}

maxrtos_status_t maxrtos_semaphore_create(
    maxrtos_partition_id_t partition_id,
    uint32_t initial_value,
    uint32_t max_value,
    maxrtos_semaphore_id_t * out_id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( out_id != NULL ) &&
        ( partition_id < MAXRTOS_MAX_PARTITIONS ) &&
        ( max_value > 0U ) &&
        ( initial_value <= max_value ) )
    {
        size_t i;

        status = MAXRTOS_ERR_POOL_FULL;

        for( i = 0U; ( i < MAXRTOS_MAX_SEMAPHORES ) &&
                     ( status != MAXRTOS_OK ); i++ )
        {
            if( s_semaphore_pool[ i ].in_use == false )
            {
                s_semaphore_pool[ i ].in_use = true;
                s_semaphore_pool[ i ].partition_id = partition_id;
                s_semaphore_pool[ i ].count = initial_value;
                s_semaphore_pool[ i ].max = max_value;
                ( void ) maxrtos_waitlist_init( &s_semaphore_pool[ i ].waiters );

                *out_id = ( maxrtos_semaphore_id_t ) i;
                status = MAXRTOS_OK;
            }
        }
    }

    return status;
}

maxrtos_status_t maxrtos_kernel_semaphore_wait(
    maxrtos_semaphore_id_t id,
    maxrtos_tick_t timeout,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id )
{
    maxrtos_status_t status;
    maxrtos_semaphore_t * semaphore;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( table != NULL ) && ( out_next_id != NULL ) )
    {
        semaphore = maxrtos_semaphore_for_process( id, current_id );

        if( semaphore == NULL )
        {
            status = MAXRTOS_ERR_INVALID_ID;
        }
        else if( semaphore->count > 0U )
        {
            semaphore->count--;
            status = MAXRTOS_OK;
        }
        else if( timeout == 0U )
        {
            status = MAXRTOS_ERR_TIMEOUT;
        }
        else
        {
            maxrtos_tick_t wake_tick;

            wake_tick = MAXRTOS_TICK_NONE;
            status = maxrtos_kernel_tick_after( timeout, &wake_tick );

            if( status == MAXRTOS_OK )
            {
                maxrtos_ipc_operation_t op;

                op.kind = MAXRTOS_IPC_OP_SEMAPHORE_WAIT;
                op.payload.semaphore_wait.id = id;

                status = maxrtos_ipc_block_current(
                    table,
                    current_id,
                    &semaphore->waiters,
                    wake_tick,
                    &op,
                    out_next_id );

                if( status == MAXRTOS_OK )
                {
                    status = MAXRTOS_PENDING;
                }
                else if( status == MAXRTOS_ERR_QUEUE_EMPTY )
                {
                    /* Nothing else to run while waiting: report it as a
                     * poll that found no unit. */
                    status = MAXRTOS_ERR_TIMEOUT;
                }
                else
                {
                    /* Any other status is returned as is. */
                }
            }
        }
    }

    return status;
}

maxrtos_status_t maxrtos_kernel_semaphore_signal(
    maxrtos_semaphore_id_t id,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id )
{
    maxrtos_status_t status;
    maxrtos_semaphore_t * semaphore;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( table != NULL )
    {
        semaphore = maxrtos_semaphore_for_process( id, current_id );

        if( semaphore == NULL )
        {
            status = MAXRTOS_ERR_INVALID_ID;
        }
        else if( maxrtos_waitlist_count( &semaphore->waiters ) == 0U )
        {
            if( semaphore->count >= semaphore->max )
            {
                status = MAXRTOS_ERR_OVERFLOW;
            }
            else
            {
                semaphore->count++;
                status = MAXRTOS_OK;
            }
        }
        else
        {
            maxrtos_process_id_t waiter;

            waiter = MAXRTOS_INVALID_PROCESS_ID;
            status = maxrtos_ipc_take_best_waiter(
                &semaphore->waiters, &waiter );

            if( status == MAXRTOS_OK )
            {
                /* The unit goes straight to the waiter; the count stays
                 * at zero. If it cannot be made READY, keep the unit. */
                status = maxrtos_ipc_resolve( table, waiter, MAXRTOS_OK );

                if( status != MAXRTOS_OK )
                {
                    semaphore->count++;
                }
            }
        }
    }

    return status;
}

maxrtos_status_t maxrtos_kernel_semaphore_get_status(
    maxrtos_semaphore_id_t id,
    maxrtos_process_id_t current_id,
    maxrtos_semaphore_status_t * out_status )
{
    maxrtos_status_t status;
    maxrtos_semaphore_t const * semaphore;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( out_status != NULL )
    {
        semaphore = maxrtos_semaphore_for_process( id, current_id );

        if( semaphore == NULL )
        {
            status = MAXRTOS_ERR_INVALID_ID;
        }
        else
        {
            out_status->current_value = semaphore->count;
            out_status->maximum_value = semaphore->max;
            out_status->waiting =
                ( uint32_t ) maxrtos_waitlist_count( &semaphore->waiters );
            status = MAXRTOS_OK;
        }
    }

    return status;
}

uint32_t maxrtos_kernel_semaphore_count(
    maxrtos_semaphore_id_t id )
{
    maxrtos_semaphore_t const * semaphore;

    semaphore = maxrtos_semaphore_get( id );

    return ( semaphore != NULL ) ? semaphore->count : 0U;
}
