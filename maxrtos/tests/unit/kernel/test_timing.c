/**
 * @file test_timing.c
 * @brief Unit tests for periodic release and deadline supervision.
 */

#include <assert.h>
#include <stdio.h>

#include "maxrtos/kernel/fault_recovery.h"
#include "maxrtos/kernel/frame.h"
#include "maxrtos/kernel/health_monitor.h"
#include "maxrtos/kernel/ipc_block.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/tick.h"
#include "maxrtos/kernel/timing.h"

#define STACK ( 128U )

static uint8_t s_stacks[ 4 ][ STACK ];
static maxrtos_partition_table_t s_table;
static maxrtos_frame_schedule_t s_frame;
static maxrtos_frame_slot_t s_slots[ 1 ] = { { 0U, 100000U } };

static void entry( void * arg ) { ( void ) arg; }

static maxrtos_process_id_t add( unsigned slot, maxrtos_partition_id_t partition )
{
    maxrtos_process_id_t id;

    assert( maxrtos_process_create( s_stacks[ slot ], STACK, partition, 5U,
                                    entry, NULL, &id ) == MAXRTOS_OK );
    assert( maxrtos_partition_add_process( &s_table, id ) == MAXRTOS_OK );

    return id;
}

static void reset( void )
{
    maxrtos_process_pool_init();
    maxrtos_kernel_tick_reset();
    assert( maxrtos_partition_table_init( &s_table ) == MAXRTOS_OK );
    assert( maxrtos_frame_init( &s_frame, s_slots, 1U ) == MAXRTOS_OK );
}

/* Advance the kernel by n ticks. */
static void advance( unsigned n )
{
    maxrtos_process_id_t next;

    while( n-- > 0U )
    {
        ( void ) maxrtos_kernel_on_tick( &s_frame, &s_table, &next );
    }
}

static maxrtos_process_id_t start_running( maxrtos_process_id_t id )
{
    maxrtos_process_id_t running;

    assert( maxrtos_partition_dispatch( &s_table, maxrtos_process_get( id )->partition_id,
                                        &running ) == MAXRTOS_OK );
    assert( running == id );

    return running;
}

static void test_set_timing_validates_and_arms( void )
{
    maxrtos_process_id_t id;
    maxrtos_process_control_block_t * pcb;

    reset();
    id = add( 0U, 0U );
    pcb = maxrtos_process_get( id );

    assert( maxrtos_process_set_timing( MAXRTOS_INVALID_PROCESS_ID, 10U, 4U ) ==
            MAXRTOS_ERR_INVALID_ID );
    assert( maxrtos_process_set_timing( id, 10U, 11U ) == MAXRTOS_ERR_INVALID_ARG );
    assert( pcb->period == 0U ); /* rejected: nothing changed */

    advance( 7U );
    assert( maxrtos_process_set_timing( id, 10U, 4U ) == MAXRTOS_OK );
    assert( pcb->period == 10U );
    assert( pcb->time_capacity == 4U );
    assert( pcb->release_time == 7U );
    assert( pcb->deadline_time == 11U );

    /* No capacity: nothing to supervise. */
    assert( maxrtos_process_set_timing( id, 10U, 0U ) == MAXRTOS_OK );
    assert( pcb->deadline_time == MAXRTOS_TICK_NONE );

    /* Aperiodic with a deadline is allowed. */
    assert( maxrtos_process_set_timing( id, 0U, 5U ) == MAXRTOS_OK );
    assert( pcb->deadline_time == pcb->release_time + 5U );

    printf( "test_set_timing_validates_and_arms: PASS\n" );
}

static void test_scheduler_start_rearms_from_tick_zero( void )
{
    maxrtos_process_id_t id;
    maxrtos_process_control_block_t * pcb;

    reset();
    id = add( 0U, 0U );
    advance( 30U );
    assert( maxrtos_process_set_timing( id, 10U, 4U ) == MAXRTOS_OK );

    maxrtos_kernel_tick_reset();
    maxrtos_kernel_rearm_all_timing();

    pcb = maxrtos_process_get( id );
    assert( pcb->release_time == 0U );
    assert( pcb->deadline_time == 4U );

    printf( "test_scheduler_start_rearms_from_tick_zero: PASS\n" );
}

