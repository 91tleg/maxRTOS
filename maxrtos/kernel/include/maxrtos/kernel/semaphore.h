/**
 * @file semaphore.h
 * @brief Kernel interface for intra-partition counting semaphores.
 *
 * A counting semaphore with a current and a maximum value.
 * A maximum of 1 gives a binary semaphore.
 * There is no owner, so any process of the partition may signal.
 *
 * Semaphores live in a kernel-owned pool and are named by handle, bound
 * to one partition when created (see kernel/mutex.h for why). Another
 * partition that names one gets MAXRTOS_ERR_INVALID_ID.
 *
 * This module contains no architecture-specific code.
 */

#ifndef MAXRTOS_KERNEL_SEMAPHORE_H
#define MAXRTOS_KERNEL_SEMAPHORE_H

#include <stdint.h>

#include "maxrtos/status.h"
#include "maxrtos/semaphore.h"
#include "maxrtos/types.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"

/**
 * @brief Free every semaphore in the pool.
 *
 * Called at startup and by tests. Do not call it while processes are
 * blocked on a semaphore.
 */
void maxrtos_semaphore_pool_init( void );

/**
 * @brief Create a semaphore owned by a partition.
 *
 * Privileged and application-owned, like process creation: call it from
 * initialization code before the scheduler starts.
 *
 * @param[in] partition_id
 *     Partition whose processes may use the semaphore.
 *
 * @param[in] initial_value
 *     Starting count. Must not exceed max_value.
 *
 * @param[in] max_value
 *     Largest count. Must be at least 1.
 *
 * @param[out] out_id
 *     Receives the semaphore handle.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if out_id is NULL, partition_id is out of
 *     range, max_value is 0 or initial_value exceeds max_value.
 *     MAXRTOS_ERR_POOL_FULL if MAXRTOS_MAX_SEMAPHORES are in use.
 */
maxrtos_status_t maxrtos_semaphore_create(
    maxrtos_partition_id_t partition_id,
    uint32_t initial_value,
    uint32_t max_value,
    maxrtos_semaphore_id_t * out_id );

/**
 * @brief Take one unit on behalf of the calling process.
 *
 * @param[in] id
 *     Semaphore to wait on.
 *
 * @param[in] timeout
 *     Ticks to wait if the count is zero. 0 fails immediately;
 *     MAXRTOS_TIMEOUT_INFINITE waits without limit.
 *
 * @param[in,out] table
 *     Partition table.
 *
 * @param[in] current_id
 *     The calling process. Must be RUNNING.
 *
 * @param[out] out_next_id
 *     Set only when MAXRTOS_PENDING is returned: the process now
 *     selected to run in place of the caller.
 *
 * @return
 *     MAXRTOS_OK if a unit was taken.
 *     MAXRTOS_PENDING if the caller is BLOCKED. Its final status
 *     (MAXRTOS_OK once it is given a unit, MAXRTOS_ERR_TIMEOUT if the
 *     wait expires) is delivered when it resumes.
 *     MAXRTOS_ERR_TIMEOUT if the count is zero and the caller cannot
 *     wait (timeout 0, or no other process in its partition is READY).
 *     MAXRTOS_ERR_INVALID_ID if the semaphore or process is invalid, or
 *     the semaphore belongs to another partition.
 *     MAXRTOS_ERR_POOL_FULL if the wait list is full.
 *     MAXRTOS_ERR_OVERFLOW if timeout would overflow the tick counter.
 *     MAXRTOS_ERR_INVALID_ARG if table or out_next_id is NULL.
 */
maxrtos_status_t maxrtos_kernel_semaphore_wait(
    maxrtos_semaphore_id_t id,
    maxrtos_tick_t timeout,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id );

/**
 * @brief Give one unit.
 *
 * The caller keeps running; a woken waiter becomes READY.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_OVERFLOW if the count is already at its maximum.
 *     MAXRTOS_ERR_INVALID_ID if the semaphore or process is invalid, or
 *     the semaphore belongs to another partition.
 *     MAXRTOS_ERR_INVALID_ARG if table is NULL.
 *     Otherwise a status from making the waiter READY, in which case
 *     the unit is kept in the count.
 */
maxrtos_status_t maxrtos_kernel_semaphore_signal(
    maxrtos_semaphore_id_t id,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id );

/**
 * @brief Return the current count. Meant for tests and diagnostics.
 *
 * @return
 *     The count, or 0 if id is invalid.
 */
uint32_t maxrtos_kernel_semaphore_count(
    maxrtos_semaphore_id_t id );

/**
 * @brief Read a semaphore's value, maximum and number of waiters.
 *
 * @param[in] id
 *     Semaphore to read.
 *
 * @param[in] current_id
 *     The calling process; the semaphore must belong to its partition.
 *
 * @param[out] out_status
 *     Receives the snapshot. Any caller-supplied address must already
 *     have been validated by the architecture layer.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ID if the semaphore or process is invalid, or
 *     the semaphore belongs to another partition.
 *     MAXRTOS_ERR_INVALID_ARG if out_status is NULL.
 */
maxrtos_status_t maxrtos_kernel_semaphore_get_status(
    maxrtos_semaphore_id_t id,
    maxrtos_process_id_t current_id,
    maxrtos_semaphore_status_t * out_status );

#endif /* MAXRTOS_KERNEL_SEMAPHORE_H */
