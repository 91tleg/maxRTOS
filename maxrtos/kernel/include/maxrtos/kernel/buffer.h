/**
 * @file buffer.h
 * @brief Kernel interface for intra-partition ARINC 653 buffers.
 *
 * Provides a FIFO message queue shared by the processes of one partition.
 * Unlike a queuing port, a buffer has no fixed sender/receiver: any process
 * of the partition may call send or receive.
 *
 * Buffers live in a kernel-owned pool and are named by handle
 * (maxrtos_buffer_id_t), bound to one partition when created (see
 * kernel/mutex.h for why). Another partition that names one gets
 * MAXRTOS_ERR_INVALID_ID.
 *
 * Semantics (ARINC 653 Part 1, buffer service):
 *  - Fixed-size messages: every message sent or received is exactly the
 *    buffer's configured message size.
 *  - A send blocks while the buffer is full; a receive blocks while it is
 *    empty. A zero timeout is non-blocking.
 *  - QUEUING_DISCIPLINE, chosen at creation, governs which blocked process
 *    is released first when a slot or message becomes available:
 *    MAXRTOS_QUEUING_FIFO releases the longest-waiting process;
 *    MAXRTOS_QUEUING_PRIORITY releases the highest-priority one, FIFO
 *    among equal priority. The discipline applies to both the sender and
 *    receiver wait lists.
 *  - Like queue ports and mutexes, a send or receive that would have to
 *    wait when no other process in the caller's partition is READY cannot
 *    block (there would be nothing to run); it fails with
 *    MAXRTOS_ERR_QUEUE_FULL / MAXRTOS_ERR_QUEUE_EMPTY instead.
 *
 * This module contains no architecture-specific code.
 */

#ifndef MAXRTOS_KERNEL_BUFFER_H
#define MAXRTOS_KERNEL_BUFFER_H

#include <stddef.h>

#include "maxrtos/status.h"
#include "maxrtos/buffer.h"
#include "maxrtos/types.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"

/**
 * @brief Free every buffer in the pool.
 *
 * Called at startup and by tests. Processes blocked on a buffer are not
 * touched, so call it only while none exist.
 */
void maxrtos_buffer_pool_init( void );

/**
 * @brief Create a buffer owned by a partition.
 *
 * Privileged and application-owned, like process creation: call it from
 * initialization code before the scheduler starts.
 *
 * @param[in] partition_id
 *     Partition whose processes may use the buffer.
 *
 * @param[in] max_message_size
 *     Size, in bytes, of every message the buffer carries. Must be
 *     greater than zero and no greater than
 *     MAXRTOS_MAX_BUFFER_MESSAGE_SIZE.
 *
 * @param[in] max_nb_message
 *     Maximum number of messages the buffer can hold. Must be greater
 *     than zero and no greater than MAXRTOS_MAX_BUFFER_CAPACITY.
 *
 * @param[in] discipline
 *     Order in which blocked senders and receivers are released.
 *
 * @param[out] out_id
 *     Receives the buffer handle.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if out_id is NULL, partition_id is out of
 *     range, or max_message_size/max_nb_message is 0 or exceeds its
 *     configured maximum.
 *     MAXRTOS_ERR_POOL_FULL if MAXRTOS_MAX_BUFFERS are in use.
 */
maxrtos_status_t maxrtos_buffer_create(
    maxrtos_partition_id_t partition_id,
    size_t max_message_size,
    size_t max_nb_message,
    maxrtos_queuing_discipline_t discipline,
    maxrtos_buffer_id_t * out_id );

