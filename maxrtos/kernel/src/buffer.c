/**
 * @file buffer.c
 * @brief Kernel implementation of intra-partition ARINC 653 buffers.
 *
 * Owns the buffer pool and its FIFO storage. Blocking, timeouts and
 * resumption go through the shared IPC block services; a context switch
 * is left to the architecture layer.
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "maxrtos/config.h"

#include "maxrtos/kernel/buffer.h"
#include "maxrtos/kernel/internal/fifo.h"
#include "maxrtos/kernel/ipc_block.h"
#include "maxrtos/kernel/ipc_priority.h"
#include "maxrtos/kernel/tick.h"
#include "maxrtos/kernel/waitlist.h"

typedef struct
{
    bool in_use;
    maxrtos_partition_id_t partition_id;
    maxrtos_queuing_discipline_t discipline;

    uint8_t storage[
        MAXRTOS_MAX_BUFFER_CAPACITY *
        MAXRTOS_MAX_BUFFER_MESSAGE_SIZE ];
    maxrtos_kernel_fifo_t fifo;

    maxrtos_waitlist_t waiting_receivers;
    maxrtos_waitlist_t waiting_senders;
} maxrtos_buffer_t;

static maxrtos_buffer_t s_buffer_pool[ MAXRTOS_MAX_BUFFERS ];

/* Release one waiter per the buffer's queuing discipline, with its IPC
 * operation. */
static maxrtos_status_t maxrtos_buffer_wake_one(
    maxrtos_waitlist_t * waitlist,
    maxrtos_queuing_discipline_t discipline,
    maxrtos_process_id_t * out_id,
    maxrtos_ipc_operation_t * out_op )
{
    return ( discipline == MAXRTOS_QUEUING_PRIORITY ) ?
        maxrtos_ipc_take_best_waiter_op( waitlist, out_id, out_op ) :
        maxrtos_ipc_wake_one( waitlist, out_id, out_op );
}

void maxrtos_buffer_pool_init( void )
{
    size_t i;

    for( i = 0U; i < MAXRTOS_MAX_BUFFERS; i++ )
    {
        s_buffer_pool[ i ].in_use = false;
        s_buffer_pool[ i ].partition_id = MAXRTOS_INVALID_PARTITION_ID;
        s_buffer_pool[ i ].discipline = MAXRTOS_QUEUING_FIFO;
        ( void ) maxrtos_waitlist_init( &s_buffer_pool[ i ].waiting_receivers );
        ( void ) maxrtos_waitlist_init( &s_buffer_pool[ i ].waiting_senders );
    }
}

static maxrtos_buffer_t * maxrtos_buffer_get( maxrtos_buffer_id_t id )
{
    maxrtos_buffer_t * buffer;

    buffer = NULL;

    if( ( id < ( maxrtos_buffer_id_t ) MAXRTOS_MAX_BUFFERS ) &&
        ( s_buffer_pool[ id ].in_use == true ) )
    {
        buffer = &s_buffer_pool[ id ];
    }

    return buffer;
}

/* The buffer, if it exists and belongs to the caller's partition. */
static maxrtos_buffer_t * maxrtos_buffer_for_process(
    maxrtos_buffer_id_t id,
    maxrtos_process_id_t process_id )
{
    maxrtos_buffer_t * buffer;
    maxrtos_process_control_block_t const * pcb;

    buffer = maxrtos_buffer_get( id );
    pcb = maxrtos_process_get( process_id );

    if( ( pcb == NULL ) ||
        ( ( buffer != NULL ) &&
          ( buffer->partition_id != pcb->partition_id ) ) )
    {
        buffer = NULL;
    }

    return buffer;
}

