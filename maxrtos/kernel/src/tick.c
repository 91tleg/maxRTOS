/**
 * @file tick.c
 * @brief Implementation of the kernel tick-to-dispatch interface.
 *
 * The kernel owns the authoritative system tick. Each call to
 * maxrtos_kernel_on_tick() advances the kernel tick by one.
 *
 * Kernel services such as timed waits can query the current tick
 * through maxrtos_kernel_tick_now() without requiring the tick
 * value.
 */

#include <stdint.h>

#include "maxrtos/kernel/tick.h"
#include "maxrtos/kernel/frame.h"
#include "maxrtos/kernel/partition.h"

static maxrtos_tick_t s_current_tick = 0U;

maxrtos_status_t maxrtos_kernel_on_tick(
    maxrtos_frame_schedule_t const * sched,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t * out_next_id )
{
    maxrtos_status_t status;
    maxrtos_partition_id_t active_partition_id;

    status = maxrtos_frame_partition_at_tick(
        sched,
        s_current_tick,
        &active_partition_id );

    s_current_tick++;

    if( status == MAXRTOS_OK )
    {
        status = maxrtos_partition_dispatch(
            table,
            active_partition_id,
            out_next_id );
    }

    return status;
}

maxrtos_tick_t maxrtos_kernel_tick_now( void )
{
    return s_current_tick;
}