/**
 * @brief Execute a kernel-side buffer send operation.
 *
 * Attempts to enqueue the message immediately. If the buffer is full and
 * the timeout permits blocking, the current process is placed on the
 * sender wait list.
 *
 * @param[in] id
 *     Buffer to send to.
 *
 * @param[in] message
 *     Message to enqueue.
 *
 * @param[in] message_size
 *     Message size in bytes. Must equal the buffer's configured message
 *     size.
 *
 * @param[in] timeout
 *     Maximum number of ticks to wait. Zero means do not block.
 *     MAXRTOS_TIMEOUT_INFINITE means wait indefinitely.
 *
 * @param[in,out] table
 *     Partition table.
 *
 * @param[in] current_id
 *     ID of the process performing the operation. Must belong to the
 *     buffer's partition.
 *
 * @param[out] out_next_id
 *     Set to the process selected to run if the current process blocks.
 *
 * If a receiver is blocked on the buffer, the message is copied directly
 * into its output and the receiver is resumed with MAXRTOS_OK.
 *
 * @return
 *     MAXRTOS_OK if the message was sent immediately.
 *     MAXRTOS_PENDING if the current process was blocked. Its final
 *     result (MAXRTOS_OK, or MAXRTOS_ERR_TIMEOUT) is delivered when it
 *     resumes, through ipc_result.
 *     MAXRTOS_ERR_INVALID_ID if the buffer or process is invalid, or the
 *     buffer belongs to another partition.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid or message_size
 *     differs from the buffer's message size.
 *     MAXRTOS_ERR_QUEUE_FULL if the buffer is full and the operation does
 *     not block, or the caller cannot block because no other process in
 *     its partition is READY.
 *     MAXRTOS_ERR_OVERFLOW if the timeout cannot be represented as an
 *     absolute wake tick.
 */
maxrtos_status_t maxrtos_kernel_buffer_send(
    maxrtos_buffer_id_t id,
    void const * message,
    size_t message_size,
    maxrtos_tick_t timeout,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id );

/**
 * @brief Execute a kernel-side buffer receive operation.
 *
 * Attempts to receive a message immediately. If the buffer is empty and
 * the timeout permits blocking, the current process is placed on the
 * receiver wait list.
 *
 * @param[in] id
 *     Buffer to receive from.
 *
 * @param[out] out_message
 *     Output buffer for the received message.
 *
 * @param[in] buffer_size
 *     Size of the output buffer in bytes. Must be at least the buffer's
 *     configured message size. Every message carried by the buffer is
 *     exactly that size, so the received length is not reported
 *     separately.
 *
 * @param[in] timeout
 *     Maximum number of ticks to wait. Zero means do not block.
 *     MAXRTOS_TIMEOUT_INFINITE means wait indefinitely.
 *
 * @param[in,out] table
 *     Partition table.
 *
 * @param[in] current_id
 *     ID of the process performing the operation. Must belong to the
 *     buffer's partition.
 *
 * @param[out] out_next_id
 *     Set to the process selected to run if the current process blocks.
 *
 * @return
 *     MAXRTOS_OK if a message was received immediately.
 *     MAXRTOS_PENDING if the current process was blocked. Its final
 *     result (MAXRTOS_OK with the message in out_message, or
 *     MAXRTOS_ERR_TIMEOUT) is delivered when it resumes.
 *     MAXRTOS_ERR_INVALID_ID if the buffer or process is invalid, or the
 *     buffer belongs to another partition.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid.
 *     MAXRTOS_ERR_QUEUE_EMPTY if the buffer is empty and the operation
 *     does not block, or the caller cannot block because no other
 *     process in its partition is READY.
 *     MAXRTOS_ERR_OVERFLOW if the timeout cannot be represented as an
 *     absolute wake tick.
 */
maxrtos_status_t maxrtos_kernel_buffer_receive(
    maxrtos_buffer_id_t id,
    void * out_message,
    size_t buffer_size,
    maxrtos_tick_t timeout,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id );

/**
 * @brief Read a buffer's configuration, fill level and waiter count.
 *
 * @param[in] id
 *     Buffer to read.
 *
 * @param[in] current_id
 *     The calling process; the buffer must belong to its partition.
 *
 * @param[out] out_status
 *     Receives the snapshot. Any caller-supplied address must already
 *     have been validated by the architecture layer.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ID if the buffer or process is invalid, or the
 *     buffer belongs to another partition.
 *     MAXRTOS_ERR_INVALID_ARG if out_status is NULL.
 */
maxrtos_status_t maxrtos_kernel_buffer_get_status(
    maxrtos_buffer_id_t id,
    maxrtos_process_id_t current_id,
    maxrtos_buffer_status_t * out_status );

#endif /* MAXRTOS_KERNEL_BUFFER_H */
