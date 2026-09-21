/**
 * @file test_queue_port.c
 * @brief Unit tests for the queuing port module.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "maxrtos/kernel/queue_port.h"
#include "maxrtos/kernel/ipc_block.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/tick.h"

static maxrtos_status_t test_queue_send(
    maxrtos_queue_port_t * port,
    void const * message,
    size_t message_size,
    maxrtos_tick_t timeout )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t next_id = 0U;

    ( void ) maxrtos_partition_table_init( &table );

    return maxrtos_kernel_queue_port_send(
        port,
        message,
        message_size,
        timeout,
        &table,
        0U,
        &next_id );
}

static maxrtos_status_t test_queue_receive(
    maxrtos_queue_port_t * port,
    void * out_message,
    size_t buffer_size,
    maxrtos_tick_t timeout )
{
    maxrtos_partition_table_t table;
    maxrtos_process_id_t next_id = 0U;

    ( void ) maxrtos_partition_table_init( &table );

    return maxrtos_kernel_queue_port_receive(
        port,
        out_message,
        buffer_size,
        timeout,
        &table,
        0U,
        &next_id );
}

static void test_init_rejects_bad_args( void )
{
    maxrtos_queue_port_t port;

    assert(
        maxrtos_queue_port_init(
            NULL,
            4U,
            8U ) == MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_queue_port_init(
            &port,
            0U,
            8U ) == MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_queue_port_init(
            &port,
            4U,
            0U ) == MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_queue_port_init(
            &port,
            MAXRTOS_MAX_QUEUE_MESSAGE_SIZE + 1U,
            8U ) == MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_queue_port_init(
            &port,
            4U,
            MAXRTOS_MAX_QUEUE_CAPACITY + 1U )
        == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_init_rejects_bad_args: PASS\n" );
}

static void test_init_succeeds_and_starts_empty( void )
{
    maxrtos_queue_port_t port;

    assert(
        maxrtos_queue_port_init(
            &port,
            4U,
            8U ) == MAXRTOS_OK );

    assert( maxrtos_kernel_queue_port_count( &port ) == 0U );
    assert( port.fifo.buffer == port.buffer );
    assert( port.fifo.count == 0U );

    printf( "test_init_succeeds_and_starts_empty: PASS\n" );
}

static void test_receive_on_empty_port_fails_cleanly( void )
{
    maxrtos_queue_port_t port;
    uint8_t buf[ 4 ];

    assert(
        maxrtos_queue_port_init(
            &port,
            4U,
            8U ) == MAXRTOS_OK );

    assert(
        test_queue_receive(
            &port,
            buf,
            sizeof( buf ),
            0U ) == MAXRTOS_ERR_QUEUE_EMPTY );

    printf( "test_receive_on_empty_port_fails_cleanly: PASS\n" );
}

static void test_send_rejects_wrong_message_size( void )
{
    maxrtos_queue_port_t port;
    uint32_t message = 42U;

    assert(
        maxrtos_queue_port_init(
            &port,
            4U,
            8U ) == MAXRTOS_OK );

    assert(
        test_queue_send(
            &port,
            &message,
            3U,
            0U ) == MAXRTOS_ERR_INVALID_ARG );

    assert(
        test_queue_send(
            &port,
            &message,
            8U,
            0U ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_send_rejects_wrong_message_size: PASS\n" );
}

static void test_receive_rejects_undersized_buffer( void )
{
    maxrtos_queue_port_t port;
    uint32_t message = 42U;
    uint8_t small_buf[ 2 ];

    assert(
        maxrtos_queue_port_init(
            &port,
            4U,
            8U ) == MAXRTOS_OK );

    assert(
        test_queue_send(
            &port,
            &message,
            4U,
            0U ) == MAXRTOS_OK );

    assert(
        test_queue_receive(
            &port,
            small_buf,
            sizeof( small_buf ),
            0U )
        == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_receive_rejects_undersized_buffer: PASS\n" );
}

static void test_single_send_receive_roundtrip_preserves_data( void )
{
    /* This test catches a reversed memcpy direction:
     * the received value must exactly match the sent value. */
    maxrtos_queue_port_t port;
    uint32_t sent_value = 0xDEADBEEFU;
    uint32_t received_value = 0U;

    assert(
        maxrtos_queue_port_init(
            &port,
            sizeof( sent_value ),
            8U ) == MAXRTOS_OK );

    assert(
        test_queue_send(
            &port,
            &sent_value,
            sizeof( sent_value ),
            0U ) == MAXRTOS_OK );

    assert( maxrtos_kernel_queue_port_count( &port ) == 1U );

    assert(
        test_queue_receive(
            &port,
            &received_value,
            sizeof( received_value ),
            0U ) == MAXRTOS_OK );

    assert( received_value == sent_value );
    assert( maxrtos_kernel_queue_port_count( &port ) == 0U );

    printf(
        "test_single_send_receive_roundtrip_preserves_data: PASS\n" );
}

