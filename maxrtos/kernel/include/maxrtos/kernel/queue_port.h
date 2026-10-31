/**
 * @file queue_port.h
 * @brief Kernel interface for fixed-size queuing ports.
 *
 * Provides the kernel-side implementation of FIFO communication
 * between processes, possibly of different partitions.
 *
 * The ports live in a kernel pool and are named by ID, like buffers. The
 * pool, the queue state and the wait lists are in kernel memory that no
 * partition maps, so a partition cannot corrupt them; it reaches a port only
 * through a system call, and every call checks the caller's partition against
 * the sender or receiver set the port was created with. Only the message
 * storage is supplied by the creator.
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
#include "maxrtos/kernel/fifo.h"
#include "maxrtos/kernel/waitlist.h"
#include "maxrtos/kernel/scheduler.h"
#include "maxrtos/kernel/partition.h"

/**
 * @brief State of a queuing port, for the caller's own information.
 */
typedef struct
{
    size_t message_size;
    size_t capacity;
    size_t nb_message;
    size_t waiting_receivers;
    size_t waiting_senders;
} maxrtos_queue_port_status_t;

/**
 * @brief Empty the port pool.
 *
 * Every port is released. The generated configuration calls this before it
 * creates its ports.
 */
void maxrtos_queue_port_pool_init( void );

/**
 * @brief Create a queuing port.
 *
 * @param[in] storage
 *     Memory for the queued messages. It must stay valid for as long as the
 *     port exists, and must be kernel memory: the kernel copies messages in
 *     and out of it with privilege, and no partition may be able to map it.
 *
 * @param[in] storage_size
 *     Size of storage in bytes; at least message_size * capacity.
 *
 * @param[in] message_size
 *     Size of every message in bytes.
 *
 * @param[in] capacity
 *     Maximum number of messages.
 *
 * @param[in] sender_mask
 *     Partitions that may send: bit n set for partition n. Not empty.
 *
 * @param[in] receiver_mask
 *     Partitions that may receive: bit n set for partition n. Not empty.
 *
 * @param[out] out_id
 *     Receives the ID of the new port.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid, a mask is empty or
 *     names a partition that does not exist, or storage is too small.
 *     MAXRTOS_ERR_POOL_FULL if MAXRTOS_MAX_QUEUE_PORTS ports exist.
 */
maxrtos_status_t maxrtos_queue_port_create(
    void * storage,
    size_t storage_size,
    size_t message_size,
    size_t capacity,
    uint32_t sender_mask,
    uint32_t receiver_mask,
    maxrtos_queue_port_id_t * out_id );

/**
 * @brief Execute a kernel-side queue send operation.
 *
 * Attempts to enqueue the message immediately. If the queue is full
 * and timeout permits blocking, the current process is placed on the
 * sender wait list.
 *
 * @param[in] id
 *     Port to send to. The process's partition must be one of its senders.
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
 *     MAXRTOS_ERR_INVALID_ID if id names no port or the process's partition
 *     is not a sender of it.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid or
 *     message_size differs from the port's message size.
 *     MAXRTOS_ERR_QUEUE_FULL if the queue is full and the operation
 *     does not block, or the caller cannot block because no other
 *     process in its partition is READY.
 *     MAXRTOS_ERR_OVERFLOW if the timeout cannot be represented as
 *     an absolute wake tick.
 */
maxrtos_status_t maxrtos_kernel_queue_port_send(
    maxrtos_queue_port_id_t id,
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
 * @param[in] id
 *     Port to receive from. The process's partition must be one of its
 *     receivers.
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
 *     MAXRTOS_ERR_INVALID_ID if id names no port or the process's partition
 *     is not a receiver of it.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid.
 *     MAXRTOS_ERR_QUEUE_EMPTY if the queue is empty and the operation
 *     does not block, or the caller cannot block because no other
 *     process in its partition is READY.
 *     MAXRTOS_ERR_OVERFLOW if the timeout cannot be represented as
 *     an absolute wake tick.
 */
maxrtos_status_t maxrtos_kernel_queue_port_receive(
    maxrtos_queue_port_id_t id,
    void * out_message,
    size_t buffer_size,
    maxrtos_tick_t timeout,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id );

/**
 * @brief Return the number of messages currently stored.
 *
 * @param[in] id
 *     Port to query.
 *
 * @param[in] current_id
 *     ID of the process asking. Its partition must be a sender or a
 *     receiver of the port.
 *
 * @return
 *     Number of messages currently stored.
 *     Zero if id names no port or the partition may not use it.
 */
size_t maxrtos_kernel_queue_port_count(
    maxrtos_queue_port_id_t id,
    maxrtos_process_id_t current_id );

/**
 * @brief Read the state of a port.
 *
 * @param[in] id
 *     Port to query.
 *
 * @param[in] current_id
 *     ID of the process asking. Its partition must be a sender or a
 *     receiver of the port.
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
maxrtos_status_t maxrtos_kernel_queue_port_get_status(
    maxrtos_queue_port_id_t id,
    maxrtos_process_id_t current_id,
    maxrtos_queue_port_status_t * out_status );

#endif /* MAXRTOS_KERNEL_QUEUE_PORT_H */
