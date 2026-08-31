/**
 * @file test_yield.c
 * @brief Unit tests for the kernel-level voluntary yield decision.
 */

#include <assert.h>
#include <stdio.h>
#include <stddef.h>

#include "maxrtos/kernel/yield.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"

#define TEST_STACK_SIZE ( 128U )

static uint8_t s_stack_a[ TEST_STACK_SIZE ];
static uint8_t s_stack_b[ TEST_STACK_SIZE ];

static void dummy_entry( void * arg )
{
    ( void ) arg;
}

static void test_rejects_null_table( void )
{
    maxrtos_process_id_t next_id;
    bool switch_needed;

    assert(
        maxrtos_kernel_yield(
            NULL,
            0U,
            0U,
            &next_id,
            &switch_needed ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_rejects_null_table: PASS\n" );
}

static void test_rejects_null_out_params( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t next_id;
    bool switch_needed;

    assert( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    assert(
        maxrtos_kernel_yield(
            &table,
            0U,
            0U,
            NULL,
            &switch_needed ) == MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_kernel_yield(
            &table,
            0U,
            0U,
            &next_id,
            NULL ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_rejects_null_out_params: PASS\n" );
}

static void test_propagates_dispatch_failure( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t next_id;
    bool switch_needed;

    assert( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    /* No processes registered with this partition at all, so
     * dispatch is expected to fail. */
    assert(
        maxrtos_kernel_yield(
            &table,
            0U,
            0U,
            &next_id,
            &switch_needed ) != MAXRTOS_OK );

    printf( "test_propagates_dispatch_failure: PASS\n" );
}

static void test_yield_with_sole_ready_process_reports_no_switch(
    void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t id_a;
    maxrtos_process_control_block_t * pcb_a;
    maxrtos_process_id_t next_id;
    bool switch_needed;

    maxrtos_process_pool_init();
    assert( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    assert(
        maxrtos_process_create(
            s_stack_a,
            TEST_STACK_SIZE,
            (maxrtos_partition_id_t) 0U,
            5U,
            dummy_entry,
            NULL,
            &id_a ) == MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process(
            &table,
            id_a ) == MAXRTOS_OK );

    pcb_a = maxrtos_process_get( id_a );
    assert( pcb_a != NULL );

    assert(
        maxrtos_kernel_yield(
            &table,
            0U,
            id_a,
            &next_id,
            &switch_needed ) == MAXRTOS_OK );

    /* Only one READY process exists in the partition, so yielding
     * must select the calling process itself. */
    assert( next_id == id_a );
    assert( switch_needed == false );

    printf(
        "test_yield_with_sole_ready_process_reports_no_switch: PASS\n" );
}

static void test_switch_needed_matches_next_id_vs_current_id( void )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t id_a;
    maxrtos_process_id_t next_id;
    bool switch_needed;

    maxrtos_process_pool_init();
    assert( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    assert(
        maxrtos_process_create(
            s_stack_a,
            TEST_STACK_SIZE,
            (maxrtos_partition_id_t) 0U,
            5U,
            dummy_entry,
            NULL,
            &id_a ) == MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process(
            &table,
            id_a ) == MAXRTOS_OK );

    assert(
        maxrtos_kernel_yield(
            &table,
            0U,
            id_a,
            &next_id,
            &switch_needed ) == MAXRTOS_OK );

    assert( switch_needed == ( next_id != id_a ) );

    printf(
        "test_switch_needed_matches_next_id_vs_current_id: PASS\n" );
}

int main( void )
{
    test_rejects_null_table();
    test_rejects_null_out_params();
    test_propagates_dispatch_failure();
    test_yield_with_sole_ready_process_reports_no_switch();
    test_switch_needed_matches_next_id_vs_current_id();

    printf( "all yield tests passed\n" );

    return 0;
}