static void test_fifo_order_preserved_across_multiple_messages( void )
{
    maxrtos_queue_port_t port;
    uint32_t i;
    uint32_t out;

    assert(
        maxrtos_queue_port_init(
            &port,
            sizeof( uint32_t ),
            8U ) == MAXRTOS_OK );

    for( i = 0U; i < 5U; i++ )
    {
        assert(
            test_queue_send(
                &port,
                &i,
                sizeof( i ),
                0U ) == MAXRTOS_OK );
    }

    for( i = 0U; i < 5U; i++ )
    {
        assert(
            test_queue_receive(
                &port,
                &out,
                sizeof( out ),
                0U ) == MAXRTOS_OK );

        assert( out == i );
    }

    printf(
        "test_fifo_order_preserved_across_multiple_messages: PASS\n" );
}

static void test_send_fails_when_genuinely_full( void )
{
    /* This test catches inverted full/not-full branches.
     * A queue with available space must accept messages, while
     * a queue at capacity must reject the next send. */
    maxrtos_queue_port_t port;
    uint32_t value;
    size_t i;

    assert(
        maxrtos_queue_port_init(
            &port,
            sizeof( uint32_t ),
            4U ) == MAXRTOS_OK );

    for( i = 0U; i < 4U; i++ )
    {
        value = ( uint32_t ) i;

        assert(
            test_queue_send(
                &port,
                &value,
                sizeof( value ),
                0U ) == MAXRTOS_OK );
    }

    assert( maxrtos_kernel_queue_port_count( &port ) == 4U );

    value = 999U;

    assert(
        test_queue_send(
            &port,
            &value,
            sizeof( value ),
            0U ) == MAXRTOS_ERR_QUEUE_FULL );

    assert( maxrtos_kernel_queue_port_count( &port ) == 4U );

    printf(
        "test_send_fails_when_genuinely_full: PASS\n" );
}

