/**
 * @file timing.c
 * @brief Cortex-M7 implementation of the public periodic process interface.
 */

#include <stdint.h>

#include "maxrtos/timing.h"
#include "maxrtos/arch/cortex_m7/svc.h"

maxrtos_status_t maxrtos_periodic_wait( void )
{
    register uint32_t r0 __asm( "r0" ) = 0U;

    __asm volatile (
        "svc %1"
        : "+r" ( r0 )
        : "i" ( MAXRTOS_SVC_PERIODIC_WAIT )
        : "memory" );

    return ( maxrtos_status_t ) r0;
}
