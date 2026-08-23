/**
 * @file health_monitor.h
 * @brief Health-monitor fault classification and recovery policy
 *        interface.
 *
 * Defines the fault classifications, recovery actions, and policy
 * table used by the kernel health-monitor service.
 *
 * The health-monitor service maps a fault classification and
 * partition identifier to a configured recovery action. It selects
 * the recovery action but does not perform the recovery operation.
 *
 * Architecture-specific fault handling, processor context
 * management, process stack reconstruction, and context switching
 * are outside the scope of this module.
 *
 * All policy entries are initialized to
 * MAXRTOS_HM_ACTION_HALT_PARTITION by maxrtos_hm_init(). Less
 * restrictive actions require explicit configuration through
 * maxrtos_hm_set_policy().
 */

#ifndef MAXRTOS_KERNEL_HEALTH_MONITOR_H
#define MAXRTOS_KERNEL_HEALTH_MONITOR_H

#include "maxrtos/kernel/process.h"
#include "maxrtos/status.h"
#include "maxrtos/config.h"

/**
 * @brief Fault classifications supported by the health monitor.
 *
 * MAXRTOS_FAULT_COUNT is a sizing sentinel and is not a valid fault
 * classification. New classifications shall be added before the
 * sentinel.
 */
typedef enum
{
    MAXRTOS_FAULT_MEMORY_ACCESS = 0U,
    MAXRTOS_FAULT_BUS_ERROR,
    MAXRTOS_FAULT_ILLEGAL_INSTRUCTION,
    MAXRTOS_FAULT_DIVIDE_BY_ZERO,
    MAXRTOS_FAULT_UNEXPECTED_RETURN,

    MAXRTOS_FAULT_COUNT,
} maxrtos_fault_type_t;

/**
 * @brief Recovery actions supported by the health monitor.
 * 
 * MAXRTOS_HM_ACTION_COUNT is a sizing sentinel and is
 * not a valid recovery action.
 */
typedef enum
{
    /* Take no recovery action. */
    MAXRTOS_HM_ACTION_IGNORE = 0U,

    /* Restart the faulting process.
     * The recovery mechanism is responsible for restoring the
     * process to a valid initial execution state and re-admitting
     * it to its partition scheduler. */
    MAXRTOS_HM_ACTION_RESTART_PROCESS,

    /* Halt the partition containing the faulting process.
     * Dispatch operations for the affected partition shall be
     * rejected while the partition remains halted. */
    MAXRTOS_HM_ACTION_HALT_PARTITION,

    MAXRTOS_HM_ACTION_COUNT,
} maxrtos_hm_action_t;

/**
 * @brief Fault-recovery policy table.
 *
 * Each entry defines the recovery action for a specific partition
 * and fault classification.
 *
 * @field policy
 *     policy[p][f] contains the recovery action for fault type f
 *     occurring in partition p.
 */
typedef struct
{
    maxrtos_hm_action_t policy[ MAXRTOS_MAX_PARTITIONS ][ MAXRTOS_FAULT_COUNT ];
} maxrtos_health_monitor_t;

/**
 * @brief Initialize a health-monitor policy table.
 *
 * All partition and fault-classification combinations are initialized
 * to MAXRTOS_HM_ACTION_HALT_PARTITION.
 *
 * @param[out] hm
 *     Health monitor to initialize. Must not be NULL.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if hm is NULL.
 */
maxrtos_status_t maxrtos_hm_init(
    maxrtos_health_monitor_t * hm );

/**
 * @brief Configure a recovery policy.
 *
 * @param[in,out] hm
 *     Health monitor to modify. Must not be NULL.
 *
 * @param[in] partition_id
 *     Partition to which the policy applies. Must be less than
 *     MAXRTOS_MAX_PARTITIONS.
 *
 * @param[in] fault_type
 *     Fault classification to which the policy applies. Must be
 *     less than MAXRTOS_FAULT_COUNT.
 *
 * @param[in] action
 *     Recovery action to configure. Must be less than
 *     MAXRTOS_HM_ACTION_COUNT.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if hm is NULL or any supplied
 *     identifier or enumeration value is outside its valid range.
 */
maxrtos_status_t maxrtos_hm_set_policy(
    maxrtos_health_monitor_t * hm,
    maxrtos_partition_id_t partition_id,
    maxrtos_fault_type_t fault_type,
    maxrtos_hm_action_t action );

/**
 * @brief Retrieve a configured recovery policy.
 *
 * @param[in] hm
 *     Initialized health monitor. Must not be NULL.
 *
 * @param[in] partition_id
 *     Partition whose policy is requested. Must be less than
 *     MAXRTOS_MAX_PARTITIONS.
 *
 * @param[in] fault_type
 *     Fault classification whose policy is requested. Must be less
 *     than MAXRTOS_FAULT_COUNT.
 *
 * @param[out] out_action
 *     Receives the configured recovery action. Must not be NULL.
 *     Remains unmodified if the function fails.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if hm or out_action is NULL, or if
 *     partition_id or fault_type is outside its valid range.
 */
maxrtos_status_t maxrtos_hm_get_policy(
    maxrtos_health_monitor_t const * hm,
    maxrtos_partition_id_t partition_id,
    maxrtos_fault_type_t fault_type,
    maxrtos_hm_action_t * out_action );

#endif /* MAXRTOS_KERNEL_HEALTH_MONITOR_H */
