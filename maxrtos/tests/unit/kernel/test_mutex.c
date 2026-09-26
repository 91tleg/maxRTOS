/**
 * @file test_mutex.c
 * @brief Unit tests for the kernel intra-partition mutex.
 */

#include <assert.h>
#include <stdio.h>
#include <stddef.h>

#include "maxrtos/kernel/mutex.h"
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
    maxrtos_mutex_pool_init();
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

static maxrtos_status_t lock(
    maxrtos_mutex_id_t m,
    maxrtos_tick_t timeout,
    maxrtos_process_id_t caller,
    maxrtos_process_id_t * next )
{
    return maxrtos_kernel_mutex_lock( m, timeout, &s_table, caller, next );
}

static maxrtos_process_state_t state_of( maxrtos_process_id_t id )
{
    return maxrtos_process_get( id )->state;
}

static void test_create_binds_partition_and_exhausts_pool( void )
{
    maxrtos_mutex_id_t id;
    maxrtos_mutex_id_t ids[ MAXRTOS_MAX_MUTEXES ];
    size_t i;

    setup();

    assert( maxrtos_mutex_create( 0U, NULL ) == MAXRTOS_ERR_INVALID_ARG );
    assert( maxrtos_mutex_create( MAXRTOS_MAX_PARTITIONS, &id ) ==
            MAXRTOS_ERR_INVALID_ARG );

    for( i = 0U; i < MAXRTOS_MAX_MUTEXES; i++ )
    {
        assert( maxrtos_mutex_create( 0U, &ids[ i ] ) == MAXRTOS_OK );
        assert( maxrtos_kernel_mutex_owner( ids[ i ] ) ==
                MAXRTOS_INVALID_PROCESS_ID );
    }

    assert( maxrtos_mutex_create( 0U, &id ) == MAXRTOS_ERR_POOL_FULL );

    printf( "test_create_binds_partition_and_exhausts_pool: PASS\n" );
}

static void test_uncontended_lock_and_unlock( void )
{
    maxrtos_mutex_id_t m;
    maxrtos_process_id_t a;
    maxrtos_process_id_t next;

    setup();
    assert( maxrtos_mutex_create( 0U, &m ) == MAXRTOS_OK );
    a = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );

    assert( lock( m, 0U, a, &next ) == MAXRTOS_OK );
    assert( maxrtos_kernel_mutex_owner( m ) == a );

    assert( maxrtos_kernel_mutex_unlock( m, &s_table, a ) == MAXRTOS_OK );
    assert( maxrtos_kernel_mutex_owner( m ) == MAXRTOS_INVALID_PROCESS_ID );

    /* And it can be taken again. */
    assert( lock( m, 0U, a, &next ) == MAXRTOS_OK );

    printf( "test_uncontended_lock_and_unlock: PASS\n" );
}

static void test_recursive_lock_and_foreign_unlock_rejected( void )
{
    maxrtos_mutex_id_t m;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t next;

    setup();
    assert( maxrtos_mutex_create( 0U, &m ) == MAXRTOS_OK );
    a = make_process( 0U, 5U );
    b = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );

    /* Unlocking an unlocked mutex is an error. */
    assert( maxrtos_kernel_mutex_unlock( m, &s_table, a ) ==
            MAXRTOS_ERR_INVALID_STATE );

    assert( lock( m, MAXRTOS_TIMEOUT_INFINITE, a, &next ) == MAXRTOS_OK );

    /* Recursive lock must not deadlock the owner on itself. */
    assert( lock( m, MAXRTOS_TIMEOUT_INFINITE, a, &next ) ==
            MAXRTOS_ERR_INVALID_STATE );
    assert( maxrtos_kernel_mutex_owner( m ) == a );

    /* Only the owner may unlock. */
    assert( maxrtos_kernel_mutex_unlock( m, &s_table, b ) ==
            MAXRTOS_ERR_INVALID_STATE );
    assert( maxrtos_kernel_mutex_owner( m ) == a );

    printf( "test_recursive_lock_and_foreign_unlock_rejected: PASS\n" );
}

