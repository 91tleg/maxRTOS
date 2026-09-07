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

#include "maxrtos/kernel/health_monitor.h"
#include "maxrtos/kernel/partition.h"

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

#endif /* MAXRTOS_ARCH_CORTEX_M7_FAULT_HANDLERS_H */
