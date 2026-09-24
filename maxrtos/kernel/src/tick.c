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
#include "maxrtos/kernel/ipc_block.h"
#include "maxrtos/kernel/timing.h"

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

    /* Resume processes whose timed IPC wait has expired before choosing
     * what to run, so a timed-out process can run in its own slot. A wait
     * of N ticks, started when the tick counter read T, expires when the
     * counter reaches T + N. */
    ( void ) maxrtos_ipc_expire_timeouts( table, s_current_tick );

    /* Periodic releases first, so a process released at this tick is judged
     * against its new deadline; then record any deadline that has passed. */
    ( void ) maxrtos_kernel_release_periodic( table, s_current_tick );
    ( void ) maxrtos_kernel_check_deadlines( table, s_current_tick );

    if( status == MAXRTOS_OK )
    {
        status = maxrtos_partition_dispatch(
            table,
            active_partition_id,
            out_next_id );
    }

    return status;
}

maxrtos_status_t maxrtos_kernel_tick_after(
    maxrtos_tick_t timeout,
    maxrtos_tick_t * out_wake_tick )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( out_wake_tick != NULL )
    {
        status = MAXRTOS_OK;

        if( timeout == MAXRTOS_TIMEOUT_INFINITE )
        {
            *out_wake_tick = MAXRTOS_TICK_NONE;
        }
        else if( timeout >= ( MAXRTOS_TICK_NONE - s_current_tick ) )
        {
            status = MAXRTOS_ERR_OVERFLOW;
        }
        else
        {
            *out_wake_tick = s_current_tick + timeout;
        }
    }

    return status;
}

maxrtos_tick_t maxrtos_kernel_tick_now( void )
{
    return s_current_tick;
}

void maxrtos_kernel_tick_reset( void )
{
    s_current_tick = 0U;
}
