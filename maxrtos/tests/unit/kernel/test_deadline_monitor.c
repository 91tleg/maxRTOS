/**
 * @file test_deadline_monitor.c
 * @brief Unit tests for deadline monitoring.
 */

#include <assert.h>
#include <stdio.h>

#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/deadline_monitor.h"

#define TEST_STACK_SIZE ( 128U )

static uint8_t s_stack_a[ TEST_STACK_SIZE ];

static void dummy_entry( void * arg )
{
    ( void ) arg;
}

static maxrtos_process_id_t create_test_process( void )
{
    maxrtos_process_id_t id;

    maxrtos_process_pool_init();

    assert(
        maxrtos_process_create(
            s_stack_a,
            TEST_STACK_SIZE,
            ( maxrtos_partition_id_t ) 0U,
            5U,
            dummy_entry,
            NULL,
            &id ) == MAXRTOS_OK );

    return id;
}

static void test_init_rejects_null( void )
{
    assert(
        maxrtos_deadline_monitor_init( 
            NULL ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_init_rejects_null: PASS\n" );
}

static void test_init_marks_every_process_unconfigured( void )
{
    maxrtos_deadline_monitor_t monitor;
    size_t process_id;

    assert(
        maxrtos_deadline_monitor_init( &monitor ) == MAXRTOS_OK );

    for( process_id = 0U;
         process_id < MAXRTOS_MAX_PROCESSES;
         process_id++ )
    {
        assert( monitor.configured[ process_id ] == false );
    }

    printf(
        "test_init_marks_every_process_unconfigured: PASS\n" );
}

static void test_set_budget_rejects_bad_args( void )
{
    maxrtos_deadline_monitor_t monitor;
    maxrtos_process_id_t id;

    id = create_test_process();

    assert(
        maxrtos_deadline_monitor_init( &monitor ) == MAXRTOS_OK );

    /* NULL monitor. */
    assert(
        maxrtos_deadline_monitor_set_budget(
            NULL,
            id,
            10U,
            5U,
            0U ) == MAXRTOS_ERR_INVALID_ARG );

    /* Invalid process ID (not allocated). */
    assert(
        maxrtos_deadline_monitor_set_budget(
            &monitor,
            (maxrtos_process_id_t) 0xFFFFU,
            10U,
            5U,
            0U ) == MAXRTOS_ERR_INVALID_ARG );

    /* period_ticks == 0. */
    assert(
        maxrtos_deadline_monitor_set_budget(
            &monitor,
            id,
            0U,
            5U,
            0U ) == MAXRTOS_ERR_INVALID_ARG );

    /* time_capacity_ticks == 0. */
    assert(
        maxrtos_deadline_monitor_set_budget(
            &monitor,
            id,
            10U,
            0U,
            0U ) == MAXRTOS_ERR_INVALID_ARG );

    /* time_capacity_ticks > period_ticks. */
    assert(
        maxrtos_deadline_monitor_set_budget(
            &monitor,
            id,
            10U,
            11U,
            0U ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_set_budget_rejects_bad_args: PASS\n" );
}

static void test_set_budget_computes_first_period_boundary( void )
{
    maxrtos_deadline_monitor_t monitor;
    maxrtos_process_id_t id;

    id = create_test_process();

    assert(
        maxrtos_deadline_monitor_init( &monitor ) == MAXRTOS_OK );

    assert(
        maxrtos_deadline_monitor_set_budget(
            &monitor,
            id,
            10U,
            4U,
            100U ) == MAXRTOS_OK );

    assert( monitor.configured[ id ] == true );
    assert( monitor.period_ticks[ id ] == 10U );
    assert( monitor.time_capacity_ticks[ id ] == 4U );
    assert( monitor.accumulated_ticks[ id ] == 0U );

    assert( monitor.next_period_tick[ id ] == 110U );

    printf(
        "test_set_budget_computes_first_period_boundary: PASS\n" );
}

static void test_record_running_rejects_bad_args( void )
{
    maxrtos_deadline_monitor_t monitor;
    maxrtos_process_id_t id;
    bool violated;

    id = create_test_process();

    assert(
        maxrtos_deadline_monitor_init( &monitor ) == MAXRTOS_OK );

    assert(
        maxrtos_deadline_monitor_record_running(
            NULL,
            id,
            0U,
            &violated ) == MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_deadline_monitor_record_running(
            &monitor,
            id,
            0U,
            NULL ) == MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_deadline_monitor_record_running(
            &monitor,
            ( maxrtos_process_id_t ) 0xFFFFU,
            0U,
            &violated ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_record_running_rejects_bad_args: PASS\n" );
}

static void test_record_running_on_unconfigured_process_is_safe_noop(
    void )
{
    maxrtos_deadline_monitor_t monitor;
    maxrtos_process_id_t id;
    bool violated;

    id = create_test_process();

    assert(
        maxrtos_deadline_monitor_init( &monitor ) == MAXRTOS_OK );

    violated = true;

    assert(
        maxrtos_deadline_monitor_record_running(
            &monitor,
            id,
            0U,
            &violated ) == MAXRTOS_OK );

    assert( violated == false );

    printf(
        "test_record_running_on_unconfigured_process_is_safe_noop: "
        "PASS\n" );
}

static void test_accumulates_and_detects_violation_within_period(
    void )
{
    maxrtos_deadline_monitor_t monitor;
    maxrtos_process_id_t id;
    bool violated;

    id = create_test_process();

    assert(
        maxrtos_deadline_monitor_init( &monitor ) == MAXRTOS_OK );

    assert(
        maxrtos_deadline_monitor_set_budget(
            &monitor,
            id,
            10U,
            3U,
            0U ) == MAXRTOS_OK );

    /* Ticks 1, 2, 3 within budget. */
    assert(
        maxrtos_deadline_monitor_record_running(
            &monitor,
            id,
            1U,
            &violated ) == MAXRTOS_OK );
    assert( violated == false );

    assert(
        maxrtos_deadline_monitor_record_running(
            &monitor,
            id,
            2U,
            &violated ) == MAXRTOS_OK );
    assert( violated == false );

    assert(
        maxrtos_deadline_monitor_record_running(
            &monitor,
            id,
            3U,
            &violated ) == MAXRTOS_OK );
    assert( violated == false );
    assert( monitor.accumulated_ticks[ id ] == 3U );

    /* 4th tick within the same period exceeds the budget of 3. */
    assert(
        maxrtos_deadline_monitor_record_running(
            &monitor,
            id,
            4U,
            &violated ) == MAXRTOS_OK );
    assert( violated == true );
    assert( monitor.accumulated_ticks[ id ] == 4U );

    printf(
        "test_accumulates_and_detects_violation_within_period: "
        "PASS\n" );
}

static void test_period_boundary_resets_accumulated_usage( void )
{
    maxrtos_deadline_monitor_t monitor;
    maxrtos_process_id_t id;
    bool violated;

    id = create_test_process();

    assert(
        maxrtos_deadline_monitor_init( &monitor ) == MAXRTOS_OK );

    assert(
        maxrtos_deadline_monitor_set_budget(
            &monitor,
            id,
            5U,
            2U, 
            0U ) == MAXRTOS_OK );

    /* Use exactly the budget in the first period: ticks 1, 2. */
    assert(
        maxrtos_deadline_monitor_record_running(
            &monitor,
            id,
            1U,
            &violated ) == MAXRTOS_OK );
    assert( violated == false );

    assert(
        maxrtos_deadline_monitor_record_running(
            &monitor,
            id,
            2U,
            &violated ) == MAXRTOS_OK );
    assert( violated == false );

    assert(
        maxrtos_deadline_monitor_record_running(
            &monitor,
            id,
            5U,
            &violated ) == MAXRTOS_OK );
    assert( violated == false );
    assert( monitor.accumulated_ticks[ id ] == 1U );

    /* next_period_tick must have advanced by one full period. */
    assert( monitor.next_period_tick[ id ] == 10U );

    /* ticks 6 and 7 bring the new period's usage to 3, exceeding
     * the budget of 2 again. */
    assert(
        maxrtos_deadline_monitor_record_running(
            &monitor,
            id,
            6U,
            &violated ) == MAXRTOS_OK );
    assert( violated == false );

    assert(
        maxrtos_deadline_monitor_record_running(
            &monitor,
            id,
            7U,
            &violated ) == MAXRTOS_OK );
    assert( violated == true );

    printf(
        "test_period_boundary_resets_accumulated_usage: PASS\n" );
}

static void test_two_processes_tracked_independently( void )
{
    maxrtos_deadline_monitor_t monitor;
    maxrtos_process_id_t id_a;
    maxrtos_process_id_t id_b;
    bool violated;
    static uint8_t stack_b[ TEST_STACK_SIZE ];

    maxrtos_process_pool_init();

    assert(
        maxrtos_process_create(
            s_stack_a,
            TEST_STACK_SIZE,
            ( maxrtos_partition_id_t ) 0U,
            5U,
            dummy_entry,
            NULL,
            &id_a ) == MAXRTOS_OK );

    assert(
        maxrtos_process_create(
            stack_b,
            TEST_STACK_SIZE,
            ( maxrtos_partition_id_t ) 1U,
            5U,
            dummy_entry,
            NULL,
            &id_b ) == MAXRTOS_OK );

    assert(
        maxrtos_deadline_monitor_init( &monitor ) == MAXRTOS_OK );

    assert(
        maxrtos_deadline_monitor_set_budget(
            &monitor, id_a, 10U, 1U, 0U ) == MAXRTOS_OK );

    assert(
        maxrtos_deadline_monitor_set_budget(
            &monitor, id_b, 10U, 9U, 0U ) == MAXRTOS_OK );

    /* Run B for 5 ticks, must not affect A's tracking at all. */
    {
        uint32_t tick;

        for( tick = 1U; tick <= 5U; tick++ )
        {
            assert(
                maxrtos_deadline_monitor_record_running(
                    &monitor,
                    id_b,
                    tick,
                    &violated ) == MAXRTOS_OK );
            assert( violated == false );
        }
    }

    assert( monitor.accumulated_ticks[ id_a ] == 0U );

    /* Run A for 2 ticks, exceeds ITS budget of 1. */
    assert(
        maxrtos_deadline_monitor_record_running(
            &monitor, id_a, 6U, &violated ) == MAXRTOS_OK );
    assert( violated == false );

    assert(
        maxrtos_deadline_monitor_record_running(
            &monitor, id_a, 7U, &violated ) == MAXRTOS_OK );
    assert( violated == true );

    assert( monitor.accumulated_ticks[ id_b ] == 5U );

    printf( "test_two_processes_tracked_independently: PASS\n" );
}

int main( void )
{
    test_init_rejects_null();
    test_init_marks_every_process_unconfigured();
    test_set_budget_rejects_bad_args();
    test_set_budget_computes_first_period_boundary();
    test_record_running_rejects_bad_args();
    test_record_running_on_unconfigured_process_is_safe_noop();
    test_accumulates_and_detects_violation_within_period();
    test_period_boundary_resets_accumulated_usage();
    test_two_processes_tracked_independently();

    printf( "all deadline monitor tests passed\n" );

    return 0;
}
