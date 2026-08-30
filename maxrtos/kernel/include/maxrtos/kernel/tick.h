/**
 * @file tick.h
 * @brief Frame-based partition selection and process dispatch
 *        interface.
 *
 * Provides the kernel-level tick operation that composes major-frame
 * partition selection with per-partition process dispatch.
 *
 * The active partition is determined from the supplied tick value
 * using the configured frame schedule. The selected partition is then
 * passed to the partition dispatcher to determine the process that
 * shall run.
 *
 * This module does not maintain scheduling state, advance the system
 * tick, or perform architecture-specific context switching.
 */

#ifndef MAXRTOS_KERNEL_TICK_H
#define MAXRTOS_KERNEL_TICK_H

#include "maxrtos/kernel/frame.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/status.h"

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
    uint32_t tick,
    maxrtos_process_id_t * out_next_id );

#endif /* MAXRTOS_KERNEL_TICK_H */
