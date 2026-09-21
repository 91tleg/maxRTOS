/**
 * @file context_switch.h
 * @brief Cortex-M7 context-switch interface.
 *
 * Provides the architecture-specific interface for process stack
 * initialization, context-switch state management, and PendSV-based
 * context switching.
 */

#ifndef MAXRTOS_ARCH_CORTEX_M7_CONTEXT_SWITCH_H
#define MAXRTOS_ARCH_CORTEX_M7_CONTEXT_SWITCH_H

#include <stdbool.h>

#include "maxrtos/kernel/process.h"

extern maxrtos_process_control_block_t * g_maxrtos_current_pcb;
extern maxrtos_process_control_block_t * g_maxrtos_next_pcb;

/**
 * @brief Initialize a process execution stack.
 *
 * Initializes the process stack so that the process can begin
 * execution through the Cortex-M7 context-switch mechanism.
 *
 * @param[in,out] pcb
 *     Process control block to initialize.
 *
 * @return 
 *     MAXRTOS_OK if the stack was initialized successfully.
 *     MAXRTOS_ERR_INVALID_ARG if pcb or any required PCB field
 *     is invalid.
 */
maxrtos_status_t maxrtos_arch_init_stack(
    maxrtos_process_control_block_t * pcb );

/**
 * @brief Set the architecture-layer current process.
 *
 * Updates the process control block used by the context-switch
 * mechanism as the currently executing process.
 *
 * A NULL value indicates that there is no process context to save.
 * This state is used when starting the first process and when the
 * current process context has been discarded during fault recovery.
 *
 * @param[in] pcb 
 *     Process control block of the currently executing
 *     process, or NULL when no current context exists.
 */
void maxrtos_arch_set_current_pcb(
    maxrtos_process_control_block_t * pcb );

/**
 * @brief Set the process for the next context switch.
 *
 * Updates the process control block that shall be restored by the
 * next PendSV context switch.
 *
 * @param[in] pcb
 *     Process control block to restore.
 *
 * @pre pcb shall not be NULL when a context switch is requested.
 */
void maxrtos_arch_set_next_pcb(
    maxrtos_process_control_block_t * pcb );

/**
 * @brief Get the architecture-layer current process.
 *
 * Returns the process control block currently registered with the
 * architecture context-switch mechanism.
 *
 * @return 
 *     Pointer to the current process control block.
 *     NULL if no process context is currently registered.
 */
maxrtos_process_control_block_t * maxrtos_arch_get_current_pcb( void );

/**
 * @brief Deliver a completed blocking operation's result to a process.
 *
 * If the kernel completed an operation the process was blocked in
 * (ipc_result_pending), writes ipc_result into the process's saved
 * exception frame as the system-call return value (r0) and clears the
 * flag. Called by the PendSV handler before the process is restored.
 *
 * @param[in,out] pcb
 *     Process about to be restored. May be NULL.
 */
void maxrtos_arch_apply_resume_result(
    maxrtos_process_control_block_t * pcb );

/**
 * @brief Apply the privilege level for the next process.
 *
 * Updates the Cortex-M CONTROL.nPRIV bit from the privilege of the
 * process's partition (see maxrtos_arch_partition_is_privileged()). A
 * process of an application partition executes in unprivileged Thread
 * mode; a process of a system partition, and the architecture idle
 * context (which belongs to no partition), execute in privileged Thread
 * mode.
 *
 * @param[in] pcb
 *     Process control block describing the privilege level to apply.
 *     If NULL, no action is performed.
 */
void maxrtos_arch_apply_privilege_for_next_pcb(
    maxrtos_process_control_block_t const * pcb );

/**
 * @brief Declare a partition a system partition (privileged).
 *
 * Partitions are unprivileged by default, which is what application
 * partitions must be: a privileged process can reprogram the MPU, mask
 * interrupts and, when PRIVDEFENA is set, access all memory, so it is not
 * isolated at all. Only partitions that hold platform software (for
 * example a driver partition) are made privileged, and all of their
 * processes are, since they share one memory domain.
 *
 * Called from the generated configuration, before the scheduler starts.
 *
 * @param[in] partition_id
 *     Partition to configure. Ignored if out of range.
 *
 * @param[in] privileged
 *     true to run the partition's processes privileged.
 */
void maxrtos_arch_set_partition_privileged(
    maxrtos_partition_id_t partition_id,
    bool privileged );

/**
 * @brief Whether a partition's processes run privileged.
 *
 * @return
 *     true if partition_id was declared a system partition, or is
 *     MAXRTOS_INVALID_PARTITION_ID (the architecture idle context).
 *     false otherwise, including for out-of-range identifiers.
 */
bool maxrtos_arch_partition_is_privileged(
    maxrtos_partition_id_t partition_id );

/**
 * @brief Request a context switch.
 *
 * Pends the Cortex-M7 PendSV exception. The requested context switch
 * is performed when PendSV is serviced.
 *
 * The current and next process state shall be configured before this
 * function is called.
 */
void maxrtos_arch_request_context_switch( void );

/**
 * @brief Initialize the Cortex-M7 context-switch mechanism.
 *
 * Configures the architecture state required for PendSV-based
 * context switching, including the PendSV exception priority.
 *
 * This function shall be called once during system initialization
 * before process execution begins.
 */
void maxrtos_arch_context_switch_init( void );

/**
 * @brief Start execution of the first process.
 *
 * Initializes the architecture context-switch state for the first
 * process and requests a PendSV exception to transfer execution to
 * that process.
 *
 * No process context is expected to be active when this function is
 * called.
 *
 * @param[in] pcb
 *     Process control block of the first process.
 *
 * @pre pcb shall not be NULL.
 * @pre pcb shall contain an initialized process stack.
 *
 * @note This function does not return during normal operation.
 */
void maxrtos_arch_start_first_process(
    maxrtos_process_control_block_t * pcb );

#endif /* MAXRTOS_ARCH_CORTEX_M7_CONTEXT_SWITCH_H */
