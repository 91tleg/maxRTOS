/**
 * @file queue_port.h
 * @brief Public queue port interface.
 */

#ifndef MAXRTOS_QUEUE_PORT_H
#define MAXRTOS_QUEUE_PORT_H

#include <stddef.h>

#include "maxrtos/status.h"
#include "maxrtos/types.h"
#include "maxrtos/config.h"

typedef struct maxrtos_queue_port_s maxrtos_queue_port_t;

/**
 * @brief Send a message to a queue port.
 *
 * Blocks until space is available or the timeout expires.
 *
 * @param port         Queue port.
 * @param message      Message to send.
 * @param message_size Size of the message in bytes.
 * @param timeout      Maximum time to wait.
 *
 * @return MAXRTOS_OK on success.
 * @return An appropriate error status otherwise.
 */
maxrtos_status_t maxrtos_queue_port_send(
    maxrtos_queue_port_t * port,
    void const * message,
    size_t message_size,
    maxrtos_tick_t timeout );

/**
 * @brief Receive a message from a queue port.
 *
 * Blocks until a message is available or the timeout expires.
 *
 * @param port         Queue port.
 * @param out_message  Destination buffer.
 * @param buffer_size  Size of the destination buffer in bytes.
 * @param timeout      Maximum time to wait.
 *
 * @return MAXRTOS_OK on success.
 * @return An appropriate error status otherwise.
 */
maxrtos_status_t maxrtos_queue_port_receive(
    maxrtos_queue_port_t * port,
    void * out_message,
    size_t buffer_size,
    maxrtos_tick_t timeout );

/**
 * @brief Return the number of messages currently queued.
 *
 * This operation does not block.
 *
 * @param port Queue port.
 *
 * @return Number of queued messages.
 */
size_t maxrtos_queue_port_count(
    maxrtos_queue_port_t const * port );

#endif /* MAXRTOS_QUEUE_PORT_H */
