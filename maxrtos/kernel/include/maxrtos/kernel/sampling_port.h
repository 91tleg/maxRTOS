/**
 * @file sampling_port.h
 * @brief Fixed-size sampling port interface.
 *
 * Provides a communication port for sampling messages.
 *
 * Each port is configured with a fixed message size and stores
 * exactly one message. Writing a new message replaces the
 * previously sampled message.
 *
 * Reading a sampling port returns the most recently written
 * message. Reading before the first successful write returns
 * an error status.
 *
 * Sampling port operations are non-blocking.
 *
 * A sampling port is caller-owned storage and does not contain
 * partition-level access-control information. Any caller with a
 * valid pointer to a port may invoke the read or write interface.
 */

#ifndef MAXRTOS_KERNEL_SAMPLING_PORT_H
#define MAXRTOS_KERNEL_SAMPLING_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "maxrtos/config.h"
#include "maxrtos/status.h"

typedef struct
{
    uint8_t buffer[ MAXRTOS_MAX_QUEUE_MESSAGE_SIZE ];
    size_t message_size;
    bool has_message;
} maxrtos_sampling_port_t;

/**
 * @brief Initialize a sampling port.
 *
 * The message size is fixed for the lifetime of the port.
 *
 * @param[out] port
 *     Port to initialize. Must not be NULL.
 *
 * @param[in] message_size
 *     Size, in bytes, of the sampled message.
 *     Must be greater than zero and no greater than
 *     MAXRTOS_MAX_SAMPLING_MESSAGE_SIZE.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid.
 */
maxrtos_status_t maxrtos_sampling_port_init(
    maxrtos_sampling_port_t * port,
    size_t message_size );

/**
 * @brief Write a message to the sampling port.
 *
 * The operation is non-blocking. If the port already contains
 * a message, the existing message is replaced.
 *
 * The message is copied into the port's internal storage. The
 * caller's buffer is not retained after this function returns.
 *
 * @param[in,out] port
 *     Initialized sampling port to which the message is written.
 *
 * @param[in] message
 *     Caller-owned message buffer. Must not be NULL and must
 *     provide at least the configured message_size bytes.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid.
 */
maxrtos_status_t maxrtos_sampling_port_write(
    maxrtos_sampling_port_t * port,
    void const * message );

/**
 * @brief Read the most recently sampled message.
 *
 * The operation is non-blocking. If no message has been
 * successfully written since initialization,
 * MAXRTOS_ERR_QUEUE_EMPTY is returned.
 *
 * The sampled message is copied into the caller-provided
 * buffer. Only the configured message_size bytes are written.
 *
 * @param[in] port
 *     Initialized sampling port from which the message is read.
 *
 * @param[out] out_message
 *     Caller-owned output buffer. Must not be NULL and must
 *     provide at least message_size bytes of storage.
 *
 * @param[in] buffer_size
 *     Size of the output buffer in bytes. Must be at least the
 *     port's configured message size.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid.
 *     MAXRTOS_ERR_QUEUE_EMPTY if no message has been written.
 */
maxrtos_status_t maxrtos_sampling_port_read(
    maxrtos_sampling_port_t const * port,
    void * out_message,
    size_t buffer_size );

/**
 * @brief Return whether the sampling port contains a message.
 *
 * @param[in] port
 *     Port to query. If NULL, false is returned.
 *
 * @return
 *     true if a message has been successfully written.
 *     false if no message has been written or port is NULL.
 */
bool maxrtos_sampling_port_is_valid(
    maxrtos_sampling_port_t const * port );

#endif /* MAXRTOS_KERNEL_SAMPLING_PORT_H */
