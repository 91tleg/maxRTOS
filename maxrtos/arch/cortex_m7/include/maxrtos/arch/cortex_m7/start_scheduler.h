/**
 * @file start_scheduler.h
 * @brief Cortex-M7 scheduler startup interface.
 *
 * Provides the architecture-specific entry point used to start
 * MAXRTOS scheduling on a Cortex-M7 target.
 *
 * The startup sequence initializes the kernel scheduler using the
 * supplied frame schedule and partition table, then transfers control
 * to the first scheduled process through the Cortex-M7 context-switch
 * implementation.
 */

#ifndef MAXRTOS_ARCH_CORTEX_M7_START_SCHEDULER_H
#define MAXRTOS_ARCH_CORTEX_M7_START_SCHEDULER_H

#include "maxrtos/kernel/frame.h"
#include "maxrtos/kernel/partition.h"

/**
 * @brief Start the scheduler on Cortex-M7.
 *
 * Starts the kernel scheduler from the beginning of the major frame
 * and transfers execution to the first scheduled process.
 *
 * This function does not return during normal operation. If kernel
 * scheduler startup fails, execution enters a debug breakpoint loop.
 *
 * @param[in] frame_schedule
 *     Initialized major-frame schedule. Must not be NULL.
 *
 * @param[in,out] partition_table
 *     Initialized partition scheduler table. Must not be NULL.
 */
void maxrtos_arch_start_scheduler(
    maxrtos_frame_schedule_t const * frame_schedule,
    maxrtos_partition_table_t * partition_table );

#endif /* MAXRTOS_ARCH_CORTEX_M7_START_SCHEDULER_H */
