/**
 * @file health_monitor.c
 * @brief Implementation of health-monitor fault classification and
 *        recovery policy services.
 *
 * Implements storage, validation, and retrieval of fault-recovery
 * policies used by the kernel fault-recovery service.
 *
 * The module maintains configured policy data only. It does not
 * maintain run-time fault state, access architecture-specific
 * hardware, or perform fault handling directly.
 */

#include <stddef.h>

#include "maxrtos/kernel/health_monitor.h"

maxrtos_status_t maxrtos_hm_init(
    maxrtos_health_monitor_t * hm )
{
    maxrtos_status_t status;
    size_t partition_id;
    size_t fault_type;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( hm != NULL )
    {
        for( partition_id = 0U;
             partition_id < MAXRTOS_MAX_PARTITIONS;
             partition_id++ )
        {
            for( fault_type = 0U;
                 fault_type < MAXRTOS_FAULT_COUNT;
                 fault_type++ )
            {
                hm->policy[ partition_id ][ fault_type ] = 
                    MAXRTOS_HM_ACTION_HALT_PARTITION;
            }
        }

        status = MAXRTOS_OK;
    }

    return status;
}

maxrtos_status_t maxrtos_hm_set_policy(
    maxrtos_health_monitor_t * hm,
    maxrtos_partition_id_t partition_id,
    maxrtos_fault_type_t fault_type,
    maxrtos_hm_action_t action )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( hm != NULL ) &&
        ( partition_id < MAXRTOS_MAX_PARTITIONS ) &&
        ( fault_type < MAXRTOS_FAULT_COUNT ) &&
        ( action < MAXRTOS_HM_ACTION_COUNT ) )
    {
        hm->policy[ partition_id ][ fault_type ] = action;
        status = MAXRTOS_OK;
    }

    return status;
}

maxrtos_status_t maxrtos_hm_get_policy(
    maxrtos_health_monitor_t const * hm,
    maxrtos_partition_id_t partition_id,
    maxrtos_fault_type_t fault_type,
    maxrtos_hm_action_t * out_action )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( hm != NULL ) &&
        ( out_action != NULL ) &&
        ( partition_id < MAXRTOS_MAX_PARTITIONS ) &&
        ( fault_type < MAXRTOS_FAULT_COUNT ) )
    {
        *out_action = hm->policy[ partition_id ][ fault_type ];
        status = MAXRTOS_OK;
    }

    return status;
}
