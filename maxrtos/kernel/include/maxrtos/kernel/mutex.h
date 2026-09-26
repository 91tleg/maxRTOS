/**
 * @file mutex.h
 * @brief Kernel interface for intra-partition mutexes.
 *
 * Provides mutual exclusion between processes of the SAME partition.
 * Partitions never run concurrently, so a mutex shared across
 * partitions has no meaning; use a queuing or sampling port instead.
 *
 * Mutexes live in a kernel-owned pool and are named by handle
 * (maxrtos_mutex_id_t). Their state is never reachable from partition
 * memory, so a partition cannot corrupt the owner or wait list, and
 * the kernel never dereferences a caller-supplied pointer. A mutex is
 * bound to one partition when it is created; any other partition that
 * names it gets MAXRTOS_ERR_INVALID_ID.
 *
 * Semantics:
 *  - Not recursive: locking a mutex the caller already owns fails.
 *  - Only the owner may unlock.
 *  - Blocked waiters are served highest priority first, FIFO within a
 *    priority, and ownership is handed directly to the chosen waiter.
 *  - A lock may wait with a timeout; timeout 0 is a try-lock.
 *  - When a process is restarted by fault recovery, every mutex it
 *    owns is released (handed to a waiter if there is one).
 *
 * There is no priority inheritance or ceiling. The partition
 * scheduler hands the CPU to the next READY process on every tick and
 * yield, so a lock holder never keeps the CPU against a lower-priority
 * process and unbounded priority inversion cannot build up; the cost
 * of a low-priority holder is bounded by its own progress per tick.
 *
 * Like queue ports, a lock that would have to wait when no other
 * process in the caller's partition is READY cannot block (there
 * would be nothing to run); it fails with MAXRTOS_ERR_TIMEOUT.
 *
 * This module contains no architecture-specific code.
 */

#ifndef MAXRTOS_KERNEL_MUTEX_H
#define MAXRTOS_KERNEL_MUTEX_H

#include "maxrtos/status.h"
#include "maxrtos/types.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"

/**
 * @brief Free every mutex in the pool.
 *
 * Called at startup and by tests. Processes blocked on a mutex are not
 * touched, so call it only while none exist.
 */
void maxrtos_mutex_pool_init( void );

/**
 * @brief Create a mutex owned by a partition.
 *
 * Privileged and application-owned, like process creation: call it
 * from initialization code before the scheduler starts.
 *
 * @param[in] partition_id
 *     Partition whose processes may use the mutex.
 *
 * @param[out] out_id
 *     Receives the mutex handle.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if out_id is NULL or partition_id is
 *     out of range.
 *     MAXRTOS_ERR_POOL_FULL if MAXRTOS_MAX_MUTEXES are in use.
 */
maxrtos_status_t maxrtos_mutex_create(
    maxrtos_partition_id_t partition_id,
    maxrtos_mutex_id_t * out_id );

/**
 * @brief Lock a mutex on behalf of the calling process.
 *
 * @param[in] id
 *     Mutex to lock.
 *
 * @param[in] timeout
 *     Ticks to wait if the mutex is held. 0 fails immediately;
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
 *     MAXRTOS_OK if the caller now owns the mutex.
 *     MAXRTOS_PENDING if the caller is BLOCKED. Its final status
 *     (MAXRTOS_OK once it is granted the mutex, MAXRTOS_ERR_TIMEOUT if
 *     the wait expires) is delivered when it resumes.
 *     MAXRTOS_ERR_TIMEOUT if the mutex is held and the caller cannot
 *     wait (timeout 0, or no other process in its partition is READY).
 *     MAXRTOS_ERR_INVALID_ID if the mutex or process is invalid, or
 *     the mutex belongs to another partition.
 *     MAXRTOS_ERR_INVALID_STATE if the caller already owns the mutex.
 *     MAXRTOS_ERR_POOL_FULL if the wait list is full.
 *     MAXRTOS_ERR_OVERFLOW if timeout would overflow the tick counter.
 *     MAXRTOS_ERR_INVALID_ARG if table or out_next_id is NULL.
 */
maxrtos_status_t maxrtos_kernel_mutex_lock(
    maxrtos_mutex_id_t id,
    maxrtos_tick_t timeout,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id,
    maxrtos_process_id_t * out_next_id );

/**
 * @brief Unlock a mutex owned by the calling process.
 *
 * If processes are waiting, the highest-priority one (longest waiting
 * among equals) becomes the owner and is made READY. The caller keeps
 * running.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ID if the mutex or process is invalid, or
 *     the mutex belongs to another partition.
 *     MAXRTOS_ERR_INVALID_STATE if the caller is not the owner
 *     (including an unlocked mutex).
 *     MAXRTOS_ERR_INVALID_ARG if table is NULL.
 *     Otherwise a status from making the new owner READY, in which
 *     case the mutex is left unlocked.
 */
maxrtos_status_t maxrtos_kernel_mutex_unlock(
    maxrtos_mutex_id_t id,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t current_id );

/**
 * @brief Release every mutex owned by a process.
 *
 * Used by fault recovery when a process is restarted, so a faulted
 * owner cannot leave its partition's mutexes locked forever. Each
 * mutex is handed to its best waiter or left unlocked.
 *
 * @param[in,out] table
 *     Partition table.
 *
 * @param[in] process_id
 *     Process whose mutexes are released.
 */
void maxrtos_kernel_mutex_release_all(
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t process_id );

/**
 * @brief Return the owner of a mutex.
 *
 * @return
 *     The owning process, or MAXRTOS_INVALID_PROCESS_ID if the mutex
 *     is unlocked or id is invalid.
 */
maxrtos_process_id_t maxrtos_kernel_mutex_owner(
    maxrtos_mutex_id_t id );

#endif /* MAXRTOS_KERNEL_MUTEX_H */
