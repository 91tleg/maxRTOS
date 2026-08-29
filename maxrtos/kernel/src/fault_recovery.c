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

maxrtos_status_t maxrtos_fault_recovery_handle(
    maxrtos_health_monitor_t const * hm,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t faulting_process_id,
    maxrtos_fault_type_t fault_type,
    maxrtos_hm_action_t * out_action )
{
    maxrtos_status_t status;
    maxrtos_process_control_block_t * pcb;

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
                        status = maxrtos_process_set_state(
                                    faulting_process_id,
                                    MAXRTOS_PROCESS_STATE_READY );

                        if( status == MAXRTOS_OK )
                        {
                            status = maxrtos_partition_add_process(
                                        table,
                                        faulting_process_id );
                        }

                        if( status == MAXRTOS_OK )
                        {
                            table->current_id[ pcb->partition_id ] =
                                MAXRTOS_INVALID_PROCESS_ID;
                        }
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
