/**
 * @file test_semaphore.c
 * @brief Unit tests for the kernel intra-partition counting semaphore.
 */

#include <assert.h>
#include <stdio.h>
#include <stddef.h>

#include "maxrtos/kernel/semaphore.h"
#include "maxrtos/kernel/ipc_block.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/tick.h"

#define STACK_SIZE ( 128U )

static uint8_t s_stacks[ MAXRTOS_MAX_PROCESSES ][ STACK_SIZE ];
static size_t s_next_stack;

static maxrtos_partition_table_t s_table;

static void dummy_entry( void * arg )
{
    ( void ) arg;
}

static void setup( void )
{
    maxrtos_process_pool_init();
    maxrtos_semaphore_pool_init();
    maxrtos_kernel_tick_reset();
    assert( maxrtos_partition_table_init( &s_table ) == MAXRTOS_OK );
    s_next_stack = 0U;
}

static maxrtos_process_id_t make_process(
    maxrtos_partition_id_t partition,
    uint8_t priority )
{
    maxrtos_process_id_t id;

    assert(
        maxrtos_process_create(
            s_stacks[ s_next_stack ], STACK_SIZE, partition, priority,
            dummy_entry, NULL, &id ) == MAXRTOS_OK );
    s_next_stack++;
    assert( maxrtos_partition_add_process( &s_table, id ) == MAXRTOS_OK );

    return id;
}

static maxrtos_process_id_t dispatch( maxrtos_partition_id_t partition )
{
    maxrtos_process_id_t running;

    assert(
        maxrtos_partition_dispatch( &s_table, partition, &running ) ==
        MAXRTOS_OK );

    return running;
}

static maxrtos_status_t wait_on(
    maxrtos_semaphore_id_t s,
    maxrtos_tick_t timeout,
    maxrtos_process_id_t caller,
    maxrtos_process_id_t * next )
{
    return maxrtos_kernel_semaphore_wait( s, timeout, &s_table, caller, next );
}

static maxrtos_status_t signal_on(
    maxrtos_semaphore_id_t s,
    maxrtos_process_id_t caller )
{
    return maxrtos_kernel_semaphore_signal( s, &s_table, caller );
}

static maxrtos_process_state_t state_of( maxrtos_process_id_t id )
{
    return maxrtos_process_get( id )->state;
}

/* Run a, then let the peer run and block on s; returns with a running. */
static void block_on( maxrtos_semaphore_id_t s,
                      maxrtos_process_id_t waiter,
                      maxrtos_process_id_t runner )
{
    maxrtos_process_id_t next;

    assert( dispatch( 0U ) == waiter );
    assert( wait_on( s, MAXRTOS_TIMEOUT_INFINITE, waiter, &next ) ==
            MAXRTOS_PENDING );
    assert( next == runner );
}

static void test_create_validates_arguments( void )
{
    maxrtos_semaphore_id_t id;
    maxrtos_semaphore_id_t ids[ MAXRTOS_MAX_SEMAPHORES ];
    size_t i;

    setup();

    assert( maxrtos_semaphore_create( 0U, 0U, 1U, NULL ) ==
            MAXRTOS_ERR_INVALID_ARG );
    assert( maxrtos_semaphore_create( MAXRTOS_MAX_PARTITIONS, 0U, 1U, &id ) ==
            MAXRTOS_ERR_INVALID_ARG );
    assert( maxrtos_semaphore_create( 0U, 0U, 0U, &id ) ==
            MAXRTOS_ERR_INVALID_ARG );
    assert( maxrtos_semaphore_create( 0U, 3U, 2U, &id ) ==
            MAXRTOS_ERR_INVALID_ARG );

    for( i = 0U; i < MAXRTOS_MAX_SEMAPHORES; i++ )
    {
        assert( maxrtos_semaphore_create( 0U, 1U, 4U, &ids[ i ] ) ==
                MAXRTOS_OK );
        assert( maxrtos_kernel_semaphore_count( ids[ i ] ) == 1U );
    }

    assert( maxrtos_semaphore_create( 0U, 0U, 1U, &id ) ==
            MAXRTOS_ERR_POOL_FULL );

    printf( "test_create_validates_arguments: PASS\n" );
}

