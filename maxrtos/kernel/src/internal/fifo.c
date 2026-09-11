/**
 * @file fifo.c
 * @brief Implementation of the internal shared FIFO engine.
 */

#include <string.h>

#include "maxrtos/kernel/internal/fifo.h"

maxrtos_status_t maxrtos_kernel_fifo_init(
    maxrtos_kernel_fifo_t * fifo,
    uint8_t * backing_buffer,
    size_t backing_buffer_size,
    size_t message_size,
    size_t capacity )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( fifo != NULL ) &&
        ( backing_buffer != NULL ) &&
        ( message_size > 0U ) &&
        ( capacity > 0U ) &&
        ( ( message_size * capacity ) <= backing_buffer_size ) )
    {
        fifo->buffer = backing_buffer;
        fifo->buffer_capacity_bytes = backing_buffer_size;
        fifo->message_size = message_size;
        fifo->capacity = capacity;
        fifo->head = 0U;
        fifo->count = 0U;

        status = MAXRTOS_OK;
    }

    return status;
}

maxrtos_status_t maxrtos_kernel_fifo_push(
    maxrtos_kernel_fifo_t * fifo,
    void const * message,
    size_t message_size )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( fifo != NULL ) &&
        ( message != NULL ) &&
        ( message_size == fifo->message_size ) )
    {
        if( fifo->count < fifo->capacity )
        {
            size_t tail;

            tail = ( fifo->head + fifo->count ) % fifo->capacity;

            ( void ) memcpy(
                &fifo->buffer[ tail * fifo->message_size ],
                message,
                fifo->message_size );

            fifo->count++;
            status = MAXRTOS_OK;
        }
        else
        {
            status = MAXRTOS_ERR_QUEUE_FULL;
        }
    }

    return status;
}

maxrtos_status_t maxrtos_kernel_fifo_pop(
    maxrtos_kernel_fifo_t * fifo,
    void * out_message,
    size_t buffer_size )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( fifo != NULL ) &&
        ( out_message != NULL ) &&
        ( buffer_size >= fifo->message_size ) )
    {
        if( fifo->count == 0U )
        {
            status = MAXRTOS_ERR_QUEUE_EMPTY;
        }
        else
        {
            ( void ) memcpy(
                out_message,
                &fifo->buffer[ fifo->head * fifo->message_size ],
                fifo->message_size );

            fifo->head = ( fifo->head + 1U ) % fifo->capacity;
            fifo->count--;
            status = MAXRTOS_OK;
        }
    }

    return status;
}

size_t maxrtos_kernel_fifo_count(
    maxrtos_kernel_fifo_t const * fifo )
{
    size_t count;

    count = 0U;

    if( fifo != NULL )
    {
        count = fifo->count;
    }

    return count;
}
