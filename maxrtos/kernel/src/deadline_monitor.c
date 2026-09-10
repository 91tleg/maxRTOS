/**
 * @file deadline_monitor.c
 * @brief Implementation of per-process PERIOD/TIME_CAPACITY deadline monitoring.
 */

#include <stddef.h>

#include "maxrtos/kernel/deadline_monitor.h"

maxrtos_status_t maxrtos_deadline_monitor_init(
    maxrtos_deadline_monitor_t * monitor )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( monitor != NULL )
    {
        size_t process_id;

        for( process_id = 0U;
             process_id < MAXRTOS_MAX_PROCESSES;
             process_id++ )
        {
            monitor->period_ticks[ process_id ] = 0U;
            monitor->time_capacity_ticks[ process_id ] = 0U;
            monitor->accumulated_ticks[ process_id ] = 0U;
            monitor->next_period_tick[ process_id ] = 0U;
            monitor->configured[ process_id ] = false;
        }

        status = MAXRTOS_OK;
    }

    return status;
}

maxrtos_status_t maxrtos_deadline_monitor_set_budget(
    maxrtos_deadline_monitor_t * monitor,
    maxrtos_process_id_t process_id,
    uint32_t period_ticks,
    uint32_t time_capacity_ticks,
    uint32_t start_tick )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( monitor != NULL ) &&
        ( maxrtos_process_get( process_id ) != NULL ) &&
        ( period_ticks > 0U ) &&
        ( time_capacity_ticks > 0U ) &&
        ( time_capacity_ticks <= period_ticks ) )
    {
        monitor->period_ticks[ process_id ] = period_ticks;
        monitor->time_capacity_ticks[ process_id ] =
            time_capacity_ticks;
        monitor->accumulated_ticks[ process_id ] = 0U;

        /* First period begins at start_tick; it ends
         * period_ticks later. */
        monitor->next_period_tick[ process_id ] =
            start_tick + period_ticks;

        monitor->configured[ process_id ] = true;

        status = MAXRTOS_OK;
    }

    return status;
}

maxrtos_status_t maxrtos_deadline_monitor_record_running(
    maxrtos_deadline_monitor_t * monitor,
    maxrtos_process_id_t process_id,
    uint32_t current_tick,
    bool * out_violated )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( monitor != NULL ) &&
        ( out_violated != NULL ) &&
        ( maxrtos_process_get( process_id ) != NULL ) )
    {
        *out_violated = false;

        if( monitor->configured[ process_id ] == true )
        {
            if( current_tick >= monitor->next_period_tick[ process_id ] )
            {
                monitor->accumulated_ticks[ process_id ] = 0U;
                monitor->next_period_tick[ process_id ] +=
                    monitor->period_ticks[ process_id ];
            }

            monitor->accumulated_ticks[ process_id ]++;

            if( monitor->accumulated_ticks[ process_id ] >
                monitor->time_capacity_ticks[ process_id ] )
            {
                *out_violated = true;
            }
        }

        status = MAXRTOS_OK;
    }

    return status;
}