static void test_wait_takes_units_and_polls_when_empty( void )
{
    maxrtos_semaphore_id_t s;
    maxrtos_process_id_t a;
    maxrtos_process_id_t next;

    setup();
    assert( maxrtos_semaphore_create( 0U, 2U, 5U, &s ) == MAXRTOS_OK );
    a = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );

    assert( wait_on( s, 0U, a, &next ) == MAXRTOS_OK );
    assert( maxrtos_kernel_semaphore_count( s ) == 1U );
    assert( wait_on( s, 0U, a, &next ) == MAXRTOS_OK );
    assert( maxrtos_kernel_semaphore_count( s ) == 0U );

    /* Empty: a poll fails without blocking. */
    assert( wait_on( s, 0U, a, &next ) == MAXRTOS_ERR_TIMEOUT );
    assert( state_of( a ) == MAXRTOS_PROCESS_STATE_RUNNING );

    printf( "test_wait_takes_units_and_polls_when_empty: PASS\n" );
}

static void test_signal_counts_up_to_the_maximum( void )
{
    maxrtos_semaphore_id_t s;
    maxrtos_process_id_t a;

    setup();
    assert( maxrtos_semaphore_create( 0U, 0U, 2U, &s ) == MAXRTOS_OK );
    a = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );

    assert( signal_on( s, a ) == MAXRTOS_OK );
    assert( signal_on( s, a ) == MAXRTOS_OK );
    assert( maxrtos_kernel_semaphore_count( s ) == 2U );

    /* At the maximum a further signal is an error and changes nothing. */
    assert( signal_on( s, a ) == MAXRTOS_ERR_OVERFLOW );
    assert( maxrtos_kernel_semaphore_count( s ) == 2U );

    printf( "test_signal_counts_up_to_the_maximum: PASS\n" );
}

static void test_binary_semaphore( void )
{
    maxrtos_semaphore_id_t s;
    maxrtos_process_id_t a;
    maxrtos_process_id_t next;

    setup();
    assert( maxrtos_semaphore_create( 0U, 1U, 1U, &s ) == MAXRTOS_OK );
    a = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );

    assert( signal_on( s, a ) == MAXRTOS_ERR_OVERFLOW );
    assert( wait_on( s, 0U, a, &next ) == MAXRTOS_OK );
    assert( wait_on( s, 0U, a, &next ) == MAXRTOS_ERR_TIMEOUT );
    assert( signal_on( s, a ) == MAXRTOS_OK );
    assert( signal_on( s, a ) == MAXRTOS_ERR_OVERFLOW );

    printf( "test_binary_semaphore: PASS\n" );
}

static void test_blocked_wait_receives_the_signalled_unit( void )
{
    maxrtos_semaphore_id_t s;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_control_block_t * pcb_b;

    setup();
    assert( maxrtos_semaphore_create( 0U, 0U, 1U, &s ) == MAXRTOS_OK );
    a = make_process( 0U, 5U );
    b = make_process( 0U, 5U );

    /* a waits and blocks; b runs. */
    block_on( s, a, b );
    assert( state_of( a ) == MAXRTOS_PROCESS_STATE_BLOCKED );
    assert( s_table.current_id[ 0 ] == b );

    /* Any process may signal (there is no owner). The unit goes to a
     * directly; the count stays zero. */
    assert( signal_on( s, b ) == MAXRTOS_OK );
    assert( maxrtos_kernel_semaphore_count( s ) == 0U );
    pcb_b = maxrtos_process_get( a );
    assert( pcb_b->state == MAXRTOS_PROCESS_STATE_READY );
    assert( pcb_b->ipc_result_pending == true );
    assert( pcb_b->ipc_result == MAXRTOS_OK );
    assert( pcb_b->waitlist == NULL );

    printf( "test_blocked_wait_receives_the_signalled_unit: PASS\n" );
}

