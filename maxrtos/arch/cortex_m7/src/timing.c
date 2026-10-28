/**
 * @file timing.c
 * @brief Cortex-M7 implementation of the public timing interface.
 */

#include <stdint.h>

#include "maxrtos/timing.h"
#include "maxrtos/arch/cortex_m7/svc.h"

maxrtos_status_t maxrtos_periodic_wait( void )
{
    register uint32_t r0 __asm( "r0" ) = 0U;

    __asm volatile( "svc %1"
                    : "+r"( r0 )
                    : "i"( MAXRTOS_SVC_PERIODIC_WAIT )
                    : "memory" );

    return ( maxrtos_status_t ) r0;
}

maxrtos_status_t maxrtos_timed_wait( maxrtos_tick_t delay_ticks )
{
    register uint32_t r0 __asm( "r0" ) = ( uint32_t ) delay_ticks;

    __asm volatile( "svc %1"
                    : "+r"( r0 )
                    : "i"( MAXRTOS_SVC_TIMED_WAIT )
                    : "memory" );

    return ( maxrtos_status_t ) r0;
}

maxrtos_tick_t maxrtos_get_time( void )
{
    register uint32_t r0 __asm( "r0" ) = 0U;

    __asm volatile( "svc %1"
                    : "+r"( r0 )
                    : "i"( MAXRTOS_SVC_TIME_GET )
                    : "memory" );

    return ( maxrtos_tick_t ) r0;
}
