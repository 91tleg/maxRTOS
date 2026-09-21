/**
 * @file systick.h
 * @brief Cortex-M7 SysTick integration.
 */

#ifndef MAXRTOS_ARCH_CORTEX_M7_SYSTICK_H
#define MAXRTOS_ARCH_CORTEX_M7_SYSTICK_H

#include "maxrtos/kernel/frame.h"
#include "maxrtos/kernel/partition.h"

/**
 * @brief Initialize the Cortex-M7 SysTick integration.
 *
 * Stores the frame schedule and partition table used by the SysTick
 * handler and resets the kernel tick count.
 *
 * @param[in] schedule
 *     Frame schedule used for kernel tick processing.
 *
 * @param[in] table
 *     Partition table used for kernel tick processing.
 */
void maxrtos_arch_systick_init(
    maxrtos_frame_schedule_t const * schedule,
    maxrtos_partition_table_t * table );

/**
 * @brief Get the frame schedule registered by maxrtos_arch_systick_init().
 *
 * @return
 *     The frame schedule, or NULL if the SysTick integration has not
 *     been initialized.
 */
maxrtos_frame_schedule_t const * maxrtos_arch_get_frame_schedule( void );

/**
 * @brief Handle a Cortex-M7 SysTick interrupt.
 *
 * Advances the kernel tick and requests a context switch when the
 * scheduler selects a process different from the current process.
 */
void maxrtos_arch_systick( void );

#endif /* MAXRTOS_ARCH_CORTEX_M7_SYSTICK_H */
