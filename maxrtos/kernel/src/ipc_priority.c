/**
 * @file ipc_priority.c
 * @brief Priority-ordered waiter selection for blocking IPC primitives.
 */

#include <stddef.h>

#include "maxrtos/kernel/ipc_priority.h"
#include "maxrtos/kernel/ipc_block.h"
#include "maxrtos/kernel/process.h"

maxrtos_status_t maxrtos_ipc_take_best_waiter_op(
    maxrtos_waitlist_t * waitlist,
    maxrtos_process_id_t * out_id,
    maxrtos_ipc_operation_t * out_op )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( waitlist != NULL ) && ( out_id != NULL ) && ( out_op != NULL ) )
    {
        maxrtos_process_id_t best_id;
        size_t best_priority;
        size_t i;

        status = MAXRTOS_ERR_QUEUE_EMPTY;
        best_id = MAXRTOS_INVALID_PROCESS_ID;
        best_priority = 0U;

        for( i = 0U; i < maxrtos_waitlist_count( waitlist ); i++ )
        {
            maxrtos_process_id_t id;
            maxrtos_process_control_block_t const * pcb;

            id = MAXRTOS_INVALID_PROCESS_ID;
            pcb = NULL;

            if( maxrtos_waitlist_get( waitlist, i, &id ) == MAXRTOS_OK )
            {
                pcb = maxrtos_process_get( id );
            }

            if( ( pcb != NULL ) &&
                ( ( best_id == MAXRTOS_INVALID_PROCESS_ID ) ||
                  ( pcb->priority < best_priority ) ) )
            {
                best_id = id;
                best_priority = pcb->priority;
            }
        }

        if( best_id != MAXRTOS_INVALID_PROCESS_ID )
        {
            /* Leaves the wait list and clears the process's blocked
             * state. (Despite its name this is the generic
             * remove-one-waiter step, not specific to timeouts.) */
            status = maxrtos_ipc_timeout_current( waitlist, best_id, out_op );

            if( status == MAXRTOS_OK )
            {
                *out_id = best_id;
            }
        }
    }

    return status;
}

maxrtos_status_t maxrtos_ipc_take_best_waiter(
    maxrtos_waitlist_t * waitlist,
    maxrtos_process_id_t * out_id )
{
    maxrtos_ipc_operation_t op;

    return maxrtos_ipc_take_best_waiter_op( waitlist, out_id, &op );
}
