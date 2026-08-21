/**
 * @file frame.h
 * @brief Static major-frame schedule for time-partitioned execution.
 *
 * The frame scheduler implements the time-partitioning layer of the
 * kernel scheduling hierarchy. A major frame consists of a fixed,
 * ordered sequence of time slots. Each slot assigns the processor to
 * one partition for a nonzero, fixed duration measured in ticks.
 */

#ifndef MAXRTOS_KERNEL_FRAME_H
#define MAXRTOS_KERNEL_FRAME_H

#include <stdint.h>
#include <stddef.h>

#include "maxrtos/kernel/process.h"
#include "maxrtos/status.h"
#include "maxrtos/config.h"

/**
 * @brief One entry in the major-frame schedule.
 *
 * Each slot assigns one partition exclusive use of the processor for
 * a fixed number of ticks.
 *
 * @field partition_id
 *     Partition assigned to this slot. The value shall be less than
 *     MAXRTOS_MAX_PARTITIONS.
 *
 * @field duration_ticks
 *     Number of ticks allocated to the partition. The value shall be
 *     greater than zero.
 */
typedef struct
{
    maxrtos_partition_id_t partition_id;
    uint32_t duration_ticks;
} maxrtos_frame_slot_t;

/**
 * @brief Static major-frame schedule configuration.
 *
 * The valid schedule entries are stored in slots[0 - slot_count).
 * Entries outside this range are not used.
 *
 * @field slots
 *     Fixed-size storage containing the configured frame slots.
 *
 * @field slot_count
 *     Number of valid entries in slots. A valid initialized schedule
 *     has a value in the range [1, MAXRTOS_MAX_FRAME_SLOTS].
 *
 * @field major_frame_length_ticks
 *     Total duration of one major frame. This is the sum of
 *     duration_ticks for all valid slots and shall be nonzero after
 *     successful initialization.
 *
 * The schedule contains no runtime execution state.
 */
typedef struct
{
    maxrtos_frame_slot_t slots[ MAXRTOS_MAX_FRAME_SLOTS ];
    size_t slot_count;
    uint32_t major_frame_length_ticks;
} maxrtos_frame_schedule_t;

/**
 * @brief Initialize a major-frame schedule.
 *
 * The input schedule is validated completely before the destination
 * schedule is populated. If validation fails, no partial schedule
 * shall be considered valid by the caller.
 *
 * @param[out] sched
 *     Destination schedule. Must not be NULL.
 *
 * @param[in] slots
 *     Source array containing the frame configuration. Must not be
 *     NULL. The contents are copied into sched.
 *
 * @param[in] slot_count
 *     Number of entries in slots. Must be greater than zero and no
 *     greater than MAXRTOS_MAX_FRAME_SLOTS.
 *
 * @return
 *     MAXRTOS_OK if the schedule is valid and initialized.
 *
 * @return
 *     MAXRTOS_ERR_INVALID_ARG if:
 *     - sched is NULL;
 *     - slots is NULL;
 *     - slot_count is zero;
 *     - slot_count exceeds MAXRTOS_MAX_FRAME_SLOTS;
 *     - a partition_id is outside the supported partition range; or
 *     - a slot has zero duration.
 */
maxrtos_status_t maxrtos_frame_init(
    maxrtos_frame_schedule_t * sched,
    maxrtos_frame_slot_t const * slots,
    size_t slot_count );

/**
 * @brief Determine the partition assigned to a system tick.
 *
 * The supplied tick value is reduced modulo the major-frame length.
 * The resulting offset is compared against the configured slots in
 * ascending slot order.
 *
 * Slot intervals are half-open: [start_tick, end_tick)
 *
 * Therefore, a tick equal to the end of one slot belongs to the
 * following slot.
 *
 * @param[in] sched
 *     Initialized major-frame schedule. Must not be NULL.
 *
 * @param[in] tick
 *     Absolute system tick. The value may exceed one major-frame
 *     period.
 *
 * @param[out] out_partition_id
 *     Destination for the partition assigned to tick. Must not be
 *     NULL. The value is not modified on failure.
 *
 * @return
 *     MAXRTOS_OK if a valid partition is identified.
 *
 * @return
 *     MAXRTOS_ERR_INVALID_ARG if sched or out_partition_id is NULL,
 *     or if sched does not contain a valid schedule.
 */
maxrtos_status_t maxrtos_frame_partition_at_tick(
    maxrtos_frame_schedule_t const * sched,
    uint32_t tick,
    maxrtos_partition_id_t * out_partition_id );

#endif /* MAXRTOS_KERNEL_FRAME_H */
