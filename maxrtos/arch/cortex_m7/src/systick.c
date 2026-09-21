/**
 * @file systick.c
 * @brief Cortex-M7 SysTick integration.
 *
 * Connects the Cortex-M7 SysTick interrupt to the kernel tick handler
 * and requests a context switch when the scheduler selects a different
 * process.
 */

#include <stdint.h>
#include <stddef.h>

#include "maxrtos/arch/cortex_m7/systick.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/tick.h"
#include "maxrtos/arch/cortex_m7/context_switch.h"
#include "maxrtos/arch/cortex_m7/idle.h"
#include "maxrtos/arch/cortex_m7/fault_handlers.h"
#include "maxrtos/kernel/timing.h"

static maxrtos_frame_schedule_t const * s_frame_schedule = NULL;
static maxrtos_partition_table_t * s_partition_table = NULL;

void maxrtos_arch_systick_init(
    maxrtos_frame_schedule_t const * schedule,
    maxrtos_partition_table_t * table )
{
    s_frame_schedule = schedule;
    s_partition_table = table;
}

maxrtos_frame_schedule_t const * maxrtos_arch_get_frame_schedule( void )
{
    return s_frame_schedule;
}

void maxrtos_arch_systick( void )
{
    maxrtos_process_id_t next_id;

    if( ( s_frame_schedule != NULL ) &&
        ( s_partition_table != NULL ) )
    {
        maxrtos_status_t tick_status;
        maxrtos_process_id_t missed_id;

        /* Deadline misses the previous tick detected. They are acted on here,
         * before this tick's dispatch, so the dispatch sees the recovered
         * state. (One tick of latency, and no stale selection.) */
        while( maxrtos_kernel_take_deadline_miss( &missed_id ) == true )
        {
            maxrtos_process_control_block_t * missed_pcb;

            missed_pcb = maxrtos_process_get( missed_id );

            if( missed_pcb != NULL )
            {
                maxrtos_arch_handle_process_fault(
                    missed_pcb, MAXRTOS_FAULT_DEADLINE_EXCEEDED );
            }
        }

        tick_status = maxrtos_kernel_on_tick(
            s_frame_schedule,
            s_partition_table,
            &next_id );

        if( ( tick_status == MAXRTOS_ERR_PARTITION_HALTED ) ||
            ( tick_status == MAXRTOS_ERR_QUEUE_EMPTY ) )
        {
            /* The slot belongs to a halted partition, or to one with nothing
             * READY (for example its only process is waiting for its next
             * release). Do not let the previous slot's process keep running
             * in it: park in idle. */
            if( maxrtos_arch_get_current_pcb() != maxrtos_arch_idle_pcb() )
            {
                maxrtos_arch_set_next_pcb( maxrtos_arch_idle_pcb() );
                maxrtos_arch_request_context_switch();
            }
        }
        else if( tick_status == MAXRTOS_OK )
        {
            maxrtos_process_control_block_t * next_pcb;
            maxrtos_process_control_block_t const * current_pcb;

            next_pcb = maxrtos_process_get( next_id );
            current_pcb = maxrtos_arch_get_current_pcb();

            if( ( next_pcb != NULL ) &&
                ( next_pcb != current_pcb ) )
            {
                maxrtos_arch_set_next_pcb( next_pcb );
                maxrtos_arch_request_context_switch();
            }
        }
    }
}
