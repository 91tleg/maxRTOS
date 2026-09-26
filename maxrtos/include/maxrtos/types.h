/**
 * @file types.h
 * @brief Common MAXRTOS type definitions and sentinel values.
 */

#ifndef MAXRTOS_TYPES_H
#define MAXRTOS_TYPES_H

#include <stdint.h>

typedef uint32_t maxrtos_tick_t;

/* Sentinel tick value indicating that no timed wait is active. */
#define MAXRTOS_TICK_NONE \
    ( ( maxrtos_tick_t ) UINT32_MAX )

/* Sentinel timeout value indicating that a wait has no deadline. */
#define MAXRTOS_TIMEOUT_INFINITE \
    ( ( maxrtos_tick_t ) UINT32_MAX )

typedef uint32_t maxrtos_process_id_t;

/* Sentinel process ID indicating that no valid process is selected. */
#define MAXRTOS_INVALID_PROCESS_ID \
    ( ( maxrtos_process_id_t ) UINT32_MAX )

typedef uint32_t maxrtos_partition_id_t;

/* Sentinel partition ID indicating that no valid partition is assigned. */
#define MAXRTOS_INVALID_PARTITION_ID \
    ( ( maxrtos_partition_id_t ) UINT32_MAX )

/* Handle for a kernel-owned mutex, as returned by maxrtos_mutex_create(). */
typedef uint32_t maxrtos_mutex_id_t;

/* Sentinel mutex ID indicating that no mutex is selected. */
#define MAXRTOS_INVALID_MUTEX_ID \
    ( ( maxrtos_mutex_id_t ) UINT32_MAX )

/* Handle for a kernel-owned semaphore, as returned by
 * maxrtos_semaphore_create(). */
typedef uint32_t maxrtos_semaphore_id_t;

/* Sentinel semaphore ID indicating that no semaphore is selected. */
#define MAXRTOS_INVALID_SEMAPHORE_ID \
    ( ( maxrtos_semaphore_id_t ) UINT32_MAX )

/* Handle for a kernel-owned buffer, as returned by
 * maxrtos_buffer_create(). */
typedef uint32_t maxrtos_buffer_id_t;

/* Sentinel buffer ID indicating that no buffer is selected. */
#define MAXRTOS_INVALID_BUFFER_ID \
    ( ( maxrtos_buffer_id_t ) UINT32_MAX )

/* Order in which blocked processes are released when a blocking IPC
 * primitive has capacity for one of them (ARINC 653 QUEUING_DISCIPLINE). */
typedef enum
{
    MAXRTOS_QUEUING_FIFO = 0,
    MAXRTOS_QUEUING_PRIORITY,
} maxrtos_queuing_discipline_t;

#endif /* MAXRTOS_TYPES_H */
