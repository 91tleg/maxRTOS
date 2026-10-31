/**
 * @file sampling_port.h
 * @brief Kernel interface for fixed-size sampling ports.
 *
 * A sampling port holds exactly one message. A write replaces the message
 * held, and a read returns the latest one without removing it. Neither
 * blocks. Reading before the first write returns an error.
 *
 * Like queuing ports, sampling ports live in a kernel pool and are named by
 * ID. The pool, the port state and the message storage are kernel memory
 * that no partition maps; a partition reaches a port only through a system
 * call, and every call checks the caller's partition against the writer or
 * reader set the port was created with.
 *
 * This interface is kernel-internal. Partition code must use the public
 * maxrtos/sampling_port.h interface, which enters the kernel through the
 * architecture-specific syscall mechanism.
 */

#ifndef MAXRTOS_KERNEL_SAMPLING_PORT_H
#define MAXRTOS_KERNEL_SAMPLING_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "maxrtos/config.h"
#include "maxrtos/sampling_port.h"
#include "maxrtos/status.h"
#include "maxrtos/types.h"

/**
 * @brief State of a sampling port, for the caller's own information.
 */
typedef struct
{
    size_t message_size;

    /* True once a message has been written. */
    bool has_message;
} maxrtos_sampling_port_status_t;

/**
 * @brief Empty the sampling port pool.
 *
 * Every port is released. The generated configuration calls this before it
 * creates its ports.
 */
void maxrtos_sampling_port_pool_init( void );

/**
 * @brief Create a sampling port.
 *
 * @param[in] storage
 *     Memory for the message. It must stay valid for as long as the port
 *     exists, and must be kernel memory: the kernel copies messages in and
 *     out of it with privilege, and no partition may be able to map it.
 *
 * @param[in] storage_size
 *     Size of storage in bytes; at least message_size.
 *
 * @param[in] message_size
 *     Size of the message in bytes. Fixed for the life of the port.
 *
 * @param[in] writer_mask
 *     Partitions that may write: bit n set for partition n. Not empty.
 *
 * @param[in] reader_mask
 *     Partitions that may read: bit n set for partition n. Not empty.
 *
 * @param[out] out_id
 *     Receives the ID of the new port.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid, a mask is empty or
 *     names a partition that does not exist, or storage is too small.
 *     MAXRTOS_ERR_POOL_FULL if MAXRTOS_MAX_SAMPLING_PORTS ports exist.
 */
maxrtos_status_t maxrtos_sampling_port_create(
    void * storage,
    size_t storage_size,
    size_t message_size,
    uint32_t writer_mask,
    uint32_t reader_mask,
    maxrtos_sampling_port_id_t * out_id );

/**
 * @brief Write a message to a sampling port, replacing the one held.
 *
 * @param[in] id
 *     Port to write. The process's partition must be one of its writers.
 *
 * @param[in] message
 *     The message.
 *
 * @param[in] message_size
 *     Must equal the port's message size.
 *
 * @param[in] current_id
 *     ID of the process performing the operation.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ID if id names no port or the process's partition
 *     is not a writer of it.
 *     MAXRTOS_ERR_INVALID_ARG if message is NULL or message_size differs
 *     from the port's message size.
 */
maxrtos_status_t maxrtos_kernel_sampling_port_write(
    maxrtos_sampling_port_id_t id,
    void const * message,
    size_t message_size,
    maxrtos_process_id_t current_id );

/**
 * @brief Read the message a sampling port holds.
 *
 * @param[in] id
 *     Port to read. The process's partition must be one of its readers.
 *
 * @param[out] out_message
 *     Receives the message.
 *
 * @param[in] buffer_size
 *     Size of out_message; at least the port's message size.
 *
 * @param[in] current_id
 *     ID of the process performing the operation.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ID if id names no port or the process's partition
 *     is not a reader of it.
 *     MAXRTOS_ERR_INVALID_ARG if out_message is NULL or buffer_size is
 *     smaller than the port's message size.
 *     MAXRTOS_ERR_QUEUE_EMPTY if nothing has been written yet.
 */
maxrtos_status_t maxrtos_kernel_sampling_port_read(
    maxrtos_sampling_port_id_t id,
    void * out_message,
    size_t buffer_size,
    maxrtos_process_id_t current_id );

/**
 * @brief Read the state of a sampling port.
 *
 * @param[in] id
 *     Port to query.
 *
 * @param[in] current_id
 *     ID of the process asking. Its partition must be a writer or a reader
 *     of the port.
 *
 * @param[out] out_status
 *     Receives the state.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ID if id names no port or the partition may not
 *     use it.
 *     MAXRTOS_ERR_INVALID_ARG if out_status is NULL.
 */
maxrtos_status_t maxrtos_kernel_sampling_port_get_status(
    maxrtos_sampling_port_id_t id,
    maxrtos_process_id_t current_id,
    maxrtos_sampling_port_status_t * out_status );

#endif /* MAXRTOS_KERNEL_SAMPLING_PORT_H */
