/**
 * @file queue_port.h
 * @brief Fixed-capacity, fixed-size FIFO queuing port interface.
 *
 * Each port is configured with a fixed message size and maximum
 * message capacity. Messages are copied into and out of statically
 * allocated storage and are delivered in FIFO order.
 *
 * Sending to a full port and receiving from an empty port are
 * non-blocking operations that return an error status.
 */

#ifndef MAXRTOS_KERNEL_QUEUE_PORT_H
#define MAXRTOS_KERNEL_QUEUE_PORT_H

#include <stddef.h>
#include <stdint.h>

#include "maxrtos/status.h"
#include "maxrtos/config.h"

/**
 * @brief A single queuing port instance.
 *
 * @field buffer
 *     Statically allocated storage. The buffer is sized for the
 *     compile-time maximum queue capacity and message size.
 *
 * @field message_size
 *     Size, in bytes, of every message carried by this port.
 *     Fixed during initialization.
 *
 * @field capacity
 *     Maximum number of messages that may be stored simultaneously.
 *     Fixed during initialization.
 *
 * @field head
 *     Index, in message-slot units, of the oldest unread message.
 *
 * @field count
 *     Number of messages currently stored in the port.
 */
typedef struct
{
    uint8_t buffer[
        MAXRTOS_MAX_QUEUE_CAPACITY *
        MAXRTOS_MAX_QUEUE_MESSAGE_SIZE ];

    size_t message_size;
    size_t capacity;
    size_t head;
    size_t count;
} maxrtos_queue_port_t;

/**
 * @brief Initialize a queuing port.
 *
 * @param[out] port
 *     Port to initialize. Must not be NULL.
 *
 * @param[in] message_size
 *     Size, in bytes, of every message carried by the port.
 *     Must be greater than zero and no greater than
 *     MAXRTOS_MAX_QUEUE_MESSAGE_SIZE.
 *
 * @param[in] capacity
 *     Maximum number of messages the port can hold.
 *     Must be greater than zero and no greater than
 *     MAXRTOS_MAX_QUEUE_CAPACITY.
 *
 * @return 
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if any argument is invalid.
 */
maxrtos_status_t maxrtos_queue_port_init(
    maxrtos_queue_port_t * port,
    size_t message_size,
    size_t capacity );

/**
 * @brief Enqueue one message into the port.
 *
 * The operation is non-blocking. If the port is full, the message is
 * not stored and MAXRTOS_ERR_QUEUE_FULL is returned.
 *
 * @param[in,out] port
 *     Initialized port to which the message is sent.
 *
 * @param[in] message
 *     Caller-owned message buffer. Must not be NULL.
 *
 * @param[in] message_size
 *     Size of the message in bytes. Must exactly match the port's
 *     configured message size.
 *
 * @return 
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid.
 *     MAXRTOS_ERR_QUEUE_FULL if the port is at capacity.
 */
maxrtos_status_t maxrtos_queue_port_send(
    maxrtos_queue_port_t * port,
    void const * message,
    size_t message_size );

/**
 * @brief Dequeue the oldest message from the port.
 *
 * The operation is non-blocking. If the port is empty,
 * MAXRTOS_ERR_QUEUE_EMPTY is returned.
 *
 * @param[in,out] port
 *     Initialized port from which the message is received.
 *
 * @param[out] out_message
 *     Caller-owned output buffer. Must not be NULL and must provide
 *     at least message_size bytes of storage.
 *
 * @param[in] buffer_size
 *     Size of the output buffer in bytes. Must be at least the
 *     port's configured message size.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid.
 *     MAXRTOS_ERR_QUEUE_EMPTY if the port contains no messages.
 */
maxrtos_status_t maxrtos_queue_port_receive(
    maxrtos_queue_port_t * port,
    void * out_message,
    size_t buffer_size );

/**
 * @brief Return the number of messages currently stored.
 *
 * @param[in] port
 *     Port to query. If NULL, zero is returned.
 *
 * @return Number of messages currently stored in the port.
 */
size_t maxrtos_queue_port_count(
    maxrtos_queue_port_t const * port );

#endif /* MAXRTOS_KERNEL_QUEUE_PORT_H */
