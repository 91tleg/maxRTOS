/**
 * @file dispatch.c
 * @brief Process dispatch and execution-state transition management.
 *
 * The dispatch layer converts a scheduler decision into process-state
 * transitions. The scheduler ready queues contain only processes in
 * READY state. A RUNNING process is not a member of a ready queue.
 */

#include <stdbool.h>
#include <stddef.h>

#include "maxrtos/kernel/dispatch.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/scheduler.h"

/* True if a READY process of priority @p next may take the CPU from a
 * RUNNING one of priority @p current.
 *
 * A higher priority (a numerically lower value) always may: that is
 * preemption. An equal one may only if @p yield is set, since there is no time
 * slice: the running process keeps the CPU until it blocks, waits, or gives
 * way to its peers. A lower priority never may. */
static bool maxrtos_dispatch_takes_over(
    uint32_t next,
    uint32_t current,
    bool yield )
{
    return yield ? ( next <= current ) : ( next < current );
}

static maxrtos_status_t maxrtos_dispatch_select(
    maxrtos_scheduler_context_t * ctx,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id,
    bool yield )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( ctx != NULL ) && ( out_next_id != NULL ) )
    {
        maxrtos_process_control_block_t * current_pcb;

        current_pcb = NULL;

        /* Validate the current process when this is not an initial
         * dispatch. A valid current process shall be in the RUNNING
         * state. */
        if( current_id != MAXRTOS_INVALID_PROCESS_ID )
        {
            current_pcb = maxrtos_process_get( current_id );

            if( current_pcb == NULL )
            {
                status = MAXRTOS_ERR_INVALID_ID;
            }
            else if( current_pcb->state != MAXRTOS_PROCESS_STATE_RUNNING )
            {
                status = MAXRTOS_ERR_INVALID_STATE;
            }
            else
            {
                status = MAXRTOS_OK;
            }
        }
        else
        {
            status = MAXRTOS_OK;
        }

        if( status == MAXRTOS_OK )
        {
            maxrtos_process_id_t next_id;

            next_id = MAXRTOS_INVALID_PROCESS_ID;

            /* If no READY process is available, retain the current
             * RUNNING process. Initial dispatch requires a READY
             * process to be selected. */
            if( ctx->ready_mask == 0U )
            {
                if( current_pcb != NULL )
                {
                    *out_next_id = current_id;
                    status = MAXRTOS_OK;
                }
                else
                {
                    status = MAXRTOS_ERR_QUEUE_EMPTY;
                }
            }
            else
            {
                maxrtos_process_control_block_t * next_pcb;
                uint32_t next_priority;
                bool take_over;

                /* Peek the best READY process; priority 0 is the highest,
                 * so the lowest set bit wins. */
                next_priority =
                    maxrtos_scheduler_lowest_set_bit( ctx->ready_mask );
                next_id = ctx->queues[ next_priority ]
                              .items[ ctx->queues[ next_priority ].head ];

                next_pcb = maxrtos_process_get( next_id );
                take_over = true;

                if( next_pcb == NULL )
                {
                    status = MAXRTOS_ERR_INVALID_ID;
                }
                else if( next_pcb->state != MAXRTOS_PROCESS_STATE_READY )
                {
                    status = MAXRTOS_ERR_INVALID_STATE;
                }
                else if(
                    ( current_pcb != NULL ) &&
                    !maxrtos_dispatch_takes_over(
                        next_priority, current_pcb->priority, yield ) )
                {
                    /* The running process keeps the CPU; nothing changes. */
                    take_over = false;
                    *out_next_id = current_id;
                }
                else
                {
                    /* Remove the selected process from the READY queue
                     * before transitioning it to RUNNING. A RUNNING
                     * process shall not be present in the READY queue. */
                    maxrtos_scheduler_pop( ctx, next_priority );
                }

                if( ( status == MAXRTOS_OK ) && take_over )
                {
                    if( current_pcb == NULL )
                    {
                        /* Initial dispatch has no current process. The
                         * selected process is therefore transitioned
                         * directly to RUNNING. */
                        next_pcb->state = MAXRTOS_PROCESS_STATE_RUNNING;
                    }
                    else
                    {
                        /* The outgoing process becomes READY. Giving way
                         * puts it behind its peers; being preempted leaves
                         * it the oldest at its priority, so it runs again
                         * before them. */
                        current_pcb->state = MAXRTOS_PROCESS_STATE_READY;

                        status =
                            yield
                                ? maxrtos_scheduler_enqueue(
                                      ctx, current_id, current_pcb->priority )
                                : maxrtos_scheduler_enqueue_front(
                                      ctx, current_id, current_pcb->priority );

                        if( status != MAXRTOS_OK )
                        {
                            /* Restore the current process to its original
                             * RUNNING state and the selected process to
                             * the head of its queue. Recovery is performed
                             * without replacing the original error status. */
                            current_pcb->state = MAXRTOS_PROCESS_STATE_RUNNING;

                            ( void ) maxrtos_scheduler_enqueue_front(
                                ctx, next_id, ( uint8_t ) next_priority );
                        }
                        else
                        {
                            next_pcb->state = MAXRTOS_PROCESS_STATE_RUNNING;
                        }
                    }
                }

                if( ( status == MAXRTOS_OK ) && take_over )
                {
                    *out_next_id = next_id;
                }
            }
        }
    }

    return status;
}

