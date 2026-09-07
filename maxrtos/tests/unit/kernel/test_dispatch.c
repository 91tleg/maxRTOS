/**
 * @file test_dispatch.c
 * @brief Unit tests for kernel dispatch behavior.
 *
 * Verifies process selection, process-state transitions, scheduler
 * queue membership, argument validation, and current-process
 * validation.
 *
 * Tests execute against the host build and do not require
 * target hardware.
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/scheduler.h"
#include "maxrtos/kernel/dispatch.h"

static uint8_t s_stacks[ MAXRTOS_MAX_PROCESSES ][ 64U ];
static size_t s_next_stack;
static maxrtos_scheduler_context_t s_ctx;

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

static void reset_all( void )
{
    maxrtos_process_pool_init();

    assert(
        maxrtos_scheduler_init( &s_ctx ) == MAXRTOS_OK );

    s_next_stack = 0U;
}

static void test_dispatch_rejects_null_ctx( void )
{
    maxrtos_process_id_t out;

    reset_all();

    assert(
        maxrtos_kernel_dispatch(
            NULL,
            MAXRTOS_INVALID_PROCESS_ID,
            &out ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_dispatch_rejects_null_ctx: PASS\n" );
}

static void test_dispatch_rejects_null_out( void )
{
    reset_all();

    assert(
        maxrtos_kernel_dispatch(
            &s_ctx,
            MAXRTOS_INVALID_PROCESS_ID,
            NULL ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_dispatch_rejects_null_out: PASS\n" );
}

static void test_dispatch_empty_queue( void )
{
    maxrtos_process_id_t out;

    reset_all();

    assert(
        maxrtos_kernel_dispatch(
            &s_ctx,
            MAXRTOS_INVALID_PROCESS_ID,
            &out ) == MAXRTOS_ERR_QUEUE_EMPTY );

    printf( "test_dispatch_empty_queue: PASS\n" );
}

static void test_boot_dispatch_no_prior_current( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t out;
    maxrtos_process_control_block_t * pcb;

    reset_all();

    a = make_ready_process( 0U, 5U );

    assert(
        maxrtos_scheduler_add_process( &s_ctx, a ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_kernel_dispatch(
            &s_ctx,
            MAXRTOS_INVALID_PROCESS_ID,
            &out ) == MAXRTOS_OK );

    assert( out == a );

    pcb = maxrtos_process_get( a );

    assert( pcb != NULL );
    assert( pcb->state == MAXRTOS_PROCESS_STATE_RUNNING );
    assert( maxrtos_scheduler_process_count( &s_ctx ) == 0U );

    printf( "test_boot_dispatch_no_prior_current: PASS\n" );
}

static void test_current_continues_when_nothing_else_ready( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t out;
    maxrtos_process_control_block_t * pcb;

    reset_all();

    a = make_ready_process( 0U, 5U );

    assert(
        maxrtos_scheduler_add_process( &s_ctx, a ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_kernel_dispatch(
            &s_ctx,
            MAXRTOS_INVALID_PROCESS_ID,
            &out ) == MAXRTOS_OK );

    assert( out == a );

    /* No other process is ready, so the currently running process
     * continues to execute. */
    assert(
        maxrtos_kernel_dispatch(
            &s_ctx,
            a,
            &out ) == MAXRTOS_OK );

    assert( out == a );

    pcb = maxrtos_process_get( a );

    assert( pcb != NULL );
    assert( pcb->state == MAXRTOS_PROCESS_STATE_RUNNING );
    assert( maxrtos_scheduler_process_count( &s_ctx ) == 0U );

    printf(
        "test_current_continues_when_nothing_else_ready: PASS\n" );
}

static void test_dispatch_rejects_current_id_not_running( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t out;

    reset_all();

    a = make_ready_process( 0U, 5U );

    assert(
        maxrtos_scheduler_add_process( &s_ctx, a ) ==
        MAXRTOS_OK );

    /* A process supplied as the current process must actually be
     * in the RUNNING state. */
    assert(
        maxrtos_kernel_dispatch(
            &s_ctx,
            a,
            &out ) == MAXRTOS_ERR_INVALID_STATE );

    printf(
        "test_dispatch_rejects_current_id_not_running: PASS\n" );
}