static void test_deadline_miss_is_detected_exactly_at_the_deadline( void )
{
    maxrtos_process_id_t id;
    maxrtos_process_id_t missed;
    maxrtos_process_control_block_t * pcb;

    reset();
    id = add( 0U, 0U );
    pcb = maxrtos_process_get( id );
    assert( maxrtos_process_set_timing( id, 10U, 4U ) == MAXRTOS_OK );

    advance( 3U );
    assert( maxrtos_kernel_take_deadline_miss( &missed ) == false );
    assert( pcb->deadline_misses == 0U );

    advance( 1U ); /* tick 4 == release 0 + capacity 4 */
    assert( maxrtos_kernel_take_deadline_miss( &missed ) == true );
    assert( missed == id );
    assert( pcb->deadline_misses == 1U );
    assert( pcb->deadline_time == MAXRTOS_TICK_NONE ); /* disarmed: no repeat */

    advance( 20U );
    assert( maxrtos_kernel_take_deadline_miss( &missed ) == false );
    assert( pcb->deadline_misses == 1U );

    printf( "test_deadline_miss_is_detected_exactly_at_the_deadline: PASS\n" );
}

static void test_a_starved_process_still_misses_its_deadline( void )
{
    maxrtos_process_id_t hog;
    maxrtos_process_id_t victim;
    maxrtos_process_id_t missed;

    reset();
    hog = add( 0U, 0U );
    victim = add( 1U, 0U );
    assert( maxrtos_process_set_timing( victim, 20U, 5U ) == MAXRTOS_OK );

    /* The victim never gets the CPU (it stays READY): the deadline is
     * wall-clock, not a CPU budget. */
    ( void ) start_running( hog );
    assert( maxrtos_process_get( victim )->state == MAXRTOS_PROCESS_STATE_READY );
    ( void ) maxrtos_kernel_check_deadlines( &s_table, 5U );

    assert( maxrtos_kernel_take_deadline_miss( &missed ) == true );
    assert( missed == victim );

    printf( "test_a_starved_process_still_misses_its_deadline: PASS\n" );
}

static void test_misses_are_taken_lowest_id_first_and_skip_excluded( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t c;
    maxrtos_process_id_t d;
    maxrtos_process_id_t id;

    reset();
    a = add( 0U, 0U );
    b = add( 1U, 0U );
    c = add( 2U, 1U ); /* partition 1 */
    d = add( 3U, 0U );

    assert( maxrtos_process_set_timing( a, 0U, 3U ) == MAXRTOS_OK );
    assert( maxrtos_process_set_timing( b, 0U, 3U ) == MAXRTOS_OK );
    assert( maxrtos_process_set_timing( c, 0U, 3U ) == MAXRTOS_OK );
    assert( maxrtos_process_set_timing( d, 0U, 3U ) == MAXRTOS_OK );

    /* b is suspended and c's partition is halted: neither is supervised. */
    assert( maxrtos_process_set_state( b, MAXRTOS_PROCESS_STATE_SUSPENDED ) == MAXRTOS_OK );
    s_table.halted[ 1 ] = true;

    assert( maxrtos_kernel_check_deadlines( &s_table, 3U ) == 2U );

    assert( maxrtos_kernel_take_deadline_miss( &id ) == true );
    assert( id == a );
    assert( maxrtos_kernel_take_deadline_miss( &id ) == true );
    assert( id == d );
    assert( maxrtos_kernel_take_deadline_miss( &id ) == false );
    assert( maxrtos_process_get( b )->deadline_misses == 0U );
    assert( maxrtos_process_get( c )->deadline_misses == 0U );

    printf( "test_misses_are_taken_lowest_id_first_and_skip_excluded: PASS\n" );
}