static void test_waiters_served_by_priority_then_fifo( void )
{
    maxrtos_semaphore_id_t s;
    maxrtos_process_id_t o;
    maxrtos_process_id_t b;
    maxrtos_process_id_t c;
    maxrtos_process_id_t next;
    maxrtos_waitlist_t elsewhere;
    maxrtos_ipc_operation_t op;
    maxrtos_process_id_t woken;

    setup();
    assert( maxrtos_semaphore_create( 0U, 0U, 3U, &s ) == MAXRTOS_OK );
    o = make_process( 0U, 0U );
    b = make_process( 0U, 5U );
    c = make_process( 0U, 2U );
    assert( dispatch( 0U ) == o );

    /* Park c elsewhere so b reaches the semaphore first. */
    assert( maxrtos_waitlist_init( &elsewhere ) == MAXRTOS_OK );
    op.kind = MAXRTOS_IPC_OP_QUEUE_RECEIVE;
    assert( dispatch( 0U ) == c );
    assert( maxrtos_ipc_block_current(
                &s_table, c, &elsewhere, MAXRTOS_TICK_NONE, &op, &next ) ==
            MAXRTOS_OK );

    assert( dispatch( 0U ) == b );
    assert( wait_on( s, MAXRTOS_TIMEOUT_INFINITE, b, &next ) ==
            MAXRTOS_PENDING );

    assert( maxrtos_ipc_wake_one( &elsewhere, &woken, &op ) == MAXRTOS_OK );
    assert( maxrtos_ipc_resolve( &s_table, c, MAXRTOS_OK ) == MAXRTOS_OK );
    assert( dispatch( 0U ) == c );
    assert( wait_on( s, MAXRTOS_TIMEOUT_INFINITE, c, &next ) ==
            MAXRTOS_PENDING );

    /* b waited first, but c has the higher priority. */
    assert( signal_on( s, o ) == MAXRTOS_OK );
    assert( state_of( c ) == MAXRTOS_PROCESS_STATE_READY );
    assert( state_of( b ) == MAXRTOS_PROCESS_STATE_BLOCKED );

    assert( signal_on( s, o ) == MAXRTOS_OK );
    assert( state_of( b ) == MAXRTOS_PROCESS_STATE_READY );
    assert( maxrtos_kernel_semaphore_count( s ) == 0U );

    printf( "test_waiters_served_by_priority_then_fifo: PASS\n" );
}

static void test_equal_priority_waiters_are_fifo( void )
{
    maxrtos_semaphore_id_t s;
    maxrtos_process_id_t o;
    maxrtos_process_id_t x;
    maxrtos_process_id_t y;
    maxrtos_process_id_t next;

    setup();
    assert( maxrtos_semaphore_create( 0U, 0U, 2U, &s ) == MAXRTOS_OK );
    o = make_process( 0U, 0U );
    x = make_process( 0U, 5U );
    y = make_process( 0U, 5U );
    assert( dispatch( 0U ) == o );

    assert( dispatch( 0U ) == x );
    assert( wait_on( s, MAXRTOS_TIMEOUT_INFINITE, x, &next ) ==
            MAXRTOS_PENDING );
    assert( dispatch( 0U ) == y );
    assert( wait_on( s, MAXRTOS_TIMEOUT_INFINITE, y, &next ) ==
            MAXRTOS_PENDING );

    assert( signal_on( s, o ) == MAXRTOS_OK );
    assert( state_of( x ) == MAXRTOS_PROCESS_STATE_READY );
    assert( state_of( y ) == MAXRTOS_PROCESS_STATE_BLOCKED );

    printf( "test_equal_priority_waiters_are_fifo: PASS\n" );
}

