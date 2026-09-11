/**
 * @file test_partition.c
 * @brief Host-side unit tests for the partition table module.
 */

#include <assert.h>
#include <stdio.h>

#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/partition.h"

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
        true,
        dummy_entry,
        NULL,
        &id );

    assert( status == MAXRTOS_OK );

    s_next_stack++;

    return id;
}

static void reset_all( maxrtos_partition_table_t * table )
{
    maxrtos_process_pool_init();

    assert( maxrtos_partition_table_init( table ) == MAXRTOS_OK );

    s_next_stack = 0U;
}

static void test_table_init_rejects_null( void )
{
    assert(
        maxrtos_partition_table_init( NULL ) ==
        MAXRTOS_ERR_INVALID_ARG );

    printf( "test_table_init_rejects_null: PASS\n" );
}

static void test_table_init_resets_current_id_for_every_partition( void )
{
    maxrtos_partition_table_t table;
    maxrtos_partition_id_t partition_id;

    reset_all( &table );

    for( partition_id = 0U;
         partition_id < MAXRTOS_MAX_PARTITIONS;
         partition_id++ )
    {
        assert(
            table.current_id[ partition_id ] ==
            MAXRTOS_INVALID_PROCESS_ID );
    }

    printf(
        "test_table_init_resets_current_id_for_every_partition: PASS\n" );
}

static void test_add_process_rejects_null_table_and_bad_id( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t id;

    reset_all( &table );

    id = make_ready_process( 0U, 5U );

    assert(
        maxrtos_partition_add_process( NULL, id ) ==
        MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_partition_add_process(
            &table,
            ( maxrtos_process_id_t ) 9999U ) ==
        MAXRTOS_ERR_INVALID_ID );

    printf(
        "test_add_process_rejects_null_table_and_bad_id: PASS\n" );
}

static void test_add_process_routes_to_own_partition_context( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t p0_proc;
    maxrtos_process_id_t p1_proc;

    reset_all( &table );

    p0_proc = make_ready_process( 0U, 5U );
    p1_proc = make_ready_process( 1U, 5U );

    assert(
        maxrtos_partition_add_process( &table, p0_proc ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process( &table, p1_proc ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_scheduler_process_count(
            &table.scheduler_ctx[ 0U ] ) == 1U );

    assert(
        maxrtos_scheduler_process_count(
            &table.scheduler_ctx[ 1U ] ) == 1U );

    printf(
        "test_add_process_routes_to_own_partition_context: PASS\n" );
}

static void test_dispatch_rejects_bad_args( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t out;

    reset_all( &table );

    assert(
        maxrtos_partition_dispatch( NULL, 0U, &out ) ==
        MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_partition_dispatch(
            &table,
            MAXRTOS_MAX_PARTITIONS,
            &out ) ==
        MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_partition_dispatch(
            &table,
            0U,
            NULL ) ==
        MAXRTOS_ERR_INVALID_ARG );

    printf( "test_dispatch_rejects_bad_args: PASS\n" );
}

static void test_dispatch_boot_case_and_current_id_persists( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t process_id;
    maxrtos_process_id_t out;

    reset_all( &table );

    process_id = make_ready_process( 2U, 5U );

    assert(
        maxrtos_partition_add_process(
            &table,
            process_id ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_partition_dispatch(
            &table,
            2U,
            &out ) ==
        MAXRTOS_OK );

    assert( out == process_id );
    assert( table.current_id[ 2U ] == process_id );

    assert(
        maxrtos_partition_dispatch(
            &table,
            2U,
            &out ) ==
        MAXRTOS_OK );

    assert( out == process_id );

    printf(
        "test_dispatch_boot_case_and_current_id_persists: PASS\n" );
}

static void test_dispatch_current_id_is_updated_after_handoff( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t out;

    reset_all( &table );

    a = make_ready_process( 0U, 5U );
    b = make_ready_process( 0U, 2U );

    assert(
        maxrtos_partition_add_process( &table, a ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process( &table, b ) ==
        MAXRTOS_OK );

    /* Higher-priority process runs first. */
    assert(
        maxrtos_partition_dispatch(
            &table,
            0U,
            &out ) ==
        MAXRTOS_OK );

    assert( out == b );
    assert( table.current_id[ 0U ] == b );

    /* Remaining READY process runs next. */
    assert(
        maxrtos_partition_dispatch(
            &table,
            0U,
            &out ) ==
        MAXRTOS_OK );

    assert( out == a );
    assert( table.current_id[ 0U ] == a );

    printf(
        "test_dispatch_current_id_is_updated_after_handoff: PASS\n" );
}

static void test_dispatch_rejects_halted_partition( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t process_id;
    maxrtos_process_id_t out;

    reset_all( &table );

    process_id = make_ready_process( 0U, 5U );

    assert(
        maxrtos_partition_add_process(
            &table,
            process_id ) ==
        MAXRTOS_OK );

    table.halted[ 0U ] = true;

    assert(
        maxrtos_partition_dispatch(
            &table,
            0U,
            &out ) ==
        MAXRTOS_ERR_PARTITION_HALTED );

    assert(
        table.current_id[ 0U ] ==
        MAXRTOS_INVALID_PROCESS_ID );

    printf(
        "test_dispatch_rejects_halted_partition: PASS\n" );
}

static void test_dispatch_partitions_are_independent( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t p0_proc;
    maxrtos_process_id_t p1_proc;
    maxrtos_process_id_t out;

    reset_all( &table );

    p0_proc = make_ready_process( 0U, 5U );
    p1_proc = make_ready_process( 1U, 5U );

    assert(
        maxrtos_partition_add_process(
            &table,
            p0_proc ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process(
            &table,
            p1_proc ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_partition_dispatch(
            &table,
            0U,
            &out ) ==
        MAXRTOS_OK );

    assert( out == p0_proc );

    assert(
        table.current_id[ 1U ] ==
        MAXRTOS_INVALID_PROCESS_ID );

    assert(
        maxrtos_scheduler_process_count(
            &table.scheduler_ctx[ 1U ] ) == 1U );

    printf(
        "test_dispatch_partitions_are_independent: PASS\n" );
}

int main( void )
{
    test_table_init_rejects_null();
    test_table_init_resets_current_id_for_every_partition();
    test_add_process_rejects_null_table_and_bad_id();
    test_add_process_routes_to_own_partition_context();
    test_dispatch_rejects_bad_args();
    test_dispatch_boot_case_and_current_id_persists();
    test_dispatch_current_id_is_updated_after_handoff();
    test_dispatch_rejects_halted_partition();
    test_dispatch_partitions_are_independent();

    printf( "all partition tests passed\n" );

    return 0;
}