maxrtos_status_t maxrtos_buffer_create(
    maxrtos_partition_id_t partition_id,
    size_t max_message_size,
    size_t max_nb_message,
    maxrtos_queuing_discipline_t discipline,
    maxrtos_buffer_id_t * out_id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( out_id != NULL ) &&
        ( partition_id < MAXRTOS_MAX_PARTITIONS ) &&
        ( max_message_size > 0U ) &&
        ( max_message_size <= MAXRTOS_MAX_BUFFER_MESSAGE_SIZE ) &&
        ( max_nb_message > 0U ) &&
        ( max_nb_message <= MAXRTOS_MAX_BUFFER_CAPACITY ) )
    {
        size_t i;

        status = MAXRTOS_ERR_POOL_FULL;

        for( i = 0U; ( i < MAXRTOS_MAX_BUFFERS ) &&
                     ( status != MAXRTOS_OK ); i++ )
        {
            if( s_buffer_pool[ i ].in_use == false )
            {
                status = maxrtos_kernel_fifo_init(
                    &s_buffer_pool[ i ].fifo,
                    s_buffer_pool[ i ].storage,
                    sizeof( s_buffer_pool[ i ].storage ),
                    max_message_size,
                    max_nb_message );

                if( status == MAXRTOS_OK )
                {
                    s_buffer_pool[ i ].in_use = true;
                    s_buffer_pool[ i ].partition_id = partition_id;
                    s_buffer_pool[ i ].discipline = discipline;
                    ( void ) maxrtos_waitlist_init(
                        &s_buffer_pool[ i ].waiting_receivers );
                    ( void ) maxrtos_waitlist_init(
                        &s_buffer_pool[ i ].waiting_senders );

                    *out_id = ( maxrtos_buffer_id_t ) i;
                }
            }
        }
    }

    return status;
}

maxrtos_status_t maxrtos_kernel_buffer_send(
    maxrtos_buffer_id_t id,
    void const * message,
    size_t message_size,
    maxrtos_tick_t timeout,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id )
{
    maxrtos_status_t status;
    maxrtos_buffer_t * buffer;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( message != NULL ) &&
        ( table != NULL ) &&
        ( out_next_id != NULL ) )
    {
        buffer = maxrtos_buffer_for_process( id, current_id );

        if( buffer == NULL )
        {
            status = MAXRTOS_ERR_INVALID_ID;
        }
        else if( message_size != buffer->fifo.message_size )
        {
            status = MAXRTOS_ERR_INVALID_ARG;
        }
        else
        {
            maxrtos_process_id_t receiver_id;
            maxrtos_ipc_operation_t receiver_op;

            if( maxrtos_buffer_wake_one(
                    &buffer->waiting_receivers,
                    buffer->discipline,
                    &receiver_id,
                    &receiver_op ) == MAXRTOS_OK )
            {
                /* A receiver only blocks on an empty buffer, so the
                 * message goes straight to it. Copying here keeps FIFO
                 * order and means the receiver needs no second call. */
                if( ( receiver_op.kind == MAXRTOS_IPC_OP_BUFFER_RECEIVE ) &&
                    ( receiver_op.payload.buffer_receive.buffer_size >=
                      message_size ) )
                {
                    ( void ) memcpy(
                        receiver_op.payload.buffer_receive.out_message,
                        message,
                        message_size );

                    status = maxrtos_ipc_resolve(
                        table, receiver_id, MAXRTOS_OK );
                }
                else
                {
                    /* Not a usable receiver. Resume it with an error and
                     * keep the message queued. */
                    ( void ) maxrtos_ipc_resolve(
                        table, receiver_id, MAXRTOS_ERR_INVALID_STATE );

                    status = maxrtos_kernel_fifo_push(
                        &buffer->fifo, message, message_size );
                }
            }
            else
            {
                status = maxrtos_kernel_fifo_push(
                    &buffer->fifo, message, message_size );
            }

            if( ( status == MAXRTOS_ERR_QUEUE_FULL ) && ( timeout != 0U ) )
            {
                maxrtos_tick_t wake_tick;

                wake_tick = MAXRTOS_TICK_NONE;
                status = maxrtos_kernel_tick_after( timeout, &wake_tick );

                if( status == MAXRTOS_OK )
                {
                    maxrtos_ipc_operation_t op;

                    op.kind = MAXRTOS_IPC_OP_BUFFER_SEND;
                    op.payload.buffer_send.id = id;
                    op.payload.buffer_send.message = message;
                    op.payload.buffer_send.message_size = message_size;

                    status = maxrtos_ipc_block_current(
                        table,
                        current_id,
                        &buffer->waiting_senders,
                        wake_tick,
                        &op,
                        out_next_id );

                    if( status == MAXRTOS_OK )
                    {
                        status = MAXRTOS_PENDING;
                    }
                    else if( status == MAXRTOS_ERR_QUEUE_EMPTY )
                    {
                        /* No other process in the partition to run, so
                         * the caller cannot be blocked: report the
                         * buffer state as a non-blocking send would. */
                        status = MAXRTOS_ERR_QUEUE_FULL;
                    }
                    else
                    {
                        /* Any other status is returned as is. */
                    }
                }
            }
        }
    }

    return status;
}

