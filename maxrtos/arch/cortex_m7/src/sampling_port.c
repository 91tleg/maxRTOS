/**
 * @file sampling_port.c
 * @brief Cortex-M7 sampling port application interface implementation.
 */

#include <stddef.h>

#include "maxrtos/sampling_port.h"
#include "maxrtos/arch/cortex_m7/svc.h"

maxrtos_status_t maxrtos_sampling_port_write(
    maxrtos_sampling_port_id_t id,
    void const * message,
    size_t message_size )
{
    register uint32_t r0 __asm( "r0" ) = ( uint32_t ) id;
    register uint32_t r1 __asm( "r1" ) = ( uint32_t ) message;
    register uint32_t r2 __asm( "r2" ) = ( uint32_t ) message_size;

    __asm volatile( "svc %3"
                    : "+r"( r0 )
                    : "r"( r1 ), "r"( r2 ), "i"( MAXRTOS_SVC_SAMPLING_WRITE )
                    : "memory" );

    return ( maxrtos_status_t ) r0;
}

maxrtos_status_t maxrtos_sampling_port_read(
    maxrtos_sampling_port_id_t id,
    void * out_message,
    size_t buffer_size )
{
    register uint32_t r0 __asm( "r0" ) = ( uint32_t ) id;
    register uint32_t r1 __asm( "r1" ) = ( uint32_t ) out_message;
    register uint32_t r2 __asm( "r2" ) = ( uint32_t ) buffer_size;

    __asm volatile( "svc %3"
                    : "+r"( r0 )
                    : "r"( r1 ), "r"( r2 ), "i"( MAXRTOS_SVC_SAMPLING_READ )
                    : "memory" );

    return ( maxrtos_status_t ) r0;
}
