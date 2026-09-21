/**
 * @file timing.c
 * @brief Periodic release and deadline supervision. See timing.h.
 */

#include <stdint.h>

#include "maxrtos/config.h"
#include "maxrtos/kernel/ipc_block.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/tick.h"
#include "maxrtos/kernel/timing.h"

_Static_assert( MAXRTOS_MAX_PROCESSES <= 32U,
                "pending deadline misses are kept in one 32-bit mask" );

/* One bit per process ID: a miss detected but not yet handled. */
static uint32_t s_pending_misses = 0U;

void maxrtos_kernel_timing_reset( void )
{
    s_pending_misses = 0U;
}

static void arm_deadline( maxrtos_process_control_block_t * pcb )
{
    if( pcb->time_capacity > 0U )
    {
        pcb->deadline_time = pcb->release_time + pcb->time_capacity;
    }
    else
    {
        pcb->deadline_time = MAXRTOS_TICK_NONE;
    }
}

void maxrtos_process_rearm_timing(
    maxrtos_process_control_block_t * pcb,
    maxrtos_tick_t now )
{
    if( pcb != NULL )
    {
        pcb->periodic_waiting = false;
        pcb->release_time = now;
        arm_deadline( pcb );
    }
}

maxrtos_status_t maxrtos_process_set_timing(
    maxrtos_process_id_t id,
    maxrtos_tick_t period_ticks,
    maxrtos_tick_t time_capacity_ticks )
{
    maxrtos_status_t status;
    maxrtos_process_control_block_t * pcb;

    pcb = maxrtos_process_get( id );

    if( pcb == NULL )
    {
        status = MAXRTOS_ERR_INVALID_ID;
    }
    else if( ( period_ticks > 0U ) && ( time_capacity_ticks > period_ticks ) )
    {
        status = MAXRTOS_ERR_INVALID_ARG;
    }
    else
    {
        pcb->period = period_ticks;
        pcb->time_capacity = time_capacity_ticks;
        maxrtos_process_rearm_timing( pcb, maxrtos_kernel_tick_now() );
        status = MAXRTOS_OK;
    }

    return status;
}

void maxrtos_kernel_rearm_all_timing( void )
{
    for( maxrtos_process_id_t id = 0U;
         id < ( maxrtos_process_id_t ) MAXRTOS_MAX_PROCESSES;
         id++ )
    {
        maxrtos_process_control_block_t * pcb = maxrtos_process_get( id );

        if( pcb != NULL )
        {
            maxrtos_process_rearm_timing( pcb, 0U );
        }
    }
}

maxrtos_status_t maxrtos_kernel_periodic_wait(
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t id,
    maxrtos_process_id_t * out_next_id )
{
    maxrtos_status_t status;
    maxrtos_process_control_block_t * pcb;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( table != NULL ) && ( out_next_id != NULL ) )
    {
        pcb = maxrtos_process_get( id );

        if( pcb == NULL )
        {
            status = MAXRTOS_ERR_INVALID_ID;
        }
        else if( ( pcb->state != MAXRTOS_PROCESS_STATE_RUNNING ) ||
                 ( pcb->period == 0U ) )
        {
            status = MAXRTOS_ERR_INVALID_STATE;
        }
        else
        {
            maxrtos_tick_t now;
            maxrtos_tick_t next_release;

            now = maxrtos_kernel_tick_now();
            next_release = pcb->release_time + pcb->period;

            /* The release is complete: its deadline is met. */
            pcb->deadline_time = MAXRTOS_TICK_NONE;

            if( now >= next_release )
            {
                /* Overrun: the next release is already due. */
                pcb->release_time = next_release;
                arm_deadline( pcb );
                status = MAXRTOS_OK;
            }
            else
            {
                status = maxrtos_partition_block_and_dispatch(
                    table, pcb->partition_id, id, out_next_id );

                if( status == MAXRTOS_ERR_QUEUE_EMPTY )
                {
                    /* Nothing else to run in this partition: block anyway
                     * and let the CPU idle until the release. */
                    status = maxrtos_process_set_state(
                        id, MAXRTOS_PROCESS_STATE_BLOCKED );
                    table->current_id[ pcb->partition_id ] =
                        MAXRTOS_INVALID_PROCESS_ID;
                    *out_next_id = MAXRTOS_INVALID_PROCESS_ID;
                }

                if( status == MAXRTOS_OK )
                {
                    pcb->periodic_waiting = true;
                    pcb->wake_tick = next_release;
                    status = MAXRTOS_PENDING;
                }
                else
                {
                    /* Could not block: the release is still open. */
                    arm_deadline( pcb );
                }
            }
        }
    }

    return status;
}

size_t maxrtos_kernel_release_periodic(
    maxrtos_partition_table_t * table,
    maxrtos_tick_t now )
{
    size_t released = 0U;

    for( maxrtos_process_id_t id = 0U;
         id < ( maxrtos_process_id_t ) MAXRTOS_MAX_PROCESSES;
         id++ )
    {
        maxrtos_process_control_block_t * pcb = maxrtos_process_get( id );

        if( ( pcb != NULL ) &&
            ( pcb->state == MAXRTOS_PROCESS_STATE_BLOCKED ) &&
            ( pcb->periodic_waiting == true ) &&
            ( now >= pcb->wake_tick ) )
        {
            pcb->release_time = pcb->wake_tick;
            pcb->wake_tick = MAXRTOS_TICK_NONE;
            pcb->periodic_waiting = false;
            arm_deadline( pcb );

            if( maxrtos_ipc_resolve( table, id, MAXRTOS_OK ) == MAXRTOS_OK )
            {
                released++;
            }
        }
    }

    return released;
}

size_t maxrtos_kernel_check_deadlines(
    maxrtos_partition_table_t const * table,
    maxrtos_tick_t now )
{
    size_t misses = 0U;

    for( maxrtos_process_id_t id = 0U;
         id < ( maxrtos_process_id_t ) MAXRTOS_MAX_PROCESSES;
         id++ )
    {
        maxrtos_process_control_block_t * pcb = maxrtos_process_get( id );

        if( ( pcb != NULL ) &&
            ( pcb->state != MAXRTOS_PROCESS_STATE_SUSPENDED ) &&
            ( pcb->deadline_time != MAXRTOS_TICK_NONE ) &&
            ( now >= pcb->deadline_time ) &&
            ( pcb->partition_id < MAXRTOS_MAX_PARTITIONS ) &&
            ( table->halted[ pcb->partition_id ] == false ) )
        {
            pcb->deadline_time = MAXRTOS_TICK_NONE;
            pcb->deadline_misses++;
            s_pending_misses |= ( UINT32_C( 1 ) << id );
            misses++;
        }
    }

    return misses;
}

bool maxrtos_kernel_take_deadline_miss( maxrtos_process_id_t * out_id )
{
    bool taken = false;

    if( ( out_id != NULL ) && ( s_pending_misses != 0U ) )
    {
        maxrtos_process_id_t id = 0U;

        while( ( s_pending_misses & ( UINT32_C( 1 ) << id ) ) == 0U )
        {
            id++;
        }

        s_pending_misses &= ~( UINT32_C( 1 ) << id );
        *out_id = id;
        taken = true;
    }

    return taken;
}