static void test_wraparound_preserves_fifo_order( void )
{
    /* Fill to capacity, drain entries, then refill.
     * This forces the ring-buffer indices to wrap around. */
    maxrtos_queue_port_t port;
    uint32_t value;
    uint32_t out;

    assert(
        maxrtos_queue_port_init(
            &port,
            sizeof( uint32_t ),
            4U ) == MAXRTOS_OK );

    value = 1U;
    assert(
        test_queue_send(
            &port,
            &value,
            sizeof( value ),
            0U ) == MAXRTOS_OK );

    value = 2U;
    assert(
        test_queue_send(
            &port,
            &value,
            sizeof( value ),
            0U ) == MAXRTOS_OK );

    value = 3U;
    assert(
        test_queue_send(
            &port,
            &value,
            sizeof( value ),
            0U ) == MAXRTOS_OK );

    assert(
        test_queue_receive(
            &port,
            &out,
            sizeof( out ),
            0U ) == MAXRTOS_OK );

    assert( out == 1U );

    assert(
        test_queue_receive(
            &port,
            &out,
            sizeof( out ),
            0U ) == MAXRTOS_OK );

    assert( out == 2U );

    /* Head has advanced to slot 2 and one message remains.
     * Send three additional messages so the tail wraps around. */
    value = 4U;
    assert(
        test_queue_send(
            &port,
            &value,
            sizeof( value ),
            0U ) == MAXRTOS_OK );

    value = 5U;
    assert(
        test_queue_send(
            &port,
            &value,
            sizeof( value ),
            0U ) == MAXRTOS_OK );

    value = 6U;
    assert(
        test_queue_send(
            &port,
            &value,
            sizeof( value ),
            0U ) == MAXRTOS_OK );

    assert( maxrtos_kernel_queue_port_count( &port ) == 4U );

    assert(
        test_queue_receive(
            &port,
            &out,
            sizeof( out ),
            0U ) == MAXRTOS_OK );

    assert( out == 3U );

    assert(
        test_queue_receive(
            &port,
            &out,
            sizeof( out ),
            0U ) == MAXRTOS_OK );

    assert( out == 4U );

    assert(
        test_queue_receive(
            &port,
            &out,
            sizeof( out ),
            0U ) == MAXRTOS_OK );

    assert( out == 5U );

    assert(
        test_queue_receive(
            &port,
            &out,
            sizeof( out ),
            0U ) == MAXRTOS_OK );

    assert( out == 6U );

    assert(
        test_queue_receive(
            &port,
            &out,
            sizeof( out ),
            0U ) == MAXRTOS_ERR_QUEUE_EMPTY );

    printf(
        "test_wraparound_preserves_fifo_order: PASS\n" );
}

/* ---- Blocking behaviour --------------------------------------------- */

#define TEST_STACK_SIZE ( 128U )

static uint8_t s_stacks[ 6 ][ TEST_STACK_SIZE ];
static maxrtos_partition_table_t s_table;
static maxrtos_queue_port_t s_port;

static void dummy_entry( void * arg )
{
    ( void ) arg;
}

static maxrtos_process_id_t add_process(
    unsigned slot,
    maxrtos_partition_id_t partition )
{
    maxrtos_process_id_t id;

    assert(
        maxrtos_process_create(
            s_stacks[ slot ],
            TEST_STACK_SIZE,
            partition,
            5U,
            true,
            dummy_entry,
            NULL,
            &id ) == MAXRTOS_OK );

    assert( maxrtos_partition_add_process( &s_table, id ) == MAXRTOS_OK );

    return id;
}

static void reset_world( void )
{
    maxrtos_process_pool_init();
    assert( maxrtos_partition_table_init( &s_table ) == MAXRTOS_OK );
    assert( maxrtos_queue_port_init( &s_port, 4U, 2U ) == MAXRTOS_OK );
}

static maxrtos_process_id_t start_partition( maxrtos_partition_id_t partition )
{
    maxrtos_process_id_t running;

    assert( maxrtos_partition_dispatch( &s_table, partition, &running ) ==
            MAXRTOS_OK );

    return running;
}

static size_t ready_count( maxrtos_partition_id_t partition )
{
    return maxrtos_scheduler_process_count(
        &s_table.scheduler_ctx[ partition ] );
}

/* A blocked receiver gets the message copied into its own buffer and
 * resumes with OK, with the result queued for delivery. */
