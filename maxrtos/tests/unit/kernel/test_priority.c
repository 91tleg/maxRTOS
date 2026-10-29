/**
 * @file test_priority.c
 * @brief Unit tests for process scheduling inside a partition.
 *
 * Priority-preemptive: the running process keeps the CPU until it blocks,
 * waits, or a READY process of strictly higher priority exists. Equal
 * priorities are first-in first-out and are not time sliced; a process gives
 * way to its peers only by yielding (TIMED_WAIT with a zero delay), and goes
 * behind them. A preempted process stays the oldest at its priority.
 */

#include <assert.h>
#include <stdio.h>

#include "maxrtos/kernel/dispatch.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/scheduler.h"

#define STACK ( 128U )

static uint8_t s_stacks[ 4 ][ STACK ];
static maxrtos_partition_table_t s_table;

static void entry( void * arg )
{
    ( void ) arg;
}

static void reset( void )
{
    maxrtos_process_pool_init();
    assert( maxrtos_partition_table_init( &s_table ) == MAXRTOS_OK );
}

static maxrtos_process_id_t add(
    unsigned slot,
    uint8_t priority )
{
    maxrtos_process_id_t id;

    assert(
        maxrtos_process_create(
            s_stacks[ slot ], STACK, 0U, priority, entry, NULL, &id ) ==
        MAXRTOS_OK );
    assert( maxrtos_partition_add_process( &s_table, id ) == MAXRTOS_OK );

    return id;
}

static maxrtos_process_id_t dispatch( void )
{
    maxrtos_process_id_t next;

    assert( maxrtos_partition_dispatch( &s_table, 0U, &next ) == MAXRTOS_OK );

    return next;
}

static maxrtos_process_id_t yield( void )
{
    maxrtos_process_id_t next;

    assert( maxrtos_partition_yield( &s_table, 0U, &next ) == MAXRTOS_OK );

    return next;
}

static maxrtos_process_state_t state_of( maxrtos_process_id_t id )
{
    return maxrtos_process_get( id )->state;
}

/* The process the scheduler would pick next, without taking it. */
static maxrtos_process_id_t peek( void )
{
    maxrtos_process_id_t id;

    assert(
        maxrtos_scheduler_next( &s_table.scheduler_ctx[ 0 ], &id ) ==
        MAXRTOS_OK );

    return id;
}

static void test_the_first_dispatch_picks_the_highest_priority( void )
{
    maxrtos_process_id_t low;
    maxrtos_process_id_t high;

    reset();
    low = add( 0U, 7U );
    high = add( 1U, 2U );

    assert( dispatch() == high );
    assert( state_of( high ) == MAXRTOS_PROCESS_STATE_RUNNING );
    assert( state_of( low ) == MAXRTOS_PROCESS_STATE_READY );

    printf( "test_the_first_dispatch_picks_the_highest_priority: PASS\n" );
}

static void test_a_higher_priority_ready_process_preempts_the_running_one(
    void )
{
    maxrtos_process_id_t low;
    maxrtos_process_id_t high;

    reset();
    low = add( 0U, 5U );
    assert( dispatch() == low );

    /* A higher-priority process becomes READY (a wake-up, say). */
    high = add( 1U, 2U );
    assert( dispatch() == high );
    assert( state_of( high ) == MAXRTOS_PROCESS_STATE_RUNNING );
    assert( state_of( low ) == MAXRTOS_PROCESS_STATE_READY );

    printf(
        "test_a_higher_priority_ready_process_preempts_the_running_one: "
        "PASS\n" );
}

static void test_the_running_process_is_not_rotated_out_by_a_peer( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    unsigned i;

    reset();
    a = add( 0U, 5U );
    b = add( 1U, 5U );
    assert( dispatch() == a );

    /* No time slice: tick after tick, a keeps the CPU. */
    for( i = 0U; i < 5U; i++ )
    {
        assert( dispatch() == a );
        assert( state_of( a ) == MAXRTOS_PROCESS_STATE_RUNNING );
        assert( state_of( b ) == MAXRTOS_PROCESS_STATE_READY );
    }

    printf( "test_the_running_process_is_not_rotated_out_by_a_peer: PASS\n" );
}

static void test_the_running_process_keeps_the_cpu_against_a_lower_priority(
    void )
{
    maxrtos_process_id_t high;
    maxrtos_process_id_t low;
    unsigned i;

    reset();
    high = add( 0U, 1U );
    low = add( 1U, 6U );
    assert( dispatch() == high );

    for( i = 0U; i < 5U; i++ )
    {
        assert( dispatch() == high );
        assert( yield() == high ); /* even giving way: low is not a peer */
    }

    assert( state_of( low ) == MAXRTOS_PROCESS_STATE_READY );

    printf(
        "test_the_running_process_keeps_the_cpu_against_a_lower_priority: "
        "PASS\n" );
}

static void test_a_yield_gives_way_to_peers_in_first_in_first_out_order( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t c;

    reset();
    a = add( 0U, 5U );
    b = add( 1U, 5U );
    c = add( 2U, 5U );
    assert( dispatch() == a );

    assert( yield() == b );
    assert( yield() == c );
    assert( yield() == a ); /* back to the first, each behind the others */
    assert( yield() == b );
    assert( state_of( b ) == MAXRTOS_PROCESS_STATE_RUNNING );

    printf(
        "test_a_yield_gives_way_to_peers_in_first_in_first_out_order: PASS\n" );
}

