/**
 * @file semaphore.c
 * @brief Cortex-M7 semaphore application interface: SVC entry points.
 */

#include <stdint.h>

#include "maxrtos/semaphore.h"
#include "maxrtos/arch/cortex_m7/svc.h"

maxrtos_status_t maxrtos_semaphore_wait(
    maxrtos_semaphore_id_t id,
    maxrtos_tick_t timeout )
{
    register uint32_t r0 __asm( "r0" ) = ( uint32_t ) id;
    register uint32_t r1 __asm( "r1" ) = ( uint32_t ) timeout;

    __asm volatile (
        "svc %2"
        : "+r" ( r0 )
        : "r" ( r1 ),
          "i" ( MAXRTOS_SVC_SEMAPHORE_WAIT )
        : "memory" );

    return ( maxrtos_status_t ) r0;
}

maxrtos_status_t maxrtos_semaphore_get_status(
    maxrtos_semaphore_id_t id,
    maxrtos_semaphore_status_t * status )
{
    register uint32_t r0 __asm( "r0" ) = ( uint32_t ) id;
    register uint32_t r1 __asm( "r1" ) = ( uint32_t ) status;

    __asm volatile (
        "svc %2"
        : "+r" ( r0 )
        : "r" ( r1 ),
          "i" ( MAXRTOS_SVC_SEMAPHORE_STATUS )
        : "memory" );

    return ( maxrtos_status_t ) r0;
}

maxrtos_status_t maxrtos_semaphore_signal(
    maxrtos_semaphore_id_t id )
{
    register uint32_t r0 __asm( "r0" ) = ( uint32_t ) id;

    __asm volatile (
        "svc %1"
        : "+r" ( r0 )
        : "i" ( MAXRTOS_SVC_SEMAPHORE_SIGNAL )
        : "memory" );

    return ( maxrtos_status_t ) r0;
}
