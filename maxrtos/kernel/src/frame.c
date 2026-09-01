/**
 * @file frame.c
 * @brief Implementation of the static major-frame scheduling interface.
 *
 * Implements validation, initialization, and partition lookup for the
 * major-frame schedule defined in frame.h.
 *
 * The implementation does not maintain system time or runtime
 * scheduling state. The caller supplies the authoritative system tick
 * to the partition lookup function.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "maxrtos/kernel/frame.h"

maxrtos_status_t maxrtos_frame_init(
    maxrtos_frame_schedule_t * sched,
    maxrtos_frame_slot_t const * slots,
    size_t slot_count )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( sched != NULL ) &&
        ( slots != NULL ) &&
        ( slot_count > 0U ) &&
        ( slot_count <= MAXRTOS_MAX_FRAME_SLOTS ) )
    {
        size_t i;
        bool valid_slot;
        uint32_t major_frame_length_ticks;

        valid_slot = true;
        major_frame_length_ticks = 0U;

        for( i = 0U;
             ( i < slot_count ) && ( valid_slot == true );
             i++ )
        {
            if( ( slots[ i ].partition_id >= MAXRTOS_MAX_PARTITIONS ) ||
                ( slots[ i ].duration_ticks == 0U ) )
            {
                valid_slot = false;
                status = MAXRTOS_ERR_INVALID_ARG;
            }
            else if( major_frame_length_ticks >
                     ( UINT32_MAX - slots[ i ].duration_ticks ) )
            {
                valid_slot = false;
                status = MAXRTOS_ERR_OVERFLOW;
            }
            else
            {
                major_frame_length_ticks += slots[ i ].duration_ticks;
            }
        }

        if( valid_slot == true )
        {
            for( i = 0U; i < slot_count; i++ )
            {
                sched->slots[ i ] = slots[ i ];
            }

            sched->slot_count = slot_count;
            sched->major_frame_length_ticks = major_frame_length_ticks;

            status = MAXRTOS_OK;
        }
    }

    return status;
}

maxrtos_status_t maxrtos_frame_partition_at_tick(
    maxrtos_frame_schedule_t const * sched,
    uint32_t tick,
    maxrtos_partition_id_t * out_partition_id )
{
    maxrtos_status_t status;
    uint32_t start;

    status = MAXRTOS_ERR_INVALID_ARG;
    start = 0U;

    if( ( sched != NULL ) &&
        ( sched->major_frame_length_ticks > 0U ) &&
        ( sched->slot_count > 0U ) &&
        ( out_partition_id != NULL ) )
    {
        uint32_t reduced_tick;
        size_t i;

        reduced_tick = tick % sched->major_frame_length_ticks;

        for( i = 0U;
             ( i < sched->slot_count ) && ( status != MAXRTOS_OK );
             i++ )
        {
            if( ( reduced_tick >= start ) &&
                ( reduced_tick < ( start + sched->slots[ i ].duration_ticks ) ) )
            {
                status = MAXRTOS_OK;
                *out_partition_id = sched->slots[ i ].partition_id;
            }
            else
            {
                start += sched->slots[i].duration_ticks;
            }
        }
    }

    return status;
}
