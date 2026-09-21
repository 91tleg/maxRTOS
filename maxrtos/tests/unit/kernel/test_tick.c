/**
 * @file test_tick.c
 * @brief Unit tests for the kernel tick-to-dispatch interface.
 *
 * Tests frame-based partition selection, per-partition dispatch,
 * error propagation, and major-frame boundary behavior.
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "maxrtos/kernel/frame.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/tick.h"

static uint8_t s_stacks[ MAXRTOS_MAX_PROCESSES ][ 64U ];
static size_t s_next_stack = 0U;

static void dummy_entry( void * arg )
{
    ( void ) arg;
}

static maxrtos_process_id_t make_ready_process(
    maxrtos_partition_id_t partition_id,
    uint8_t priority )
{
    maxrtos_process_id_t id;
    maxrtos_status_t status;

    status = maxrtos_process_create(
        s_stacks[ s_next_stack ],
        sizeof( s_stacks[ s_next_stack ] ),
        partition_id,
        priority,
        dummy_entry,
        NULL,
        &id );

    assert( status == MAXRTOS_OK );

    s_next_stack++;

    return id;
}

static void test_tick_now_initial_value( void )
{
    maxrtos_process_pool_init();
    maxrtos_kernel_tick_reset();

    assert( maxrtos_kernel_tick_now() == 0U );

    printf( "test_tick_now_initial_value: PASS\n" );
}

static void test_on_tick_rejects_bad_frame_schedule( void )
{
    maxrtos_partition_table_t table;
    maxrtos_frame_schedule_t bad_sched;
    maxrtos_process_id_t out;

    maxrtos_process_pool_init();
    maxrtos_kernel_tick_reset();

    assert(
        maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    s_next_stack = 0U;

    bad_sched.slot_count = 0U;
    bad_sched.major_frame_length_ticks = 0U;

    assert(
        maxrtos_kernel_on_tick(
            &bad_sched,
            &table,
            &out ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_on_tick_rejects_bad_frame_schedule: PASS\n" );
}

static void test_on_tick_updates_current_tick( void )
{
    maxrtos_partition_table_t table;
    maxrtos_frame_schedule_t sched;
    maxrtos_frame_slot_t slots[ 1 ] = { { 0U, 10U } };
    maxrtos_process_id_t process;
    maxrtos_process_id_t out;

    maxrtos_process_pool_init();
    maxrtos_kernel_tick_reset();

    assert(
        maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    assert(
        maxrtos_frame_init(
            &sched,
            slots,
            1U ) == MAXRTOS_OK );

    s_next_stack = 0U;

    process = make_ready_process( 0U, 0U );

    assert(
        maxrtos_partition_add_process(
            &table,
            process ) == MAXRTOS_OK );

    assert(
        maxrtos_kernel_on_tick(
            &sched,
            &table,
            &out ) == MAXRTOS_OK );

    assert( maxrtos_kernel_tick_now() == 1U );
    assert( out == process );

    assert(
        maxrtos_kernel_on_tick(
            &sched,
            &table,
            &out ) == MAXRTOS_OK );

    assert( maxrtos_kernel_tick_now() == 2U );
    assert( out == process );

    printf( "test_on_tick_updates_current_tick: PASS\n" );
}

static void test_on_tick_single_partition_resumes_correctly( void )
{
    maxrtos_partition_table_t table;
    maxrtos_frame_schedule_t sched;
    maxrtos_frame_slot_t slots[ 1 ] = { { 0U, 10U } };
    maxrtos_process_id_t process_a;
    maxrtos_process_id_t process_b;
    maxrtos_process_id_t out;

    maxrtos_process_pool_init();
    maxrtos_kernel_tick_reset();

    assert(
        maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    assert(
        maxrtos_frame_init(
            &sched,
            slots,
            1U ) == MAXRTOS_OK );

    s_next_stack = 0U;

    process_a = make_ready_process( 0U, 5U );
    process_b = make_ready_process( 0U, 2U );

    assert(
        maxrtos_partition_add_process(
            &table,
            process_a ) == MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process(
            &table,
            process_b ) == MAXRTOS_OK );

    assert(
        maxrtos_kernel_on_tick(
            &sched,
            &table,
            &out ) == MAXRTOS_OK );

    assert( out == process_b );
    assert( maxrtos_kernel_tick_now() == 1U );

    assert(
        maxrtos_kernel_on_tick(
            &sched,
            &table,
            &out ) == MAXRTOS_OK );

    assert( out == process_a );
    assert( maxrtos_kernel_tick_now() == 2U );

    printf(
        "test_on_tick_single_partition_resumes_correctly: PASS\n" );
}

static void test_on_tick_switches_partitions_at_frame_boundary( void )
{
    maxrtos_partition_table_t table;
    maxrtos_frame_schedule_t sched;
    maxrtos_frame_slot_t slots[ 2 ] =
    {
        { 0U, 5U },
        { 1U, 3U }
    };
    maxrtos_process_id_t process_0;
    maxrtos_process_id_t process_1;
    maxrtos_process_id_t out;
    size_t i;

    maxrtos_process_pool_init();
    maxrtos_kernel_tick_reset();

    assert(
        maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    assert(
        maxrtos_frame_init(
            &sched,
            slots,
            2U ) == MAXRTOS_OK );

    s_next_stack = 0U;

    process_0 = make_ready_process( 0U, 5U );
    process_1 = make_ready_process( 1U, 5U );

    assert(
        maxrtos_partition_add_process(
            &table,
            process_0 ) == MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process(
            &table,
            process_1 ) == MAXRTOS_OK );

    /*
     * Advance through ticks 0..4 (five ticks). These ticks belong
     * to partition 0's [0, 5) frame slot.
     */
    for( i = 0U; i < 5U; i++ )
    {
        assert(
            maxrtos_kernel_on_tick(
                &sched,
                &table,
                &out ) == MAXRTOS_OK );

        assert( out == process_0 );
    }

    assert( maxrtos_kernel_tick_now() == 5U );

    /*
     * Tick 5 is the first tick of partition 1's [5, 8) slot.
     */
    assert(
        maxrtos_kernel_on_tick(
            &sched,
            &table,
            &out ) == MAXRTOS_OK );

    assert( maxrtos_kernel_tick_now() == 6U );
    assert( out == process_1 );

    /*
     * Advance through ticks 6 and 7.
     */
    assert(
        maxrtos_kernel_on_tick(
            &sched,
            &table,
            &out ) == MAXRTOS_OK );

    assert( maxrtos_kernel_tick_now() == 7U );
    assert( out == process_1 );

    assert(
        maxrtos_kernel_on_tick(
            &sched,
            &table,
            &out ) == MAXRTOS_OK );

    assert( maxrtos_kernel_tick_now() == 8U );
    assert( out == process_1 );

    /*
     * Tick 8 wraps to partition 0.
     */
    assert(
        maxrtos_kernel_on_tick(
            &sched,
            &table,
            &out ) == MAXRTOS_OK );

    assert( maxrtos_kernel_tick_now() == 9U );
    assert( out == process_0 );

    printf(
        "test_on_tick_switches_partitions_at_frame_boundary: PASS\n" );
}

static void test_on_tick_propagates_queue_empty( void )
{
    maxrtos_partition_table_t table;
    maxrtos_frame_schedule_t sched;
    maxrtos_frame_slot_t slots[ 1 ] = { { 0U, 10U } };
    maxrtos_process_id_t out;

    maxrtos_process_pool_init();
    maxrtos_kernel_tick_reset();

    assert(
        maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    assert(
        maxrtos_frame_init(
            &sched,
            slots,
            1U ) == MAXRTOS_OK );

    s_next_stack = 0U;

    assert(
        maxrtos_kernel_on_tick(
            &sched,
            &table,
            &out ) == MAXRTOS_ERR_QUEUE_EMPTY );

    assert( maxrtos_kernel_tick_now() == 1U );

    printf( "test_on_tick_propagates_queue_empty: PASS\n" );
}

int main( void )
{
    test_tick_now_initial_value();
    test_on_tick_rejects_bad_frame_schedule();
    test_on_tick_updates_current_tick();
    test_on_tick_single_partition_resumes_correctly();
    test_on_tick_switches_partitions_at_frame_boundary();
    test_on_tick_propagates_queue_empty();

    printf( "all tick tests passed\n" );

    return 0;
}
