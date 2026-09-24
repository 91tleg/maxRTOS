/**
 * @file queue_port.c
 * @brief Implementation of the kernel queuing port interface.
 *
 * Provides bounded FIFO queuing ports and blocking send/receive
 * operations using kernel wait-list and partition scheduling services.
 *
 * This module contains no architecture-specific code.
 */

#include <stddef.h>
#include <string.h>

#include "maxrtos/kernel/queue_port.h"
#include "maxrtos/kernel/tick.h"
#include "maxrtos/kernel/ipc_block.h"

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
        status = maxrtos_kernel_fifo_init(
            &port->fifo,
            port->buffer,
            sizeof( port->buffer ),
            message_size,
            capacity );

        if( status == MAXRTOS_OK )
        {
            status = maxrtos_waitlist_init(
                &port->waiting_receivers );
        }

        if( status == MAXRTOS_OK )
        {
            status = maxrtos_waitlist_init(
                &port->waiting_senders );
        }
    }

    return status;
}

size_t maxrtos_kernel_queue_port_count(
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

maxrtos_status_t maxrtos_kernel_queue_port_send(
    maxrtos_queue_port_t * port,
    void const * message,
    size_t message_size,
    maxrtos_tick_t timeout,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( port != NULL ) &&
        ( message != NULL ) &&
        ( message_size == port->fifo.message_size ) &&
        ( table != NULL ) &&
        ( out_next_id != NULL ) )
    {
        maxrtos_process_id_t receiver_id;
        maxrtos_ipc_operation_t receiver_op;

        if( maxrtos_ipc_wake_one(
                &port->waiting_receivers,
                &receiver_id,
                &receiver_op ) == MAXRTOS_OK )
        {
            /* A receiver only blocks on an empty queue, so the message
             * goes straight to it. Copying here keeps FIFO order and
             * means the receiver needs no second call. */
            if( ( receiver_op.kind == MAXRTOS_IPC_OP_QUEUE_RECEIVE ) &&
                ( receiver_op.payload.queue_receive.buffer_size >= message_size ) )
            {
                ( void ) memcpy(
                    receiver_op.payload.queue_receive.out_message,
                    message,
                    message_size );

                status = maxrtos_ipc_resolve( table, receiver_id, MAXRTOS_OK );
            }
            else
            {
                /* Not a usable receiver. Resume it with an error and keep
                 * the message queued. */
                ( void ) maxrtos_ipc_resolve(
                    table, receiver_id, MAXRTOS_ERR_INVALID_STATE );

                status = maxrtos_kernel_fifo_push(
                    &port->fifo, message, message_size );
            }
        }
        else
        {
            status = maxrtos_kernel_fifo_push(
                &port->fifo, message, message_size );
        }

        if( ( status == MAXRTOS_ERR_QUEUE_FULL ) && ( timeout != 0U ) )
        {
            maxrtos_tick_t wake_tick;

            wake_tick = MAXRTOS_TICK_NONE;
            status = maxrtos_kernel_tick_after( timeout, &wake_tick );

            if( status == MAXRTOS_OK )
            {
                maxrtos_ipc_operation_t op;

                op.kind = MAXRTOS_IPC_OP_QUEUE_SEND;
                op.payload.queue_send.port = port;
                op.payload.queue_send.message = message;
                op.payload.queue_send.message_size = message_size;

                status = maxrtos_ipc_block_current(
                    table,
                    current_id,
                    &port->waiting_senders,
                    wake_tick,
                    &op,
                    out_next_id );

                if( status == MAXRTOS_OK )
                {
                    status = MAXRTOS_PENDING;
                }
                else if( status == MAXRTOS_ERR_QUEUE_EMPTY )
                {
                    /* No other process in the partition to run, so the
                     * caller cannot be blocked: report the queue state
                     * as a non-blocking send would. */
                    status = MAXRTOS_ERR_QUEUE_FULL;
                }
                else
                {
                    /* Any other status is returned as is. */
                }
            }
        }
    }

    return status;
}

maxrtos_status_t maxrtos_kernel_queue_port_receive(
    maxrtos_queue_port_t * port,
    void * out_message,
    size_t buffer_size,
    maxrtos_tick_t timeout,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( port != NULL ) &&
        ( out_message != NULL ) &&
        ( table != NULL ) &&
        ( out_next_id != NULL ) )
    {
        status = maxrtos_kernel_fifo_pop(
            &port->fifo,
            out_message,
            buffer_size );

        if( status == MAXRTOS_OK )
        {
            maxrtos_process_id_t sender_id;
            maxrtos_ipc_operation_t sender_op;

            /* A slot just freed: admit one blocked sender. */
            if( maxrtos_ipc_wake_one(
                    &port->waiting_senders,
                    &sender_id,
                    &sender_op ) == MAXRTOS_OK )
            {
                maxrtos_status_t sender_result;

                sender_result = MAXRTOS_ERR_INVALID_STATE;

                if( sender_op.kind == MAXRTOS_IPC_OP_QUEUE_SEND )
                {
                    sender_result = maxrtos_kernel_fifo_push(
                        &port->fifo,
                        sender_op.payload.queue_send.message,
                        sender_op.payload.queue_send.message_size );
                }

                ( void ) maxrtos_ipc_resolve( table, sender_id, sender_result );
            }
        }
        else if( ( status == MAXRTOS_ERR_QUEUE_EMPTY ) && ( timeout != 0U ) )
        {
            maxrtos_tick_t wake_tick;

            wake_tick = MAXRTOS_TICK_NONE;
            status = maxrtos_kernel_tick_after( timeout, &wake_tick );

            if( status == MAXRTOS_OK )
            {
                maxrtos_ipc_operation_t op;

                op.kind = MAXRTOS_IPC_OP_QUEUE_RECEIVE;
                op.payload.queue_receive.port = port;
                op.payload.queue_receive.out_message = out_message;
                op.payload.queue_receive.buffer_size = buffer_size;

                status = maxrtos_ipc_block_current(
                    table,
                    current_id,
                    &port->waiting_receivers,
                    wake_tick,
                    &op,
                    out_next_id );

                if( status == MAXRTOS_OK )
                {
                    status = MAXRTOS_PENDING;
                }
                else
                {
                    /* No other process in the partition to run (or the call
                     * was rejected): return the status as a non-blocking
                     * receive would. */
                }
            }
        }
        else
        {
            /* Non-blocking receive, or an error: return the status. */
        }
    }

    return status;
}