static void test_blocked_receiver_gets_message_from_sender( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t next_id;
    uint32_t rx;
    uint32_t tx;
    maxrtos_process_control_block_t * pcb_a;

    reset_world();
    a = add_process( 0U, 0U );
    b = add_process( 1U, 0U );
    assert( start_partition( 0U ) == a );

    rx = 0U;
    assert(
        maxrtos_kernel_queue_port_receive(
            &s_port, &rx, sizeof( rx ), MAXRTOS_TIMEOUT_INFINITE,
            &s_table, a, &next_id ) == MAXRTOS_PENDING );

    /* a is BLOCKED; b runs and is the partition's current process. */
    assert( next_id == b );
    assert( s_table.current_id[ 0 ] == b );
    pcb_a = maxrtos_process_get( a );
    assert( pcb_a->state == MAXRTOS_PROCESS_STATE_BLOCKED );
    assert( maxrtos_waitlist_count( &s_port.waiting_receivers ) == 1U );

    tx = 0xA5A5F00DU;
    assert(
        maxrtos_kernel_queue_port_send(
            &s_port, &tx, sizeof( tx ), 0U,
            &s_table, b, &next_id ) == MAXRTOS_OK );

    assert( rx == tx );
    assert( maxrtos_kernel_queue_port_count( &s_port ) == 0U );
    assert( maxrtos_waitlist_count( &s_port.waiting_receivers ) == 0U );
    assert( pcb_a->state == MAXRTOS_PROCESS_STATE_READY );
    assert( pcb_a->ipc_result_pending == true );
    assert( pcb_a->ipc_result == MAXRTOS_OK );
    assert( pcb_a->waitlist == NULL );
    assert( ready_count( 0U ) == 1U );

    printf( "test_blocked_receiver_gets_message_from_sender: PASS\n" );
}

/* The woken process goes back to ITS partition's ready queue, not the
 * sender's. */
static void test_wake_targets_the_receivers_own_partition( void )
{
    maxrtos_process_id_t sender;
    maxrtos_process_id_t recv;
    maxrtos_process_id_t recv_helper;
    maxrtos_process_id_t next_id;
    uint32_t rx;
    uint32_t tx;

    reset_world();
    sender = add_process( 0U, 0U );
    recv = add_process( 1U, 1U );
    recv_helper = add_process( 2U, 1U );
    ( void ) recv_helper;

    assert( start_partition( 0U ) == sender );
    assert( start_partition( 1U ) == recv );

    rx = 0U;
    assert(
        maxrtos_kernel_queue_port_receive(
            &s_port, &rx, sizeof( rx ), MAXRTOS_TIMEOUT_INFINITE,
            &s_table, recv, &next_id ) == MAXRTOS_PENDING );

    assert( ready_count( 0U ) == 0U );
    assert( ready_count( 1U ) == 0U );   /* recv_helper is RUNNING */

    tx = 7U;
    assert(
        maxrtos_kernel_queue_port_send(
            &s_port, &tx, sizeof( tx ), 0U,
            &s_table, sender, &next_id ) == MAXRTOS_OK );

    assert( rx == 7U );
    assert( ready_count( 0U ) == 0U );   /* NOT the sender's queue */
    assert( ready_count( 1U ) == 1U );   /* the receiver's own */
    assert( maxrtos_process_get( recv )->state ==
            MAXRTOS_PROCESS_STATE_READY );

    printf( "test_wake_targets_the_receivers_own_partition: PASS\n" );
}

/* A blocked sender's message is enqueued, in order, when a receive
 * frees a slot; the sender resumes with OK. */
