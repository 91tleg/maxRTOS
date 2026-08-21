/**
 * @file config.h
 * @brief central definitions for maxrtos-wide static limits.
 *
 * Every value here is a compile-time constant used to size static arrays.
 */
#ifndef MAXRTOS_CONFIG_H
#define MAXRTOS_CONFIG_H

/* Maximum number of processes the kernel supports, system-wide,
 * across all partitions. */
#ifndef MAXRTOS_MAX_PROCESSES
    #define MAXRTOS_MAX_PROCESSES ( 16U )
#endif /* MAXRTOS_MAX_PROCESSES */

/* Maximum supported process priority. 0 is the highest priority. */
#ifndef MAXRTOS_MAX_PRIORITY
    #define MAXRTOS_MAX_PRIORITY ( 31U )
#endif /* MAXRTOS_MAX_PRIORITY */

/* Maximum number of partitions the kernel supports. */
#ifndef MAXRTOS_MAX_PARTITIONS
    #define MAXRTOS_MAX_PARTITIONS ( 8U )
#endif /* MAXRTOS_MAX_PARTITIONS */

/* Maximum number of processes that may be simultaneously READY within a
 * single priority level of a single partition's ready queue. */
#ifndef MAXRTOS_MAX_READY_PER_PRIORITY
    #define MAXRTOS_MAX_READY_PER_PRIORITY ( 8U )
#endif /* MAXRTOS_MAX_READY_PER_PRIORITY */

/* Maximum number of slots in a single major-frame schedule.
 * Each slot assigns a fixed duration to one partition. */
#ifndef MAXRTOS_MAX_FRAME_SLOTS
    #define MAXRTOS_MAX_FRAME_SLOTS ( 16U )
#endif /* MAXRTOS_MAX_FRAME_SLOTS */

#endif /* MAXRTOS_CONFIG_H */
