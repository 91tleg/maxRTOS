/**
 * @file idle.h
 * @brief Cortex-M7 idle context.
 *
 * The idle context is an architecture-owned, privileged thread that
 * executes WFI. It runs when the frame schedule has no runnable process
 * to give the CPU to, for example during a slot of a halted partition.
 *
 * It is not a kernel process: it has no process ID, is not in the process
 * pool, is never on a ready queue, and belongs to no partition, so it is
 * mapped by no partition MPU region.
 */

#ifndef MAXRTOS_ARCH_CORTEX_M7_IDLE_H
#define MAXRTOS_ARCH_CORTEX_M7_IDLE_H

#include "maxrtos/kernel/process.h"

/**
 * @brief Prepare the idle context's initial stack frame.
 *
 * Called from maxrtos_arch_context_switch_init().
 */
void maxrtos_arch_idle_init( void );

/**
 * @brief Get the idle context's control block.
 *
 * The block has partition_id MAXRTOS_INVALID_PARTITION_ID, which the MPU
 * configuration treats as "no partition memory to map".
 */
maxrtos_process_control_block_t * maxrtos_arch_idle_pcb( void );

/**
 * @brief Request a context switch to the idle context.
 *
 * The current context is not saved. Use this only when the current
 * context must never resume (for example after its partition is halted),
 * or when it is already saved/irrelevant. Takes effect when PendSV runs.
 */
void maxrtos_arch_idle_enter_discarding_current( void );

#endif /* MAXRTOS_ARCH_CORTEX_M7_IDLE_H */
