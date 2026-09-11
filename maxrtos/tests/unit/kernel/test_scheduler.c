/**
 * @file test_scheduler.c
 * @brief Unit tests for fixed-priority process scheduling.
 *
 * Verifies scheduler initialization, priority selection, FIFO ordering,
 * queue operations, argument validation, queue capacity, and
 * independence of scheduler contexts.
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
static maxrtos_scheduler_context_t s_ctx;

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
        true,
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

    assert( maxrtos_scheduler_init( &s_ctx ) == MAXRTOS_OK );

    s_next_stack = 0U;
}

static void test_scheduler_init_rejects_null( void )
{
    assert( maxrtos_scheduler_init( NULL )
            == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_scheduler_init_rejects_null: PASS\n" );
}

static void test_single_process( void )
{
    maxrtos_process_id_t id;
    maxrtos_process_id_t out;

    reset_all();

    id = make_ready_process( 0U, 10U );

    assert( maxrtos_scheduler_add_process(
                &s_ctx,
                id ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_next(
                &s_ctx,
                &out ) == MAXRTOS_OK );

    assert( out == id );

    printf( "test_single_process: PASS\n" );
}

static void test_higher_priority_wins( void )
{
    maxrtos_process_id_t low;
    maxrtos_process_id_t high;
    maxrtos_process_id_t out;

    reset_all();

    low = make_ready_process( 0U, 20U );
    high = make_ready_process( 0U, 5U );

    assert( maxrtos_scheduler_add_process(
                &s_ctx,
                low ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_add_process(
                &s_ctx,
                high ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_next(
                &s_ctx,
                &out ) == MAXRTOS_OK );

    assert( out == high );

    printf( "test_higher_priority_wins: PASS\n" );
}

static void test_fifo_tiebreak_same_priority( void )
{
    maxrtos_process_id_t first;
    maxrtos_process_id_t second;
    maxrtos_process_id_t out;

    reset_all();

    first = make_ready_process( 0U, 7U );
    second = make_ready_process( 0U, 7U );

    assert( maxrtos_scheduler_add_process(
                &s_ctx,
                first ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_add_process(
                &s_ctx,
                second ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_next(
                &s_ctx,
                &out ) == MAXRTOS_OK );

    assert( out == first );

    printf( "test_fifo_tiebreak_same_priority: PASS\n" );
}

static void test_empty_queue( void )
{
    maxrtos_process_id_t out;

    reset_all();

    assert( maxrtos_scheduler_next(
                &s_ctx,
                &out ) == MAXRTOS_ERR_QUEUE_EMPTY );

    printf( "test_empty_queue: PASS\n" );
}

static void test_late_higher_priority_preempts_decision( void )
{
    maxrtos_process_id_t low;
    maxrtos_process_id_t high;
    maxrtos_process_id_t out;

    reset_all();

    low = make_ready_process( 0U, 15U );
    high = make_ready_process( 0U, 2U );

    assert( maxrtos_scheduler_add_process(
                &s_ctx,
                low ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_next(
                &s_ctx,
                &out ) == MAXRTOS_OK );

    assert( out == low );

    assert( maxrtos_scheduler_add_process(
                &s_ctx,
                high ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_next(
                &s_ctx,
                &out ) == MAXRTOS_OK );

    assert( out == high );

    printf( "test_late_higher_priority_preempts_decision: PASS\n" );
}

static void test_next_peeks_does_not_consume( void )
{
    maxrtos_process_id_t id;
    maxrtos_process_id_t out_first;
    maxrtos_process_id_t out_second;

    reset_all();

    id = make_ready_process( 0U, 4U );

    assert( maxrtos_scheduler_add_process(
                &s_ctx,
                id ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_next(
                &s_ctx,
                &out_first ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_next(
                &s_ctx,
                &out_second ) == MAXRTOS_OK );

    assert( out_first == id );
    assert( out_second == id );

    assert( maxrtos_scheduler_process_count( &s_ctx ) == 1U );

    printf( "test_next_peeks_does_not_consume: PASS\n" );
}

static void test_remove_process_dequeues( void )
{
    maxrtos_process_id_t first;
    maxrtos_process_id_t second;
    maxrtos_process_id_t out;

    reset_all();

    first = make_ready_process( 0U, 9U );
    second = make_ready_process( 0U, 9U );

    assert( maxrtos_scheduler_add_process(
                &s_ctx,
                first ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_add_process(
                &s_ctx,
                second ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_process_count( &s_ctx ) == 2U );

    assert( maxrtos_scheduler_remove_process(
                &s_ctx,
                first ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_process_count( &s_ctx ) == 1U );

    assert( maxrtos_scheduler_next(
                &s_ctx,
                &out ) == MAXRTOS_OK );

    assert( out == second );

    printf( "test_remove_process_dequeues: PASS\n" );
}

static void test_next_rejects_null_args( void )
{
    maxrtos_process_id_t out;

    reset_all();

    assert( maxrtos_scheduler_next(
                &s_ctx,
                NULL ) == MAXRTOS_ERR_INVALID_ARG );

    assert( maxrtos_scheduler_next(
                NULL,
                &out ) == MAXRTOS_ERR_INVALID_ARG );

    assert( maxrtos_scheduler_next(
                NULL,
                NULL ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_next_rejects_null_args: PASS\n" );
}

static void test_process_count_null_ctx_is_zero( void )
{
    assert( maxrtos_scheduler_process_count( NULL ) == 0U );

    printf( "test_process_count_null_ctx_is_zero: PASS\n" );
}

static void test_add_rejects_non_ready_process( void )
{
    maxrtos_process_id_t id;

    reset_all();

    id = make_ready_process( 0U, 5U );

    assert( maxrtos_process_set_state(
                id,
                MAXRTOS_PROCESS_STATE_RUNNING ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_add_process(
                &s_ctx,
                id ) == MAXRTOS_ERR_INVALID_STATE );

    assert( maxrtos_scheduler_process_count( &s_ctx ) == 0U );

    printf( "test_add_rejects_non_ready_process: PASS\n" );
}

static void test_remove_rejects_non_ready_process( void )
{
    maxrtos_process_id_t id;

    reset_all();

    id = make_ready_process( 0U, 5U );

    assert( maxrtos_scheduler_add_process(
                &s_ctx,
                id ) == MAXRTOS_OK );

    assert( maxrtos_process_set_state(
                id,
                MAXRTOS_PROCESS_STATE_RUNNING ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_remove_process(
                &s_ctx,
                id ) == MAXRTOS_ERR_INVALID_STATE );

    printf( "test_remove_rejects_non_ready_process: PASS\n" );
}

static void test_invalid_process_id( void )
{
    reset_all();

    assert( maxrtos_scheduler_add_process(
                &s_ctx,
                MAXRTOS_INVALID_PROCESS_ID ) == MAXRTOS_ERR_INVALID_ID );

    assert( maxrtos_scheduler_remove_process(
                &s_ctx,
                MAXRTOS_INVALID_PROCESS_ID ) == MAXRTOS_ERR_INVALID_ID );

    printf( "test_invalid_process_id: PASS\n" );
}

static void test_queue_full( void )
{
    maxrtos_process_id_t ids[ MAXRTOS_MAX_READY_PER_PRIORITY ];
    maxrtos_process_id_t extra;
    size_t i;

    reset_all();

    for( i = 0U;
         i < MAXRTOS_MAX_READY_PER_PRIORITY;
         i++ )
    {
        ids[ i ] = make_ready_process( 0U, 5U );

        assert( maxrtos_scheduler_add_process(
                    &s_ctx,
                    ids[ i ] ) == MAXRTOS_OK );
    }

    extra = make_ready_process( 0U, 5U );

    assert( maxrtos_scheduler_add_process(
                &s_ctx,
                extra ) == MAXRTOS_ERR_QUEUE_FULL );

    assert( maxrtos_scheduler_process_count( &s_ctx )
            == MAXRTOS_MAX_READY_PER_PRIORITY );

    printf( "test_queue_full: PASS\n" );
}

static void test_scheduler_contexts_are_independent( void )
{
    maxrtos_scheduler_context_t ctx_a;
    maxrtos_scheduler_context_t ctx_b;
    maxrtos_process_id_t process_a;
    maxrtos_process_id_t process_b;
    maxrtos_process_id_t out;

    maxrtos_process_pool_init();

    assert( maxrtos_scheduler_init( &ctx_a ) == MAXRTOS_OK );
    assert( maxrtos_scheduler_init( &ctx_b ) == MAXRTOS_OK );

    s_next_stack = 0U;

    process_a = make_ready_process( 0U, 1U );
    process_b = make_ready_process( 1U, 1U );

    assert( maxrtos_scheduler_add_process(
                &ctx_a,
                process_a ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_add_process(
                &ctx_b,
                process_b ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_process_count( &ctx_a ) == 1U );
    assert( maxrtos_scheduler_process_count( &ctx_b ) == 1U );

    assert( maxrtos_scheduler_next(
                &ctx_a,
                &out ) == MAXRTOS_OK );

    assert( out == process_a );

    assert( maxrtos_scheduler_next(
                &ctx_b,
                &out ) == MAXRTOS_OK );

    assert( out == process_b );

    printf( "test_scheduler_contexts_are_independent: PASS\n" );
}

int main( void )
{
    test_scheduler_init_rejects_null();
    test_single_process();
    test_higher_priority_wins();
    test_fifo_tiebreak_same_priority();
    test_empty_queue();
    test_late_higher_priority_preempts_decision();
    test_next_peeks_does_not_consume();
    test_remove_process_dequeues();
    test_next_rejects_null_args();
    test_process_count_null_ctx_is_zero();
    test_add_rejects_non_ready_process();
    test_remove_rejects_non_ready_process();
    test_invalid_process_id();
    test_queue_full();
    test_scheduler_contexts_are_independent();

    printf( "all scheduler tests passed\n" );

    return 0;
}
