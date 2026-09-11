/**
 * @file fifo.h
 * @brief Internal bounded FIFO ring-buffer engine.
 *
 * Internal kernel implementation used by IPC that require
 * fixed-size, fixed-capacity FIFO message storage.
 *
 * This module provides FIFO storage and message operations only. It
 * does not define or enforce ownership, partition membership, or
 * access-control policy.
 */

#ifndef MAXRTOS_KERNEL_INTERNAL_FIFO_H
#define MAXRTOS_KERNEL_INTERNAL_FIFO_H

#include <stddef.h>
#include <stdint.h>

#include "maxrtos/status.h"

/**
 * @brief Generic bounded FIFO instance.
 *
 * The FIFO does not own its backing storage. The caller provides a
 * byte array and specifies its size during initialization.
 */
typedef struct
{
    uint8_t * buffer;
    size_t buffer_capacity_bytes;

    size_t message_size;
    size_t capacity;
    size_t head;
    size_t count;
} maxrtos_kernel_fifo_t;

/**
 * @brief Initialize a FIFO over caller-provided storage.
 *
 * @param[out] fifo
 *     FIFO to initialize. Must not be NULL.
 *
 * @param[in] backing_buffer
 *     Caller-provided storage for FIFO messages. Must not be NULL.
 *     The storage must remain valid for the lifetime of the FIFO.
 *
 * @param[in] backing_buffer_size
 *     Size of backing_buffer in bytes. Must be sufficient to store
 *     message_size * capacity bytes.
 *
 * @param[in] message_size
 *     Size of each message in bytes. Must be greater than zero.
 *
 * @param[in] capacity
 *     Maximum number of messages the FIFO can hold. Must be greater
 *     than zero.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid or the
 *     backing storage is insufficient.
 */
maxrtos_status_t maxrtos_kernel_fifo_init(
    maxrtos_kernel_fifo_t * fifo,
    uint8_t * backing_buffer,
    size_t backing_buffer_size,
    size_t message_size,
    size_t capacity );

/**
 * @brief Enqueue one message into the FIFO.
 *
 * The operation is non-blocking. The message is copied into the FIFO
 * when space is available.
 *
 * @param[in,out] fifo
 *     FIFO to modify. Must not be NULL.
 *
 * @param[in] message
 *     Message to enqueue. Must not be NULL.
 *
 * @param[in] message_size
 *     Size of the message in bytes. Must match the configured
 *     message size.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid.
 *     MAXRTOS_ERR_QUEUE_FULL if the FIFO is at capacity.
 */
maxrtos_status_t maxrtos_kernel_fifo_push(
    maxrtos_kernel_fifo_t * fifo,
    void const * message,
    size_t message_size );

/**
 * @brief Dequeue the oldest message from the FIFO.
 *
 * The operation is non-blocking. The oldest queued message is copied
 * to the caller-provided storage when a message is available.
 *
 * @param[in,out] fifo
 *     FIFO to modify. Must not be NULL.
 *
 * @param[out] out_message
 *     Storage receiving the dequeued message. Must not be NULL.
 *
 * @param[in] buffer_size
 *     Size of out_message in bytes. Must be at least the configured
 *     message size.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid.
 *     MAXRTOS_ERR_QUEUE_EMPTY if the FIFO contains no messages.
 */
maxrtos_status_t maxrtos_kernel_fifo_pop(
    maxrtos_kernel_fifo_t * fifo,
    void * out_message,
    size_t buffer_size );

/**
 * @brief Return the number of messages currently stored in the FIFO.
 *
 * @param[in] fifo
 *     FIFO to query. May be NULL.
 *
 * @return Number of messages currently stored, or zero if fifo is NULL.
 */
size_t maxrtos_kernel_fifo_count(
    maxrtos_kernel_fifo_t const * fifo );

#endif /* MAXRTOS_KERNEL_INTERNAL_FIFO_H */