static void test_periodic_wait_blocks_until_the_next_release( void )
{
    maxrtos_process_id_t p;
    maxrtos_process_id_t other;
    maxrtos_process_id_t next;
    maxrtos_process_control_block_t * pcb;

    reset();
    p = add( 0U, 0U );
    other = add( 1U, 0U );
    pcb = maxrtos_process_get( p );
    assert( maxrtos_process_set_timing( p, 10U, 6U ) == MAXRTOS_OK );

    ( void ) start_running( p );
    advance( 2U ); /* the process completes its release at tick 2 */

    next = MAXRTOS_INVALID_PROCESS_ID;
    assert( maxrtos_kernel_periodic_wait( &s_table, p, &next ) == MAXRTOS_PENDING );
    assert( next == other );
    assert( pcb->state == MAXRTOS_PROCESS_STATE_BLOCKED );
    assert( pcb->periodic_waiting == true );
    assert( pcb->wake_tick == 10U );
    assert( pcb->deadline_time == MAXRTOS_TICK_NONE ); /* met */
    assert( s_table.current_id[ 0 ] == other );

    /* Nothing happens before the release... */
    advance( 7U );
    assert( pcb->state == MAXRTOS_PROCESS_STATE_BLOCKED );

    /* ...and at tick 10 it is released with a fresh deadline. */
    advance( 1U );

    /* Released: runnable again (the same tick's dispatch may already have
     * picked it), with its result waiting to be delivered. */
    assert( ( pcb->state == MAXRTOS_PROCESS_STATE_READY ) ||
            ( pcb->state == MAXRTOS_PROCESS_STATE_RUNNING ) );
    assert( pcb->periodic_waiting == false );
    assert( pcb->release_time == 10U );
    assert( pcb->deadline_time == 16U );
    assert( pcb->ipc_result_pending == true );
    assert( pcb->ipc_result == MAXRTOS_OK );

    printf( "test_periodic_wait_blocks_until_the_next_release: PASS\n" );
}

static void test_a_lone_periodic_process_blocks_and_the_partition_idles( void )
{
    maxrtos_process_id_t p;
    maxrtos_process_id_t next;
    maxrtos_process_id_t dispatched;

    reset();
    p = add( 0U, 0U );
    assert( maxrtos_process_set_timing( p, 10U, 6U ) == MAXRTOS_OK );

    ( void ) start_running( p );
    advance( 1U );

    /* Nothing else is READY: it blocks anyway and the CPU is to idle. */
    assert( maxrtos_kernel_periodic_wait( &s_table, p, &next ) == MAXRTOS_PENDING );
    assert( next == MAXRTOS_INVALID_PROCESS_ID );
    assert( maxrtos_process_get( p )->state == MAXRTOS_PROCESS_STATE_BLOCKED );
    assert( s_table.current_id[ 0 ] == MAXRTOS_INVALID_PROCESS_ID );

    /* Nothing READY to dispatch until the release. */
    assert( maxrtos_partition_dispatch( &s_table, 0U, &dispatched ) ==
            MAXRTOS_ERR_QUEUE_EMPTY );

    advance( 9U ); /* tick 10 */
    assert( maxrtos_partition_dispatch( &s_table, 0U, &dispatched ) == MAXRTOS_OK );
    assert( dispatched == p );

    printf( "test_a_lone_periodic_process_blocks_and_the_partition_idles: PASS\n" );
}

static void test_periodic_wait_after_an_overrun_returns_at_once( void )
{
    maxrtos_process_id_t p;
    maxrtos_process_id_t next;
    maxrtos_process_control_block_t * pcb;

    reset();
    p = add( 0U, 0U );
    pcb = maxrtos_process_get( p );
    assert( maxrtos_process_set_timing( p, 10U, 6U ) == MAXRTOS_OK );

    ( void ) start_running( p );
    advance( 12U ); /* ran past the next release point (tick 10) */

    assert( maxrtos_kernel_periodic_wait( &s_table, p, &next ) == MAXRTOS_OK );
    assert( pcb->state == MAXRTOS_PROCESS_STATE_RUNNING );
    assert( pcb->release_time == 10U ); /* the release that was due */
    assert( pcb->deadline_time == 16U );

    printf( "test_periodic_wait_after_an_overrun_returns_at_once: PASS\n" );
}

static void test_periodic_wait_rejects_bad_callers( void )
{
    maxrtos_process_id_t p;
    maxrtos_process_id_t other;
    maxrtos_process_id_t next;

    reset();
    p = add( 0U, 0U );
    other = add( 1U, 0U );
    ( void ) other;
    ( void ) start_running( p );

    /* Aperiodic. */
    assert( maxrtos_kernel_periodic_wait( &s_table, p, &next ) == MAXRTOS_ERR_INVALID_STATE );

    /* Periodic but READY, not RUNNING. */
    assert( maxrtos_process_set_timing( other, 10U, 5U ) == MAXRTOS_OK );
    assert( maxrtos_kernel_periodic_wait( &s_table, other, &next ) == MAXRTOS_ERR_INVALID_STATE );

    assert( maxrtos_kernel_periodic_wait( NULL, p, &next ) == MAXRTOS_ERR_INVALID_ARG );
    assert( maxrtos_kernel_periodic_wait( &s_table, p, NULL ) == MAXRTOS_ERR_INVALID_ARG );
    assert( maxrtos_kernel_periodic_wait( &s_table, MAXRTOS_INVALID_PROCESS_ID, &next ) ==
            MAXRTOS_ERR_INVALID_ID );

    printf( "test_periodic_wait_rejects_bad_callers: PASS\n" );
}

