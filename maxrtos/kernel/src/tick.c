/**
 * @file tick.c
 * @brief Implementation of the tick-to-dispatch interface.
 */

#include <stdint.h>

#include "maxrtos/kernel/tick.h"

maxrtos_status_t maxrtos_kernel_on_tick(
    maxrtos_frame_schedule_t const * sched,
    maxrtos_partition_table_t * table,
    uint32_t tick,
    maxrtos_process_id_t * out_next_id )
{
    maxrtos_status_t status;
    maxrtos_partition_id_t active_partition_id;

    status = maxrtos_frame_partition_at_tick(
                sched,
                tick,
                &active_partition_id );

    if( status == MAXRTOS_OK )
    {
        status = maxrtos_partition_dispatch(
                    table,
                    active_partition_id,
                    out_next_id ); 
    }

    return status;
}
