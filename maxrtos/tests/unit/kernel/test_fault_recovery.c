/**
 * @file test_fault_recovery.c
 * @brief Unit tests for the fault recovery module.
 */

#include <assert.h>
#include <stdio.h>

#include "maxrtos/kernel/fault_recovery.h"
#include "maxrtos/kernel/health_monitor.h"
#include "maxrtos/kernel/mutex.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"

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

static void reset_all(
    maxrtos_partition_table_t * table,
    maxrtos_health_monitor_t * hm )
{
    maxrtos_process_pool_init();
    assert( maxrtos_partition_table_init( table ) == MAXRTOS_OK );
    assert( maxrtos_hm_init( hm ) == MAXRTOS_OK );

    s_next_stack = 0U;
}

static void test_handle_rejects_null_args( void )
{
    maxrtos_partition_table_t table;
    maxrtos_health_monitor_t hm;
    maxrtos_hm_action_t action;

    reset_all( &table, &hm );

    assert( maxrtos_fault_recovery_handle(
                NULL,
                &table,
                0U,
                MAXRTOS_FAULT_BUS_ERROR,
                &action ) == MAXRTOS_ERR_INVALID_ARG );

    assert( maxrtos_fault_recovery_handle(
                &hm,
                NULL,
                0U,
                MAXRTOS_FAULT_BUS_ERROR,
                &action ) == MAXRTOS_ERR_INVALID_ARG );

    assert( maxrtos_fault_recovery_handle(
                &hm,
                &table,
                0U,
                MAXRTOS_FAULT_BUS_ERROR,
                NULL ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_handle_rejects_null_args: PASS\n" );
}

static void test_handle_rejects_unknown_process( void )
{
    maxrtos_partition_table_t table;
    maxrtos_health_monitor_t hm;
    maxrtos_hm_action_t action;

    reset_all( &table, &hm );

    assert( maxrtos_fault_recovery_handle(
                &hm,
                &table,
                ( maxrtos_process_id_t ) 9999,
                MAXRTOS_FAULT_BUS_ERROR,
                &action ) == MAXRTOS_ERR_INVALID_ID );

    printf( "test_handle_rejects_unknown_process: PASS\n" );
}

static void test_handle_ignore_leaves_state_untouched( void )
{
    maxrtos_partition_table_t table;
    maxrtos_health_monitor_t hm;
    maxrtos_process_id_t a;
    maxrtos_hm_action_t action;
    maxrtos_process_control_block_t * pcb;
    maxrtos_process_id_t out;

    reset_all( &table, &hm );

    a = make_ready_process( 0U, 5 );

    assert( maxrtos_partition_add_process(
                &table,
                a ) == MAXRTOS_OK );

    assert( maxrtos_partition_dispatch(
                &table,
                0U,
                &out ) == MAXRTOS_OK );

    assert( out == a );

    assert( maxrtos_hm_set_policy(
                &hm,
                0U,
                MAXRTOS_FAULT_BUS_ERROR,
                MAXRTOS_HM_ACTION_IGNORE ) == MAXRTOS_OK );

    assert( maxrtos_fault_recovery_handle(
                &hm,
                &table,
                a,
                MAXRTOS_FAULT_BUS_ERROR,
                &action ) == MAXRTOS_OK );

    assert( action == MAXRTOS_HM_ACTION_IGNORE );

    pcb = maxrtos_process_get( a );

    /* IGNORE must not modify process or partition state. */
    assert( pcb->state == MAXRTOS_PROCESS_STATE_RUNNING );
    assert( table.current_id[ 0 ] == a );

    printf( "test_handle_ignore_leaves_state_untouched: PASS\n" );
}

static void test_handle_halt_partition_sets_flag_and_blocks_dispatch( void )
{
    maxrtos_partition_table_t table;
    maxrtos_health_monitor_t hm;
    maxrtos_process_id_t a;
    maxrtos_hm_action_t action;
    maxrtos_process_id_t out;

    reset_all( &table, &hm );

    a = make_ready_process( 1U, 5 );

    assert( maxrtos_partition_add_process(
                &table,
                a ) == MAXRTOS_OK );

    assert( maxrtos_partition_dispatch(
                &table,
                1U,
                &out ) == MAXRTOS_OK );

    assert( out == a );

    assert( maxrtos_hm_set_policy(
                &hm,
                1U,
                MAXRTOS_FAULT_MEMORY_ACCESS,
                MAXRTOS_HM_ACTION_HALT_PARTITION ) == MAXRTOS_OK );

    assert( maxrtos_fault_recovery_handle(
                &hm,
                &table,
                a,
                MAXRTOS_FAULT_MEMORY_ACCESS,
                &action ) == MAXRTOS_OK );

    assert( action == MAXRTOS_HM_ACTION_HALT_PARTITION );
    assert( table.halted[ 1 ] == true );

    /* A halted partition must reject subsequent dispatches. */
    assert( maxrtos_partition_dispatch(
                &table,
                1U,
                &out ) == MAXRTOS_ERR_PARTITION_HALTED );

    printf( "test_handle_halt_partition_sets_flag_and_blocks_dispatch: PASS\n" );
}

static void test_handle_halt_does_not_affect_other_partitions( void )
{
    maxrtos_partition_table_t table;
    maxrtos_health_monitor_t hm;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_hm_action_t action;
    maxrtos_process_id_t out;

    reset_all( &table, &hm );

    a = make_ready_process( 0U, 5 );
    b = make_ready_process( 1U, 5 );

    assert( maxrtos_partition_add_process(
                &table,
                a ) == MAXRTOS_OK );

    assert( maxrtos_partition_add_process(
                &table,
                b ) == MAXRTOS_OK );

    assert( maxrtos_hm_set_policy(
                &hm,
                0U,
                MAXRTOS_FAULT_BUS_ERROR,
                MAXRTOS_HM_ACTION_HALT_PARTITION ) == MAXRTOS_OK );

    assert( maxrtos_fault_recovery_handle(
                &hm,
                &table,
                a,
                MAXRTOS_FAULT_BUS_ERROR,
                &action ) == MAXRTOS_OK );

    assert( table.halted[ 0 ] == true );
    assert( table.halted[ 1 ] == false );

    assert( maxrtos_partition_dispatch(
                &table,
                1U,
                &out ) == MAXRTOS_OK );

    assert( out == b );

    printf( "test_handle_halt_does_not_affect_other_partitions: PASS\n" );
}

static void test_handle_restart_process_resets_current_id_correctly( void )
{
    maxrtos_partition_table_t table;
    maxrtos_health_monitor_t hm;
    maxrtos_process_id_t a;
    maxrtos_hm_action_t action;
    maxrtos_process_id_t out;
    maxrtos_process_control_block_t * pcb;

    reset_all( &table, &hm );

    a = make_ready_process( 0U, 5 );

    assert( maxrtos_partition_add_process(
                &table,
                a ) == MAXRTOS_OK );

    assert( maxrtos_partition_dispatch(
                &table,
                0U,
                &out ) == MAXRTOS_OK );

    assert( out == a );
    assert( table.current_id[ 0 ] == a );

    assert( maxrtos_hm_set_policy(
                &hm,
                0U,
                MAXRTOS_FAULT_DIVIDE_BY_ZERO,
                MAXRTOS_HM_ACTION_RESTART_PROCESS ) == MAXRTOS_OK );

    assert( maxrtos_fault_recovery_handle(
                &hm,
                &table,
                a,
                MAXRTOS_FAULT_DIVIDE_BY_ZERO,
                &action ) == MAXRTOS_OK );

    assert( action == MAXRTOS_HM_ACTION_RESTART_PROCESS );

    pcb = maxrtos_process_get( a );

    assert( pcb->state == MAXRTOS_PROCESS_STATE_READY );
    assert( table.current_id[ 0 ] == MAXRTOS_INVALID_PROCESS_ID );

    /* The restarted process must be dispatchable again. */
    assert( maxrtos_partition_dispatch(
                &table,
                0U,
                &out ) == MAXRTOS_OK );

    assert( out == a );

    printf( "test_handle_restart_process_resets_current_id_correctly: PASS\n" );
}

/* Common setup: L (priority 9) holds a mutex, H (priority 1) waits for it,
 * so L runs at priority 1 and H is BLOCKED. */
static void contend(
    maxrtos_partition_table_t * table,
    maxrtos_health_monitor_t * hm,
    maxrtos_mutex_id_t * m,
    maxrtos_process_id_t * l,
    maxrtos_process_id_t * h )
{
    maxrtos_process_id_t out;

    reset_all( table, hm );
    maxrtos_mutex_pool_init();
    assert( maxrtos_mutex_create( 0U, m ) == MAXRTOS_OK );
    assert( maxrtos_hm_set_policy(
                hm, 0U, MAXRTOS_FAULT_DIVIDE_BY_ZERO,
                MAXRTOS_HM_ACTION_RESTART_PROCESS ) == MAXRTOS_OK );

    *l = make_ready_process( 0U, 9U );
    assert( maxrtos_partition_add_process( table, *l ) == MAXRTOS_OK );
    assert( maxrtos_partition_dispatch( table, 0U, &out ) == MAXRTOS_OK );
    assert( maxrtos_kernel_mutex_lock(
                *m, MAXRTOS_TIMEOUT_INFINITE, table, *l, &out ) ==
            MAXRTOS_OK );

    *h = make_ready_process( 0U, 1U );
    assert( maxrtos_partition_add_process( table, *h ) == MAXRTOS_OK );
    assert( maxrtos_partition_dispatch( table, 0U, &out ) == MAXRTOS_OK );
    assert( out == *h );
    assert( maxrtos_kernel_mutex_lock(
                *m, MAXRTOS_TIMEOUT_INFINITE, table, *h, &out ) ==
            MAXRTOS_PENDING );
    assert( out == *l );
    assert( maxrtos_process_get( *l )->priority == 1U );
}

static void test_restart_of_waiter_drops_holder_boost( void )
{
    maxrtos_partition_table_t table;
    maxrtos_health_monitor_t hm;
    maxrtos_mutex_id_t m;
    maxrtos_process_id_t l;
    maxrtos_process_id_t h;
    maxrtos_hm_action_t action;

    contend( &table, &hm, &m, &l, &h );

    assert( maxrtos_fault_recovery_handle(
                &hm, &table, h, MAXRTOS_FAULT_DIVIDE_BY_ZERO, &action ) ==
            MAXRTOS_OK );

    assert( maxrtos_process_get( h )->state == MAXRTOS_PROCESS_STATE_READY );
    assert( maxrtos_kernel_mutex_owner( m ) == l );
    assert( maxrtos_process_get( l )->priority == 9U );

    printf( "test_restart_of_waiter_drops_holder_boost: PASS\n" );
}

static void test_restart_of_boosted_holder_restores_base_priority( void )
{
    maxrtos_partition_table_t table;
    maxrtos_health_monitor_t hm;
    maxrtos_mutex_id_t m;
    maxrtos_process_id_t l;
    maxrtos_process_id_t h;
    maxrtos_hm_action_t action;

    contend( &table, &hm, &m, &l, &h );

    assert( maxrtos_fault_recovery_handle(
                &hm, &table, l, MAXRTOS_FAULT_DIVIDE_BY_ZERO, &action ) ==
            MAXRTOS_OK );

    /* The mutex went to h; l is back in the queue at its own priority. */
    assert( maxrtos_kernel_mutex_owner( m ) == h );
    assert( maxrtos_process_get( l )->state == MAXRTOS_PROCESS_STATE_READY );
    assert( maxrtos_process_get( l )->priority == 9U );

    printf( "test_restart_of_boosted_holder_restores_base_priority: PASS\n" );
}

int main( void )
{
    test_handle_rejects_null_args();
    test_handle_rejects_unknown_process();
    test_handle_ignore_leaves_state_untouched();
    test_handle_halt_partition_sets_flag_and_blocks_dispatch();
    test_handle_halt_does_not_affect_other_partitions();
    test_handle_restart_process_resets_current_id_correctly();

    test_restart_of_waiter_drops_holder_boost();
    test_restart_of_boosted_holder_restores_base_priority();

    printf( "all fault recovery tests passed\n" );

    return 0;
}