maxrtos_status_t maxrtos_kernel_buffer_receive(
    maxrtos_buffer_id_t id,
    void * out_message,
    size_t buffer_size,
    maxrtos_tick_t timeout,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id )
{
    maxrtos_status_t status;
    maxrtos_buffer_t * buffer;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( out_message != NULL ) &&
        ( table != NULL ) &&
        ( out_next_id != NULL ) )
    {
        buffer = maxrtos_buffer_for_process( id, current_id );

        if( buffer == NULL )
        {
            status = MAXRTOS_ERR_INVALID_ID;
        }
        else
        {
            status = maxrtos_kernel_fifo_pop(
                &buffer->fifo, out_message, buffer_size );

            if( status == MAXRTOS_OK )
            {
                maxrtos_process_id_t sender_id;
                maxrtos_ipc_operation_t sender_op;

                /* A slot just freed: admit one blocked sender. */
                if( maxrtos_buffer_wake_one(
                        &buffer->waiting_senders,
                        buffer->discipline,
                        &sender_id,
                        &sender_op ) == MAXRTOS_OK )
                {
                    maxrtos_status_t sender_result;

                    sender_result = MAXRTOS_ERR_INVALID_STATE;

                    if( sender_op.kind == MAXRTOS_IPC_OP_BUFFER_SEND )
                    {
                        sender_result = maxrtos_kernel_fifo_push(
                            &buffer->fifo,
                            sender_op.payload.buffer_send.message,
                            sender_op.payload.buffer_send.message_size );
                    }

                    ( void ) maxrtos_ipc_resolve(
                        table, sender_id, sender_result );
                }
            }
            else if( ( status == MAXRTOS_ERR_QUEUE_EMPTY ) &&
                     ( timeout != 0U ) )
            {
                maxrtos_tick_t wake_tick;

                wake_tick = MAXRTOS_TICK_NONE;
                status = maxrtos_kernel_tick_after( timeout, &wake_tick );

                if( status == MAXRTOS_OK )
                {
                    maxrtos_ipc_operation_t op;

                    op.kind = MAXRTOS_IPC_OP_BUFFER_RECEIVE;
                    op.payload.buffer_receive.id = id;
                    op.payload.buffer_receive.out_message = out_message;
                    op.payload.buffer_receive.buffer_size = buffer_size;

                    status = maxrtos_ipc_block_current(
                        table,
                        current_id,
                        &buffer->waiting_receivers,
                        wake_tick,
                        &op,
                        out_next_id );

                    if( status == MAXRTOS_OK )
                    {
                        status = MAXRTOS_PENDING;
                    }
                    else
                    {
                        /* No other process in the partition to run (or the
                         * call was rejected): return the status as a
                         * non-blocking receive would. */
                    }
                }
            }
            else
            {
                /* Non-blocking receive, or an error: return the status. */
            }
        }
    }

    return status;
}

maxrtos_status_t maxrtos_kernel_buffer_get_status(
    maxrtos_buffer_id_t id,
    maxrtos_process_id_t current_id,
    maxrtos_buffer_status_t * out_status )
{
    maxrtos_status_t status;
    maxrtos_buffer_t const * buffer;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( out_status != NULL )
    {
        buffer = maxrtos_buffer_for_process( id, current_id );

        if( buffer == NULL )
        {
            status = MAXRTOS_ERR_INVALID_ID;
        }
        else
        {
            out_status->max_message_size = buffer->fifo.message_size;
            out_status->max_nb_message = buffer->fifo.capacity;
            out_status->nb_message = maxrtos_kernel_fifo_count( &buffer->fifo );
            out_status->waiting_processes =
                ( uint32_t ) ( maxrtos_waitlist_count( &buffer->waiting_senders ) +
                               maxrtos_waitlist_count( &buffer->waiting_receivers ) );
            status = MAXRTOS_OK;
        }
    }

    return status;
}