static void test_blocked_sender_admitted_when_receiver_frees_slot( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t next_id;
    uint32_t m;
    uint32_t out;
    maxrtos_process_control_block_t * pcb_a;

    reset_world();
    a = add_process( 0U, 0U );
    b = add_process( 1U, 0U );
    assert( start_partition( 0U ) == a );

    /* Capacity is 2: fill it, then a third send blocks. */
    m = 1U;
    assert( maxrtos_kernel_queue_port_send(
                &s_port, &m, 4U, 0U, &s_table, a, &next_id ) == MAXRTOS_OK );
    m = 2U;
    assert( maxrtos_kernel_queue_port_send(
                &s_port, &m, 4U, 0U, &s_table, a, &next_id ) == MAXRTOS_OK );

    m = 3U;
    assert( maxrtos_kernel_queue_port_send(
                &s_port, &m, 4U, MAXRTOS_TIMEOUT_INFINITE,
                &s_table, a, &next_id ) == MAXRTOS_PENDING );
    assert( next_id == b );
    pcb_a = maxrtos_process_get( a );
    assert( pcb_a->state == MAXRTOS_PROCESS_STATE_BLOCKED );

    assert( maxrtos_kernel_queue_port_receive(
                &s_port, &out, 4U, 0U, &s_table, b, &next_id ) == MAXRTOS_OK );
    assert( out == 1U );

    /* Slot freed: the blocked sender's message (3) is now queued and the
     * sender is READY with OK. */
    assert( maxrtos_kernel_queue_port_count( &s_port ) == 2U );
    assert( pcb_a->state == MAXRTOS_PROCESS_STATE_READY );
    assert( pcb_a->ipc_result_pending == true );
    assert( pcb_a->ipc_result == MAXRTOS_OK );

    assert( maxrtos_kernel_queue_port_receive(
                &s_port, &out, 4U, 0U, &s_table, b, &next_id ) == MAXRTOS_OK );
    assert( out == 2U );
    assert( maxrtos_kernel_queue_port_receive(
                &s_port, &out, 4U, 0U, &s_table, b, &next_id ) == MAXRTOS_OK );
    assert( out == 3U );

    printf( "test_blocked_sender_admitted_when_receiver_frees_slot: PASS\n" );
}

/* Timed waits expire exactly at their deadline with MAXRTOS_ERR_TIMEOUT
 * and leave no wait-list residue. */
static void test_timed_wait_expires_with_timeout( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t next_id;
    maxrtos_process_control_block_t * pcb_a;
    maxrtos_tick_t now;
    uint32_t rx;

    reset_world();
    a = add_process( 0U, 0U );
    ( void ) add_process( 1U, 0U );
    assert( start_partition( 0U ) == a );

    now = maxrtos_kernel_tick_now();
    assert(
        maxrtos_kernel_queue_port_receive(
            &s_port, &rx, sizeof( rx ), 3U, &s_table, a, &next_id ) ==
        MAXRTOS_PENDING );

    pcb_a = maxrtos_process_get( a );
    assert( pcb_a->wake_tick == now + 3U );

    assert( maxrtos_ipc_expire_timeouts( &s_table, now + 2U ) == 0U );
    assert( pcb_a->state == MAXRTOS_PROCESS_STATE_BLOCKED );

    assert( maxrtos_ipc_expire_timeouts( &s_table, now + 3U ) == 1U );
    assert( pcb_a->state == MAXRTOS_PROCESS_STATE_READY );
    assert( pcb_a->ipc_result_pending == true );
    assert( pcb_a->ipc_result == MAXRTOS_ERR_TIMEOUT );
    assert( pcb_a->waitlist == NULL );
    assert( pcb_a->wake_tick == MAXRTOS_TICK_NONE );
    assert( maxrtos_waitlist_count( &s_port.waiting_receivers ) == 0U );

    /* Expiring again is a no-op. */
    assert( maxrtos_ipc_expire_timeouts( &s_table, now + 10U ) == 0U );

    printf( "test_timed_wait_expires_with_timeout: PASS\n" );
}

/* An untimed wait never expires. */
static void test_infinite_wait_never_expires( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t next_id;
    uint32_t rx;

    reset_world();
    a = add_process( 0U, 0U );
    ( void ) add_process( 1U, 0U );
    assert( start_partition( 0U ) == a );

    assert(
        maxrtos_kernel_queue_port_receive(
            &s_port, &rx, sizeof( rx ), MAXRTOS_TIMEOUT_INFINITE,
            &s_table, a, &next_id ) == MAXRTOS_PENDING );

    assert( maxrtos_ipc_expire_timeouts( &s_table, 0xFFFFFFF0U ) == 0U );
    assert( maxrtos_process_get( a )->state == MAXRTOS_PROCESS_STATE_BLOCKED );

    printf( "test_infinite_wait_never_expires: PASS\n" );
}

