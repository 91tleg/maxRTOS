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

#include "maxrtos/arch/cortex_m7/svc.h"
#include "maxrtos/arch/cortex_m7/internal/yield.h"

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
            maxrtos_arch_yield();
            break;
        }

        default:
        {
            maxrtos_arch_svc_invalid();
            break;
        }
    }
}
