/**
 * @file queue_port.c
 * @brief Cortex-M7 queue port application interface implementation.
 */

#include <stddef.h>

#include "maxrtos/queue_port.h"
#include "maxrtos/arch/cortex_m7/svc.h"
#include "maxrtos/kernel/queue_port.h"

maxrtos_status_t maxrtos_queue_port_send(
    maxrtos_queue_port_t * port,
    void const * message,
    size_t message_size,
    maxrtos_tick_t timeout )
{
    register uint32_t r0 __asm( "r0" ) = ( uint32_t ) port;
    register uint32_t r1 __asm( "r1" ) = ( uint32_t ) message;
    register uint32_t r2 __asm( "r2" ) = ( uint32_t ) message_size;
    register uint32_t r3 __asm( "r3" ) = ( uint32_t ) timeout;

    __asm volatile (
        "svc %4"
        : "+r" ( r0 )
        : "r" ( r1 ),
          "r" ( r2 ),
          "r" ( r3 ),
          "i" ( MAXRTOS_SVC_QUEUE_SEND )
        : "memory" );

    return ( maxrtos_status_t ) r0;
}

maxrtos_status_t maxrtos_queue_port_receive(
    maxrtos_queue_port_t * port,
    void * out_message,
    size_t buffer_size,
    maxrtos_tick_t timeout )
{
    register uint32_t r0 __asm( "r0" ) = ( uint32_t ) port;
    register uint32_t r1 __asm( "r1" ) = ( uint32_t ) out_message;
    register uint32_t r2 __asm( "r2" ) = ( uint32_t ) buffer_size;
    register uint32_t r3 __asm( "r3" ) = ( uint32_t ) timeout;

    __asm volatile (
        "svc %4"
        : "+r" ( r0 )
        : "r" ( r1 ),
          "r" ( r2 ),
          "r" ( r3 ),
          "i" ( MAXRTOS_SVC_QUEUE_RECEIVE )
        : "memory" );

    return ( maxrtos_status_t ) r0;
}

size_t maxrtos_queue_port_count(
    maxrtos_queue_port_t const * port )
{
    register uint32_t r0 __asm( "r0" ) = ( uint32_t ) port;

    __asm volatile (
        "svc %1"
        : "+r" ( r0 )
        : "i" ( MAXRTOS_SVC_QUEUE_COUNT )
        : "memory" );

    return ( size_t ) r0;
}