static void test_dispatch_rejects_unknown_current_id( void )
{
    maxrtos_process_id_t out;

    reset_all();

    assert(
        maxrtos_kernel_dispatch(
            &s_ctx,
            ( maxrtos_process_id_t ) 9999U,
            &out ) == MAXRTOS_ERR_INVALID_ID );

    printf(
        "test_dispatch_rejects_unknown_current_id: PASS\n" );
}

static void test_normal_handoff_between_two_processes( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t out;
    maxrtos_process_control_block_t * pcb_a;
    maxrtos_process_control_block_t * pcb_b;

    reset_all();

    a = make_ready_process( 0U, 5U );
    b = make_ready_process( 0U, 2U );

    assert(
        maxrtos_scheduler_add_process( &s_ctx, a ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_scheduler_add_process( &s_ctx, b ) ==
        MAXRTOS_OK );

    /* b has the higher priority and therefore runs first. */
    assert(
        maxrtos_kernel_dispatch(
            &s_ctx,
            MAXRTOS_INVALID_PROCESS_ID,
            &out ) == MAXRTOS_OK );

    assert( out == b );

    pcb_a = maxrtos_process_get( a );
    pcb_b = maxrtos_process_get( b );

    assert( pcb_a != NULL );
    assert( pcb_b != NULL );

    assert( pcb_b->state == MAXRTOS_PROCESS_STATE_RUNNING );
    assert( pcb_a->state == MAXRTOS_PROCESS_STATE_READY );
    assert( maxrtos_scheduler_process_count( &s_ctx ) == 1U );

    /* Dispatch again. The current process b is returned to READY
     * and a becomes the next RUNNING process. */
    assert(
        maxrtos_kernel_dispatch(
            &s_ctx,
            b,
            &out ) == MAXRTOS_OK );

    assert( out == a );

    pcb_a = maxrtos_process_get( a );
    pcb_b = maxrtos_process_get( b );

    assert( pcb_a != NULL );
    assert( pcb_b != NULL );

    assert( pcb_a->state == MAXRTOS_PROCESS_STATE_RUNNING );
    assert( pcb_b->state == MAXRTOS_PROCESS_STATE_READY );
    assert( maxrtos_scheduler_process_count( &s_ctx ) == 1U );

    printf(
        "test_normal_handoff_between_two_processes: PASS\n" );
}

static void test_dispatch_context_isolation( void )
{
    maxrtos_scheduler_context_t ctx_a;
    maxrtos_scheduler_context_t ctx_b;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t out;
    maxrtos_process_control_block_t * pcb_a;
    maxrtos_process_control_block_t * pcb_b;

    maxrtos_process_pool_init();

    assert(
        maxrtos_scheduler_init( &ctx_a ) == MAXRTOS_OK );

    assert(
        maxrtos_scheduler_init( &ctx_b ) == MAXRTOS_OK );

    s_next_stack = 0U;

    a = make_ready_process( 0U, 1U );
    b = make_ready_process( 1U, 0U );

    /* Each process belongs to a different partition and therefore
     * is placed into a different scheduler context. */
    assert(
        maxrtos_scheduler_add_process( &ctx_a, a ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_scheduler_add_process( &ctx_b, b ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_kernel_dispatch(
            &ctx_a,
            MAXRTOS_INVALID_PROCESS_ID,
            &out ) == MAXRTOS_OK );

    assert( out == a );

    pcb_a = maxrtos_process_get( a );
    pcb_b = maxrtos_process_get( b );

    assert( pcb_a != NULL );
    assert( pcb_b != NULL );

    assert( pcb_a->state == MAXRTOS_PROCESS_STATE_RUNNING );
    assert( pcb_b->state == MAXRTOS_PROCESS_STATE_READY );

    /* Dispatching ctx_a must never select a process queued in
     * ctx_b. */
    assert( maxrtos_scheduler_process_count( &ctx_a ) == 0U );
    assert( maxrtos_scheduler_process_count( &ctx_b ) == 1U );

    printf( "test_dispatch_context_isolation: PASS\n" );
}

int main( void )
{
    test_dispatch_rejects_null_ctx();
    test_dispatch_rejects_null_out();
    test_dispatch_empty_queue();
    test_boot_dispatch_no_prior_current();
    test_current_continues_when_nothing_else_ready();
    test_dispatch_rejects_current_id_not_running();
    test_dispatch_rejects_unknown_current_id();
    test_normal_handoff_between_two_processes();
    test_dispatch_context_isolation();

    printf( "all dispatch tests passed\n" );

    return 0;
}
