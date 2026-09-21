/**
 * @file fault_handlers.h
 * @brief Cortex-M7 fault exception configuration and fault-handler
 *        interface.
 *
 * Enables the configurable Cortex-M7 fault exceptions and integer
 * divide-by-zero trapping during system initialization. The
 * architecture fault handlers classify faults and delegate recovery
 * policy to the kernel fault-recovery service.
 *
 * MemManage, BusFault, and UsageFault exceptions are disabled by
 * default and must be enabled through SCB->SHCSR. Integer
 * divide-by-zero trapping is disabled by default and must be enabled
 * through SCB->CCR.DIV_0_TRP. maxrtos_arch_fault_handlers_init()
 * performs these configuration steps and shall be called once during
 * system initialization before process execution begins.
 *
 * The fault handlers obtain the currently executing process through
 * the context-switch architecture interface, classify the exception,
 * and invoke the kernel fault-recovery service. The resulting recovery
 * action determines the architecture-specific response, which may
 * include process restart, partition halt, or continuation.
 *
 * Because ARMv7-M exception handlers have fixed entry-point
 * signatures, the health-monitor and partition-table references
 * required by the recovery path are registered during system
 * initialization through the setter functions provided by this
 * interface.
 *
 * When a partition-halt action is selected, immediate selection of a
 * process in another partition is deferred to the system's scheduling
 * mechanism. The current implementation waits for a subsequent
 * scheduling event rather than performing an independent partition
 * selection from the fault handler.
 */

#ifndef MAXRTOS_ARCH_CORTEX_M7_FAULT_HANDLERS_H
#define MAXRTOS_ARCH_CORTEX_M7_FAULT_HANDLERS_H

#include <stdint.h>

#include "maxrtos/kernel/health_monitor.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"

/**
 * @brief Enable configurable fault exceptions and divide-by-zero
 *        trapping.
 *
 * Enables the MemManage, BusFault, UsageFault, integer divide-by-zero
 * trapping.
 *
 * This function shall be called once during system initialization
 * before process execution begins.
 */
void maxrtos_arch_fault_handlers_init( void );

/**
 * @brief Status of the most recent fault exception.
 *
 * Captured by the fault exception handlers before the status bits are
 * cleared. Intended for a debugger: after an unexpected fault, read
 * g_maxrtos_arch_last_fault. mmfar is the refused data address of a memory
 * fault (0 if not valid), which identifies a missing MPU region directly.
 */
typedef struct
{
    uint32_t cfsr;  /* Configurable Fault Status Register */
    uint32_t hfsr;  /* HardFault Status Register */
    uint32_t mmfar; /* MemManage Fault Address, if valid */
    uint32_t bfar;  /* BusFault Address, if valid */
    uint32_t count; /* faults taken since reset */
} maxrtos_arch_fault_info_t;

extern volatile maxrtos_arch_fault_info_t g_maxrtos_arch_last_fault;

/**
 * @brief Register the health-monitor instance used by fault recovery.
 *
 * @param[in] hm
 *     Health-monitor instance. The caller shall ensure that the
 *     referenced object remains valid while fault handling is enabled.
 */
void maxrtos_arch_set_health_monitor(
    maxrtos_health_monitor_t const * hm );

/**
 * @brief Register the partition table used by fault recovery.
 *
 * @param[in,out] table
 *     Partition table. The caller shall ensure that the referenced
 *     object remains valid while fault handling is enabled.
 */
void maxrtos_arch_set_partition_table(
    maxrtos_partition_table_t * table );

/**
 * @brief Retrieve the registered partition table used by fault recovery.
 *
 * @return The partition-table pointer previously registered via
 * `maxrtos_arch_set_partition_table()` or NULL if none has been set.
 */
maxrtos_partition_table_t * maxrtos_arch_get_partition_table( void );

/**
 * @brief Recover from a classified fault of the current process.
 *
 * Asks the kernel health monitor for the action configured for the
 * current process's partition and fault type, and enacts it: IGNORE
 * returns to the caller, RESTART_PROCESS gives the process a fresh
 * context and reselects, HALT_PARTITION leaves through the idle context.
 * A context switch, if any, takes effect when the pending switch runs.
 * Halts the CPU on an unrecoverable configuration error.
 *
 * Called from the fault exception handlers. Hardware independent.
 *
 * @param[in] fault_type
 *     Classification of the fault.
 */
void maxrtos_arch_handle_fault( maxrtos_fault_type_t fault_type );

/**
 * @brief Recover from a fault attributed to a given process.
 *
 * Like maxrtos_arch_handle_fault(), for a process that need not be the
 * running one, such as a deadline miss of a process that is ready or
 * blocked. A restarted process that is not running gets a fresh context and
 * is dispatched normally; a halt of its partition also parks the CPU in the
 * idle context if the running process belongs to that partition.
 *
 * @param[in] faulting_pcb
 *     The process the fault is charged to. Shall not be NULL.
 *
 * @param[in] fault_type
 *     Classification of the fault.
 */
void maxrtos_arch_handle_process_fault(
    maxrtos_process_control_block_t * faulting_pcb,
    maxrtos_fault_type_t fault_type );

#endif /* MAXRTOS_ARCH_CORTEX_M7_FAULT_HANDLERS_H */
