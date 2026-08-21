/**
 * @file test_process.c
 * @brief Unit tests for process lifecycle management.
 *
 * Verifies process creation, argument validation, process-pool
 * capacity, state transitions, partition assignment, and process
 * ID validation.
 *
 * Tests execute against the host build and do not require target
 * hardware.
 */

#include <assert.h>
#include <stdio.h>

#include "maxrtos/config.h"
#include "maxrtos/kernel/process.h"

static uint8_t s_stack_a[ 256U ];
static uint8_t s_stack_b[ 256U ];

static void dummy_entry( void * arg )
{
    ( void ) arg;
}

static void test_create_basic( void )
{
    maxrtos_process_id_t id;
    maxrtos_status_t status;
    maxrtos_process_control_block_t * pcb;

    maxrtos_process_pool_init();

    status = maxrtos_process_create(
        s_stack_a,
        sizeof( s_stack_a ),
        0U,
        5U,
        dummy_entry,
        NULL,
        &id );

    assert( status == MAXRTOS_OK );

    pcb = maxrtos_process_get( id );

    assert( pcb != NULL );
    assert( pcb->priority == 5U );
    assert( pcb->partition_id == 0U );
    assert( pcb->state == MAXRTOS_PROCESS_STATE_READY );
    assert( pcb->stack_base == s_stack_a );
    assert( pcb->stack_size == sizeof( s_stack_a ) );

    printf( "test_create_basic: PASS\n" );
}

static void test_create_rejects_bad_args( void )
{
    maxrtos_process_id_t id;

    maxrtos_process_pool_init();

    /* NULL stack storage shall be rejected. */
    assert( maxrtos_process_create(
                NULL,
                256U,
                0U,
                0U,
                dummy_entry,
                NULL,
                &id ) == MAXRTOS_ERR_INVALID_ARG );

    /* A zero-length stack shall be rejected. */
    assert( maxrtos_process_create(
                s_stack_a,
                0U,
                0U,
                0U,
                dummy_entry,
                NULL,
                &id ) == MAXRTOS_ERR_INVALID_ARG );

    /* A partition ID outside the supported range shall be rejected. */
    assert( maxrtos_process_create(
                s_stack_a,
                sizeof( s_stack_a ),
                MAXRTOS_MAX_PARTITIONS,
                0U,
                dummy_entry,
                NULL,
                &id ) == MAXRTOS_ERR_INVALID_ARG );

    /* A priority above the supported range shall be rejected. */
    assert( maxrtos_process_create(
                s_stack_a,
                sizeof( s_stack_a ),
                0U,
                MAXRTOS_MAX_PRIORITY + 1U,
                dummy_entry,
                NULL,
                &id ) == MAXRTOS_ERR_INVALID_ARG );

    /* A NULL entry function shall be rejected. */
    assert( maxrtos_process_create(
                s_stack_a,
                sizeof( s_stack_a ),
                0U,
                0U,
                NULL,
                NULL,
                &id ) == MAXRTOS_ERR_INVALID_ARG );

    /* A NULL output ID shall be rejected. */
    assert( maxrtos_process_create(
                s_stack_a,
                sizeof( s_stack_a ),
                0U,
                0U,
                dummy_entry,
                NULL,
                NULL ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_create_rejects_bad_args: PASS\n" );
}

static void test_pool_exhaustion( void )
{
    static uint8_t s_big_stacks[
        MAXRTOS_MAX_PROCESSES + 1U ][ 64U ];

    size_t i;
    maxrtos_process_id_t id;
    maxrtos_status_t status;

    maxrtos_process_pool_init();

    /* Allocate every available process slot. Each creation shall
     * succeed until the configured process limit is reached. */
    for( i = 0U; i < MAXRTOS_MAX_PROCESSES; i++ )
    {
        status = maxrtos_process_create(
            s_big_stacks[ i ],
            sizeof( s_big_stacks[ i ] ),
            0U,
            0U,
            dummy_entry,
            NULL,
            &id );

        assert( status == MAXRTOS_OK );
    }

    /* Creation shall fail once all process slots are allocated. */
    status = maxrtos_process_create(
        s_big_stacks[ MAXRTOS_MAX_PROCESSES ],
        sizeof( s_big_stacks[ MAXRTOS_MAX_PROCESSES ] ),
        0U,
        0U,
        dummy_entry,
        NULL,
        &id );

    assert( status == MAXRTOS_ERR_POOL_FULL );

    printf( "test_pool_exhaustion: PASS\n" );
}

static void test_set_state_and_invalid_id( void )
{
    maxrtos_process_id_t id;
    maxrtos_status_t status;
    maxrtos_process_control_block_t * pcb;

    maxrtos_process_pool_init();

    status = maxrtos_process_create(
        s_stack_b,
        sizeof( s_stack_b ),
        3U,
        0U,
        dummy_entry,
        NULL,
        &id );

    assert( status == MAXRTOS_OK );

    assert( maxrtos_process_set_state(
                id,
                MAXRTOS_PROCESS_STATE_RUNNING ) == MAXRTOS_OK );

    pcb = maxrtos_process_get( id );

    assert( pcb != NULL );
    assert( pcb->state == MAXRTOS_PROCESS_STATE_RUNNING );

    /* Invalid process IDs shall be rejected without accessing
     * process-pool storage outside the valid index range. */
    assert( maxrtos_process_get(
                MAXRTOS_INVALID_PROCESS_ID ) == NULL );

    assert( maxrtos_process_get(
                ( maxrtos_process_id_t ) 9999U ) == NULL );

    assert( maxrtos_process_set_state(
                ( maxrtos_process_id_t ) 9999U,
                MAXRTOS_PROCESS_STATE_READY ) == MAXRTOS_ERR_INVALID_ID );

    printf( "test_set_state_and_invalid_id: PASS\n" );
}

int main( void )
{
    test_create_basic();
    test_create_rejects_bad_args();
    test_pool_exhaustion();
    test_set_state_and_invalid_id();

    printf( "all process tests passed\n" );

    return 0;
}
