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

/* Upper bounds for a single queuing port (queue_port.h). A port's
 * ACTUAL message_size and capacity are configured per-instance at
 * init time, within these compile-time maximums -- the same pattern
 * MAXRTOS_MAX_FRAME_SLOTS uses relative to a schedule's actual
 * slot_count. Static allocation only: a port's backing buffer is
 * sized MAXRTOS_MAX_QUEUE_CAPACITY * MAXRTOS_MAX_QUEUE_MESSAGE_SIZE
 * bytes regardless of the smaller actual values a given instance
 * configures, so these bounds directly affect every port's static
 * memory footprint -- keep them only as large as real ports
 * actually need. */
#ifndef MAXRTOS_MAX_QUEUE_MESSAGE_SIZE
    #define MAXRTOS_MAX_QUEUE_MESSAGE_SIZE ( 64U )
#endif
 
#ifndef MAXRTOS_MAX_QUEUE_CAPACITY
    #define MAXRTOS_MAX_QUEUE_CAPACITY ( 8U )
#endif

#endif /* MAXRTOS_CONFIG_H */
