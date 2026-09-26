/**
 * @file mutex.h
 * @brief Public mutex interface.
 *
 * Mutual exclusion between processes of one partition. A mutex is
 * named by a handle from maxrtos_mutex_create() (kernel/mutex.h),
 * called by the application's initialization code before the
 * scheduler starts and distributed to the partition's processes.
 * See maxrtos/kernel/mutex.h for the full semantics.
 */

#ifndef MAXRTOS_MUTEX_H
#define MAXRTOS_MUTEX_H

#include "maxrtos/status.h"
#include "maxrtos/types.h"

/**
 * @brief Lock a mutex.
 *
 * @param id       Mutex handle. Must belong to the caller's partition.
 * @param timeout  Ticks to wait if the mutex is held: 0 returns
 *                 immediately, MAXRTOS_TIMEOUT_INFINITE waits without
 *                 limit.
 *
 * @return
 *     MAXRTOS_OK if the caller now owns the mutex.
 *     MAXRTOS_ERR_TIMEOUT if it could not be acquired in time.
 *     MAXRTOS_ERR_INVALID_STATE if the caller already owns it
 *     (the mutex is not recursive).
 *     MAXRTOS_ERR_INVALID_ID if the handle is invalid or belongs to
 *     another partition.
 */
maxrtos_status_t maxrtos_mutex_lock(
    maxrtos_mutex_id_t id,
    maxrtos_tick_t timeout );

/**
 * @brief Unlock a mutex owned by the caller.
 *
 * @param id  Mutex handle.
 *
 * @return MAXRTOS_OK on success.
 * @return MAXRTOS_ERR_INVALID_STATE if the caller is not the owner.
 * @return MAXRTOS_ERR_INVALID_ID if the handle is invalid or belongs to
 *         another partition.
 */
maxrtos_status_t maxrtos_mutex_unlock(
    maxrtos_mutex_id_t id );

#endif /* MAXRTOS_MUTEX_H */
