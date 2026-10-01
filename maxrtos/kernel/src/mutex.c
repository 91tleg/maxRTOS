/**
 * @file mutex.c
 * @brief Kernel implementation of intra-partition mutexes.
 *
 * Owns the mutex pool, ownership hand-off, waiter selection and
 * priority inheritance.
 * Blocking, timeouts and resumption go through the shared IPC block
 * services; a context switch is left to the architecture layer.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

/* "No extra claim" for maxrtos_mutex_refresh_chain(): numerically larger
 * than any real priority, so it never wins the minimum. */
#define MAXRTOS_MUTEX_NO_CLAIM ( ( uint8_t ) 0xFFU )

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

/* The priority a process has to run at: its configured one, raised to that
 * of the most urgent process waiting on any mutex it owns, and to claim.
 * Waiters are compared by their effective priority, which is what carries
 * an inherited priority along a chain of blocked owners. */
static uint8_t maxrtos_mutex_wanted_priority(
    maxrtos_process_id_t id,
    maxrtos_process_control_block_t const * pcb,
    uint8_t claim )
{
    uint8_t wanted;
    size_t i;

    wanted = ( pcb->base_priority < claim ) ? pcb->base_priority : claim;

    for( i = 0U; i < MAXRTOS_MAX_MUTEXES; i++ )
    {
        if( ( s_mutex_pool[ i ].in_use == true ) &&
            ( s_mutex_pool[ i ].owner == id ) )
        {
            size_t w;

            for( w = 0U;
                 w < maxrtos_waitlist_count( &s_mutex_pool[ i ].waiters );
                 w++ )
            {
                maxrtos_process_id_t waiter_id;
                maxrtos_process_control_block_t const * waiter;

                waiter_id = MAXRTOS_INVALID_PROCESS_ID;

                if( maxrtos_waitlist_get(
                        &s_mutex_pool[ i ].waiters, w, &waiter_id ) ==
                    MAXRTOS_OK )
                {
                    waiter = maxrtos_process_get( waiter_id );

                    if( ( waiter != NULL ) && ( waiter->priority < wanted ) )
                    {
                        wanted = waiter->priority;
                    }
                }
            }
        }
    }

    return wanted;
}

/* Re-derive the effective priority of a mutex owner, then of whoever that
 * owner is itself blocked on, and so on down the chain. claim is an extra
 * priority the first owner must be at least as urgent as: the process that
 * is about to wait on it and is not on the wait list yet.
 *
 * Raising and lowering are the same operation, so this serves lock (raise),
 * unlock, timeout and restart (lower). The walk is bounded by the number of
 * mutexes so a deadlock cycle cannot loop. */
static maxrtos_status_t maxrtos_mutex_refresh_chain(
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t owner,
    uint8_t claim )
{
    maxrtos_status_t status;
    maxrtos_process_id_t current;
    uint8_t extra;
    size_t hops;

    status = MAXRTOS_OK;
    current = owner;
    extra = claim;

    for( hops = 0U;
         ( hops <= MAXRTOS_MAX_MUTEXES ) &&
         ( current != MAXRTOS_INVALID_PROCESS_ID ) &&
         ( status == MAXRTOS_OK );
         hops++ )
    {
        maxrtos_process_control_block_t const * pcb;
        maxrtos_process_id_t next;

        pcb = maxrtos_process_get( current );
        next = MAXRTOS_INVALID_PROCESS_ID;

        if( pcb != NULL )
        {
            status = maxrtos_partition_set_process_priority(
                table,
                current,
                maxrtos_mutex_wanted_priority( current, pcb, extra ) );

            if( ( status == MAXRTOS_OK ) &&
                ( pcb->state == MAXRTOS_PROCESS_STATE_BLOCKED ) &&
                ( pcb->ipc_operation.kind == MAXRTOS_IPC_OP_MUTEX_LOCK ) )
            {
                next = maxrtos_kernel_mutex_owner(
                    pcb->ipc_operation.payload.mutex_lock.id );
            }
        }

        extra = MAXRTOS_MUTEX_NO_CLAIM;
        current = next;
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
    maxrtos_process_id_t previous_owner;

    next_owner = MAXRTOS_INVALID_PROCESS_ID;
    previous_owner = mutex->owner;
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

    /* The old owner no longer inherits from this mutex's waiters. It keeps
     * whatever its other mutexes still demand. The new owner needs no
     * adjustment: it was the most urgent waiter, so nobody left behind is
     * more urgent than it already is. Failing to lower a priority only
     * leaves it too high until the next refresh, so it is not an error. */
    ( void ) maxrtos_mutex_refresh_chain(
        table, previous_owner, MAXRTOS_MUTEX_NO_CLAIM );

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
                /* Priority inheritance: the owner has to run at least as
                 * urgently as the caller from now on, or a process of
                 * in-between priority could keep it off the CPU for as
                 * long as it likes while the caller waits. Done before
                 * blocking, so a failure leaves the caller running. */
                status = maxrtos_mutex_refresh_chain(
                    table, mutex->owner, pcb->priority );
            }

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

            if( status != MAXRTOS_PENDING )
            {
                /* The caller is not waiting after all: take back what
                 * was raised for it. */
                ( void ) maxrtos_mutex_refresh_chain(
                    table, mutex->owner, MAXRTOS_MUTEX_NO_CLAIM );
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

void maxrtos_kernel_mutex_refresh_priorities(
    maxrtos_partition_table_t * table )
{
    if( table != NULL )
    {
        size_t i;

        for( i = 0U; i < MAXRTOS_MAX_MUTEXES; i++ )
        {
            if( ( s_mutex_pool[ i ].in_use == true ) &&
                ( s_mutex_pool[ i ].owner != MAXRTOS_INVALID_PROCESS_ID ) )
            {
                ( void ) maxrtos_mutex_refresh_chain(
                    table, s_mutex_pool[ i ].owner, MAXRTOS_MUTEX_NO_CLAIM );
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
