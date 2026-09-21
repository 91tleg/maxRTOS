/**
 * @file ipc_block.c
 * @brief Generic IPC blocking, wake, timeout, and resolution primitives.
 *
 * Architecture-independent implementation shared by IPC primitives.
 *
 * This module owns only generic process, wait-list, and scheduler
 * transitions. It does not interpret IPC payloads and does not perform
 * context switches.
 */

#include <stddef.h>

#include "maxrtos/config.h"

#include "maxrtos/kernel/ipc_block.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/scheduler.h"
#include "maxrtos/kernel/waitlist.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/dispatch.h"

/**
 * @brief Clear the blocked IPC state of a process.
 *
 * The IPC operation is copied before its kind is cleared so the caller
 * receives a complete snapshot of the operation that was blocked.
 */
static void maxrtos_ipc_clear_blocked_state(
    maxrtos_process_control_block_t * pcb,
    maxrtos_ipc_operation_t * out_op )
{
    *out_op = pcb->ipc_operation;

    pcb->ipc_operation.kind = MAXRTOS_IPC_OP_NONE;
    pcb->waitlist = NULL;
    pcb->wake_tick = MAXRTOS_TICK_NONE;
}

maxrtos_status_t maxrtos_ipc_block_current(
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id,
    maxrtos_waitlist_t * waitlist,
    maxrtos_tick_t wake_tick,
    maxrtos_ipc_operation_t const * op,
    maxrtos_process_id_t * out_next_id )
{
    maxrtos_status_t status;
    maxrtos_process_control_block_t * pcb;

    status = MAXRTOS_ERR_INVALID_ARG;
    pcb = NULL;

    if( ( table != NULL ) &&
        ( waitlist != NULL ) &&
        ( op != NULL ) &&
        ( out_next_id != NULL ) )
    {
        pcb = maxrtos_process_get( current_id );

        if( pcb == NULL )
        {
            status = MAXRTOS_ERR_INVALID_ID;
        }
        else if( pcb->state != MAXRTOS_PROCESS_STATE_RUNNING )
        {
            status = MAXRTOS_ERR_INVALID_STATE;
        }
        else
        {
            /*
             * Save the IPC operation before blocking the process.
             */
            pcb->ipc_operation = *op;

            /*
             * Put the process on the IPC wait list.
             */
            status = maxrtos_waitlist_push(
                waitlist,
                current_id );

            if( status == MAXRTOS_OK )
            {
                pcb->waitlist = waitlist;
                pcb->wake_tick = wake_tick;

                /*
                 * RUNNING -> BLOCKED
                 * READY   -> RUNNING
                 */
                status = maxrtos_partition_block_and_dispatch(
                    table,
                    pcb->partition_id,
                    current_id,
                    out_next_id );

                if( status != MAXRTOS_OK )
                {
                    pcb->waitlist = NULL;
                    pcb->wake_tick = MAXRTOS_TICK_NONE;
                    pcb->ipc_operation.kind =
                        MAXRTOS_IPC_OP_NONE;

                    ( void ) maxrtos_waitlist_remove(
                        waitlist,
                        current_id );
                }
            }
        }
    }

    return status;
}


maxrtos_status_t maxrtos_ipc_wake_one(
    maxrtos_waitlist_t * waitlist,
    maxrtos_process_id_t * out_id,
    maxrtos_ipc_operation_t * out_op )
{
    maxrtos_status_t status;
    maxrtos_process_control_block_t * pcb;

    status = MAXRTOS_ERR_INVALID_ARG;
    pcb = NULL;

    if( ( waitlist != NULL ) &&
        ( out_id != NULL ) &&
        ( out_op != NULL ) )
    {
        status = maxrtos_waitlist_pop_front(
            waitlist,
            out_id );

        if( status == MAXRTOS_OK )
        {
            pcb = maxrtos_process_get( *out_id );

            if( pcb == NULL )
            {
                status = MAXRTOS_ERR_INVALID_ID;
            }
            else
            {
                maxrtos_ipc_clear_blocked_state(
                    pcb,
                    out_op );
            }
        }
    }

    return status;
}

maxrtos_status_t maxrtos_ipc_timeout_current(
    maxrtos_waitlist_t * waitlist,
    maxrtos_process_id_t id,
    maxrtos_ipc_operation_t * out_op )
{
    maxrtos_status_t status;
    maxrtos_process_control_block_t * pcb;
    bool removed;

    status = MAXRTOS_ERR_INVALID_ARG;
    pcb = NULL;

    if( ( waitlist != NULL ) &&
        ( out_op != NULL ) )
    {
        removed = maxrtos_waitlist_remove(
            waitlist,
            id );

        if( removed == false )
        {
            status = MAXRTOS_ERR_NOT_FOUND;
        }
        else
        {
            pcb = maxrtos_process_get( id );

            if( pcb == NULL )
            {
                status = MAXRTOS_ERR_INVALID_ID;
            }
            else
            {
                maxrtos_ipc_clear_blocked_state(
                    pcb,
                    out_op );

                status = MAXRTOS_OK;
            }
        }
    }

    return status;
}

maxrtos_status_t maxrtos_ipc_resolve(
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t id,
    maxrtos_status_t result )
{
    maxrtos_status_t status;
    maxrtos_process_control_block_t * pcb;

    status = MAXRTOS_ERR_INVALID_ARG;
    pcb = NULL;

    if( table != NULL )
    {
        pcb = maxrtos_process_get( id );

        if( pcb == NULL )
        {
            status = MAXRTOS_ERR_INVALID_ID;
        }
        else if( pcb->state != MAXRTOS_PROCESS_STATE_BLOCKED )
        {
            status = MAXRTOS_ERR_INVALID_STATE;
        }
        else
        {
            /* The process goes back to the ready queue of its own
             * partition, not the waker's. */
            status = maxrtos_partition_ready_process(
                table,
                pcb->partition_id,
                id );

            if( status == MAXRTOS_OK )
            {
                pcb->ipc_result = result;
                pcb->ipc_result_pending = true;
            }
        }
    }

    return status;
}

size_t maxrtos_ipc_expire_timeouts(
    maxrtos_partition_table_t * table,
    maxrtos_tick_t now )
{
    size_t resumed;

    resumed = 0U;

    if( table != NULL )
    {
        for( maxrtos_process_id_t id = 0U;
             id < ( maxrtos_process_id_t ) MAXRTOS_MAX_PROCESSES;
             id++ )
        {
            maxrtos_process_control_block_t const * pcb;

            pcb = maxrtos_process_get( id );

            if( ( pcb != NULL ) &&
                ( pcb->state == MAXRTOS_PROCESS_STATE_BLOCKED ) &&
                ( pcb->waitlist != NULL ) &&
                ( pcb->wake_tick != MAXRTOS_TICK_NONE ) &&
                ( now >= pcb->wake_tick ) )
            {
                maxrtos_ipc_operation_t op;

                if( ( maxrtos_ipc_timeout_current(
                          pcb->waitlist, id, &op ) == MAXRTOS_OK ) &&
                    ( maxrtos_ipc_resolve(
                          table, id, MAXRTOS_ERR_TIMEOUT ) == MAXRTOS_OK ) )
                {
                    resumed++;
                }
            }
        }
    }

    return resumed;
}