static void test_release_is_exact_and_period_does_not_drift( void )
{
    maxrtos_process_id_t p;
    maxrtos_process_id_t other;
    maxrtos_process_id_t next;
    maxrtos_process_control_block_t * pcb;
    unsigned i;

    reset();
    p = add( 0U, 0U );
    other = add( 1U, 0U );
    ( void ) other;
    pcb = maxrtos_process_get( p );
    assert( maxrtos_process_set_timing( p, 10U, 6U ) == MAXRTOS_OK );

    ( void ) start_running( p );

    /* Complete each release at a varying point: the next release stays on
     * the 10-tick grid. */
    for( i = 0U; i < 20U; i++ )
    {
        advance( 1U + ( i % 5U ) );

        /* Ticks rotate the partition's processes; bring p back to RUNNING
         * before it completes its release. */
        while( pcb->state != MAXRTOS_PROCESS_STATE_RUNNING )
        {
            assert( maxrtos_partition_dispatch( &s_table, 0U, &next ) == MAXRTOS_OK );
        }

        assert( maxrtos_kernel_periodic_wait( &s_table, p, &next ) == MAXRTOS_PENDING );

        while( pcb->state == MAXRTOS_PROCESS_STATE_BLOCKED )
        {
            advance( 1U );
        }

        assert( pcb->release_time % 10U == 0U );
        assert( pcb->release_time == maxrtos_kernel_tick_now() );
    }

    printf( "test_release_is_exact_and_period_does_not_drift: PASS\n" );
}

static void test_restart_of_a_ready_process_keeps_it_queued_once( void )
{
    maxrtos_health_monitor_t hm;
    maxrtos_process_id_t running;
    maxrtos_process_id_t ready;
    maxrtos_hm_action_t action;
    size_t queued;

    reset();
    running = add( 0U, 0U );
    ready = add( 1U, 0U );
    ( void ) start_running( running );
    advance( 3U );

    /* Ticks rotate the two; take whichever is READY now as the victim. */
    if( maxrtos_process_get( running )->state != MAXRTOS_PROCESS_STATE_RUNNING )
    {
        maxrtos_process_id_t swap = running;

        running = ready;
        ready = swap;
    }

    assert( maxrtos_process_get( ready )->state == MAXRTOS_PROCESS_STATE_READY );
    assert( maxrtos_process_set_timing( ready, 0U, 5U ) == MAXRTOS_OK );

    assert( maxrtos_hm_init( &hm ) == MAXRTOS_OK );
    queued = maxrtos_scheduler_process_count( &s_table.scheduler_ctx[ 0 ] );

    assert( maxrtos_fault_recovery_handle( &hm, &s_table, ready,
                MAXRTOS_FAULT_DEADLINE_EXCEEDED, &action ) == MAXRTOS_OK );
    (void) action;
    assert( maxrtos_hm_set_policy( &hm, 0U, MAXRTOS_FAULT_DEADLINE_EXCEEDED,
                MAXRTOS_HM_ACTION_RESTART_PROCESS ) == MAXRTOS_OK );
    assert( maxrtos_fault_recovery_handle( &hm, &s_table, ready,
                MAXRTOS_FAULT_DEADLINE_EXCEEDED, &action ) == MAXRTOS_OK );
    assert( action == MAXRTOS_HM_ACTION_RESTART_PROCESS );

    /* Still READY, still queued exactly once, and a new release began. */
    assert( maxrtos_process_get( ready )->state == MAXRTOS_PROCESS_STATE_READY );
    assert( maxrtos_scheduler_process_count( &s_table.scheduler_ctx[ 0 ] ) == queued );
    assert( maxrtos_process_get( ready )->release_time == 3U );
    assert( maxrtos_process_get( ready )->deadline_time == 8U );
    assert( s_table.current_id[ 0 ] == running ); /* the running one is untouched */

    printf( "test_restart_of_a_ready_process_keeps_it_queued_once: PASS\n" );
}