static void test_timed_wait_expires_and_leaves_wait_list( void )
{
    maxrtos_semaphore_id_t s;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t next;
    maxrtos_process_control_block_t * pcb_a;

    setup();
    assert( maxrtos_semaphore_create( 0U, 0U, 2U, &s ) == MAXRTOS_OK );
    a = make_process( 0U, 5U );
    b = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );
    assert( wait_on( s, 3U, a, &next ) == MAXRTOS_PENDING );
    assert( next == b );

    assert( maxrtos_ipc_expire_timeouts( &s_table, 2U ) == 0U );
    assert( maxrtos_ipc_expire_timeouts( &s_table, 3U ) == 1U );
    pcb_a = maxrtos_process_get( a );
    assert( pcb_a->state == MAXRTOS_PROCESS_STATE_READY );
    assert( pcb_a->ipc_result == MAXRTOS_ERR_TIMEOUT );

    /* It no longer waits: the next signal is kept in the count. */
    assert( signal_on( s, b ) == MAXRTOS_OK );
    assert( maxrtos_kernel_semaphore_count( s ) == 1U );

    printf( "test_timed_wait_expires_and_leaves_wait_list: PASS\n" );
}

static void test_wait_fails_when_nothing_else_can_run( void )
{
    maxrtos_semaphore_id_t s;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t next;
    maxrtos_waitlist_t other;
    maxrtos_ipc_operation_t op;

    setup();
    assert( maxrtos_semaphore_create( 0U, 0U, 1U, &s ) == MAXRTOS_OK );
    a = make_process( 0U, 5U );
    b = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );

    /* a blocks elsewhere, leaving b as the only runner. */
    op.kind = MAXRTOS_IPC_OP_QUEUE_RECEIVE;
    assert( maxrtos_waitlist_init( &other ) == MAXRTOS_OK );
    assert( maxrtos_ipc_block_current(
                &s_table, a, &other, MAXRTOS_TICK_NONE, &op, &next ) ==
            MAXRTOS_OK );
    assert( next == b );

    assert( wait_on( s, MAXRTOS_TIMEOUT_INFINITE, b, &next ) ==
            MAXRTOS_ERR_TIMEOUT );
    assert( state_of( b ) == MAXRTOS_PROCESS_STATE_RUNNING );

    printf( "test_wait_fails_when_nothing_else_can_run: PASS\n" );
}

static void test_other_partition_and_bad_handles_rejected( void )
{
    maxrtos_semaphore_id_t s;
    maxrtos_process_id_t a;
    maxrtos_process_id_t other;
    maxrtos_process_id_t next;

    setup();
    assert( maxrtos_semaphore_create( 0U, 1U, 1U, &s ) == MAXRTOS_OK );
    a = make_process( 0U, 5U );
    other = make_process( 1U, 5U );
    assert( dispatch( 0U ) == a );
    assert( dispatch( 1U ) == other );

    assert( wait_on( s, 0U, other, &next ) == MAXRTOS_ERR_INVALID_ID );
    assert( signal_on( s, other ) == MAXRTOS_ERR_INVALID_ID );
    assert( maxrtos_kernel_semaphore_count( s ) == 1U );

    assert( wait_on( MAXRTOS_MAX_SEMAPHORES, 0U, a, &next ) ==
            MAXRTOS_ERR_INVALID_ID );
    assert( wait_on( MAXRTOS_INVALID_SEMAPHORE_ID, 0U, a, &next ) ==
            MAXRTOS_ERR_INVALID_ID );
    assert( wait_on( MAXRTOS_MAX_SEMAPHORES - 1U, 0U, a, &next ) ==
            MAXRTOS_ERR_INVALID_ID ); /* never created */
    assert( wait_on( s, 0U, MAXRTOS_INVALID_PROCESS_ID, &next ) ==
            MAXRTOS_ERR_INVALID_ID );
    assert( signal_on( MAXRTOS_INVALID_SEMAPHORE_ID, a ) ==
            MAXRTOS_ERR_INVALID_ID );

    assert( maxrtos_kernel_semaphore_wait( s, 0U, NULL, a, &next ) ==
            MAXRTOS_ERR_INVALID_ARG );
    assert( maxrtos_kernel_semaphore_wait( s, 0U, &s_table, a, NULL ) ==
            MAXRTOS_ERR_INVALID_ARG );
    assert( maxrtos_kernel_semaphore_signal( s, NULL, a ) ==
            MAXRTOS_ERR_INVALID_ARG );
    assert( maxrtos_kernel_semaphore_count( MAXRTOS_INVALID_SEMAPHORE_ID ) ==
            0U );

    printf( "test_other_partition_and_bad_handles_rejected: PASS\n" );
}

