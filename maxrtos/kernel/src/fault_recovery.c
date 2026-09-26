/**
 * @file fault_recovery.c
 * @brief Implementation of the health-monitor fault recovery service.
 *
 * Applies configured health-monitor recovery actions to process and
 * partition state.
 */

#include <stdbool.h>
#include <stddef.h>

#include "maxrtos/kernel/fault_recovery.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/mutex.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/tick.h"
#include "maxrtos/kernel/timing.h"
#include "maxrtos/kernel/waitlist.h"

/* Make a process runnable again from its entry point, whatever it was doing:
 * running (the usual fault), ready (a deadline miss of a starved process) or
 * blocked (a miss while waiting on IPC or on its release). */
static maxrtos_status_t maxrtos_fault_recovery_restart(
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t id )
{
    maxrtos_status_t status;
    maxrtos_process_control_block_t * pcb;

    pcb = maxrtos_process_get( id );
    status = MAXRTOS_OK;

    /* A restarted process forgets what it held: release its mutexes so its
     * partition peers are not locked out forever. */
    maxrtos_kernel_mutex_release_all( table, id );

    if( pcb == NULL )
    {
        status = MAXRTOS_ERR_INVALID_ID;
    }
    else if( pcb->state == MAXRTOS_PROCESS_STATE_RUNNING )
    {
        status = maxrtos_process_set_state( id, MAXRTOS_PROCESS_STATE_READY );

        if( status == MAXRTOS_OK )
        {
            status = maxrtos_partition_add_process( table, id );
        }

        if( status == MAXRTOS_OK )
        {
            table->current_id[ pcb->partition_id ] = MAXRTOS_INVALID_PROCESS_ID;
        }
    }
    else if( pcb->state == MAXRTOS_PROCESS_STATE_BLOCKED )
    {
        /* Leave whatever it was waiting on. */
        if( pcb->waitlist != NULL )
        {
            ( void ) maxrtos_waitlist_remove( pcb->waitlist, id );
        }

        pcb->waitlist = NULL;
        pcb->wake_tick = MAXRTOS_TICK_NONE;
        pcb->ipc_operation.kind = MAXRTOS_IPC_OP_NONE;
        pcb->ipc_result_pending = false;

        status = maxrtos_process_set_state( id, MAXRTOS_PROCESS_STATE_READY );

        if( status == MAXRTOS_OK )
        {
            status = maxrtos_partition_add_process( table, id );
        }
    }
    else
    {
        /* READY: already queued, its context is reset by the caller.
         * SUSPENDED: stays suspended. */
    }

    if( ( status == MAXRTOS_OK ) && ( pcb != NULL ) )
    {
        /* A restarted process begins a new release. */
        maxrtos_process_rearm_timing( pcb, maxrtos_kernel_tick_now() );
    }

    return status;
}

maxrtos_status_t maxrtos_fault_recovery_handle(
    maxrtos_health_monitor_t const * hm,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t faulting_process_id,
    maxrtos_fault_type_t fault_type,
    maxrtos_hm_action_t * out_action )
{
    maxrtos_status_t status;
    maxrtos_process_control_block_t const * pcb;

    status = MAXRTOS_ERR_INVALID_ARG;
    pcb = NULL;

    if( ( hm != NULL ) &&
        ( table != NULL ) &&
        ( out_action != NULL ) )
    {
        pcb = maxrtos_process_get( faulting_process_id );

        if( pcb == NULL )
        {
            status = MAXRTOS_ERR_INVALID_ID;
        }
        else
        {
            status = maxrtos_hm_get_policy(
                         hm,
                         pcb->partition_id,
                         fault_type,
                         out_action );

            if( status == MAXRTOS_OK )
            {
                switch( *out_action )
                {
                    case MAXRTOS_HM_ACTION_IGNORE:
                        status = MAXRTOS_OK;
                        break;

                    case MAXRTOS_HM_ACTION_RESTART_PROCESS:
                        status = maxrtos_fault_recovery_restart(
                                    table, faulting_process_id );
                        break;

                    case MAXRTOS_HM_ACTION_HALT_PARTITION:
                        table->halted[ pcb->partition_id ] = true;
                        status = MAXRTOS_OK;
                        break;

                    default:
                        status = MAXRTOS_ERR_INVALID_STATE;
                        break;
                }
            }
        }
    }

    return status;
}
