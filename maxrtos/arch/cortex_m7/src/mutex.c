/**
 * @file mutex.c
 * @brief Cortex-M7 mutex application interface: SVC entry points.
 */

#include <stdint.h>

#include "maxrtos/mutex.h"
#include "maxrtos/arch/cortex_m7/svc.h"

maxrtos_status_t maxrtos_mutex_lock(
    maxrtos_mutex_id_t id,
    maxrtos_tick_t timeout )
{
    register uint32_t r0 __asm( "r0" ) = ( uint32_t ) id;
    register uint32_t r1 __asm( "r1" ) = ( uint32_t ) timeout;

    __asm volatile (
        "svc %2"
        : "+r" ( r0 )
        : "r" ( r1 ),
          "i" ( MAXRTOS_SVC_MUTEX_LOCK )
        : "memory" );

    return ( maxrtos_status_t ) r0;
}

maxrtos_status_t maxrtos_mutex_unlock(
    maxrtos_mutex_id_t id )
{
    register uint32_t r0 __asm( "r0" ) = ( uint32_t ) id;

    __asm volatile (
        "svc %1"
        : "+r" ( r0 )
        : "i" ( MAXRTOS_SVC_MUTEX_UNLOCK )
        : "memory" );

    return ( maxrtos_status_t ) r0;
}
