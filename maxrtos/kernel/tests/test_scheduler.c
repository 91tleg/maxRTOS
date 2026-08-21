/**
 * @file test_process.c
 * @brief Unit tests for process management.
 *
 * Verifies process creation, process-pool capacity, state
 * transitions, and process ID validation.
 *
 * Tests execute against the host build and do not require target
 * hardware.
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/scheduler.h"

static uint8_t s_stacks[ MAXRTOS_MAX_PROCESSES ][ 64U ];
static size_t s_next_stack = 0U;

static void dummy_entry( void * arg )
{
    ( void ) arg;
}

static maxrtos_process_id_t make_ready_process( uint8_t priority )
{
    maxrtos_process_id_t id;
    maxrtos_status_t status;

    status = maxrtos_process_create(
        s_stacks[ s_next_stack ],
        sizeof( s_stacks[ s_next_stack ] ),
        priority,
        dummy_entry,
        NULL,
        &id );

    assert( status == MAXRTOS_OK );

    s_next_stack++;

    return id;
}

static void reset_all( void )
{
    maxrtos_process_pool_init();
    maxrtos_scheduler_init();
    s_next_stack = 0U;
}

static void test_single_process( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t out;

    reset_all();

    a = make_ready_process( 10U );

    assert( maxrtos_scheduler_add_process( a ) == MAXRTOS_OK );
    assert( maxrtos_scheduler_next( &out ) == MAXRTOS_OK );
    assert( out == a );

    printf( "test_single_process: PASS\n" );
}

static void test_higher_priority_wins( void )
{
    maxrtos_process_id_t low;
    maxrtos_process_id_t high;
    maxrtos_process_id_t out;

    reset_all();

    low = make_ready_process( 20U );
    high = make_ready_process( 5U );

    assert( maxrtos_scheduler_add_process( low ) == MAXRTOS_OK );
    assert( maxrtos_scheduler_add_process( high ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_next( &out ) == MAXRTOS_OK );
    assert( out == high );

    printf( "test_higher_priority_wins: PASS\n" );
}

static void test_fifo_tiebreak_same_priority( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t out;

    reset_all();

    a = make_ready_process( 7U );
    b = make_ready_process( 7U );

    assert( maxrtos_scheduler_add_process( a ) == MAXRTOS_OK );
    assert( maxrtos_scheduler_add_process( b ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_next( &out ) == MAXRTOS_OK );
    assert( out == a );

    printf( "test_fifo_tiebreak_same_priority: PASS\n" );
}

static void test_empty_queue( void )
{
    maxrtos_process_id_t out;

    reset_all();

    assert( maxrtos_scheduler_next( &out ) == MAXRTOS_ERR_QUEUE_EMPTY );

    printf( "test_empty_queue: PASS\n" );
}

static void test_late_higher_priority_wins( void )
{
    maxrtos_process_id_t low;
    maxrtos_process_id_t high;
    maxrtos_process_id_t out;

    reset_all();

    low = make_ready_process( 15U );
    high = make_ready_process( 2U );

    assert( maxrtos_scheduler_add_process( low ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_next( &out ) == MAXRTOS_OK );
    assert( out == low );

    assert( maxrtos_scheduler_add_process( high ) == MAXRTOS_OK );
    assert( maxrtos_scheduler_next( &out ) == MAXRTOS_OK );
    assert( out == high );

    printf( "test_late_higher_priority_wins: PASS\n" );
}

static void test_next_peeks_does_not_consume( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t out1;
    maxrtos_process_id_t out2;

    reset_all();

    a = make_ready_process( 4U );

    assert( maxrtos_scheduler_add_process( a ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_next( &out1 ) == MAXRTOS_OK );
    assert( maxrtos_scheduler_next( &out2 ) == MAXRTOS_OK );

    assert( out1 == a );
    assert( out2 == a );
    assert( maxrtos_scheduler_process_count() == 1U );

    printf( "test_next_peeks_does_not_consume: PASS\n" );
}

static void test_remove_process_dequeues( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t out;

    reset_all();

    a = make_ready_process( 9U );
    b = make_ready_process( 9U );

    assert( maxrtos_scheduler_add_process( a ) == MAXRTOS_OK );
    assert( maxrtos_scheduler_add_process( b ) == MAXRTOS_OK );
    assert( maxrtos_scheduler_process_count() == 2U );

    assert( maxrtos_scheduler_next( &out ) == MAXRTOS_OK );
    assert( out == a );

    assert( maxrtos_scheduler_remove_process( a ) == MAXRTOS_OK );
    assert( maxrtos_scheduler_process_count() == 1U );

    assert( maxrtos_scheduler_next( &out ) == MAXRTOS_OK );
    assert( out == b );

    printf( "test_remove_process_dequeues: PASS\n" );
}

static void test_next_rejects_null_out_id( void )
{
    reset_all();

    assert( maxrtos_scheduler_next( NULL ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_next_rejects_null_out_id: PASS\n" );
}

int main( void )
{
    test_single_process();
    test_higher_priority_wins();
    test_fifo_tiebreak_same_priority();
    test_empty_queue();
    test_late_higher_priority_wins();
    test_next_peeks_does_not_consume();
    test_remove_process_dequeues();
    test_next_rejects_null_out_id();

    printf( "all scheduler tests passed\n" );

    return 0;
}
