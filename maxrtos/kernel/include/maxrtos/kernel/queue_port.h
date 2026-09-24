/**
 * @file queue_port.h
 * @brief Kernel interface for fixed-size queuing ports.
 *
 * Provides the kernel-side implementation of FIFO communication
 * between processes.
 *
 * This interface is kernel-internal. Partition code must use the
 * public maxrtos/queue_port.h interface, which enters the kernel
 * through the architecture-specific syscall mechanism.
 */

#ifndef MAXRTOS_KERNEL_QUEUE_PORT_H
#define MAXRTOS_KERNEL_QUEUE_PORT_H

#include <stddef.h>
#include <stdint.h>

#include "maxrtos/status.h"
#include "maxrtos/queue_port.h"
#include "maxrtos/config.h"
#include "maxrtos/types.h"
#include "maxrtos/kernel/internal/fifo.h"
#include "maxrtos/kernel/waitlist.h"
#include "maxrtos/kernel/scheduler.h"
#include "maxrtos/kernel/partition.h"

/**
 * @brief A single queuing port instance.
 *
 * @field buffer
 *     Statically allocated queue storage.
 *
 * @field fifo
 *     FIFO engine operating on buffer.
 *
 * @field waiting_receivers
 *     Processes blocked waiting for a message.
 *
 * @field waiting_senders
 *     Processes blocked waiting for space.
 */
struct maxrtos_queue_port_s
{
    uint8_t buffer[
        MAXRTOS_MAX_QUEUE_CAPACITY *
        MAXRTOS_MAX_QUEUE_MESSAGE_SIZE ];

    maxrtos_kernel_fifo_t fifo;

    maxrtos_waitlist_t waiting_receivers;
    maxrtos_waitlist_t waiting_senders;
};

/**
 * @brief Initialize a queuing port.
 *
 * @param[out] port
 *     Port to initialize. Must not be NULL.
 *
 * @param[in] message_size
 *     Size of every message in bytes.
 *
 * @param[in] capacity
 *     Maximum number of messages.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid.
 */
maxrtos_status_t maxrtos_queue_port_init(
    maxrtos_queue_port_t * port,
    size_t message_size,
    size_t capacity );

/**
 * @brief Execute a kernel-side queue send operation.
 *
 * Attempts to enqueue the message immediately. If the queue is full
 * and timeout permits blocking, the current process is placed on the
 * sender wait list.
 *
 * @param[in,out] port
 *     Initialized queue port.
 *
 * @param[in] message
 *     Message to enqueue.
 *
 * @param[in] message_size
 *     Message size in bytes.
 *
 * @param[in] timeout
 *     Maximum number of ticks to wait.
 *     Zero means do not block.
 *     MAXRTOS_TIMEOUT_INFINITE means wait indefinitely.
 *
 * @param[in,out] table
 *     Partition table. A blocked or woken process is always handled in
 *     its own partition, so a sender may wake a receiver in another
 *     partition.
 *
 * @param[in] current_id
 *     ID of the process performing the operation.
 *
 * @param[out] out_next_id
 *     Set to the process selected to run if the current process
 *     blocks.
 *
 * If a receiver is blocked on the port, the message is copied directly
 * into its buffer and the receiver is resumed with MAXRTOS_OK.
 *
 * @return
 *     MAXRTOS_OK if the message was sent immediately.
 *     MAXRTOS_PENDING if the current process was blocked. Its final
 *     result (MAXRTOS_OK, or MAXRTOS_ERR_TIMEOUT) is delivered when it
 *     resumes, through ipc_result.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid or
 *     message_size differs from the port's message size.
 *     MAXRTOS_ERR_QUEUE_FULL if the queue is full and the operation
 *     does not block, or the caller cannot block because no other
 *     process in its partition is READY.
 *     MAXRTOS_ERR_OVERFLOW if the timeout cannot be represented as
 *     an absolute wake tick.
 */
maxrtos_status_t maxrtos_kernel_queue_port_send(
    maxrtos_queue_port_t * port,
    void const * message,
    size_t message_size,
    maxrtos_tick_t timeout,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id );

/**
 * @brief Execute a kernel-side queue receive operation.
 *
 * Attempts to receive a message immediately. If the queue is empty
 * and timeout permits blocking, the current process is placed on
 * the receiver wait list.
 *
 * @param[in,out] port
 *     Initialized queue port.
 *
 * @param[out] out_message
 *     Output buffer for the received message.
 *
 * @param[in] buffer_size
 *     Size of the output buffer in bytes.
 *
 * @param[in] timeout
 *     Maximum number of ticks to wait.
 *     Zero means do not block.
 *     MAXRTOS_TIMEOUT_INFINITE means wait indefinitely.
 *
 * @param[in] current_id
 *     ID of the process performing the operation.
 *
 * @param[out] out_next_id
 *     Set to the process selected to run if the current process
 *     blocks.
 *
 * @return
 *     MAXRTOS_OK if a message was received immediately.
 *     MAXRTOS_PENDING if the current process was blocked. Its final
 *     result (MAXRTOS_OK with the message in out_message, or
 *     MAXRTOS_ERR_TIMEOUT) is delivered when it resumes.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid.
 *     MAXRTOS_ERR_QUEUE_EMPTY if the queue is empty and the operation
 *     does not block, or the caller cannot block because no other
 *     process in its partition is READY.
 *     MAXRTOS_ERR_OVERFLOW if the timeout cannot be represented as
 *     an absolute wake tick.
 */
maxrtos_status_t maxrtos_kernel_queue_port_receive(
    maxrtos_queue_port_t * port,
    void * out_message,
    size_t buffer_size,
    maxrtos_tick_t timeout,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id );

/**
 * @brief Return the number of messages currently stored.
 *
 * @param[in] port
 *     Port to query.
 *
 * @return
 *     Number of messages currently stored.
 *     Zero if port is NULL.
 */
size_t maxrtos_kernel_queue_port_count(
    maxrtos_queue_port_t const * port );

#endif /* MAXRTOS_KERNEL_QUEUE_PORT_H */
