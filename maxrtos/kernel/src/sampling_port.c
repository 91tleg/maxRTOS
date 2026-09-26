/**
 * @file sampling_port.c
 * @brief Implementation of the sampling port interface.
 *
 * Provides a fixed-size, single-message sampling port.
 *
 * A write replaces the previously sampled message. A read returns
 * the most recently written message without removing it.
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "maxrtos/kernel/sampling_port.h"

maxrtos_status_t maxrtos_sampling_port_init(
    maxrtos_sampling_port_t * port,
    size_t message_size )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( port != NULL ) &&
        ( message_size > 0U ) &&
        ( message_size <= MAXRTOS_MAX_QUEUE_MESSAGE_SIZE ) )
    {
        port->message_size = message_size;
        port->has_message = false;

        status = MAXRTOS_OK;
    }

    return status;
}

maxrtos_status_t maxrtos_sampling_port_write(
    maxrtos_sampling_port_t * port,
    void const * message )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( port != NULL ) &&
        ( message != NULL ) )
    {
        ( void ) memcpy( port->buffer,
                         message,
                         port->message_size );

        port->has_message = true;

        status = MAXRTOS_OK;
    }

    return status;
}

maxrtos_status_t maxrtos_sampling_port_read(
    maxrtos_sampling_port_t const * port,
    void * out_message,
    size_t buffer_size )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( port != NULL ) &&
        ( out_message != NULL ) &&
        ( buffer_size >= port->message_size ) )
    {
        if( port->has_message == false )
        {
            status = MAXRTOS_ERR_QUEUE_EMPTY;
        }
        else
        {
            ( void ) memcpy( out_message,
                             port->buffer,
                             port->message_size );

            status = MAXRTOS_OK;
        }
    }

    return status;
}

bool maxrtos_sampling_port_is_valid(
    maxrtos_sampling_port_t const * port )
{
    bool valid;

    valid = false;

    if( port != NULL )
    {
        valid = port->has_message;
    }

    return valid;
}
