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

static void test_ready_process_rejects_bad_args( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t id;

    reset_all( &table );

    id = make_ready_process( 0U, 5U );

    assert(
        maxrtos_partition_ready_process(
            NULL,
            0U,
            id ) ==
        MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_partition_ready_process(
            &table,
            MAXRTOS_MAX_PARTITIONS,
            id ) ==
        MAXRTOS_ERR_INVALID_ARG );

    printf(
        "test_ready_process_rejects_bad_args: PASS\n" );
}

static void test_ready_process_rejects_invalid_id( void )
{
    maxrtos_partition_table_t table;

    reset_all( &table );

    assert(
        maxrtos_partition_ready_process(
            &table,
            0U,
            ( maxrtos_process_id_t ) 9999U ) ==
        MAXRTOS_ERR_INVALID_ID );

    printf(
        "test_ready_process_rejects_invalid_id: PASS\n" );
}

static void test_ready_process_sets_ready_and_adds_to_queue( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t id;
    maxrtos_process_control_block_t * pcb;

    reset_all( &table );

    id = make_ready_process( 0U, 5U );

    pcb = maxrtos_process_get( id );

    assert( pcb != NULL );

    /* make_ready_process() creates the process in READY state, so
     * move it to another state before testing the READY transition. */
    assert(
        maxrtos_process_set_state(
            id,
            MAXRTOS_PROCESS_STATE_BLOCKED ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_scheduler_process_count(
            &table.scheduler_ctx[ 0U ] ) == 0U );

    assert(
        maxrtos_partition_ready_process(
            &table,
            0U,
            id ) ==
        MAXRTOS_OK );

    assert(
        pcb->state ==
        MAXRTOS_PROCESS_STATE_READY );

    assert(
        maxrtos_scheduler_process_count(
            &table.scheduler_ctx[ 0U ] ) == 1U );

    printf(
        "test_ready_process_sets_ready_and_adds_to_queue: PASS\n" );
}

static void test_ready_process_uses_specified_partition_context( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t id;

    reset_all( &table );

    id = make_ready_process( 0U, 5U );

    assert(
        maxrtos_process_set_state(
            id,
            MAXRTOS_PROCESS_STATE_BLOCKED ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_partition_ready_process(
            &table,
            1U,
            id ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_scheduler_process_count(
            &table.scheduler_ctx[ 0U ] ) == 0U );

    assert(
        maxrtos_scheduler_process_count(
            &table.scheduler_ctx[ 1U ] ) == 1U );

    printf(
        "test_ready_process_uses_specified_partition_context: PASS\n" );
}

static void test_block_and_dispatch_rejects_bad_args( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t id;
    maxrtos_process_id_t out;

    reset_all( &table );

    id = make_ready_process( 0U, 5U );

    assert(
        maxrtos_partition_block_and_dispatch(
            NULL,
            0U,
            id,
            &out ) ==
        MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_partition_block_and_dispatch(
            &table,
            MAXRTOS_MAX_PARTITIONS,
            id,
            &out ) ==
        MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_partition_block_and_dispatch(
            &table,
            0U,
            id,
            NULL ) ==
        MAXRTOS_ERR_INVALID_ARG );

    printf(
        "test_block_and_dispatch_rejects_bad_args: PASS\n" );
}

static void test_block_and_dispatch_rejects_halted_partition( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t id;
    maxrtos_process_id_t out;

    reset_all( &table );

    id = make_ready_process( 0U, 5U );

    assert(
        maxrtos_partition_add_process(
            &table,
            id ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_partition_dispatch(
            &table,
            0U,
            &out ) ==
        MAXRTOS_OK );

    table.halted[ 0U ] = true;

    assert(
        maxrtos_partition_block_and_dispatch(
            &table,
            0U,
            id,
            &out ) ==
        MAXRTOS_ERR_PARTITION_HALTED );

    assert( table.current_id[ 0U ] == id );

    printf(
        "test_block_and_dispatch_rejects_halted_partition: PASS\n" );
}

static void test_block_and_dispatch_rejects_non_running_process( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t id;
    maxrtos_process_id_t out;

    reset_all( &table );

    id = make_ready_process( 0U, 5U );

    assert(
        maxrtos_partition_block_and_dispatch(
            &table,
            0U,
            id,
            &out ) ==
        MAXRTOS_ERR_INVALID_STATE );

    printf(
        "test_block_and_dispatch_rejects_non_running_process: PASS\n" );
}

static void test_block_and_dispatch_rejects_empty_ready_queue( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t id;
    maxrtos_process_id_t out;

    reset_all( &table );

    id = make_ready_process( 0U, 5U );

    assert(
        maxrtos_partition_add_process(
            &table,
            id ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_partition_dispatch(
            &table,
            0U,
            &out ) ==
        MAXRTOS_OK );

    assert( out == id );
    assert( table.current_id[ 0U ] == id );

    /* The current process is RUNNING and is not in the READY queue.
     * No other process is available, so blocking cannot proceed. */
    assert(
        maxrtos_partition_block_and_dispatch(
            &table,
            0U,
            id,
            &out ) ==
        MAXRTOS_ERR_QUEUE_EMPTY );

    assert( table.current_id[ 0U ] == id );

    printf(
        "test_block_and_dispatch_rejects_empty_ready_queue: PASS\n" );
}

static void test_block_and_dispatch_blocks_current_and_runs_next(
    void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t running_id;
    maxrtos_process_id_t next_id;
    maxrtos_process_id_t out;
    maxrtos_process_control_block_t * running_pcb;
    maxrtos_process_control_block_t * next_pcb;

    reset_all( &table );

    running_id = make_ready_process( 0U, 5U );
    next_id = make_ready_process( 0U, 2U );

    assert(
        maxrtos_partition_add_process(
            &table,
            running_id ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process(
            &table,
            next_id ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_partition_dispatch(
            &table,
            0U,
            &out ) ==
        MAXRTOS_OK );

    assert( out == next_id );

    running_pcb = maxrtos_process_get( running_id );
    next_pcb = maxrtos_process_get( next_id );

    assert( running_pcb != NULL );
    assert( next_pcb != NULL );

    assert(
        running_pcb->state ==
        MAXRTOS_PROCESS_STATE_READY );

    assert(
        next_pcb->state ==
        MAXRTOS_PROCESS_STATE_RUNNING );

    assert( table.current_id[ 0U ] == next_id );

    /* The running process is now in the ready queue. Blocking it
     * must select the remaining READY process. */
    assert(
        maxrtos_partition_block_and_dispatch(
            &table,
            0U,
            next_id,
            &out ) ==
        MAXRTOS_OK );

    assert( out == running_id );

    assert(
        next_pcb->state ==
        MAXRTOS_PROCESS_STATE_BLOCKED );

    assert(
        running_pcb->state ==
        MAXRTOS_PROCESS_STATE_RUNNING );

    assert( table.current_id[ 0U ] == running_id );

    assert(
        maxrtos_scheduler_process_count(
            &table.scheduler_ctx[ 0U ] ) == 0U );

    printf(
        "test_block_and_dispatch_blocks_current_and_runs_next: PASS\n" );
}

static void test_block_and_dispatch_does_not_change_other_partition(
    void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t p0;
    maxrtos_process_id_t p1;
    maxrtos_process_id_t out;

    reset_all( &table );

    p0 = make_ready_process( 0U, 5U );
    p1 = make_ready_process( 1U, 5U );

    assert(
        maxrtos_partition_add_process(
            &table,
            p0 ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process(
            &table,
            p1 ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_partition_dispatch(
            &table,
            0U,
            &out ) ==
        MAXRTOS_OK );

    assert( out == p0 );

    assert(
        maxrtos_partition_block_and_dispatch(
            &table,
            0U,
            p0,
            &out ) ==
        MAXRTOS_ERR_QUEUE_EMPTY );

    assert( table.current_id[ 0U ] == p0 );
    assert(
        table.current_id[ 1U ] ==
        MAXRTOS_INVALID_PROCESS_ID );

    assert(
        maxrtos_scheduler_process_count(
            &table.scheduler_ctx[ 1U ] ) == 1U );

    printf(
        "test_block_and_dispatch_does_not_change_other_partition: PASS\n" );
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

    test_ready_process_rejects_bad_args();
    test_ready_process_rejects_invalid_id();
    test_ready_process_sets_ready_and_adds_to_queue();
    test_ready_process_uses_specified_partition_context();

    test_block_and_dispatch_rejects_bad_args();
    test_block_and_dispatch_rejects_halted_partition();
    test_block_and_dispatch_rejects_non_running_process();
    test_block_and_dispatch_rejects_empty_ready_queue();
    test_block_and_dispatch_blocks_current_and_runs_next();
    test_block_and_dispatch_does_not_change_other_partition();

    printf( "all partition tests passed\n" );

    return 0;
}