static void test_get_status_reports_value_maximum_and_waiters( void )
{
    maxrtos_semaphore_id_t s;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t other;
    maxrtos_semaphore_status_t st;

    setup();
    assert( maxrtos_semaphore_create( 0U, 1U, 3U, &s ) == MAXRTOS_OK );
    a = make_process( 0U, 5U );
    b = make_process( 0U, 5U );
    other = make_process( 1U, 5U );

    assert( dispatch( 0U ) == a );

    assert( maxrtos_kernel_semaphore_get_status( s, a, &st ) == MAXRTOS_OK );
    assert( st.current_value == 1U );
    assert( st.maximum_value == 3U );
    assert( st.waiting == 0U );

    /* Empty it, then block a waiter. */
    assert( wait_on( s, 0U, a, &( maxrtos_process_id_t ){ 0U } ) == MAXRTOS_OK );
    {
        maxrtos_process_id_t next;

        assert( wait_on( s, MAXRTOS_TIMEOUT_INFINITE, a, &next ) ==
                MAXRTOS_PENDING );
        assert( next == b );
    }

    assert( maxrtos_kernel_semaphore_get_status( s, b, &st ) == MAXRTOS_OK );
    assert( st.current_value == 0U );
    assert( st.waiting == 1U );

    /* Signalling hands the unit to the waiter: nobody waits, count 0. */
    assert( signal_on( s, b ) == MAXRTOS_OK );
    assert( maxrtos_kernel_semaphore_get_status( s, b, &st ) == MAXRTOS_OK );
    assert( st.current_value == 0U );
    assert( st.waiting == 0U );

    /* Errors leave the output alone. */
    st.current_value = 77U;
    assert( maxrtos_kernel_semaphore_get_status( s, other, &st ) ==
            MAXRTOS_ERR_INVALID_ID );
    assert( maxrtos_kernel_semaphore_get_status(
                MAXRTOS_INVALID_SEMAPHORE_ID, a, &st ) ==
            MAXRTOS_ERR_INVALID_ID );
    assert( maxrtos_kernel_semaphore_get_status( s, a, NULL ) ==
            MAXRTOS_ERR_INVALID_ARG );
    assert( st.current_value == 77U );

    printf( "test_get_status_reports_value_maximum_and_waiters: PASS\n" );
}

int main( void )
{
    test_create_validates_arguments();
    test_wait_takes_units_and_polls_when_empty();
    test_signal_counts_up_to_the_maximum();
    test_binary_semaphore();
    test_blocked_wait_receives_the_signalled_unit();
    test_waiters_served_by_priority_then_fifo();
    test_equal_priority_waiters_are_fifo();
    test_timed_wait_expires_and_leaves_wait_list();
    test_wait_fails_when_nothing_else_can_run();
    test_other_partition_and_bad_handles_rejected();
    test_get_status_reports_value_maximum_and_waiters();

    printf( "all semaphore tests passed\n" );

    return 0;
}
