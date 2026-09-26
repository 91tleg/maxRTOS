/**
 * @file buffer.c
 * @brief Cortex-M7 buffer application interface: SVC entry points.
 */

#include <stddef.h>
#include <stdint.h>

#include "maxrtos/buffer.h"
#include "maxrtos/arch/cortex_m7/svc.h"

maxrtos_status_t maxrtos_buffer_send(
    maxrtos_buffer_id_t id,
    void const * message,
    size_t message_size,
    maxrtos_tick_t timeout )
{
    register uint32_t r0 __asm( "r0" ) = ( uint32_t ) id;
    register uint32_t r1 __asm( "r1" ) = ( uint32_t ) message;
    register uint32_t r2 __asm( "r2" ) = ( uint32_t ) message_size;
    register uint32_t r3 __asm( "r3" ) = ( uint32_t ) timeout;

    __asm volatile (
        "svc %4"
        : "+r" ( r0 )
        : "r" ( r1 ),
          "r" ( r2 ),
          "r" ( r3 ),
          "i" ( MAXRTOS_SVC_BUFFER_SEND )
        : "memory" );

    return ( maxrtos_status_t ) r0;
}

maxrtos_status_t maxrtos_buffer_receive(
    maxrtos_buffer_id_t id,
    void * out_message,
    size_t buffer_size,
    maxrtos_tick_t timeout )
{
    register uint32_t r0 __asm( "r0" ) = ( uint32_t ) id;
    register uint32_t r1 __asm( "r1" ) = ( uint32_t ) out_message;
    register uint32_t r2 __asm( "r2" ) = ( uint32_t ) buffer_size;
    register uint32_t r3 __asm( "r3" ) = ( uint32_t ) timeout;

    __asm volatile (
        "svc %4"
        : "+r" ( r0 )
        : "r" ( r1 ),
          "r" ( r2 ),
          "r" ( r3 ),
          "i" ( MAXRTOS_SVC_BUFFER_RECEIVE )
        : "memory" );

    return ( maxrtos_status_t ) r0;
}

maxrtos_status_t maxrtos_buffer_get_status(
    maxrtos_buffer_id_t id,
    maxrtos_buffer_status_t * status )
{
    register uint32_t r0 __asm( "r0" ) = ( uint32_t ) id;
    register uint32_t r1 __asm( "r1" ) = ( uint32_t ) status;

    __asm volatile (
        "svc %2"
        : "+r" ( r0 )
        : "r" ( r1 ),
          "i" ( MAXRTOS_SVC_BUFFER_STATUS )
        : "memory" );

    return ( maxrtos_status_t ) r0;
}
