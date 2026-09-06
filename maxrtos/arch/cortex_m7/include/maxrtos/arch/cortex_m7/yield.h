/**
 * @file yield.h
 * @brief ARM Cortex-M7 hardware execution context switch trigger.
 *
 * Provides the target-specific interface for initiating voluntary
 * process yields on ARM Cortex-M7.
 */

#ifndef MAXRTOS_ARCH_CORTEX_M7_YIELD_H
#define MAXRTOS_ARCH_CORTEX_M7_YIELD_H

/**
 * @brief Initiates a voluntary process yield request.
 *
 * Identifies the currently active process and partition context, evaluates
 * next process selection via the kernel, and executes a context switch if a
 * different process is selected.
 *
 * @post If a distinct READY process exists within the current partition, a
 *       hardware context switch occurs to that process.
 *
 * @post If no partition table is configured or no other process is READY,
 *       no context switch occurs and execution continues in the calling 
 *       process context.
 */
void maxrtos_arch_yield( void );

#endif /* MAXRTOS_ARCH_CORTEX_M7_YIELD_H */
