/**
 * @file timing.h
 * @brief Periodic release and deadline supervision (ARINC 653 PERIOD and
 *        TIME_CAPACITY).
 *
 * A process with a period is released every `period` ticks. Its time
 * capacity is a deadline: measured from the start of the current release,
 * it is the wall-clock time by which the process must complete the release
 * by calling maxrtos_periodic_wait(). It is not a CPU budget: a process
 * starved by others misses its deadline without having used any of it.
 *
 * Lifecycle of a release
 *   release      at release_time the deadline is armed for
 *                release_time + time_capacity
 *   completion   maxrtos_periodic_wait() disarms the deadline and blocks the
 *                process until the next release (release_time + period)
 *   miss         if the tick reaches the deadline first, the miss is
 *                recorded, the deadline is disarmed, and the architecture
 *                layer raises MAXRTOS_FAULT_DEADLINE_EXCEEDED into the
 *                health monitor of the process's partition, which applies
 *                its configured action (restart the process, halt the
 *                partition)
 *
 * A restarted process starts a new release at the restart tick.
 *
 * The tick drives everything: maxrtos_kernel_on_tick() releases due
 * processes and detects misses. Detection is recorded by the kernel and
 * consumed by the architecture layer, which enacts the recovery (the kernel
 * does not call into the architecture layer).
 */

#ifndef MAXRTOS_KERNEL_TIMING_H
#define MAXRTOS_KERNEL_TIMING_H

#include <stdbool.h>
#include <stddef.h>

#include "maxrtos/status.h"
#include "maxrtos/types.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"

/**
 * @brief Configure a process's period and time capacity.
 *
 * Starts a release at the current tick and arms the deadline. When the
 * scheduler starts, all timing is re-armed from tick zero.
 *
 * @param[in] id
 *     Process to configure.
 *
 * @param[in] period_ticks
 *     Release interval in ticks. 0 makes the process aperiodic: its
 *     deadline, if any, is measured once from the start.
 *
 * @param[in] time_capacity_ticks
 *     Deadline relative to each release, in ticks. 0 means no deadline. For
 *     a periodic process it shall not exceed period_ticks.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ID if id does not identify a process.
 *     MAXRTOS_ERR_INVALID_ARG if time_capacity_ticks exceeds a non-zero
 *     period_ticks.
 */
maxrtos_status_t maxrtos_process_set_timing(
    maxrtos_process_id_t id,
    maxrtos_tick_t period_ticks,
    maxrtos_tick_t time_capacity_ticks );

/**
 * @brief Begin a new release of a process at tick now.
 *
 * Used when a process (re)starts. Clears any periodic wait and arms the
 * deadline for now + time_capacity.
 */
void maxrtos_process_rearm_timing(
    maxrtos_process_control_block_t * pcb,
    maxrtos_tick_t now );

/**
 * @brief Complete the running process's release and wait for the next.
 *
 * Disarms the deadline. If the next release point has not been reached,
 * blocks the process until it. If it has already passed (an overrun), the
 * next release begins immediately.
 *
 * If no other process of the partition is READY the process still blocks
 * and the CPU idles until the release or the end of the slot; out_next_id
 * is then MAXRTOS_INVALID_PROCESS_ID.
 *
 * @param[in,out] table
 *     Partition table.
 *
 * @param[in] id
 *     The calling process. Shall be RUNNING and periodic.
 *
 * @param[out] out_next_id
 *     The process to run in its place if it blocked, or
 *     MAXRTOS_INVALID_PROCESS_ID to idle. Valid only for MAXRTOS_PENDING.
 *
 * @return
 *     MAXRTOS_OK if the next release began immediately.
 *     MAXRTOS_PENDING if the process was blocked until its next release. Its
 *     final result (MAXRTOS_OK) is delivered when it resumes.
 *     MAXRTOS_ERR_INVALID_ARG on a NULL argument.
 *     MAXRTOS_ERR_INVALID_ID if id is not a process.
 *     MAXRTOS_ERR_INVALID_STATE if the process is not RUNNING or is not
 *     periodic.
 */
maxrtos_status_t maxrtos_kernel_periodic_wait(
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t id,
    maxrtos_process_id_t * out_next_id );

/**
 * @brief Release every process whose next release point has been reached.
 *
 * @return Number of processes released.
 */
size_t maxrtos_kernel_release_periodic(
    maxrtos_partition_table_t * table,
    maxrtos_tick_t now );

/**
 * @brief Detect deadline misses at tick now.
 *
 * Every armed deadline with deadline_time <= now, belonging to a
 * non-suspended process of a non-halted partition, is recorded as a miss
 * and disarmed. Retrieve them with maxrtos_kernel_take_deadline_miss().
 *
 * @return Number of misses recorded.
 */
size_t maxrtos_kernel_check_deadlines(
    maxrtos_partition_table_t const * table,
    maxrtos_tick_t now );

/**
 * @brief Take one recorded deadline miss, lowest process ID first.
 *
 * @param[out] out_id
 *     Receives the process that missed.
 *
 * @return true if a miss was taken, false if none are pending.
 */
bool maxrtos_kernel_take_deadline_miss( maxrtos_process_id_t * out_id );

/**
 * @brief Re-arm the timing of every process from tick zero.
 *
 * Called when the scheduler starts, since the major frame and all releases
 * begin at tick zero.
 */
void maxrtos_kernel_rearm_all_timing( void );

/** @brief Forget recorded misses. Called when the process pool is reset. */
void maxrtos_kernel_timing_reset( void );

#endif /* MAXRTOS_KERNEL_TIMING_H */
