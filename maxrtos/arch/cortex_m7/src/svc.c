/**
 * @file svc.c
 * @brief C-side SVC dispatch.
 *
 * Runs privileged on behalf of whatever process executed an 
 * svc instruction. This is the only entry point through which
 * an unprivileged partition may reach kernel functionality
 * that touches privileged-only state.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "maxrtos/arch/cortex_m7/svc.h"
#include "maxrtos/arch/cortex_m7/context_switch.h"
#include "maxrtos/arch/cortex_m7/fault_handlers.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/yield.h"

/* There is no valid recovery action for a syscall gate that
 * cannot identify what was requested. */
static void maxrtos_arch_svc_invalid( void )
{
    __asm volatile ( "cpsid i" );

    for( ;; )
    {
        __asm volatile ( "bkpt #0" );
    }
}

static void maxrtos_arch_svc_yield(
    uint32_t const * stacked_args )
{
    maxrtos_process_control_block_t const * current_pcb;

    ( void ) stacked_args;

    current_pcb = maxrtos_arch_get_current_pcb();

    if( current_pcb != NULL )
    {
        maxrtos_status_t status;
        maxrtos_process_id_t next_id;
        bool switch_needed;

        status = maxrtos_kernel_yield(
            maxrtos_arch_get_partition_table(),
            current_pcb->partition_id,
            current_pcb->id,
            &next_id,
            &switch_needed );

        if( ( status == MAXRTOS_OK ) &&
            ( switch_needed == true ) )
        {
            maxrtos_process_control_block_t * next_pcb;

            next_pcb = maxrtos_process_get( next_id );

            if( next_pcb != NULL )
            {
                maxrtos_arch_set_next_pcb( next_pcb );
                maxrtos_arch_request_context_switch();
            }
        }
    }
}

void maxrtos_arch_svc_dispatch(
    uint32_t const * stacked_args,
    uint8_t svc_number )
{
    if( ( stacked_args == NULL ) ||
        ( svc_number >= ( uint8_t ) MAXRTOS_SVC_COUNT ) )
    {
        maxrtos_arch_svc_invalid();
    }

    switch( ( maxrtos_svc_number_t ) svc_number )
    {
        case MAXRTOS_SVC_YIELD:
        {
            maxrtos_arch_svc_yield( stacked_args );
            break;
        }

        default:
        {
            maxrtos_arch_svc_invalid();
            break;
        }
    }
}
