/**
 * @file fault_recovery.h
 * @brief Health-monitor fault recovery interface.
 *
 * Applies configured recovery actions to process and partition state.
 *
 * Supported actions:
 * - MAXRTOS_HM_ACTION_IGNORE
 * - MAXRTOS_HM_ACTION_RESTART_PROCESS
 * - MAXRTOS_HM_ACTION_HALT_PARTITION
 *
 * This module is architecture-independent. Architecture-specific
 * process stack initialization remains the responsibility of the
 * caller when restarting a process.
 */

#ifndef MAXRTOS_KERNEL_FAULT_RECOVERY_H
#define MAXRTOS_KERNEL_FAULT_RECOVERY_H

#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/health_monitor.h"
#include "maxrtos/status.h"

/**
 * @brief Enact the health-monitor recovery action for a fault.
 *
 * @param[in] hm
 *     Initialized health-monitor configuration. Must not be NULL.
 *
 * @param[in,out] table
 *     Initialized partition table. Must not be NULL.
 *
 * @param[in] faulting_process_id
 *     Identifier of the process associated with the fault.
 *
 * @param[in] fault_type
 *     Fault classification used to select the configured recovery
 *     action.
 *
 * @param[out] out_action
 *     Receives the recovery action selected by the health-monitor
 *     policy. Must not be NULL.
 *
 * @return
 *     MAXRTOS_OK if the configured recovery action was successfully
 *     enacted.
 *     MAXRTOS_ERR_INVALID_ARG if a required pointer argument is NULL.
 *     MAXRTOS_ERR_INVALID_ID if faulting_process_id does not identify
 *     an allocated process.
 *
 * @note
 *     MAXRTOS_HM_ACTION_IGNORE is a successful outcome and does not
 *     modify kernel state.
 *
 * @note
 *     When MAXRTOS_HM_ACTION_RESTART_PROCESS is returned successfully,
 *     the process has been returned to READY state and re-admitted to
 *     its partition scheduler. Architecture-specific stack
 *     initialization remains the responsibility of the caller.
 */
maxrtos_status_t maxrtos_fault_recovery_handle(
    maxrtos_health_monitor_t const * hm,
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t faulting_process_id,
    maxrtos_fault_type_t fault_type,
    maxrtos_hm_action_t * out_action );

#endif /* MAXRTOS_KERNEL_FAULT_RECOVERY_H */
