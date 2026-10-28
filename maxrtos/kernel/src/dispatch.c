/**
 * @file dispatch.c
 * @brief Process dispatch and execution-state transition management.
 *
 * The dispatch layer converts a scheduler decision into process-state
 * transitions. The scheduler ready queues contain only processes in
 * READY state. A RUNNING process is not a member of a ready queue.
 */

#include <stddef.h>

#include "maxrtos/kernel/dispatch.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/scheduler.h"

maxrtos_status_t maxrtos_kernel_dispatch(
    maxrtos_scheduler_context_t * ctx,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id )
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

            status = maxrtos_scheduler_next( ctx, &next_id );

            /* If no READY process is available, retain the current
             * RUNNING process. Initial dispatch requires a READY
             * process to be selected. */
            if( ( status == MAXRTOS_ERR_QUEUE_EMPTY ) &&
                ( current_pcb != NULL ) )
            {
                *out_next_id = current_id;
                status = MAXRTOS_OK;
            }
            else if( status == MAXRTOS_OK )
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
                     * process shall not be present in the READY queue.
                     * It was just peeked, so it is the one taken. */
                    status = maxrtos_scheduler_take_next( ctx, &next_id );
                }

                if( status == MAXRTOS_OK )
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
                        /* Transition the current process to READY before
                         * inserting it into the READY queue. */
                        current_pcb->state = MAXRTOS_PROCESS_STATE_READY;

                        status = maxrtos_scheduler_enqueue(
                            ctx, current_id, current_pcb->priority );

                        if( status != MAXRTOS_OK )
                        {
                            /* Restore the current process to its original
                             * RUNNING state and restore the selected
                             * process to the READY queue. Recovery is
                             * performed without replacing the original
                             * error status. */
                            current_pcb->state = MAXRTOS_PROCESS_STATE_RUNNING;

                            ( void ) maxrtos_scheduler_enqueue(
                                ctx, next_id, next_pcb->priority );
                        }
                        else
                        {
                            next_pcb->state = MAXRTOS_PROCESS_STATE_RUNNING;
                        }
                    }
                }

                if( status == MAXRTOS_OK )
                {
                    *out_next_id = next_id;
                }
            }
            else
            {
                /* The scheduler reported an error; return it. */
            }
        }
    }

    return status;
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