static void test_other_partition_and_bad_handles_rejected( void )
{
    maxrtos_mutex_id_t m;
    maxrtos_process_id_t a;
    maxrtos_process_id_t other;
    maxrtos_process_id_t next;

    setup();
    assert( maxrtos_mutex_create( 0U, &m ) == MAXRTOS_OK );
    a = make_process( 0U, 5U );
    other = make_process( 1U, 5U );
    assert( dispatch( 0U ) == a );
    assert( dispatch( 1U ) == other );

    assert( lock( m, 0U, other, &next ) == MAXRTOS_ERR_INVALID_ID );
    assert( maxrtos_kernel_mutex_unlock( m, &s_table, other ) ==
            MAXRTOS_ERR_INVALID_ID );
    assert( maxrtos_kernel_mutex_owner( m ) == MAXRTOS_INVALID_PROCESS_ID );

    assert( lock( MAXRTOS_MAX_MUTEXES, 0U, a, &next ) ==
            MAXRTOS_ERR_INVALID_ID );
    assert( lock( MAXRTOS_INVALID_MUTEX_ID, 0U, a, &next ) ==
            MAXRTOS_ERR_INVALID_ID );
    assert( lock( m, 0U, MAXRTOS_INVALID_PROCESS_ID, &next ) ==
            MAXRTOS_ERR_INVALID_ID );

    /* A handle that was never created. */
    assert( lock( MAXRTOS_MAX_MUTEXES - 1U, 0U, a, &next ) ==
            MAXRTOS_ERR_INVALID_ID );

    assert( maxrtos_kernel_mutex_lock( m, 0U, NULL, a, &next ) ==
            MAXRTOS_ERR_INVALID_ARG );
    assert( maxrtos_kernel_mutex_lock( m, 0U, &s_table, a, NULL ) ==
            MAXRTOS_ERR_INVALID_ARG );
    assert( maxrtos_kernel_mutex_unlock( m, NULL, a ) ==
            MAXRTOS_ERR_INVALID_ARG );

    printf( "test_other_partition_and_bad_handles_rejected: PASS\n" );
}

static void test_try_lock_fails_when_held( void )
{
    maxrtos_mutex_id_t m;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t next;

    setup();
    assert( maxrtos_mutex_create( 0U, &m ) == MAXRTOS_OK );
    a = make_process( 0U, 5U );
    b = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );
    assert( lock( m, 0U, a, &next ) == MAXRTOS_OK );

    assert( dispatch( 0U ) == b );
    assert( lock( m, 0U, b, &next ) == MAXRTOS_ERR_TIMEOUT );
    assert( state_of( b ) == MAXRTOS_PROCESS_STATE_RUNNING );

    printf( "test_try_lock_fails_when_held: PASS\n" );
}

static void test_contended_lock_blocks_and_unlock_hands_off( void )
{
    maxrtos_mutex_id_t m;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t next;
    maxrtos_process_control_block_t * pcb_b;

    setup();
    assert( maxrtos_mutex_create( 0U, &m ) == MAXRTOS_OK );
    a = make_process( 0U, 5U );
    b = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );
    assert( lock( m, MAXRTOS_TIMEOUT_INFINITE, a, &next ) == MAXRTOS_OK );

    assert( dispatch( 0U ) == b );
    assert( lock( m, MAXRTOS_TIMEOUT_INFINITE, b, &next ) ==
            MAXRTOS_PENDING );

    /* b is blocked and a was selected to run in its place. */
    assert( next == a );
    assert( state_of( b ) == MAXRTOS_PROCESS_STATE_BLOCKED );
    assert( state_of( a ) == MAXRTOS_PROCESS_STATE_RUNNING );
    assert( s_table.current_id[ 0 ] == a );
    assert( maxrtos_kernel_mutex_owner( m ) == a );

    /* Unlock passes ownership straight to b and readies it. */
    assert( maxrtos_kernel_mutex_unlock( m, &s_table, a ) == MAXRTOS_OK );
    assert( maxrtos_kernel_mutex_owner( m ) == b );
    pcb_b = maxrtos_process_get( b );
    assert( pcb_b->state == MAXRTOS_PROCESS_STATE_READY );
    assert( pcb_b->ipc_result_pending == true );
    assert( pcb_b->ipc_result == MAXRTOS_OK );
    assert( pcb_b->waitlist == NULL );
    assert( state_of( a ) == MAXRTOS_PROCESS_STATE_RUNNING );

    printf( "test_contended_lock_blocks_and_unlock_hands_off: PASS\n" );
}

