/**
 * @file queue_port.c
 * @brief Implementation of the queuing port interface.
 *
 * Provides a bounded, fixed-size FIFO communication port for
 * inter-process communication.
 *
 * The implementation uses a circular buffer to maintain FIFO
 * ordering while avoiding dynamic memory allocation.
 */

#include <stddef.h>
#include <string.h>

#include "maxrtos/kernel/queue_port.h"

maxrtos_status_t maxrtos_queue_port_init(
    maxrtos_queue_port_t * port,
    size_t message_size,
    size_t capacity )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( port != NULL ) &&
        ( message_size > 0U ) &&
        ( message_size <= MAXRTOS_MAX_QUEUE_MESSAGE_SIZE ) &&
        ( capacity > 0U ) &&
        ( capacity <= MAXRTOS_MAX_QUEUE_CAPACITY ) )
    {
        port->message_size = message_size;
        port->capacity = capacity;
        port->head = 0U;
        port->count = 0U;

        status = MAXRTOS_OK;
    }

    return status;
}

maxrtos_status_t maxrtos_queue_port_send(
    maxrtos_queue_port_t * port,
    void const * message,
    size_t message_size )
{
    maxrtos_status_t status;
    size_t tail;

    status = MAXRTOS_ERR_INVALID_ARG;
    tail = 0;

    if( ( port != NULL ) &&
        ( message != NULL ) &&
        ( message_size == port->message_size ) )
    {
        if( port->count < port->capacity )
        {
            tail = ( port->head + port->count ) % port->capacity;

            ( void ) memcpy(
                &port->buffer[ tail * port->message_size ],
                message,
                port->message_size );

            port->count++;
            status = MAXRTOS_OK;
        }
        else
        {
            status = MAXRTOS_ERR_QUEUE_FULL;
        }
    }

    return status;
}

maxrtos_status_t maxrtos_queue_port_receive(
    maxrtos_queue_port_t * port,
    void * out_message,
    size_t buffer_size )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( port != NULL ) &&
        ( out_message != NULL ) &&
        ( buffer_size >= port->message_size ) )
    {
        if( port->count == 0U )
        {
            status = MAXRTOS_ERR_QUEUE_EMPTY;
        }
        else
        {
            ( void ) memcpy(
                out_message,
                &port->buffer[ port->head * port->message_size ],
                port->message_size );

            port->head = ( port->head + 1U ) % port->capacity;
            port->count--;
            status = MAXRTOS_OK;
        }
    }

    return status;
}

size_t maxrtos_queue_port_count(
    maxrtos_queue_port_t const * port )
{
    size_t count;

    count = 0;

    if( port != NULL )
    {
        count = port->count;
    }

    return count;
}