/**
 * @file dispatch.c
 * @brief Process dispatch and execution-state transition management.
 *
 * Implements the dispatch interface declared in dispatch.h.
 *
 * The dispatch layer converts a scheduler decision into process-state
 * transitions. The scheduler ready queues contain only processes in
 * READY state. A RUNNING process is not a member of a ready queue.
 */

#include <stddef.h>

#include "maxrtos/kernel/dispatch.h"

maxrtos_status_t maxrtos_kernel_dispatch(
    maxrtos_scheduler_context_t * ctx,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( ctx != NULL ) && ( out_next_id != NULL ) )
    {
        /* Validate the currently running process when this is not
         * the initial dispatch. */
        if( current_id != MAXRTOS_INVALID_PROCESS_ID )
        {
            maxrtos_process_control_block_t const * current_pcb;

            current_pcb = maxrtos_process_get( current_id );

            if( current_pcb == NULL )
            {
                status = MAXRTOS_ERR_INVALID_ID;
            }
            else if( current_pcb->state !=
                     MAXRTOS_PROCESS_STATE_RUNNING )
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

            /* No READY process is available. The current process
             * continues executing when one already exists. */
            if( ( status == MAXRTOS_ERR_QUEUE_EMPTY ) &&
                ( current_id != MAXRTOS_INVALID_PROCESS_ID ) )
            {
                *out_next_id = current_id;
                status = MAXRTOS_OK;
            }
            else
            {
                /* Continue only when the scheduler selected a
                 * READY process. */
                if( status == MAXRTOS_OK )
                {
                    maxrtos_process_control_block_t const * next_pcb;

                    next_pcb = maxrtos_process_get( next_id );

                    if( next_pcb == NULL )
                    {
                        status = MAXRTOS_ERR_INVALID_ID;
                    }
                    else if( next_pcb->state !=
                             MAXRTOS_PROCESS_STATE_READY )
                    {
                        status = MAXRTOS_ERR_INVALID_STATE;
                    }
                    else
                    {
                        /* Remove the selected process before changing
                         * its state so that a RUNNING process is never
                         * present in the READY queue. */
                        status = maxrtos_scheduler_remove_process(
                            ctx,
                            next_id );
                    }

                    if( status == MAXRTOS_OK )
                    {
                        if( current_id == MAXRTOS_INVALID_PROCESS_ID )
                        {
                            /* Initial dispatch has no current process
                             * to return to the READY state. */
                            status = maxrtos_process_set_state(
                                next_id,
                                MAXRTOS_PROCESS_STATE_RUNNING );
                        }
                        else
                        {
                            /* The current process must become READY
                             * before it can be inserted into the
                             * scheduler. */
                            status = maxrtos_process_set_state(
                                current_id,
                                MAXRTOS_PROCESS_STATE_READY );

                            if( status == MAXRTOS_OK )
                            {
                                status = maxrtos_scheduler_add_process(
                                    ctx,
                                    current_id );
                            }

                            if( status != MAXRTOS_OK )
                            {
                                /* Restore the original state and
                                 * restore the selected process to its
                                 * READY queue. Recovery is best-effort. */
                                ( void ) maxrtos_process_set_state(
                                    current_id,
                                    MAXRTOS_PROCESS_STATE_RUNNING );

                                ( void ) maxrtos_scheduler_add_process(
                                    ctx,
                                    next_id );
                            }
                            else
                            {
                                status = maxrtos_process_set_state(
                                    next_id,
                                    MAXRTOS_PROCESS_STATE_RUNNING );
                            }
                        }
                    }

                    if( status == MAXRTOS_OK )
                    {
                        *out_next_id = next_id;
                    }
                }
            }
        }
    }

    return status;
}
