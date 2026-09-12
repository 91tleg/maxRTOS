/**
 * @file yield.c
 * @brief implementation of voluntary CPU yield processing.
 *
 * Implements the target-specific wrapper for voluntary process yielding on
 * Cortex-M7. Queries the active PCB context, invokes the kernel yield
 * evaluation logic, and execute a context switch when needed.
 */

#include <stdbool.h>
#include <stddef.h>

#include "maxrtos/arch/cortex_m7/yield.h"
#include "maxrtos/arch/cortex_m7/internal/yield.h"
#include "maxrtos/arch/cortex_m7/context_switch.h"
#include "maxrtos/arch/cortex_m7/svc.h"
#include "maxrtos/kernel/yield.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/arch/cortex_m7/fault_handlers.h"

void maxrtos_yield( void )
{
    __asm volatile (
        "svc %0"
        :
        : "i" ( MAXRTOS_SVC_YIELD )
        : "memory" );
}

void maxrtos_arch_yield( void )
{
    maxrtos_process_control_block_t * self_pcb;
    maxrtos_partition_table_t * table;

    self_pcb = maxrtos_arch_get_current_pcb();
    table = maxrtos_arch_get_partition_table();

    if( ( self_pcb != NULL ) && ( table != NULL ) )
    {
        maxrtos_status_t status;
        maxrtos_process_id_t next_id;
        bool switch_needed;

        status = maxrtos_kernel_yield(
            table,
            self_pcb->partition_id,
            self_pcb->id,
            &next_id,
            &switch_needed );

        if( ( status == MAXRTOS_OK ) && ( switch_needed == true ) )
        {
            maxrtos_process_control_block_t * next_pcb;

            next_pcb = maxrtos_process_get( next_id );

            if( next_pcb != NULL )
            {
                maxrtos_arch_set_current_pcb( self_pcb );
                maxrtos_arch_set_next_pcb( next_pcb );
                maxrtos_arch_request_context_switch();
            }
        }
    }
}