static void test_restart_of_a_periodic_waiter_makes_it_ready_now( void )
{
    maxrtos_health_monitor_t hm;
    maxrtos_process_id_t p;
    maxrtos_process_id_t other;
    maxrtos_process_id_t next;
    maxrtos_hm_action_t action;
    maxrtos_process_control_block_t * pcb;

    reset();
    p = add( 0U, 0U );
    other = add( 1U, 0U );
    ( void ) other;
    pcb = maxrtos_process_get( p );
    assert( maxrtos_process_set_timing( p, 50U, 20U ) == MAXRTOS_OK );
    ( void ) start_running( p );
    advance( 2U );
    assert( maxrtos_kernel_periodic_wait( &s_table, p, &next ) == MAXRTOS_PENDING );

    assert( maxrtos_hm_init( &hm ) == MAXRTOS_OK );
    assert( maxrtos_hm_set_policy( &hm, 0U, MAXRTOS_FAULT_DEADLINE_EXCEEDED,
                MAXRTOS_HM_ACTION_RESTART_PROCESS ) == MAXRTOS_OK );
    assert( maxrtos_fault_recovery_handle( &hm, &s_table, p,
                MAXRTOS_FAULT_DEADLINE_EXCEEDED, &action ) == MAXRTOS_OK );

    assert( pcb->state == MAXRTOS_PROCESS_STATE_READY );
    assert( pcb->periodic_waiting == false );
    assert( pcb->wake_tick == MAXRTOS_TICK_NONE );
    assert( pcb->release_time == 2U );
    assert( pcb->deadline_time == 22U );

    printf( "test_restart_of_a_periodic_waiter_makes_it_ready_now: PASS\n" );
}

static void test_restart_of_an_ipc_waiter_leaves_the_wait_list( void )
{
    maxrtos_health_monitor_t hm;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t next;
    maxrtos_hm_action_t action;
    maxrtos_waitlist_t waitlist;
    maxrtos_ipc_operation_t op = { 0 };

    reset();
    a = add( 0U, 0U );
    b = add( 1U, 0U );
    ( void ) start_running( a );
    assert( maxrtos_waitlist_init( &waitlist ) == MAXRTOS_OK );
    op.kind = MAXRTOS_IPC_OP_QUEUE_RECEIVE;
    assert( maxrtos_ipc_block_current( &s_table, a, &waitlist, MAXRTOS_TICK_NONE, &op, &next ) ==
            MAXRTOS_OK );
    assert( next == b );
    assert( maxrtos_waitlist_count( &waitlist ) == 1U );

    assert( maxrtos_hm_init( &hm ) == MAXRTOS_OK );
    assert( maxrtos_hm_set_policy( &hm, 0U, MAXRTOS_FAULT_DEADLINE_EXCEEDED,
                MAXRTOS_HM_ACTION_RESTART_PROCESS ) == MAXRTOS_OK );
    assert( maxrtos_fault_recovery_handle( &hm, &s_table, a,
                MAXRTOS_FAULT_DEADLINE_EXCEEDED, &action ) == MAXRTOS_OK );

    assert( maxrtos_waitlist_count( &waitlist ) == 0U );
    assert( maxrtos_process_get( a )->state == MAXRTOS_PROCESS_STATE_READY );
    assert( maxrtos_process_get( a )->waitlist == NULL );
    assert( maxrtos_process_get( a )->ipc_operation.kind == MAXRTOS_IPC_OP_NONE );

    printf( "test_restart_of_an_ipc_waiter_leaves_the_wait_list: PASS\n" );
}

int main( void )
{
    test_set_timing_validates_and_arms();
    test_scheduler_start_rearms_from_tick_zero();
    test_deadline_miss_is_detected_exactly_at_the_deadline();
    test_a_starved_process_still_misses_its_deadline();
    test_misses_are_taken_lowest_id_first_and_skip_excluded();
    test_periodic_wait_blocks_until_the_next_release();
    test_a_lone_periodic_process_blocks_and_the_partition_idles();
    test_periodic_wait_after_an_overrun_returns_at_once();
    test_periodic_wait_rejects_bad_callers();
    test_release_is_exact_and_period_does_not_drift();
    test_restart_of_a_ready_process_keeps_it_queued_once();
    test_restart_of_a_periodic_waiter_makes_it_ready_now();
    test_restart_of_an_ipc_waiter_leaves_the_wait_list();

    printf( "all timing tests passed\n" );

    return 0;
}
