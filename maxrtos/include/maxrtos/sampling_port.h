/**
 * @file sampling_port.h
 * @brief Public sampling port interface.
 *
 * A sampling port holds one message: a write replaces it, and a read returns
 * the latest without removing it. Neither blocks.
 *
 * A port is named by its generated ID (SAMPLING_PORT_ID_<name>, see the
 * generated configuration header). The port itself lives in kernel memory
 * that no partition can map; the kernel allows a call only from a partition
 * the module lists as a writer of the port (write) or a reader of it (read).
 * Any other call, and any ID that names no port, returns
 * MAXRTOS_ERR_INVALID_ID.
 *
 * The message and the receive buffer must lie in the calling process's own
 * partition memory (its stack domain); the kernel refuses an address outside
 * it, since it would copy there with privilege.
 *
 * The message has the fixed size the module gives the port. A write must pass
 * exactly that many bytes, and a read buffer must hold at least that many.
 */

#ifndef MAXRTOS_SAMPLING_PORT_H
#define MAXRTOS_SAMPLING_PORT_H

#include <stddef.h>

#include "maxrtos/status.h"
#include "maxrtos/types.h"

/**
 * @brief Write a message to a sampling port, replacing the one held.
 *
 * @param id           Sampling port ID.
 * @param message      Message to write.
 * @param message_size Size of the message in bytes.
 *
 * @return MAXRTOS_OK on success.
 * @return MAXRTOS_ERR_INVALID_ID if the ID names no port or the caller's
 *         partition is not a writer of it.
 * @return MAXRTOS_ERR_INVALID_ARG if the message is not the port's size or
 *         does not lie in memory the caller may read.
 */
maxrtos_status_t maxrtos_sampling_port_write(
    maxrtos_sampling_port_id_t id,
    void const * message,
    size_t message_size );

/**
 * @brief Read the latest message of a sampling port.
 *
 * @param id          Sampling port ID.
 * @param out_message Destination buffer.
 * @param buffer_size Size of the destination buffer in bytes.
 *
 * @return MAXRTOS_OK on success.
 * @return MAXRTOS_ERR_QUEUE_EMPTY if nothing has been written yet.
 * @return MAXRTOS_ERR_INVALID_ID if the ID names no port or the caller's
 *         partition is not a reader of it.
 * @return MAXRTOS_ERR_INVALID_ARG if the buffer is smaller than the port's
 *         message or does not lie in memory the caller may write.
 */
maxrtos_status_t maxrtos_sampling_port_read(
    maxrtos_sampling_port_id_t id,
    void * out_message,
    size_t buffer_size );

#endif /* MAXRTOS_SAMPLING_PORT_H */
