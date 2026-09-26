/**
 * @file buffer.h
 * @brief Public buffer interface.
 *
 * A FIFO message queue shared by the processes of one partition.
 * Any process of the partition may send or receive; the sender and
 * receiver need not be fixed roles, unlike a queuing port.
 */

#ifndef MAXRTOS_BUFFER_H
#define MAXRTOS_BUFFER_H

#include <stddef.h>
#include <stdint.h>

#include "maxrtos/status.h"
#include "maxrtos/types.h"

/**
 * @brief Snapshot of a buffer.
 */
typedef struct
{
    size_t max_message_size;    /* configured message size, in bytes */
    size_t max_nb_message;      /* configured message capacity */
    size_t nb_message;          /* messages currently stored */
    uint32_t waiting_processes; /* processes currently blocked */
} maxrtos_buffer_status_t;

/**
 * @brief Send a message to a buffer.
 *
 * Blocks until space is available or the timeout expires.
 *
 * @param id           Buffer handle. Must belong to the caller's partition.
 * @param message      Message to send.
 * @param message_size Size of the message in bytes. Must equal the
 *                      buffer's configured message size.
 * @param timeout      Ticks to wait if the buffer is full: 0 returns
 *                      immediately, MAXRTOS_TIMEOUT_INFINITE waits without
 *                      limit.
 *
 * @return MAXRTOS_OK on success.
 * @return MAXRTOS_ERR_QUEUE_FULL if it could not be sent in time.
 * @return MAXRTOS_ERR_INVALID_ID if the handle is invalid or belongs to
 *         another partition.
 */
maxrtos_status_t maxrtos_buffer_send(
    maxrtos_buffer_id_t id,
    void const * message,
    size_t message_size,
    maxrtos_tick_t timeout );

/**
 * @brief Receive a message from a buffer.
 *
 * Blocks until a message is available or the timeout expires.
 *
 * Every message carried by a buffer is exactly its configured message
 * size (see maxrtos_buffer_get_status()), so the received length is not
 * reported separately; it always equals that size.
 *
 * @param id           Buffer handle. Must belong to the caller's
 *                     partition.
 * @param out_message  Destination buffer.
 * @param buffer_size  Size of the destination buffer in bytes. Must be
 *                     at least the buffer's configured message size.
 * @param timeout      Ticks to wait if the buffer is empty: 0 returns
 *                     immediately, MAXRTOS_TIMEOUT_INFINITE waits without
 *                     limit.
 *
 * @return MAXRTOS_OK on success.
 * @return MAXRTOS_ERR_QUEUE_EMPTY if no message arrived in time.
 * @return MAXRTOS_ERR_INVALID_ID if the handle is invalid or belongs to
 *         another partition.
 */
maxrtos_status_t maxrtos_buffer_receive(
    maxrtos_buffer_id_t id,
    void * out_message,
    size_t buffer_size,
    maxrtos_tick_t timeout );

/**
 * @brief Read a buffer's configuration, fill level and waiter count
 *        (ARINC 653 GET_BUFFER_STATUS).
 *
 * Does not block. The values can change as soon as the call returns.
 *
 * @param id      Buffer handle. Must belong to the caller's partition.
 * @param status  Receives the snapshot. Must lie in the caller's own
 *                partition memory.
 *
 * @return MAXRTOS_OK on success.
 * @return MAXRTOS_ERR_INVALID_ID if the handle is invalid or belongs to
 *         another partition.
 * @return MAXRTOS_ERR_INVALID_ARG if status is NULL or not in the
 *         caller's partition memory.
 */
maxrtos_status_t maxrtos_buffer_get_status(
    maxrtos_buffer_id_t id,
    maxrtos_buffer_status_t * status );

#endif /* MAXRTOS_BUFFER_H */
