/**
 * @file mutex.c
 * @brief Kernel implementation of intra-partition mutexes.
 *
 * Owns the mutex pool, ownership hand-off and waiter selection.
 * Blocking, timeouts and resumption go through the shared IPC block
 * services; a context switch is left to the architecture layer.
 */

#include <stdbool.h>
#include <stddef.h>

#include "maxrtos/config.h"

#include "maxrtos/kernel/mutex.h"
#include "maxrtos/kernel/ipc_block.h"
#include "maxrtos/kernel/ipc_priority.h"
#include "maxrtos/kernel/tick.h"
#include "maxrtos/kernel/waitlist.h"

typedef struct
{
    bool in_use;
    maxrtos_partition_id_t partition_id;
    maxrtos_process_id_t owner;
    maxrtos_waitlist_t waiters;
} maxrtos_mutex_t;

static maxrtos_mutex_t s_mutex_pool[ MAXRTOS_MAX_MUTEXES ];

void maxrtos_mutex_pool_init( void )
{
    size_t i;

    for( i = 0U; i < MAXRTOS_MAX_MUTEXES; i++ )
    {
        s_mutex_pool[ i ].in_use = false;
        s_mutex_pool[ i ].partition_id = MAXRTOS_INVALID_PARTITION_ID;
        s_mutex_pool[ i ].owner = MAXRTOS_INVALID_PROCESS_ID;
        ( void ) maxrtos_waitlist_init( &s_mutex_pool[ i ].waiters );
    }
}

static maxrtos_mutex_t * maxrtos_mutex_get( maxrtos_mutex_id_t id )
{
    maxrtos_mutex_t * mutex;

    mutex = NULL;

    if( ( id < ( maxrtos_mutex_id_t ) MAXRTOS_MAX_MUTEXES ) &&
        ( s_mutex_pool[ id ].in_use == true ) )
    {
        mutex = &s_mutex_pool[ id ];
    }

    return mutex;
}

maxrtos_status_t maxrtos_mutex_create(
    maxrtos_partition_id_t partition_id,
    maxrtos_mutex_id_t * out_id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( out_id != NULL ) &&
        ( partition_id < MAXRTOS_MAX_PARTITIONS ) )
    {
        status = MAXRTOS_ERR_POOL_FULL;

        size_t i;

        for( i = 0U; ( i < MAXRTOS_MAX_MUTEXES ) &&
                     ( status != MAXRTOS_OK ); i++ )
        {
            if( s_mutex_pool[ i ].in_use == false )
            {
                s_mutex_pool[ i ].in_use = true;
                s_mutex_pool[ i ].partition_id = partition_id;
                s_mutex_pool[ i ].owner = MAXRTOS_INVALID_PROCESS_ID;
                ( void ) maxrtos_waitlist_init( &s_mutex_pool[ i ].waiters );

                *out_id = ( maxrtos_mutex_id_t ) i;
                status = MAXRTOS_OK;
            }
        }
    }

    return status;
}

/* Give a released mutex to its best waiter, or leave it unlocked. */
static maxrtos_status_t maxrtos_mutex_hand_off(
    maxrtos_mutex_t * mutex,
    maxrtos_partition_table_t * table )
{
    maxrtos_status_t status;
    maxrtos_process_id_t next_owner;

    next_owner = MAXRTOS_INVALID_PROCESS_ID;
    mutex->owner = MAXRTOS_INVALID_PROCESS_ID;
    status = MAXRTOS_OK;

    if( maxrtos_ipc_take_best_waiter( &mutex->waiters, &next_owner ) == MAXRTOS_OK )
    {
        mutex->owner = next_owner;
        status = maxrtos_ipc_resolve( table, next_owner, MAXRTOS_OK );

        if( status != MAXRTOS_OK )
        {
            mutex->owner = MAXRTOS_INVALID_PROCESS_ID;
        }
    }

    return status;
}

maxrtos_status_t maxrtos_kernel_mutex_lock(
    maxrtos_mutex_id_t id,
    maxrtos_tick_t timeout,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( table != NULL ) && ( out_next_id != NULL ) )
    {
        maxrtos_mutex_t * mutex;
        maxrtos_process_control_block_t const * pcb;

        mutex = maxrtos_mutex_get( id );
        pcb = maxrtos_process_get( current_id );

        if( ( mutex == NULL ) ||
            ( pcb == NULL ) ||
            ( mutex->partition_id != pcb->partition_id ) )
        {
            status = MAXRTOS_ERR_INVALID_ID;
        }
        else if( mutex->owner == current_id )
        {
            status = MAXRTOS_ERR_INVALID_STATE;
        }
        else if( mutex->owner == MAXRTOS_INVALID_PROCESS_ID )
        {
            mutex->owner = current_id;
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

                op.kind = MAXRTOS_IPC_OP_MUTEX_LOCK;
                op.payload.mutex_lock.id = id;

                status = maxrtos_ipc_block_current(
                    table,
                    current_id,
                    &mutex->waiters,
                    wake_tick,
                    &op,
                    out_next_id );

                if( status == MAXRTOS_OK )
                {
                    status = MAXRTOS_PENDING;
                }
                else if( status == MAXRTOS_ERR_QUEUE_EMPTY )
                {
                    /* Nothing else to run while waiting: report it as
                     * a try-lock that failed. */
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

maxrtos_status_t maxrtos_kernel_mutex_unlock(
    maxrtos_mutex_id_t id,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( table != NULL )
    {
        maxrtos_mutex_t * mutex;
        maxrtos_process_control_block_t const * pcb;

        mutex = maxrtos_mutex_get( id );
        pcb = maxrtos_process_get( current_id );

        if( ( mutex == NULL ) ||
            ( pcb == NULL ) ||
            ( mutex->partition_id != pcb->partition_id ) )
        {
            status = MAXRTOS_ERR_INVALID_ID;
        }
        else if( mutex->owner != current_id )
        {
            status = MAXRTOS_ERR_INVALID_STATE;
        }
        else
        {
            status = maxrtos_mutex_hand_off( mutex, table );
        }
    }

    return status;
}

void maxrtos_kernel_mutex_release_all(
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t process_id )
{
    if( table != NULL )
    {
        size_t i;

        for( i = 0U; i < MAXRTOS_MAX_MUTEXES; i++ )
        {
            if( ( s_mutex_pool[ i ].in_use == true ) &&
                ( s_mutex_pool[ i ].owner == process_id ) )
            {
                ( void ) maxrtos_mutex_hand_off( &s_mutex_pool[ i ], table );
            }
        }
    }
}

maxrtos_process_id_t maxrtos_kernel_mutex_owner(
    maxrtos_mutex_id_t id )
{
    maxrtos_mutex_t const * mutex;

    mutex = maxrtos_mutex_get( id );

    return ( mutex != NULL ) ? mutex->owner : MAXRTOS_INVALID_PROCESS_ID;
}