/* With no other READY process in the partition the caller cannot be
 * blocked; it gets the same status a non-blocking call would, and its
 * state is untouched. */
static void test_cannot_block_without_another_ready_process( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t next_id;
    uint32_t v;

    reset_world();
    a = add_process( 0U, 0U );
    assert( start_partition( 0U ) == a );

    assert(
        maxrtos_kernel_queue_port_receive(
            &s_port, &v, sizeof( v ), MAXRTOS_TIMEOUT_INFINITE,
            &s_table, a, &next_id ) == MAXRTOS_ERR_QUEUE_EMPTY );

    v = 1U;
    assert( maxrtos_kernel_queue_port_send(
                &s_port, &v, 4U, 0U, &s_table, a, &next_id ) == MAXRTOS_OK );
    assert( maxrtos_kernel_queue_port_send(
                &s_port, &v, 4U, 0U, &s_table, a, &next_id ) == MAXRTOS_OK );

    assert(
        maxrtos_kernel_queue_port_send(
            &s_port, &v, 4U, MAXRTOS_TIMEOUT_INFINITE,
            &s_table, a, &next_id ) == MAXRTOS_ERR_QUEUE_FULL );

    assert( maxrtos_process_get( a )->state == MAXRTOS_PROCESS_STATE_RUNNING );
    assert( maxrtos_waitlist_count( &s_port.waiting_senders ) == 0U );
    assert( maxrtos_waitlist_count( &s_port.waiting_receivers ) == 0U );

    printf( "test_cannot_block_without_another_ready_process: PASS\n" );
}

/* Several blocked receivers are served in arrival order. */
static void test_waiting_receivers_served_in_fifo_order( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t c;
    maxrtos_process_id_t next_id;
    uint32_t rx_a;
    uint32_t rx_b;
    uint32_t tx;

    reset_world();
    a = add_process( 0U, 0U );
    b = add_process( 1U, 0U );
    c = add_process( 2U, 0U );
    assert( start_partition( 0U ) == a );

    rx_a = 0U;
    assert( maxrtos_kernel_queue_port_receive(
                &s_port, &rx_a, 4U, MAXRTOS_TIMEOUT_INFINITE,
                &s_table, a, &next_id ) == MAXRTOS_PENDING );
    assert( next_id == b );

    rx_b = 0U;
    assert( maxrtos_kernel_queue_port_receive(
                &s_port, &rx_b, 4U, MAXRTOS_TIMEOUT_INFINITE,
                &s_table, b, &next_id ) == MAXRTOS_PENDING );
    assert( next_id == c );

    tx = 100U;
    assert( maxrtos_kernel_queue_port_send(
                &s_port, &tx, 4U, 0U, &s_table, c, &next_id ) == MAXRTOS_OK );
    tx = 200U;
    assert( maxrtos_kernel_queue_port_send(
                &s_port, &tx, 4U, 0U, &s_table, c, &next_id ) == MAXRTOS_OK );

    assert( rx_a == 100U );
    assert( rx_b == 200U );

    printf( "test_waiting_receivers_served_in_fifo_order: PASS\n" );
}

int main( void )
{
    test_init_rejects_bad_args();
    test_init_succeeds_and_starts_empty();
    test_receive_on_empty_port_fails_cleanly();
    test_send_rejects_wrong_message_size();
    test_receive_rejects_undersized_buffer();
    test_single_send_receive_roundtrip_preserves_data();
    test_fifo_order_preserved_across_multiple_messages();
    test_send_fails_when_genuinely_full();
    test_wraparound_preserves_fifo_order();
    test_blocked_receiver_gets_message_from_sender();
    test_wake_targets_the_receivers_own_partition();
    test_blocked_sender_admitted_when_receiver_frees_slot();
    test_timed_wait_expires_with_timeout();
    test_infinite_wait_never_expires();
    test_cannot_block_without_another_ready_process();
    test_waiting_receivers_served_in_fifo_order();

    printf( "all queue port tests passed\n" );

    return 0;
}