static void test_a_yield_alone_keeps_the_cpu( void )
{
    maxrtos_process_id_t only;

    reset();
    only = add( 0U, 5U );
    assert( dispatch() == only );

    assert( yield() == only );
    assert( state_of( only ) == MAXRTOS_PROCESS_STATE_RUNNING );

    printf( "test_a_yield_alone_keeps_the_cpu: PASS\n" );
}

static void test_a_yield_gives_way_to_a_higher_priority_too( void )
{
    maxrtos_process_id_t low;
    maxrtos_process_id_t high;

    reset();
    low = add( 0U, 6U );
    assert( dispatch() == low );
    high = add( 1U, 3U );

    assert( yield() == high );

    printf( "test_a_yield_gives_way_to_a_higher_priority_too: PASS\n" );
}

static void test_a_preempted_process_stays_ahead_of_its_peers( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t peer;
    maxrtos_process_id_t high;
    maxrtos_process_id_t next;

    reset();
    a = add( 0U, 5U );
    peer = add( 1U, 5U );
    assert( dispatch() == a ); /* a runs, peer waits behind it */

    /* A higher-priority process preempts a. a was running before peer was
     * next in line, so it is still the oldest READY process at priority 5. */
    high = add( 2U, 1U );
    assert( dispatch() == high );
    assert( peek() == a );

    /* When high is done, a runs again, not peer. */
    assert(
        maxrtos_kernel_block_and_dispatch(
            &s_table.scheduler_ctx[ 0 ], high, &next ) == MAXRTOS_OK );
    assert( next == a );
    assert( state_of( peer ) == MAXRTOS_PROCESS_STATE_READY );

    printf( "test_a_preempted_process_stays_ahead_of_its_peers: PASS\n" );
}

static void test_scheduler_has_higher_looks_only_at_strictly_higher_priorities(
    void )
{
    maxrtos_scheduler_context_t ctx;

    assert( maxrtos_scheduler_init( &ctx ) == MAXRTOS_OK );
    assert( !maxrtos_scheduler_has_higher( &ctx, 5U ) );

    assert( maxrtos_scheduler_enqueue( &ctx, 0U, 5U ) == MAXRTOS_OK );
    assert( !maxrtos_scheduler_has_higher( &ctx, 5U ) ); /* equal: no */
    assert( maxrtos_scheduler_has_higher( &ctx, 6U ) );

    assert( maxrtos_scheduler_enqueue( &ctx, 1U, 0U ) == MAXRTOS_OK );
    assert( maxrtos_scheduler_has_higher( &ctx, 1U ) ); /* priority 0 beats 1 */
    assert( !maxrtos_scheduler_has_higher( &ctx, 0U ) );
    assert( !maxrtos_scheduler_has_higher( NULL, 5U ) );

    printf(
        "test_scheduler_has_higher_looks_only_at_strictly_higher_priorities: "
        "PASS\n" );
}

static void test_enqueue_front_puts_a_process_ahead_of_the_queue( void )
{
    maxrtos_scheduler_context_t ctx;
    maxrtos_process_id_t id;

    assert( maxrtos_scheduler_init( &ctx ) == MAXRTOS_OK );
    assert( maxrtos_scheduler_enqueue( &ctx, 1U, 4U ) == MAXRTOS_OK );
    assert( maxrtos_scheduler_enqueue( &ctx, 2U, 4U ) == MAXRTOS_OK );
    assert( maxrtos_scheduler_enqueue_front( &ctx, 3U, 4U ) == MAXRTOS_OK );

    assert( maxrtos_scheduler_take_next( &ctx, &id ) == MAXRTOS_OK );
    assert( id == 3U );
    assert( maxrtos_scheduler_take_next( &ctx, &id ) == MAXRTOS_OK );
    assert( id == 1U );
    assert( maxrtos_scheduler_take_next( &ctx, &id ) == MAXRTOS_OK );
    assert( id == 2U );
    assert(
        maxrtos_scheduler_take_next( &ctx, &id ) == MAXRTOS_ERR_QUEUE_EMPTY );

    printf( "test_enqueue_front_puts_a_process_ahead_of_the_queue: PASS\n" );
}

int main( void )
{
    test_the_first_dispatch_picks_the_highest_priority();
    test_a_higher_priority_ready_process_preempts_the_running_one();
    test_the_running_process_is_not_rotated_out_by_a_peer();
    test_the_running_process_keeps_the_cpu_against_a_lower_priority();
    test_a_yield_gives_way_to_peers_in_first_in_first_out_order();
    test_a_yield_alone_keeps_the_cpu();
    test_a_yield_gives_way_to_a_higher_priority_too();
    test_a_preempted_process_stays_ahead_of_its_peers();
    test_scheduler_has_higher_looks_only_at_strictly_higher_priorities();
    test_enqueue_front_puts_a_process_ahead_of_the_queue();

    printf( "all priority tests passed\n" );

    return 0;
}
