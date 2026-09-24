/**
 * @file tick.h
 * @brief Kernel system tick and frame-based scheduling interface.
 *
 * Provides the kernel-level tick interface used to maintain the
 * authoritative system tick and compose major-frame partition
 * selection with per-partition process dispatch.
 *
 * The kernel owns and advances the current system tick. The
 * architecture layer signals the kernel when a hardware timer tick
 * occurs; it does not supply or maintain the tick value.
 *
 * The active partition is determined from the kernel's current tick
 * using the configured frame schedule. The selected partition is then
 * passed to the partition dispatcher to determine the process that
 * shall run.
 */

#ifndef MAXRTOS_KERNEL_TICK_H
#define MAXRTOS_KERNEL_TICK_H

#include "maxrtos/status.h"
#include "maxrtos/types.h"
#include "maxrtos/kernel/frame.h"
#include "maxrtos/kernel/partition.h"

/**
 * @brief Select the active partition and dispatch a process within it.
 *
 * @param[in] sched
 *     Initialized frame schedule. Must not be NULL.
 *
 * @param[in,out] table
 *     Initialized partition table. Must not be NULL.
 *
 * @param[in] tick
 *     Current system tick supplied by the caller.
 *
 * @param[out] out_next_id
 *     Receives the ID of the process selected for execution. Must
 *     not be NULL.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is NULL or invalid.
 */
maxrtos_status_t maxrtos_kernel_on_tick(
    maxrtos_frame_schedule_t const * sched,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t * out_next_id );

/**
 * @brief Return the most recently observed kernel tick.
 *
 * @return Current kernel tick.
 *
 * The value is updated by maxrtos_kernel_on_tick(). Kernel services
 * use this value when calculating timed-wait deadlines.
 */
maxrtos_tick_t maxrtos_kernel_tick_now( void );

/**
 * @brief Convert a relative timeout into an absolute wake tick.
 *
 * @param[in] timeout
 *     Ticks to wait, or MAXRTOS_TIMEOUT_INFINITE for no deadline.
 *
 * @param[out] out_wake_tick
 *     Receives now + timeout, or MAXRTOS_TICK_NONE for an infinite
 *     timeout.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if out_wake_tick is NULL.
 *     MAXRTOS_ERR_OVERFLOW if the wake tick would reach
 *     MAXRTOS_TICK_NONE.
 */
maxrtos_status_t maxrtos_kernel_tick_after(
    maxrtos_tick_t timeout,
    maxrtos_tick_t * out_wake_tick );

/**
 * @brief Reset the kernel tick to zero.
 *
 * The scheduler starts the major frame from tick zero, so this is called
 * by maxrtos_kernel_start_scheduler() before the first tick is processed.
 * It is also used to give unit tests a known starting tick.
 *
 * Any timed-wait deadline recorded earlier is relative to the old
 * counter and is not adjusted, so this must not be called while
 * processes are blocked with timeouts.
 */
void maxrtos_kernel_tick_reset( void );

#endif /* MAXRTOS_KERNEL_TICK_H */
