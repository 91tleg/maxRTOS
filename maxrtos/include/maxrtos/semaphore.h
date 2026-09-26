/**
 * @file semaphore.h
 * @brief Public semaphore interface.
 *
 * counting semaphore shared by the processes of one partition
 * (a maximum of 1 makes it binary).
 */

#ifndef MAXRTOS_SEMAPHORE_H
#define MAXRTOS_SEMAPHORE_H

#include <stdint.h>

#include "maxrtos/status.h"
#include "maxrtos/types.h"

/**
 * @brief Snapshot of a semaphore.
 */
typedef struct
{
    uint32_t current_value; /* units available now */
    uint32_t maximum_value; /* largest value it can hold */
    uint32_t waiting;       /* processes blocked in wait */
} maxrtos_semaphore_status_t;

/**
 * @brief Take one unit, waiting if none is available.
 *
 * @param id       Semaphore handle. Must belong to the caller's partition.
 * @param timeout  Ticks to wait: 0 polls, MAXRTOS_TIMEOUT_INFINITE waits
 *                 without limit.
 *
 * @return MAXRTOS_OK if a unit was taken.
 * @return MAXRTOS_ERR_TIMEOUT if none became available in time.
 * @return MAXRTOS_ERR_INVALID_ID if the handle is invalid or belongs to
 *         another partition.
 */
maxrtos_status_t maxrtos_semaphore_wait(
    maxrtos_semaphore_id_t id,
    maxrtos_tick_t timeout );

/**
 * @brief Give one unit, waking the highest-priority waiter if any.
 *
 * @param id  Semaphore handle.
 *
 * @return MAXRTOS_OK on success.
 * @return MAXRTOS_ERR_OVERFLOW if the count is already at its maximum.
 * @return MAXRTOS_ERR_INVALID_ID if the handle is invalid or belongs to
 *         another partition.
 */
maxrtos_status_t maxrtos_semaphore_signal(
    maxrtos_semaphore_id_t id );

/**
 * @brief Read a semaphore's value, maximum and number of waiters.
 *
 * Does not block. The values can change as soon as the call returns.
 *
 * @param id      Semaphore handle. Must belong to the caller's partition.
 * @param status  Receives the snapshot. Must lie in the caller's own
 *                partition memory.
 *
 * @return MAXRTOS_OK on success.
 * @return MAXRTOS_ERR_INVALID_ID if the handle is invalid or belongs to
 *         another partition.
 * @return MAXRTOS_ERR_INVALID_ARG if status is NULL or not in the
 *         caller's partition memory.
 */
maxrtos_status_t maxrtos_semaphore_get_status(
    maxrtos_semaphore_id_t id,
    maxrtos_semaphore_status_t * status );

#endif /* MAXRTOS_SEMAPHORE_H */
