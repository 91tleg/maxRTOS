/**
 * @file test_yield.c
 * @brief Unit tests for the kernel-level voluntary yield decision.
 */

#include <assert.h>
#include <stdio.h>
#include <stddef.h>

#include "maxrtos/kernel/yield.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"

#define TEST_STACK_SIZE ( 128U )

static uint8_t s_stack_a[ TEST_STACK_SIZE ];
static uint8_t s_stack_b[ TEST_STACK_SIZE ];

static void dummy_entry( void * arg )
{
    ( void ) arg;
}

static void test_rejects_null_table( void )
{
    maxrtos_process_id_t next_id;
    bool switch_needed;

    assert(
        maxrtos_kernel_yield(
            NULL,
            0U,
            0U,
            &next_id,
            &switch_needed ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_rejects_null_table: PASS\n" );
}

static void test_rejects_null_out_params( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t next_id;
    bool switch_needed;

    assert( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    assert(
        maxrtos_kernel_yield(
            &table,
            0U,
            0U,
            NULL,
            &switch_needed ) == MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_kernel_yield(
            &table,
            0U,
            0U,
            &next_id,
            NULL ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_rejects_null_out_params: PASS\n" );
}

static void test_propagates_dispatch_failure( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t next_id;
    bool switch_needed;

    assert( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    /* No processes registered with this partition at all, so
     * dispatch is expected to fail. */
    assert(
        maxrtos_kernel_yield(
            &table,
            0U,
            0U,
            &next_id,
            &switch_needed ) != MAXRTOS_OK );

    printf( "test_propagates_dispatch_failure: PASS\n" );
}

static void test_yield_with_sole_ready_process_reports_no_switch(
    void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t id_a;
    maxrtos_process_control_block_t * pcb_a;
    maxrtos_process_id_t next_id;
    bool switch_needed;

    maxrtos_process_pool_init();
    assert( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    assert(
        maxrtos_process_create(
            s_stack_a,
            TEST_STACK_SIZE,
            (maxrtos_partition_id_t) 0U,
            5U,
            dummy_entry,
            NULL,
            &id_a ) == MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process(
            &table,
            id_a ) == MAXRTOS_OK );

    pcb_a = maxrtos_process_get( id_a );
    assert( pcb_a != NULL );

    assert(
        maxrtos_kernel_yield(
            &table,
            0U,
            id_a,
            &next_id,
            &switch_needed ) == MAXRTOS_OK );

    /* Only one READY process exists in the partition, so yielding
     * must select the calling process itself. */
    assert( next_id == id_a );
    assert( switch_needed == false );

    printf(
        "test_yield_with_sole_ready_process_reports_no_switch: PASS\n" );
}

static void test_switch_needed_matches_next_id_vs_current_id( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t id_a;
    maxrtos_process_id_t next_id;
    bool switch_needed;

    maxrtos_process_pool_init();
    assert( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    assert(
        maxrtos_process_create(
            s_stack_a,
            TEST_STACK_SIZE,
            (maxrtos_partition_id_t) 0U,
            5U,
            dummy_entry,
            NULL,
            &id_a ) == MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process(
            &table,
            id_a ) == MAXRTOS_OK );

    assert(
        maxrtos_kernel_yield(
            &table,
            0U,
            id_a,
            &next_id,
            &switch_needed ) == MAXRTOS_OK );

    assert( switch_needed == ( next_id != id_a ) );

    printf(
        "test_switch_needed_matches_next_id_vs_current_id: PASS\n" );
}

static maxrtos_process_id_t create_in_partition(
    uint8_t * stack,
    maxrtos_partition_id_t partition,
    uint8_t priority )
{
    maxrtos_process_id_t id;

    assert(
        maxrtos_process_create(
            stack,
            TEST_STACK_SIZE,
            partition,
            priority,
            dummy_entry,
            NULL,
            &id ) == MAXRTOS_OK );

    return id;
}

/* Two equal-priority processes in one partition: each yield hands the
 * CPU to the other, and the yielder is queued behind it. */
static void test_yield_alternates_between_equal_priority_processes( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t running;
    maxrtos_process_id_t next_id;
    bool switch_needed;
    int i;

    maxrtos_process_pool_init();
    assert( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    a = create_in_partition( s_stack_a, 0U, 5U );
    b = create_in_partition( s_stack_b, 0U, 5U );
    assert( maxrtos_partition_add_process( &table, a ) == MAXRTOS_OK );
    assert( maxrtos_partition_add_process( &table, b ) == MAXRTOS_OK );

    /* Initial dispatch (what the frame tick does): a runs first. */
    assert( maxrtos_partition_dispatch( &table, 0U, &running ) == MAXRTOS_OK );
    assert( running == a );

    for( i = 0; i < 6; i++ )
    {
        maxrtos_process_id_t expected;

        expected = ( running == a ) ? b : a;

        assert(
            maxrtos_kernel_yield(
                &table, 0U, running, &next_id, &switch_needed ) ==
            MAXRTOS_OK );

        assert( next_id == expected );
        assert( switch_needed == true );
        assert( table.current_id[ 0 ] == expected );
        assert( maxrtos_process_get( expected )->state ==
                MAXRTOS_PROCESS_STATE_RUNNING );
        assert( maxrtos_process_get( running )->state ==
                MAXRTOS_PROCESS_STATE_READY );

        running = next_id;
    }

    printf( "test_yield_alternates_between_equal_priority_processes: PASS\n" );
}

/* Yield in one partition never touches another partition's state. */
static void test_yield_is_confined_to_its_partition( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t a0;
    maxrtos_process_id_t a1;
    maxrtos_process_id_t b0;
    maxrtos_process_id_t running_b;
    maxrtos_process_id_t running_a;
    maxrtos_process_id_t next_id;
    bool switch_needed;

    static uint8_t s_stack_c[ TEST_STACK_SIZE ];

    maxrtos_process_pool_init();
    assert( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    a0 = create_in_partition( s_stack_a, 0U, 5U );
    a1 = create_in_partition( s_stack_b, 0U, 5U );
    b0 = create_in_partition( s_stack_c, 1U, 5U );
    assert( maxrtos_partition_add_process( &table, a0 ) == MAXRTOS_OK );
    assert( maxrtos_partition_add_process( &table, a1 ) == MAXRTOS_OK );
    assert( maxrtos_partition_add_process( &table, b0 ) == MAXRTOS_OK );

    assert( maxrtos_partition_dispatch( &table, 0U, &running_a ) == MAXRTOS_OK );
    assert( maxrtos_partition_dispatch( &table, 1U, &running_b ) == MAXRTOS_OK );
    assert( running_a == a0 );
    assert( running_b == b0 );

    assert(
        maxrtos_kernel_yield(
            &table, 0U, a0, &next_id, &switch_needed ) == MAXRTOS_OK );
    assert( next_id == a1 );
    assert( table.current_id[ 1 ] == b0 );
    assert( maxrtos_process_get( b0 )->state == MAXRTOS_PROCESS_STATE_RUNNING );

    printf( "test_yield_is_confined_to_its_partition: PASS\n" );
}

/* Regression: after a process blocks and hands RUNNING to another one
 * (as blocking IPC does), the partition's current process must be the
 * new RUNNING process, or every later yield is rejected. */
static void test_yield_after_block_and_dispatch( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t c;
    maxrtos_process_id_t running;
    maxrtos_process_id_t next_id;
    bool switch_needed;

    static uint8_t s_stack_c[ TEST_STACK_SIZE ];

    maxrtos_process_pool_init();
    assert( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    a = create_in_partition( s_stack_a, 0U, 5U );
    b = create_in_partition( s_stack_b, 0U, 5U );
    c = create_in_partition( s_stack_c, 0U, 5U );
    assert( maxrtos_partition_add_process( &table, a ) == MAXRTOS_OK );
    assert( maxrtos_partition_add_process( &table, b ) == MAXRTOS_OK );
    assert( maxrtos_partition_add_process( &table, c ) == MAXRTOS_OK );
    assert( maxrtos_partition_dispatch( &table, 0U, &running ) == MAXRTOS_OK );
    assert( running == a );

    /* a blocks; b takes over and is recorded as current. */
    assert(
        maxrtos_partition_block_and_dispatch(
            &table, 0U, a, &next_id ) == MAXRTOS_OK );
    assert( next_id == b );
    assert( table.current_id[ 0 ] == b );

    /* b yields to c, c yields back to b: a stays out (BLOCKED). */
    assert(
        maxrtos_kernel_yield(
            &table, 0U, b, &next_id, &switch_needed ) == MAXRTOS_OK );
    assert( next_id == c );
    assert( switch_needed == true );

    assert(
        maxrtos_kernel_yield(
            &table, 0U, c, &next_id, &switch_needed ) == MAXRTOS_OK );
    assert( next_id == b );
    assert( maxrtos_process_get( a )->state == MAXRTOS_PROCESS_STATE_BLOCKED );

    printf( "test_yield_after_block_and_dispatch: PASS\n" );
}

int main( void )
{
    test_rejects_null_table();
    test_rejects_null_out_params();
    test_propagates_dispatch_failure();
    test_yield_with_sole_ready_process_reports_no_switch();
    test_switch_needed_matches_next_id_vs_current_id();
    test_yield_alternates_between_equal_priority_processes();
    test_yield_is_confined_to_its_partition();
    test_yield_after_block_and_dispatch();

    printf( "all yield tests passed\n" );

    return 0;
}
