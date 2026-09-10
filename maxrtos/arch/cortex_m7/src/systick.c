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

static maxrtos_frame_schedule_t const * s_frame_schedule = NULL;
static maxrtos_partition_table_t * s_partition_table = NULL;
static uint32_t s_tick_count = 0U;

void maxrtos_arch_systick_init(
    maxrtos_frame_schedule_t const * schedule,
    maxrtos_partition_table_t * table )
{
    s_frame_schedule = schedule;
    s_partition_table = table;
    s_tick_count = 0U;
}

void maxrtos_arch_systick( void )
{
    maxrtos_process_id_t next_id;

    if( ( s_frame_schedule != NULL ) && ( s_partition_table != NULL ) )
    {
        s_tick_count++;

        if( maxrtos_kernel_on_tick(
                s_frame_schedule,
                s_partition_table,
                s_tick_count,
                &next_id ) == MAXRTOS_OK )
        {
            maxrtos_process_control_block_t * next_pcb;
            maxrtos_process_control_block_t * current_pcb;

            next_pcb = maxrtos_process_get( next_id );
            current_pcb = maxrtos_arch_get_current_pcb();

            if( ( next_pcb != NULL ) && ( next_pcb != current_pcb ) )
            {
                maxrtos_arch_set_next_pcb( next_pcb );
                maxrtos_arch_request_context_switch();
            }
        }
    }
}
