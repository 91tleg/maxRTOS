/**
 * @file scheduler.c
 * @brief Process scheduling and ready-queue management.
 *
 * Implements the scheduler interface declared in scheduler.h.
 *
 * The scheduler uses strict fixed-priority scheduling. Priority 0 is
 * the highest priority. Processes with equal priority are scheduled
 * in FIFO order.
 */

#include <stddef.h>
#include <stdbool.h>

#include "maxrtos/kernel/scheduler.h"

#define MAXRTOS_MAX_READY_PER_PRIORITY \
    MAXRTOS_MAX_PROCESSES

typedef struct
{
    maxrtos_process_id_t items[ MAXRTOS_MAX_READY_PER_PRIORITY ];
    size_t head;
    size_t count;
} maxrtos_ready_queue_t;

static maxrtos_ready_queue_t
    s_ready_queues[ MAXRTOS_MAX_PRIORITY + 1U ];

static size_t maxrtos_scheduler_queue_tail(
    maxrtos_ready_queue_t const * queue )
{
    size_t tail;

    tail = ( queue->head + queue->count ) %
           MAXRTOS_MAX_READY_PER_PRIORITY;

    return tail;
}

static maxrtos_status_t maxrtos_scheduler_queue_push(
    maxrtos_ready_queue_t * queue,
    maxrtos_process_id_t id )
{
    maxrtos_status_t status;
    size_t tail;

    status = MAXRTOS_ERR_QUEUE_FULL;
    tail = 0U;

    if( queue != NULL )
    {
        if( queue->count < MAXRTOS_MAX_READY_PER_PRIORITY )
        {
            tail = maxrtos_scheduler_queue_tail( queue );

            queue->items[ tail ] = id;
            queue->count++;
            status = MAXRTOS_OK;
        }
    }

    return status;
}

/* Remove a process ID from a ready queue.
 * Entries following the removed entry are shifted toward the head
 * to preserve FIFO ordering. */
static maxrtos_status_t maxrtos_scheduler_queue_remove_id(
    maxrtos_ready_queue_t * queue,
    maxrtos_process_id_t id )
{
    maxrtos_status_t status;
    size_t scan;
    size_t index;
    size_t found_index;
    bool found;

    status = MAXRTOS_ERR_NOT_FOUND;
    scan = 0U;
    index = 0U;
    found_index = 0U;
    found = false;

    if( queue != NULL )
    {
        for( scan = 0U;
             ( scan < queue->count ) && ( found == false );
             scan++ )
        {
            index = ( queue->head + scan ) %
                    MAXRTOS_MAX_READY_PER_PRIORITY;

            if( queue->items[ index ] == id )
            {
                found = true;
                found_index = scan;
            }
        }

        if( found == true )
        {
            for( scan = found_index;
                 ( scan + 1U ) < queue->count;
                 scan++ )
            {
                index = ( queue->head + scan ) %
                        MAXRTOS_MAX_READY_PER_PRIORITY;

                queue->items[ index ] =
                    queue->items[
                        ( queue->head + scan + 1U ) %
                        MAXRTOS_MAX_READY_PER_PRIORITY ];
            }

            queue->count--;
            status = MAXRTOS_OK;
        }
    }

    return status;
}

void maxrtos_scheduler_init( void )
{
    size_t priority;

    for( priority = 0U;
         priority <= MAXRTOS_MAX_PRIORITY;
         priority++ )
    {
        s_ready_queues[ priority ].head = 0U;
        s_ready_queues[ priority ].count = 0U;
    }
}

maxrtos_status_t maxrtos_scheduler_add_process(
    maxrtos_process_id_t id )
{
    maxrtos_status_t status;
    maxrtos_process_control_block_t * pcb;

    status = MAXRTOS_ERR_INVALID_ID;
    pcb = maxrtos_process_get( id );

    if( pcb != NULL )
    {
        if( pcb->state != MAXRTOS_PROCESS_STATE_READY )
        {
            status = MAXRTOS_ERR_INVALID_STATE;
        }
        else
        {
            status = maxrtos_scheduler_queue_push(
                &s_ready_queues[ pcb->priority ],
                id );
        }
    }

    return status;
}

maxrtos_status_t maxrtos_scheduler_remove_process(
    maxrtos_process_id_t id )
{
    maxrtos_status_t status;
    maxrtos_process_control_block_t * pcb;

    status = MAXRTOS_ERR_INVALID_ID;
    pcb = maxrtos_process_get( id );

    if( pcb != NULL )
    {
        if( pcb->state != MAXRTOS_PROCESS_STATE_READY )
        {
            status = MAXRTOS_ERR_INVALID_STATE;
        }
        else
        {
            status = maxrtos_scheduler_queue_remove_id(
                &s_ready_queues[ pcb->priority ],
                id );
        }
    }

    return status;
}

maxrtos_status_t maxrtos_scheduler_next(
    maxrtos_process_id_t * out_id )
{
    maxrtos_status_t status;
    size_t priority;

    status = MAXRTOS_ERR_INVALID_ARG;
    priority = 0U;

    if( out_id != NULL )
    {
        status = MAXRTOS_ERR_QUEUE_EMPTY;

        for( priority = 0U;
             ( priority <= MAXRTOS_MAX_PRIORITY ) &&
             ( status != MAXRTOS_OK );
             priority++ )
        {
            if( s_ready_queues[ priority ].count > 0U )
            {
                /* Peek only. Selection does not remove the process from the
                 * ready queue or modify queue state. */
                *out_id = s_ready_queues[ priority ].items[
                    s_ready_queues[ priority ].head ];

                status = MAXRTOS_OK;
            }
        }
    }

    return status;
}

size_t maxrtos_scheduler_process_count( void )
{
    size_t priority;
    size_t count;

    count = 0U;

    for( priority = 0U;
         priority <= MAXRTOS_MAX_PRIORITY;
         priority++ )
    {
        count += s_ready_queues[ priority ].count;
    }

    return count;
}
