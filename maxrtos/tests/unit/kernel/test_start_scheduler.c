/**
 * @file test_start_scheduler.c
 * @brief Unit tests for scheduler startup.
 *
 * Verifies scheduler startup argument validation, initial frame-based
 * partition selection, first-process dispatch, and propagation of
 * scheduler errors when no process is available.
 */

#include <assert.h>
#include <stdio.h>

#include "maxrtos/kernel/frame.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/start_scheduler.h"

static uint8_t s_stack[ 256U ];

static void dummy_entry( void * arg )
{
    ( void ) arg;
}

static void test_start_rejects_bad_args( void )
{
    maxrtos_frame_schedule_t frame_schedule;
    maxrtos_partition_table_t partition_table;
    maxrtos_process_control_block_t * first_pcb;

    /* Initialize the objects so they are valid non-NULL arguments. */
    assert( maxrtos_frame_init(
                &frame_schedule,
                &( maxrtos_frame_slot_t ){
                    .partition_id = 0U,
                    .duration_ticks = 10U
                },
                1U ) == MAXRTOS_OK );

    assert( maxrtos_partition_table_init(
                &partition_table ) == MAXRTOS_OK );

    first_pcb = NULL;

    /* A NULL frame schedule shall be rejected. */
    assert( maxrtos_kernel_start_scheduler(
                NULL,
                &partition_table,
                &first_pcb ) == MAXRTOS_ERR_INVALID_ARG );

    /* A NULL partition table shall be rejected. */
    assert( maxrtos_kernel_start_scheduler(
                &frame_schedule,
                NULL,
                &first_pcb ) == MAXRTOS_ERR_INVALID_ARG );

    /* A NULL output pointer shall be rejected. */
    assert( maxrtos_kernel_start_scheduler(
                &frame_schedule,
                &partition_table,
                NULL ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_start_rejects_bad_args: PASS\n" );
}

static void test_start_dispatches_first_process( void )
{
    maxrtos_frame_schedule_t frame_schedule;
    maxrtos_partition_table_t partition_table;
    maxrtos_frame_slot_t slot;
    maxrtos_process_id_t process_id;
    maxrtos_process_control_block_t * first_pcb;
    maxrtos_status_t status;

    slot.partition_id = 0U;
    slot.duration_ticks = 10U;

    maxrtos_process_pool_init();

    assert( maxrtos_frame_init(
                &frame_schedule,
                &slot,
                1U ) == MAXRTOS_OK );

    assert( maxrtos_partition_table_init(
                &partition_table ) == MAXRTOS_OK );

    status = maxrtos_process_create(
        s_stack,
        sizeof( s_stack ),
        0U,
        0U,
        dummy_entry,
        NULL,
        &process_id );

    assert( status == MAXRTOS_OK );

    assert( maxrtos_partition_add_process(
                &partition_table,
                process_id ) == MAXRTOS_OK );

    first_pcb = NULL;

    status = maxrtos_kernel_start_scheduler(
        &frame_schedule,
        &partition_table,
        &first_pcb );

    assert( status == MAXRTOS_OK );
    assert( first_pcb != NULL );
    assert( first_pcb->id == process_id );
    assert( first_pcb->partition_id == 0U );
    assert( first_pcb->state == MAXRTOS_PROCESS_STATE_RUNNING );
    assert( partition_table.current_id[ 0U ] == process_id );

    printf( "test_start_dispatches_first_process: PASS\n" );
}

static void test_start_propagates_dispatch_error( void )
{
    maxrtos_frame_schedule_t frame_schedule;
    maxrtos_partition_table_t partition_table;
    maxrtos_frame_slot_t slot;
    maxrtos_process_control_block_t * first_pcb;
    maxrtos_status_t status;

    slot.partition_id = 0U;
    slot.duration_ticks = 10U;

    maxrtos_process_pool_init();

    assert( maxrtos_frame_init(
                &frame_schedule,
                &slot,
                1U ) == MAXRTOS_OK );

    assert( maxrtos_partition_table_init(
                &partition_table ) == MAXRTOS_OK );

    first_pcb = NULL;

    /* No process is added to partition 0. The dispatch operation
     * shall therefore report an empty scheduler queue. */
    status = maxrtos_kernel_start_scheduler(
        &frame_schedule,
        &partition_table,
        &first_pcb );

    assert( status == MAXRTOS_ERR_QUEUE_EMPTY );
    assert( first_pcb == NULL );
    assert( partition_table.current_id[ 0U ] ==
            MAXRTOS_INVALID_PROCESS_ID );

    printf( "test_start_propagates_dispatch_error: PASS\n" );
}

static void test_start_selects_partition_at_tick_zero( void )
{
    maxrtos_frame_schedule_t frame_schedule;
    maxrtos_partition_table_t partition_table;
    maxrtos_frame_slot_t slots[ 2U ];
    maxrtos_process_id_t process_a;
    maxrtos_process_id_t process_b;
    maxrtos_process_control_block_t * first_pcb;
    maxrtos_status_t status;

    slots[ 0U ].partition_id = 1U;
    slots[ 0U ].duration_ticks = 5U;

    slots[ 1U ].partition_id = 0U;
    slots[ 1U ].duration_ticks = 5U;

    maxrtos_process_pool_init();

    assert( maxrtos_frame_init(
                &frame_schedule,
                slots,
                2U ) == MAXRTOS_OK );

    assert( maxrtos_partition_table_init(
                &partition_table ) == MAXRTOS_OK );

    /* Tick zero belongs to the first frame slot, which assigns
     * execution to partition 1. */
    assert( maxrtos_process_create(
                s_stack,
                sizeof( s_stack ),
                1U,
                0U,
                dummy_entry,
                NULL,
                &process_a ) == MAXRTOS_OK );

    assert( maxrtos_process_create(
                s_stack,
                sizeof( s_stack ),
                0U,
                0U,
                dummy_entry,
                NULL,
                &process_b ) == MAXRTOS_OK );

    assert( maxrtos_partition_add_process(
                &partition_table,
                process_a ) == MAXRTOS_OK );

    assert( maxrtos_partition_add_process(
                &partition_table,
                process_b ) == MAXRTOS_OK );

    first_pcb = NULL;

    status = maxrtos_kernel_start_scheduler(
        &frame_schedule,
        &partition_table,
        &first_pcb );

    assert( status == MAXRTOS_OK );
    assert( first_pcb != NULL );
    assert( first_pcb->id == process_a );
    assert( first_pcb->partition_id == 1U );

    assert( partition_table.current_id[ 1U ] == process_a );
    assert( partition_table.current_id[ 0U ] ==
            MAXRTOS_INVALID_PROCESS_ID );

    printf( "test_start_selects_partition_at_tick_zero: PASS\n" );
}

int main( void )
{
    test_start_rejects_bad_args();
    test_start_dispatches_first_process();
    test_start_propagates_dispatch_error();
    test_start_selects_partition_at_tick_zero();

    printf( "all start scheduler tests passed\n" );

    return 0;
}