maxrtos_status_t maxrtos_kernel_dispatch(
    maxrtos_scheduler_context_t * ctx,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id )
{
    return maxrtos_dispatch_select( ctx, current_id, out_next_id, false );
}

maxrtos_status_t maxrtos_kernel_dispatch_yield(
    maxrtos_scheduler_context_t * ctx,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id )
{
    return maxrtos_dispatch_select( ctx, current_id, out_next_id, true );
}

maxrtos_status_t maxrtos_kernel_block_and_dispatch(
    maxrtos_scheduler_context_t * ctx,
    maxrtos_process_id_t blocking_id,
    maxrtos_process_id_t * out_next_id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( ctx != NULL ) && ( out_next_id != NULL ) &&
        ( blocking_id != MAXRTOS_INVALID_PROCESS_ID ) )
    {
        maxrtos_process_control_block_t * blocking_pcb;

        blocking_pcb = maxrtos_process_get( blocking_id );

        if( blocking_pcb == NULL )
        {
            status = MAXRTOS_ERR_INVALID_ID;
        }
        else if( blocking_pcb->state != MAXRTOS_PROCESS_STATE_RUNNING )
        {
            status = MAXRTOS_ERR_INVALID_STATE;
        }
        else
        {
            maxrtos_process_id_t next_id;

            status = maxrtos_scheduler_next( ctx, &next_id );

            /* A blocking transition requires another READY process.
             * If the READY queue is empty, leave the blocking process
             * in the RUNNING state and return the scheduler error. */
            if( status == MAXRTOS_OK )
            {
                maxrtos_process_control_block_t * next_pcb;

                next_pcb = maxrtos_process_get( next_id );

                if( next_pcb == NULL )
                {
                    status = MAXRTOS_ERR_INVALID_ID;
                }
                else if( next_pcb->state != MAXRTOS_PROCESS_STATE_READY )
                {
                    status = MAXRTOS_ERR_INVALID_STATE;
                }
                else
                {
                    /* Remove the selected process from the READY queue
                     * before transitioning it to RUNNING. A RUNNING
                     * process shall not be present in the READY queue. */
                    status = maxrtos_scheduler_take_next( ctx, &next_id );

                    if( status == MAXRTOS_OK )
                    {
                        blocking_pcb->state = MAXRTOS_PROCESS_STATE_BLOCKED;
                        next_pcb->state = MAXRTOS_PROCESS_STATE_RUNNING;
                        *out_next_id = next_id;
                    }
                }
            }
        }
    }

    return status;
}
