/**
 * @file config.h
 * @brief System-wide compile-time configuration parameters.
 *
 * Defines compile-time limits used for static memory allocation across
 * kernel components.
 */

#ifndef MAXRTOS_CONFIG_H
#define MAXRTOS_CONFIG_H

/**
 * @brief Maximum system-wide process count across all partitions.
 */
#ifndef MAXRTOS_MAX_PROCESSES
    #define MAXRTOS_MAX_PROCESSES ( 16U )
#endif /* MAXRTOS_MAX_PROCESSES */

/**
 * @brief Maximum process priority value.
 *
 * Priority 0 represents the highest priority level.
 */
#ifndef MAXRTOS_MAX_PRIORITY
    #define MAXRTOS_MAX_PRIORITY ( 31U )
#endif /* MAXRTOS_MAX_PRIORITY */

/**
 * @brief Maximum supported partition count system-wide.
 */
#ifndef MAXRTOS_MAX_PARTITIONS
    #define MAXRTOS_MAX_PARTITIONS ( 8U )
#endif /* MAXRTOS_MAX_PARTITIONS */

/**
 * @brief Maximum number of simultaneously READY processes per
 *        priority level within a single partition.
 */
#ifndef MAXRTOS_MAX_READY_PER_PRIORITY
    #define MAXRTOS_MAX_READY_PER_PRIORITY ( 8U )
#endif /* MAXRTOS_MAX_READY_PER_PRIORITY */

/**
 * @brief Maximum number of slots in a single major-frame schedule.
 */
#ifndef MAXRTOS_MAX_FRAME_SLOTS
    #define MAXRTOS_MAX_FRAME_SLOTS ( 16U )
#endif /* MAXRTOS_MAX_FRAME_SLOTS */

/**
 * @brief Maximum allowable payload size for a single queuing port, in bytes.
 *
 * Determines static buffer allocation for queuing port instances.
 */
#ifndef MAXRTOS_MAX_QUEUE_MESSAGE_SIZE
    #define MAXRTOS_MAX_QUEUE_MESSAGE_SIZE ( 64U )
#endif /* MAXRTOS_MAX_QUEUE_MESSAGE_SIZE */

/**
 * @brief Maximum allowable message capacity for a single queuing port.
 *
 * Defines the maximum number of messages a queuing port buffer can hold.
 */
#ifndef MAXRTOS_MAX_QUEUE_CAPACITY
    #define MAXRTOS_MAX_QUEUE_CAPACITY ( 8U )
#endif /* MAXRTOS_MAX_QUEUE_CAPACITY */

/**
 * @brief Maximum allowable payload size for a single buffer, in bytes.
 */
#ifndef MAXRTOS_MAX_BUFFER_MESSAGE_SIZE
    #define MAXRTOS_MAX_BUFFER_MESSAGE_SIZE ( 64U )
#endif /* MAXRTOS_MAX_BUFFER_MESSAGE_SIZE */

/**
 * @brief Maximum allowable message capacity for a single buffer.
 *
 * Defines the maximum number of messages a buffer can hold.
 */
#ifndef MAXRTOS_MAX_BUFFER_CAPACITY
    #define MAXRTOS_MAX_BUFFER_CAPACITY ( 8U )
#endif /* MAXRTOS_MAX_BUFFER_CAPACITY */

/**
 * @brief Maximum number of processes that may simultaneously block on
 *        one blocking primitive.
 */
#ifndef MAXRTOS_WAITLIST_MAX
    #define MAXRTOS_WAITLIST_MAX ( 8U )
#endif /* MAXRTOS_WAITLIST_MAX */

#endif /* MAXRTOS_CONFIG_H */
