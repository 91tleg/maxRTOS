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

    status = MAXRTOS_ERR_QUEUE_FULL;

    if( queue != NULL )
    {
        if( queue->count < MAXRTOS_MAX_READY_PER_PRIORITY )
        {
            size_t tail;

            tail = maxrtos_scheduler_queue_tail( queue );

            queue->items[ tail ] = id;
            queue->count++;
            status = MAXRTOS_OK;
        }
    }

    return status;
}

/* Searches the queue for the specified process ID. If found, entries
 * after the removed process are shifted toward the head to preserve
 * FIFO ordering. */
static maxrtos_status_t maxrtos_scheduler_queue_remove_id(
    maxrtos_ready_queue_t * queue,
    maxrtos_process_id_t id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_NOT_FOUND;

    if( queue != NULL )
    {
        size_t scan;
        size_t index;
        size_t found_index;
        bool found;

        found_index = 0U;
        found = false;

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

maxrtos_status_t maxrtos_scheduler_init(
    maxrtos_scheduler_context_t * ctx )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ctx != NULL )
    {
        size_t priority;

        for( priority = 0U;
             priority <= MAXRTOS_MAX_PRIORITY;
             priority++ )
        {
            ctx->queues[ priority ].head = 0U;
            ctx->queues[ priority ].count = 0U;
        }

        status = MAXRTOS_OK;
    }

    return status;
}

maxrtos_status_t maxrtos_scheduler_add_process(
    maxrtos_scheduler_context_t * ctx,
    maxrtos_process_id_t id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ctx != NULL )
    {
        maxrtos_process_control_block_t * pcb;

        pcb = NULL;
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
                    &ctx->queues[ pcb->priority ],
                    id );
            }
        }
    }

    return status;
}

maxrtos_status_t maxrtos_scheduler_remove_process(
    maxrtos_scheduler_context_t * ctx,
    maxrtos_process_id_t id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ctx != NULL )
    {
        maxrtos_process_control_block_t * pcb;

        pcb = NULL;
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
                    &ctx->queues[ pcb->priority ],
                    id );
            }
        }
    }

    return status;
}

maxrtos_status_t maxrtos_scheduler_next(
    maxrtos_scheduler_context_t const * ctx,
    maxrtos_process_id_t * out_id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( ctx != NULL ) && ( out_id != NULL ) )
    {
        size_t priority;

        priority = 0U;
        status = MAXRTOS_ERR_QUEUE_EMPTY;

        for( priority = 0U;
             ( priority <= MAXRTOS_MAX_PRIORITY ) &&
             ( status != MAXRTOS_OK );
             priority++ )
        {
            if( ctx->queues[ priority ].count > 0U )
            {
                /* Peek only. Selection does not remove the process from the
                 * ready queue or modify queue state. */
                *out_id = ctx->queues[ priority ].items[
                    ctx->queues[ priority ].head ];

                status = MAXRTOS_OK;
            }
        }
    }

    return status;
}

size_t maxrtos_scheduler_process_count(
    maxrtos_scheduler_context_t const * ctx )
{
    size_t count;

    count = 0U;

    if( ctx != NULL )
    {
        size_t priority;

        for( priority = 0U;
             priority <= MAXRTOS_MAX_PRIORITY;
             priority++ )
        {
            count += ctx->queues[ priority ].count;
        }
    }

    return count;
}
