/**
 * @file start_scheduler.c
 * @brief Cortex-M7 scheduler startup implementation.
 *
 * Provides the architecture-specific scheduler startup sequence for
 * Cortex-M7 targets.
 *
 * The implementation delegates scheduler initialization and initial
 * process selection to the kernel scheduler startup routine. Once the
 * first process is selected, control is transferred to the process
 * through the Cortex-M7 context-switch.
 *
 * Scheduler startup failures are treated as fatal at this layer and
 * enter a debug breakpoint loop.
 */

 #include <stddef.h>

#include "maxrtos/kernel/start_scheduler.h"
#include "maxrtos/arch/cortex_m7/context_switch.h"

void maxrtos_arch_start_scheduler(
    maxrtos_frame_schedule_t const * frame_schedule,
    maxrtos_partition_table_t * partition_table )
{
    maxrtos_status_t status;
    maxrtos_process_control_block_t * first_pcb;

    first_pcb = NULL;

    status = maxrtos_kernel_start_scheduler(
        frame_schedule,
        partition_table,
        &first_pcb );

    if( status != MAXRTOS_OK )
    {
        for( ;; )
        {
            __asm volatile ( "bkpt #0" );
        }
    }

    /* Never returns. */
    maxrtos_arch_start_first_process( first_pcb );
}