static void test_lock_fails_when_nothing_else_can_run( void )
{
    maxrtos_mutex_id_t m;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t next;

    setup();
    assert( maxrtos_mutex_create( 0U, &m ) == MAXRTOS_OK );
    a = make_process( 0U, 5U );
    b = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );
    assert( lock( m, MAXRTOS_TIMEOUT_INFINITE, a, &next ) == MAXRTOS_OK );

    /* a blocks on something else, leaving b as the only runner. */
    {
        maxrtos_waitlist_t other;
        maxrtos_ipc_operation_t op;

        op.kind = MAXRTOS_IPC_OP_QUEUE_RECEIVE;
        assert( maxrtos_waitlist_init( &other ) == MAXRTOS_OK );
        assert( maxrtos_ipc_block_current(
                    &s_table, a, &other, MAXRTOS_TICK_NONE, &op, &next ) ==
                MAXRTOS_OK );
        assert( next == b );
    }

    /* b wants the mutex a holds, but no READY process could run while
     * it waits, so it must fail instead of blocking. */
    assert( lock( m, MAXRTOS_TIMEOUT_INFINITE, b, &next ) ==
            MAXRTOS_ERR_TIMEOUT );
    assert( state_of( b ) == MAXRTOS_PROCESS_STATE_RUNNING );

    printf( "test_lock_fails_when_nothing_else_can_run: PASS\n" );
}

/* Owner o (priority 0) holds the mutex. b (priority 5) waits first, c
 * (priority 2) second. Unlock must pick c: priority beats arrival. */
static void test_waiters_served_by_priority( void )
{
    maxrtos_mutex_id_t m;
    maxrtos_process_id_t o;
    maxrtos_process_id_t b;
    maxrtos_process_id_t c;
    maxrtos_process_id_t next;
    maxrtos_waitlist_t elsewhere;
    maxrtos_ipc_operation_t op;
    maxrtos_process_id_t woken;

    setup();
    assert( maxrtos_mutex_create( 0U, &m ) == MAXRTOS_OK );
    o = make_process( 0U, 0U );
    b = make_process( 0U, 5U );
    c = make_process( 0U, 2U );

    assert( dispatch( 0U ) == o );
    assert( lock( m, MAXRTOS_TIMEOUT_INFINITE, o, &next ) == MAXRTOS_OK );

    /* Park c on an unrelated wait list so b reaches the mutex first. */
    assert( maxrtos_waitlist_init( &elsewhere ) == MAXRTOS_OK );
    op.kind = MAXRTOS_IPC_OP_QUEUE_RECEIVE;
    assert( dispatch( 0U ) == c );
    assert( maxrtos_ipc_block_current(
                &s_table, c, &elsewhere, MAXRTOS_TICK_NONE, &op, &next ) ==
            MAXRTOS_OK );
    assert( next == o );

    assert( dispatch( 0U ) == b );
    assert( lock( m, MAXRTOS_TIMEOUT_INFINITE, b, &next ) ==
            MAXRTOS_PENDING );
    assert( next == o );

    /* c comes back and queues behind b. */
    assert( maxrtos_ipc_wake_one( &elsewhere, &woken, &op ) == MAXRTOS_OK );
    assert( woken == c );
    assert( maxrtos_ipc_resolve( &s_table, c, MAXRTOS_OK ) == MAXRTOS_OK );
    assert( dispatch( 0U ) == c );
    assert( lock( m, MAXRTOS_TIMEOUT_INFINITE, c, &next ) ==
            MAXRTOS_PENDING );
    assert( next == o );

    assert( maxrtos_kernel_mutex_unlock( m, &s_table, o ) == MAXRTOS_OK );
    assert( maxrtos_kernel_mutex_owner( m ) == c );
    assert( state_of( c ) == MAXRTOS_PROCESS_STATE_READY );
    assert( state_of( b ) == MAXRTOS_PROCESS_STATE_BLOCKED );

    printf( "test_waiters_served_by_priority: PASS\n" );
}

static void test_equal_priority_waiters_are_fifo( void )
{
    maxrtos_mutex_id_t m;
    maxrtos_process_id_t o;
    maxrtos_process_id_t x;
    maxrtos_process_id_t y;
    maxrtos_process_id_t next;

    setup();
    assert( maxrtos_mutex_create( 0U, &m ) == MAXRTOS_OK );
    o = make_process( 0U, 0U );
    x = make_process( 0U, 5U );
    y = make_process( 0U, 5U );

    assert( dispatch( 0U ) == o );
    assert( lock( m, MAXRTOS_TIMEOUT_INFINITE, o, &next ) == MAXRTOS_OK );

    assert( dispatch( 0U ) == x );
    assert( lock( m, MAXRTOS_TIMEOUT_INFINITE, x, &next ) ==
            MAXRTOS_PENDING );
    assert( dispatch( 0U ) == y );
    assert( lock( m, MAXRTOS_TIMEOUT_INFINITE, y, &next ) ==
            MAXRTOS_PENDING );

    assert( maxrtos_kernel_mutex_unlock( m, &s_table, o ) == MAXRTOS_OK );
    assert( maxrtos_kernel_mutex_owner( m ) == x );

    printf( "test_equal_priority_waiters_are_fifo: PASS\n" );
}

