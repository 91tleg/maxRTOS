/**
 * @file queue_port.c
 * @brief Implementation of the queuing port interface.
 *
 * Provides a bounded, fixed-size FIFO queuing port for
 * inter-partition communication.
 */

#include <stddef.h>

#include "maxrtos/kernel/queue_port.h"

maxrtos_status_t maxrtos_queue_port_init(
    maxrtos_queue_port_t * port,
    size_t message_size,
    size_t capacity )
{
    maxrtos_status_t status;
 
    status = MAXRTOS_ERR_INVALID_ARG;
 
    if( ( port != NULL ) &&
        ( message_size <= MAXRTOS_MAX_QUEUE_MESSAGE_SIZE ) &&
        ( capacity <= MAXRTOS_MAX_QUEUE_CAPACITY ) )
    {
        status = maxrtos_kernel_fifo_init(
            &port->fifo,
            port->buffer,
            sizeof( port->buffer ),
            message_size,
            capacity );
    }
 
    return status;
}
 
maxrtos_status_t maxrtos_queue_port_send(
    maxrtos_queue_port_t * port,
    void const * message,
    size_t message_size )
{
    maxrtos_status_t status;
 
    status = MAXRTOS_ERR_INVALID_ARG;
 
    if( port != NULL )
    {
        status = maxrtos_kernel_fifo_push(
            &port->fifo,
            message,
            message_size );
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
 
    if( port != NULL )
    {
        status = maxrtos_kernel_fifo_pop(
            &port->fifo,
            out_message,
            buffer_size );
    }
 
    return status;
}
 
size_t maxrtos_queue_port_count(
    maxrtos_queue_port_t const * port )
{
    size_t count;
 
    count = 0U;
 
    if( port != NULL )
    {
        count = maxrtos_kernel_fifo_count( &port->fifo );
    }
 
    return count;
}
