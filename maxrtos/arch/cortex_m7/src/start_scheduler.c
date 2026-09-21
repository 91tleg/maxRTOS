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

#include "maxrtos/config.h"
#include "maxrtos/scheduler.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/start_scheduler.h"
#include "maxrtos/arch/cortex_m7/context_switch.h"
#include "maxrtos/arch/cortex_m7/fault_handlers.h"
#include "maxrtos/arch/cortex_m7/start_scheduler.h"
#include "maxrtos/arch/cortex_m7/systick.h"
#include "maxrtos/arch/cortex_m7/port.h"

#define MAXRTOS_START_HALT() MAXRTOS_PORT_HALT()

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
        MAXRTOS_PORT_HALT();
    }

    /* Never returns. */
    maxrtos_arch_start_first_process( first_pcb );
}

void maxrtos_scheduler_start( void )
{
    maxrtos_frame_schedule_t const * frame_schedule;
    maxrtos_partition_table_t * partition_table;
    maxrtos_process_id_t id;

    frame_schedule = maxrtos_arch_get_frame_schedule();
    partition_table = maxrtos_arch_get_partition_table();

    if( ( frame_schedule == NULL ) || ( partition_table == NULL ) )
    {
        MAXRTOS_START_HALT();
    }

    /* Process IDs index the static pool, so walking the IDs visits every
     * created process without the application passing any of them in. */
    for( id = 0U; id < ( maxrtos_process_id_t ) MAXRTOS_MAX_PROCESSES; id++ )
    {
        maxrtos_process_control_block_t * pcb;

        pcb = maxrtos_process_get( id );

        if( ( pcb != NULL ) &&
            ( pcb->state == MAXRTOS_PROCESS_STATE_READY ) )
        {
            if( ( maxrtos_arch_init_stack( pcb ) != MAXRTOS_OK ) ||
                ( maxrtos_partition_add_process( partition_table, id ) != MAXRTOS_OK ) )
            {
                MAXRTOS_START_HALT();
            }
        }
    }

    /* Never returns. */
    maxrtos_arch_start_scheduler( frame_schedule, partition_table );

    MAXRTOS_START_HALT();
}