static void test_timed_lock_expires_and_leaves_wait_list( void )
{
    maxrtos_mutex_id_t m;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t next;
    maxrtos_process_control_block_t * pcb_b;

    setup();
    assert( maxrtos_mutex_create( 0U, &m ) == MAXRTOS_OK );
    a = make_process( 0U, 5U );
    b = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );
    assert( lock( m, MAXRTOS_TIMEOUT_INFINITE, a, &next ) == MAXRTOS_OK );
    assert( dispatch( 0U ) == b );
    assert( lock( m, 3U, b, &next ) == MAXRTOS_PENDING );

    /* Not yet. */
    assert( maxrtos_ipc_expire_timeouts( &s_table, 2U ) == 0U );
    assert( state_of( b ) == MAXRTOS_PROCESS_STATE_BLOCKED );

    assert( maxrtos_ipc_expire_timeouts( &s_table, 3U ) == 1U );
    pcb_b = maxrtos_process_get( b );
    assert( pcb_b->state == MAXRTOS_PROCESS_STATE_READY );
    assert( pcb_b->ipc_result == MAXRTOS_ERR_TIMEOUT );

    /* It no longer waits: unlock leaves the mutex free, not owned by b. */
    assert( maxrtos_kernel_mutex_unlock( m, &s_table, a ) == MAXRTOS_OK );
    assert( maxrtos_kernel_mutex_owner( m ) == MAXRTOS_INVALID_PROCESS_ID );

    printf( "test_timed_lock_expires_and_leaves_wait_list: PASS\n" );
}

static void test_release_all_hands_off_or_unlocks( void )
{
    maxrtos_mutex_id_t m1;
    maxrtos_mutex_id_t m2;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t next;

    setup();
    assert( maxrtos_mutex_create( 0U, &m1 ) == MAXRTOS_OK );
    assert( maxrtos_mutex_create( 0U, &m2 ) == MAXRTOS_OK );
    a = make_process( 0U, 5U );
    b = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );
    assert( lock( m1, MAXRTOS_TIMEOUT_INFINITE, a, &next ) == MAXRTOS_OK );
    assert( lock( m2, MAXRTOS_TIMEOUT_INFINITE, a, &next ) == MAXRTOS_OK );

    /* b waits for m1 only. */
    assert( dispatch( 0U ) == b );
    assert( lock( m1, MAXRTOS_TIMEOUT_INFINITE, b, &next ) ==
            MAXRTOS_PENDING );

    /* a is restarted by fault recovery: m1 goes to b, m2 is freed. */
    maxrtos_kernel_mutex_release_all( &s_table, a );
    assert( maxrtos_kernel_mutex_owner( m1 ) == b );
    assert( maxrtos_kernel_mutex_owner( m2 ) == MAXRTOS_INVALID_PROCESS_ID );
    assert( state_of( b ) == MAXRTOS_PROCESS_STATE_READY );

    /* Releasing what a process does not own changes nothing. */
    maxrtos_kernel_mutex_release_all( &s_table, a );
    assert( maxrtos_kernel_mutex_owner( m1 ) == b );

    printf( "test_release_all_hands_off_or_unlocks: PASS\n" );
}

int main( void )
{
    test_create_binds_partition_and_exhausts_pool();
    test_uncontended_lock_and_unlock();
    test_recursive_lock_and_foreign_unlock_rejected();
    test_other_partition_and_bad_handles_rejected();
    test_try_lock_fails_when_held();
    test_contended_lock_blocks_and_unlock_hands_off();
    test_lock_fails_when_nothing_else_can_run();
    test_waiters_served_by_priority();
    test_equal_priority_waiters_are_fifo();
    test_timed_lock_expires_and_leaves_wait_list();
    test_release_all_hands_off_or_unlocks();

    printf( "all mutex tests passed\n" );

    return 0;
}
