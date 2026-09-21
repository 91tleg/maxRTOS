/**
 * @file fault_dispatch.c
 * @brief Architecture-side fault recovery enactment.
 *
 * Hardware-independent: given a classified fault, ask the kernel health
 * monitor for the recovery action and carry it out (restart the process
 * with a fresh context, or park the CPU in the idle context after a
 * partition halt). The exception entry points that classify the
 * hardware fault live in fault_handlers.c.
 */

#include <stddef.h>
#include <stdint.h>

#include "maxrtos/arch/cortex_m7/fault_handlers.h"
#include "maxrtos/arch/cortex_m7/idle.h"
#include "maxrtos/arch/cortex_m7/context_switch.h"
#include "maxrtos/arch/cortex_m7/port.h"
#include "maxrtos/kernel/fault_recovery.h"

static maxrtos_health_monitor_t const * s_hm = NULL;
static maxrtos_partition_table_t * s_table = NULL;

static void maxrtos_arch_halt( void )
{
    MAXRTOS_PORT_HALT();
}

void maxrtos_arch_set_health_monitor(
    maxrtos_health_monitor_t const * hm )
{
    s_hm = hm;
}

void maxrtos_arch_set_partition_table(
    maxrtos_partition_table_t * table )
{
    s_table = table;
}

maxrtos_partition_table_t * maxrtos_arch_get_partition_table( void )
{
    return s_table;
}

/* Handle a fault after architecture-specific classification.
 * Recovery policy is selected by the kernel health monitor. The
 * selected action is then enacted here because process context and
 * context-switch operations are architecture-specific. */
void maxrtos_arch_handle_fault(
    maxrtos_fault_type_t fault_type )
{
    maxrtos_process_control_block_t * faulting_pcb;
    maxrtos_hm_action_t action;
    maxrtos_status_t status;

    if( ( s_hm == NULL ) || ( s_table == NULL ) )
    {
        maxrtos_arch_halt();
    }

    faulting_pcb = maxrtos_arch_get_current_pcb();

    if( faulting_pcb == NULL )
    {
        maxrtos_arch_halt();
    }

    status = maxrtos_fault_recovery_handle(
                 s_hm,
                 s_table,
                 faulting_pcb->id,
                 fault_type,
                 &action );

    if( status != MAXRTOS_OK )
    {
        maxrtos_arch_halt();
    }

    switch( action )
    {
        case MAXRTOS_HM_ACTION_IGNORE:
        {
            /* Returning from the exception resumes the faulting
             * instruction. This is only useful for fault classes
             * where retrying the instruction is known to be valid. */
            break;
        }

        case MAXRTOS_HM_ACTION_RESTART_PROCESS:
        {
            maxrtos_process_id_t next_id;
            maxrtos_process_control_block_t * next_pcb;
            maxrtos_status_t dispatch_status;

            status = maxrtos_arch_init_stack( faulting_pcb );

            if( status != MAXRTOS_OK )
            {
                maxrtos_arch_halt();
            }

            dispatch_status = maxrtos_partition_dispatch(
                                  s_table,
                                  faulting_pcb->partition_id,
                                  &next_id );

            if( dispatch_status != MAXRTOS_OK )
            {
                maxrtos_arch_halt();
            }

            next_pcb = maxrtos_process_get( next_id );

            if( next_pcb == NULL )
            {
                maxrtos_arch_halt();
            }

            /* The faulting context must not be saved. Its stack now contains
             * the fresh initial frame prepared by maxrtos_arch_init_stack(). */
            maxrtos_arch_set_current_pcb( NULL );
            maxrtos_arch_set_next_pcb( next_pcb );
            maxrtos_arch_request_context_switch();

            break;
        }

        case MAXRTOS_HM_ACTION_HALT_PARTITION:
        {
            /* The recovery layer has marked the partition halted, so the
             * frame schedule will not dispatch it again. Its faulting
             * context must not resume, and this handler must return so
             * that lower-priority exceptions (SysTick, PendSV) can run:
             * leave through the idle context. SysTick then hands the CPU
             * to the next partition's slot. */
            maxrtos_arch_idle_enter_discarding_current();

            break;
        }

        default:
        {
            maxrtos_arch_halt();
            break;
        }
    }
}
