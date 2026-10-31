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

/*
 * A port is named by its generated ID (PORT_ID_<name>, see the generated
 * configuration header). The port itself lives in kernel memory that no
 * partition can map; the kernel allows a call only from a partition the
 * module lists as a sender of the port (send) or a receiver of it (receive).
 * Any other call, and any ID that names no port, returns
 * MAXRTOS_ERR_INVALID_ID.
 *
 * The message and the receive buffer must lie in the calling process's own
 * partition memory (its stack domain); the kernel refuses an address outside
 * it, since it would copy there with privilege.
 *
 * Messages have the fixed size the module gives the port. A send must pass
 * exactly that many bytes, and a receive buffer must hold at least that many.
 */

/**
 * @brief Send a message to a queue port.
 *
 * Blocks until space is available or the timeout expires.
 *
 * @param id           Queue port ID.
 * @param message      Message to send.
 * @param message_size Size of the message in bytes.
 * @param timeout      Maximum time to wait.
 *
 * @return MAXRTOS_OK on success.
 * @return An appropriate error status otherwise.
 */
maxrtos_status_t maxrtos_queue_port_send(
    maxrtos_queue_port_id_t id,
    void const * message,
    size_t message_size,
    maxrtos_tick_t timeout );

/**
 * @brief Receive a message from a queue port.
 *
 * Blocks until a message is available or the timeout expires.
 *
 * @param id           Queue port ID.
 * @param out_message  Destination buffer.
 * @param buffer_size  Size of the destination buffer in bytes.
 * @param timeout      Maximum time to wait.
 *
 * @return MAXRTOS_OK on success.
 * @return An appropriate error status otherwise.
 */
maxrtos_status_t maxrtos_queue_port_receive(
    maxrtos_queue_port_id_t id,
    void * out_message,
    size_t buffer_size,
    maxrtos_tick_t timeout );

/**
 * @brief Return the number of messages currently queued.
 *
 * This operation does not block.
 *
 * @param id Queue port ID.
 *
 * @return Number of queued messages, or zero if the caller's partition is
 *         neither a sender nor a receiver of the port.
 */
size_t maxrtos_queue_port_count( maxrtos_queue_port_id_t id );

#endif /* MAXRTOS_QUEUE_PORT_H */
