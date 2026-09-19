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

#endif /* MAXRTOS_TYPES_H */
