/**
 * @file start_scheduler.c
 * @brief Starts the MAXRTOS scheduler.
 *
 * This module provides the implementation for starting scheduler execution
 * from tick zero and obtaining the first process to execute.
 */

 #include "maxrtos/kernel/start_scheduler.h"
#include "maxrtos/kernel/tick.h"

maxrtos_status_t maxrtos_kernel_start_scheduler(
    maxrtos_frame_schedule_t const * frame_schedule,
    maxrtos_partition_table_t * partition_table,
    maxrtos_process_control_block_t ** out_first_pcb )
{
    maxrtos_status_t status;
    maxrtos_process_id_t first_id;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( frame_schedule != NULL ) &&
        ( partition_table != NULL ) &&
        ( out_first_pcb != NULL ) )
    {
        status = maxrtos_kernel_on_tick(
            frame_schedule,
            partition_table,
            0U,
            &first_id );

        if( status == MAXRTOS_OK )
        {
            *out_first_pcb = maxrtos_process_get( first_id );

            if( *out_first_pcb == NULL )
            {
                status = MAXRTOS_ERR_INVALID_ARG;
            }
        }
    }

    return status;
}
