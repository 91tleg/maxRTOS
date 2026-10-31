/**
 * @file sampling_port.c
 * @brief Implementation of the sampling port interface.
 *
 * A pool of fixed-size, single-message sampling ports in kernel memory.
 * A write replaces the message held; a read returns it without removing it.
 * See sampling_port.h for how ports are named and who may use them.
 *
 * This module contains no architecture-specific code.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/sampling_port.h"

typedef struct
{
    bool in_use;
    uint32_t writer_mask;
    uint32_t reader_mask;

    void * storage;
    size_t message_size;
    bool has_message;
} maxrtos_sampling_port_t;

static maxrtos_sampling_port_t
    s_sampling_port_pool[ MAXRTOS_MAX_SAMPLING_PORTS ];

/* The partition bits of a mask must name partitions that exist. */
#define MAXRTOS_PARTITION_MASK_VALID( mask_ )                                  \
    ( ( ( mask_ ) >> MAXRTOS_MAX_PARTITIONS ) == 0U )

_Static_assert(
    MAXRTOS_MAX_PARTITIONS < 32U,
    "a partition set is one bit per partition in 32 bits" );

void maxrtos_sampling_port_pool_init( void )
{
    size_t i;

    for( i = 0U; i < MAXRTOS_MAX_SAMPLING_PORTS; i++ )
    {
        s_sampling_port_pool[ i ].in_use = false;
        s_sampling_port_pool[ i ].writer_mask = 0U;
        s_sampling_port_pool[ i ].reader_mask = 0U;
        s_sampling_port_pool[ i ].storage = NULL;
        s_sampling_port_pool[ i ].message_size = 0U;
        s_sampling_port_pool[ i ].has_message = false;
    }
}

/* The port, if it exists and the process's partition is in the writer set
 * (when @p writing), the reader set (when @p reading), or either. */
static maxrtos_sampling_port_t * maxrtos_sampling_port_for_process(
    maxrtos_sampling_port_id_t id,
    maxrtos_process_id_t process_id,
    bool writing,
    bool reading )
{
    maxrtos_sampling_port_t * port;
    maxrtos_process_control_block_t const * pcb;

    port = NULL;
    pcb = maxrtos_process_get( process_id );

    if( ( pcb != NULL ) && ( pcb->partition_id < MAXRTOS_MAX_PARTITIONS ) &&
        ( id < ( maxrtos_sampling_port_id_t ) MAXRTOS_MAX_SAMPLING_PORTS ) &&
        ( s_sampling_port_pool[ id ].in_use == true ) )
    {
        uint32_t allowed;

        allowed = 0U;

        if( writing )
        {
            allowed |= s_sampling_port_pool[ id ].writer_mask;
        }

        if( reading )
        {
            allowed |= s_sampling_port_pool[ id ].reader_mask;
        }

        if( ( allowed & ( UINT32_C( 1 ) << pcb->partition_id ) ) != 0U )
        {
            port = &s_sampling_port_pool[ id ];
        }
    }

    return port;
}

maxrtos_status_t maxrtos_sampling_port_create(
    void * storage,
    size_t storage_size,
    size_t message_size,
    uint32_t writer_mask,
    uint32_t reader_mask,
    maxrtos_sampling_port_id_t * out_id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( storage != NULL ) && ( out_id != NULL ) && ( message_size > 0U ) &&
        ( storage_size >= message_size ) && ( writer_mask != 0U ) &&
        ( reader_mask != 0U ) && MAXRTOS_PARTITION_MASK_VALID( writer_mask ) &&
        MAXRTOS_PARTITION_MASK_VALID( reader_mask ) )
    {
        size_t i;

        status = MAXRTOS_ERR_POOL_FULL;

        for( i = 0U;
             ( i < MAXRTOS_MAX_SAMPLING_PORTS ) && ( status != MAXRTOS_OK );
             i++ )
        {
            if( s_sampling_port_pool[ i ].in_use == false )
            {
                s_sampling_port_pool[ i ].in_use = true;
                s_sampling_port_pool[ i ].writer_mask = writer_mask;
                s_sampling_port_pool[ i ].reader_mask = reader_mask;
                s_sampling_port_pool[ i ].storage = storage;
                s_sampling_port_pool[ i ].message_size = message_size;
                s_sampling_port_pool[ i ].has_message = false;

                *out_id = ( maxrtos_sampling_port_id_t ) i;
                status = MAXRTOS_OK;
            }
        }
    }

    return status;
}

maxrtos_status_t maxrtos_kernel_sampling_port_write(
    maxrtos_sampling_port_id_t id,
    void const * message,
    size_t message_size,
    maxrtos_process_id_t current_id )
{
    maxrtos_status_t status;
    maxrtos_sampling_port_t * port;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( message != NULL )
    {
        port = maxrtos_sampling_port_for_process( id, current_id, true, false );

        if( port == NULL )
        {
            status = MAXRTOS_ERR_INVALID_ID;
        }
        else if( message_size == port->message_size )
        {
            ( void ) memcpy( port->storage, message, port->message_size );
            port->has_message = true;
            status = MAXRTOS_OK;
        }
        else
        {
            /* Not the port's message size. */
        }
    }

    return status;
}

maxrtos_status_t maxrtos_kernel_sampling_port_read(
    maxrtos_sampling_port_id_t id,
    void * out_message,
    size_t buffer_size,
    maxrtos_process_id_t current_id )
{
    maxrtos_status_t status;
    maxrtos_sampling_port_t const * port;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( out_message != NULL )
    {
        port = maxrtos_sampling_port_for_process( id, current_id, false, true );

        if( port == NULL )
        {
            status = MAXRTOS_ERR_INVALID_ID;
        }
        else if( buffer_size < port->message_size )
        {
            /* The buffer cannot hold a message. */
        }
        else if( port->has_message == false )
        {
            status = MAXRTOS_ERR_QUEUE_EMPTY;
        }
        else
        {
            ( void ) memcpy( out_message, port->storage, port->message_size );
            status = MAXRTOS_OK;
        }
    }

    return status;
}

maxrtos_status_t maxrtos_kernel_sampling_port_get_status(
    maxrtos_sampling_port_id_t id,
    maxrtos_process_id_t current_id,
    maxrtos_sampling_port_status_t * out_status )
{
    maxrtos_status_t status;
    maxrtos_sampling_port_t const * port;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( out_status != NULL )
    {
        port = maxrtos_sampling_port_for_process( id, current_id, true, true );

        if( port == NULL )
        {
            status = MAXRTOS_ERR_INVALID_ID;
        }
        else
        {
            out_status->message_size = port->message_size;
            out_status->has_message = port->has_message;
            status = MAXRTOS_OK;
        }
    }

    return status;
}
