/**
 * @file test_buffer.c
 * @brief Unit tests for the kernel intra-partition ARINC 653 buffer.
 */

#include <assert.h>
#include <stdio.h>
#include <stddef.h>

#include "maxrtos/kernel/buffer.h"
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
    maxrtos_buffer_pool_init();
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

static maxrtos_status_t send_on(
    maxrtos_buffer_id_t id,
    void const * message,
    size_t message_size,
    maxrtos_tick_t timeout,
    maxrtos_process_id_t caller,
    maxrtos_process_id_t * next )
{
    return maxrtos_kernel_buffer_send(
        id, message, message_size, timeout, &s_table, caller, next );
}

static maxrtos_status_t receive_on(
    maxrtos_buffer_id_t id,
    void * out_message,
    size_t buffer_size,
    maxrtos_tick_t timeout,
    maxrtos_process_id_t caller,
    maxrtos_process_id_t * next )
{
    return maxrtos_kernel_buffer_receive(
        id, out_message, buffer_size, timeout, &s_table, caller, next );
}

static maxrtos_process_state_t state_of( maxrtos_process_id_t id )
{
    return maxrtos_process_get( id )->state;
}

static void test_create_validates_arguments_and_exhausts_pool( void )
{
    maxrtos_buffer_id_t id;
    maxrtos_buffer_id_t ids[ MAXRTOS_MAX_BUFFERS ];
    size_t i;

    setup();

    assert( maxrtos_buffer_create(
                0U, 4U, 4U, MAXRTOS_QUEUING_FIFO, NULL ) ==
            MAXRTOS_ERR_INVALID_ARG );
    assert( maxrtos_buffer_create(
                MAXRTOS_MAX_PARTITIONS, 4U, 4U, MAXRTOS_QUEUING_FIFO, &id ) ==
            MAXRTOS_ERR_INVALID_ARG );
    assert( maxrtos_buffer_create(
                0U, 0U, 4U, MAXRTOS_QUEUING_FIFO, &id ) ==
            MAXRTOS_ERR_INVALID_ARG );
    assert( maxrtos_buffer_create(
                0U, 4U, 0U, MAXRTOS_QUEUING_FIFO, &id ) ==
            MAXRTOS_ERR_INVALID_ARG );
    assert( maxrtos_buffer_create(
                0U, MAXRTOS_MAX_BUFFER_MESSAGE_SIZE + 1U, 4U,
                MAXRTOS_QUEUING_FIFO, &id ) == MAXRTOS_ERR_INVALID_ARG );
    assert( maxrtos_buffer_create(
                0U, 4U, MAXRTOS_MAX_BUFFER_CAPACITY + 1U,
                MAXRTOS_QUEUING_FIFO, &id ) == MAXRTOS_ERR_INVALID_ARG );

    for( i = 0U; i < MAXRTOS_MAX_BUFFERS; i++ )
    {
        assert( maxrtos_buffer_create(
                    0U, 4U, 4U, MAXRTOS_QUEUING_FIFO, &ids[ i ] ) ==
                MAXRTOS_OK );
    }

    assert( maxrtos_buffer_create(
                0U, 4U, 4U, MAXRTOS_QUEUING_FIFO, &id ) ==
            MAXRTOS_ERR_POOL_FULL );

    printf( "test_create_validates_arguments_and_exhausts_pool: PASS\n" );
}

static void test_send_rejects_wrong_message_size( void )
{
    maxrtos_buffer_id_t buf;
    maxrtos_process_id_t a;
    maxrtos_process_id_t next;
    uint32_t message = 42U;

    setup();
    assert( maxrtos_buffer_create(
                0U, 4U, 4U, MAXRTOS_QUEUING_FIFO, &buf ) == MAXRTOS_OK );
    a = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );

    assert( send_on( buf, &message, 3U, 0U, a, &next ) ==
            MAXRTOS_ERR_INVALID_ARG );

    printf( "test_send_rejects_wrong_message_size: PASS\n" );
}

static void test_single_send_receive_roundtrip_preserves_data( void )
{
    maxrtos_buffer_id_t buf;
    maxrtos_process_id_t a;
    maxrtos_process_id_t next;
    uint32_t sent_value = 0xDEADBEEFU;
    uint32_t received_value = 0U;

    setup();
    assert( maxrtos_buffer_create(
                0U, sizeof( sent_value ), 8U, MAXRTOS_QUEUING_FIFO, &buf ) ==
            MAXRTOS_OK );
    a = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );

    assert( send_on( buf, &sent_value, sizeof( sent_value ), 0U, a, &next ) ==
            MAXRTOS_OK );
    assert( receive_on(
                buf, &received_value, sizeof( received_value ), 0U, a,
                &next ) == MAXRTOS_OK );
    assert( received_value == sent_value );

    printf( "test_single_send_receive_roundtrip_preserves_data: PASS\n" );
}

static void test_send_fails_when_genuinely_full( void )
{
    maxrtos_buffer_id_t buf;
    maxrtos_process_id_t a;
    maxrtos_process_id_t next;
    uint32_t value;
    size_t i;

    setup();
    assert( maxrtos_buffer_create(
                0U, sizeof( uint32_t ), 4U, MAXRTOS_QUEUING_FIFO, &buf ) ==
            MAXRTOS_OK );
    a = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );

    for( i = 0U; i < 4U; i++ )
    {
        value = ( uint32_t ) i;
        assert( send_on( buf, &value, sizeof( value ), 0U, a, &next ) ==
                MAXRTOS_OK );
    }

    value = 999U;
    assert( send_on( buf, &value, sizeof( value ), 0U, a, &next ) ==
            MAXRTOS_ERR_QUEUE_FULL );

    printf( "test_send_fails_when_genuinely_full: PASS\n" );
}

static void test_other_partition_and_bad_handles_rejected( void )
{
    maxrtos_buffer_id_t buf;
    maxrtos_process_id_t a;
    maxrtos_process_id_t other;
    maxrtos_process_id_t next;
    uint32_t value = 1U;

    setup();
    assert( maxrtos_buffer_create(
                0U, sizeof( uint32_t ), 4U, MAXRTOS_QUEUING_FIFO, &buf ) ==
            MAXRTOS_OK );
    a = make_process( 0U, 5U );
    other = make_process( 1U, 5U );
    assert( dispatch( 0U ) == a );
    assert( dispatch( 1U ) == other );

    assert( send_on( buf, &value, sizeof( value ), 0U, other, &next ) ==
            MAXRTOS_ERR_INVALID_ID );
    assert( receive_on( buf, &value, sizeof( value ), 0U, other, &next ) ==
            MAXRTOS_ERR_INVALID_ID );

    assert( send_on(
                MAXRTOS_MAX_BUFFERS, &value, sizeof( value ), 0U, a,
                &next ) == MAXRTOS_ERR_INVALID_ID );
    assert( send_on(
                MAXRTOS_INVALID_BUFFER_ID, &value, sizeof( value ), 0U, a,
                &next ) == MAXRTOS_ERR_INVALID_ID );
    assert( send_on( buf, &value, sizeof( value ), 0U,
                      MAXRTOS_INVALID_PROCESS_ID, &next ) ==
            MAXRTOS_ERR_INVALID_ID );

    assert( maxrtos_kernel_buffer_send(
                buf, &value, sizeof( value ), 0U, NULL, a, &next ) ==
            MAXRTOS_ERR_INVALID_ARG );
    assert( maxrtos_kernel_buffer_send(
                buf, &value, sizeof( value ), 0U, &s_table, a, NULL ) ==
            MAXRTOS_ERR_INVALID_ARG );

    printf( "test_other_partition_and_bad_handles_rejected: PASS\n" );
}

/* A blocked receiver gets the message copied into its own buffer and
 * resumes with OK. */
static void test_blocked_receiver_gets_message_from_sender( void )
{
    maxrtos_buffer_id_t buf;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t next;
    uint32_t rx;
    uint32_t tx;
    maxrtos_process_control_block_t * pcb_a;

    setup();
    assert( maxrtos_buffer_create(
                0U, sizeof( uint32_t ), 2U, MAXRTOS_QUEUING_FIFO, &buf ) ==
            MAXRTOS_OK );
    a = make_process( 0U, 5U );
    b = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );

    rx = 0U;
    assert( receive_on( buf, &rx, sizeof( rx ), MAXRTOS_TIMEOUT_INFINITE,
                        a, &next ) == MAXRTOS_PENDING );
    assert( next == b );
    pcb_a = maxrtos_process_get( a );
    assert( pcb_a->state == MAXRTOS_PROCESS_STATE_BLOCKED );

    tx = 0xA5A5F00DU;
    assert( dispatch( 0U ) == b );
    assert( send_on( buf, &tx, sizeof( tx ), 0U, b, &next ) == MAXRTOS_OK );

    assert( rx == tx );
    assert( pcb_a->state == MAXRTOS_PROCESS_STATE_READY );
    assert( pcb_a->ipc_result_pending == true );
    assert( pcb_a->ipc_result == MAXRTOS_OK );
    assert( pcb_a->waitlist == NULL );

    printf( "test_blocked_receiver_gets_message_from_sender: PASS\n" );
}

/* A blocked sender's message is enqueued, in order, when a receive frees
 * a slot; the sender resumes with OK. */
static void test_blocked_sender_admitted_when_receiver_frees_slot( void )
{
    maxrtos_buffer_id_t buf;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t next;
    uint32_t m;
    uint32_t out;
    maxrtos_process_control_block_t * pcb_a;

    setup();
    assert( maxrtos_buffer_create(
                0U, sizeof( uint32_t ), 2U, MAXRTOS_QUEUING_FIFO, &buf ) ==
            MAXRTOS_OK );
    a = make_process( 0U, 5U );
    b = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );

    m = 1U;
    assert( send_on( buf, &m, sizeof( m ), 0U, a, &next ) == MAXRTOS_OK );
    m = 2U;
    assert( send_on( buf, &m, sizeof( m ), 0U, a, &next ) == MAXRTOS_OK );

    m = 3U;
    assert( send_on( buf, &m, sizeof( m ), MAXRTOS_TIMEOUT_INFINITE, a,
                     &next ) == MAXRTOS_PENDING );
    assert( next == b );
    pcb_a = maxrtos_process_get( a );
    assert( pcb_a->state == MAXRTOS_PROCESS_STATE_BLOCKED );

    assert( dispatch( 0U ) == b );
    assert( receive_on( buf, &out, sizeof( out ), 0U, b, &next ) ==
            MAXRTOS_OK );
    assert( out == 1U );

    assert( pcb_a->state == MAXRTOS_PROCESS_STATE_READY );
    assert( pcb_a->ipc_result_pending == true );
    assert( pcb_a->ipc_result == MAXRTOS_OK );

    assert( receive_on( buf, &out, sizeof( out ), 0U, b, &next ) ==
            MAXRTOS_OK );
    assert( out == 2U );
    assert( receive_on( buf, &out, sizeof( out ), 0U, b, &next ) ==
            MAXRTOS_OK );
    assert( out == 3U );

    printf( "test_blocked_sender_admitted_when_receiver_frees_slot: PASS\n" );
}

/* With MAXRTOS_QUEUING_FIFO, waiters are released in arrival order
 * regardless of priority. o (priority 0) fills the buffer.
 * high_prio_second (priority 2) is parked on an unrelated wait list so
 * low_prio_first (priority 9) reaches the buffer first despite its lower
 * priority; high_prio_second then queues behind it. FIFO must still pick
 * low_prio_first. */
static void test_fifo_discipline_ignores_priority( void )
{
    maxrtos_buffer_id_t buf;
    maxrtos_process_id_t o;
    maxrtos_process_id_t low_prio_first;
    maxrtos_process_id_t high_prio_second;
    maxrtos_process_id_t next;
    maxrtos_waitlist_t elsewhere;
    maxrtos_ipc_operation_t op;
    maxrtos_process_id_t woken;
    uint32_t m;

    setup();
    assert( maxrtos_buffer_create(
                0U, sizeof( uint32_t ), 1U, MAXRTOS_QUEUING_FIFO, &buf ) ==
            MAXRTOS_OK );
    o = make_process( 0U, 0U );
    low_prio_first = make_process( 0U, 9U );
    high_prio_second = make_process( 0U, 2U );

    assert( dispatch( 0U ) == o );
    m = 1U;
    assert( send_on( buf, &m, sizeof( m ), 0U, o, &next ) == MAXRTOS_OK );

    /* Park high_prio_second elsewhere so low_prio_first reaches the
     * buffer first. */
    assert( maxrtos_waitlist_init( &elsewhere ) == MAXRTOS_OK );
    op.kind = MAXRTOS_IPC_OP_QUEUE_RECEIVE;
    assert( dispatch( 0U ) == high_prio_second );
    assert( maxrtos_ipc_block_current(
                &s_table, high_prio_second, &elsewhere, MAXRTOS_TICK_NONE,
                &op, &next ) == MAXRTOS_OK );
    assert( next == o );

    /* Buffer is now full (capacity 1). low_prio_first blocks first. */
    assert( dispatch( 0U ) == low_prio_first );
    m = 2U;
    assert( send_on( buf, &m, sizeof( m ), MAXRTOS_TIMEOUT_INFINITE,
                     low_prio_first, &next ) == MAXRTOS_PENDING );
    assert( next == o );

    /* high_prio_second comes back and queues behind low_prio_first. */
    assert( maxrtos_ipc_wake_one( &elsewhere, &woken, &op ) == MAXRTOS_OK );
    assert( woken == high_prio_second );
    assert( maxrtos_ipc_resolve( &s_table, high_prio_second, MAXRTOS_OK ) ==
            MAXRTOS_OK );
    assert( dispatch( 0U ) == high_prio_second );
    m = 3U;
    assert( send_on( buf, &m, sizeof( m ), MAXRTOS_TIMEOUT_INFINITE,
                     high_prio_second, &next ) == MAXRTOS_PENDING );
    assert( next == o );

    {
        uint32_t out;

        assert( receive_on( buf, &out, sizeof( out ), 0U, o, &next ) ==
                MAXRTOS_OK );
        assert( out == 1U );
    }

    /* FIFO: the longer-waiting (lower priority) sender is admitted first,
     * despite the other waiter having higher priority. */
    assert( state_of( low_prio_first ) == MAXRTOS_PROCESS_STATE_READY );
    assert( state_of( high_prio_second ) == MAXRTOS_PROCESS_STATE_BLOCKED );

    printf( "test_fifo_discipline_ignores_priority: PASS\n" );
}

/* Same arrival order as above (low_prio_first first, high_prio_second
 * second), but with MAXRTOS_QUEUING_PRIORITY: the highest-priority waiter
 * must be released first regardless of arrival order. */
static void test_priority_discipline_serves_by_priority_then_fifo( void )
{
    maxrtos_buffer_id_t buf;
    maxrtos_process_id_t o;
    maxrtos_process_id_t low_prio_first;
    maxrtos_process_id_t high_prio_second;
    maxrtos_process_id_t next;
    maxrtos_waitlist_t elsewhere;
    maxrtos_ipc_operation_t op;
    maxrtos_process_id_t woken;
    uint32_t m;

    setup();
    assert( maxrtos_buffer_create(
                0U, sizeof( uint32_t ), 1U, MAXRTOS_QUEUING_PRIORITY,
                &buf ) == MAXRTOS_OK );
    o = make_process( 0U, 0U );
    low_prio_first = make_process( 0U, 9U );
    high_prio_second = make_process( 0U, 2U );

    assert( dispatch( 0U ) == o );
    m = 1U;
    assert( send_on( buf, &m, sizeof( m ), 0U, o, &next ) == MAXRTOS_OK );

    assert( maxrtos_waitlist_init( &elsewhere ) == MAXRTOS_OK );
    op.kind = MAXRTOS_IPC_OP_QUEUE_RECEIVE;
    assert( dispatch( 0U ) == high_prio_second );
    assert( maxrtos_ipc_block_current(
                &s_table, high_prio_second, &elsewhere, MAXRTOS_TICK_NONE,
                &op, &next ) == MAXRTOS_OK );
    assert( next == o );

    assert( dispatch( 0U ) == low_prio_first );
    m = 2U;
    assert( send_on( buf, &m, sizeof( m ), MAXRTOS_TIMEOUT_INFINITE,
                     low_prio_first, &next ) == MAXRTOS_PENDING );
    assert( next == o );

    assert( maxrtos_ipc_wake_one( &elsewhere, &woken, &op ) == MAXRTOS_OK );
    assert( woken == high_prio_second );
    assert( maxrtos_ipc_resolve( &s_table, high_prio_second, MAXRTOS_OK ) ==
            MAXRTOS_OK );
    assert( dispatch( 0U ) == high_prio_second );
    m = 3U;
    assert( send_on( buf, &m, sizeof( m ), MAXRTOS_TIMEOUT_INFINITE,
                     high_prio_second, &next ) == MAXRTOS_PENDING );
    assert( next == o );

    {
        uint32_t out;

        assert( receive_on( buf, &out, sizeof( out ), 0U, o, &next ) ==
                MAXRTOS_OK );
        assert( out == 1U );
    }

    /* PRIORITY: priority beats arrival order. */
    assert( state_of( high_prio_second ) == MAXRTOS_PROCESS_STATE_READY );
    assert( state_of( low_prio_first ) == MAXRTOS_PROCESS_STATE_BLOCKED );

    printf( "test_priority_discipline_serves_by_priority_then_fifo: PASS\n" );
}

static void test_cannot_block_without_another_ready_process( void )
{
    maxrtos_buffer_id_t buf;
    maxrtos_process_id_t a;
    maxrtos_process_id_t next;
    uint32_t v;

    setup();
    assert( maxrtos_buffer_create(
                0U, sizeof( uint32_t ), 1U, MAXRTOS_QUEUING_FIFO, &buf ) ==
            MAXRTOS_OK );
    a = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );

    assert( receive_on( buf, &v, sizeof( v ), MAXRTOS_TIMEOUT_INFINITE, a,
                        &next ) == MAXRTOS_ERR_QUEUE_EMPTY );

    v = 1U;
    assert( send_on( buf, &v, sizeof( v ), 0U, a, &next ) == MAXRTOS_OK );
    assert( send_on( buf, &v, sizeof( v ), MAXRTOS_TIMEOUT_INFINITE, a,
                     &next ) == MAXRTOS_ERR_QUEUE_FULL );

    assert( state_of( a ) == MAXRTOS_PROCESS_STATE_RUNNING );

    printf( "test_cannot_block_without_another_ready_process: PASS\n" );
}

static void test_timed_receive_expires_and_leaves_wait_list( void )
{
    maxrtos_buffer_id_t buf;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t next;
    maxrtos_process_control_block_t * pcb_a;
    uint32_t rx;

    setup();
    assert( maxrtos_buffer_create(
                0U, sizeof( uint32_t ), 2U, MAXRTOS_QUEUING_FIFO, &buf ) ==
            MAXRTOS_OK );
    a = make_process( 0U, 5U );
    b = make_process( 0U, 5U );
    assert( dispatch( 0U ) == a );

    assert( receive_on( buf, &rx, sizeof( rx ), 3U, a, &next ) ==
            MAXRTOS_PENDING );
    assert( next == b );

    assert( maxrtos_ipc_expire_timeouts( &s_table, 2U ) == 0U );
    pcb_a = maxrtos_process_get( a );
    assert( pcb_a->state == MAXRTOS_PROCESS_STATE_BLOCKED );

    assert( maxrtos_ipc_expire_timeouts( &s_table, 3U ) == 1U );
    assert( pcb_a->state == MAXRTOS_PROCESS_STATE_READY );
    assert( pcb_a->ipc_result == MAXRTOS_ERR_TIMEOUT );

    printf( "test_timed_receive_expires_and_leaves_wait_list: PASS\n" );
}

static void test_get_status_reports_configuration_fill_and_waiters( void )
{
    maxrtos_buffer_id_t buf;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t other;
    maxrtos_process_id_t next;
    maxrtos_buffer_status_t st;
    uint32_t v;

    setup();
    assert( maxrtos_buffer_create(
                0U, sizeof( uint32_t ), 3U, MAXRTOS_QUEUING_FIFO, &buf ) ==
            MAXRTOS_OK );
    a = make_process( 0U, 5U );
    b = make_process( 0U, 5U );
    other = make_process( 1U, 5U );
    assert( dispatch( 0U ) == a );

    assert( maxrtos_kernel_buffer_get_status( buf, a, &st ) == MAXRTOS_OK );
    assert( st.max_message_size == sizeof( uint32_t ) );
    assert( st.max_nb_message == 3U );
    assert( st.nb_message == 0U );
    assert( st.waiting_processes == 0U );

    v = 1U;
    assert( send_on( buf, &v, sizeof( v ), 0U, a, &next ) == MAXRTOS_OK );

    assert( maxrtos_kernel_buffer_get_status( buf, a, &st ) == MAXRTOS_OK );
    assert( st.nb_message == 1U );

    /* Fill it and block a sender. */
    assert( send_on( buf, &v, sizeof( v ), 0U, a, &next ) == MAXRTOS_OK );
    assert( send_on( buf, &v, sizeof( v ), 0U, a, &next ) == MAXRTOS_OK );
    assert( send_on( buf, &v, sizeof( v ), MAXRTOS_TIMEOUT_INFINITE, a,
                     &next ) == MAXRTOS_PENDING );
    assert( next == b );

    assert( maxrtos_kernel_buffer_get_status( buf, b, &st ) == MAXRTOS_OK );
    assert( st.nb_message == 3U );
    assert( st.waiting_processes == 1U );

    /* Errors leave the output alone. */
    st.nb_message = 77U;
    assert( maxrtos_kernel_buffer_get_status( buf, other, &st ) ==
            MAXRTOS_ERR_INVALID_ID );
    assert( maxrtos_kernel_buffer_get_status(
                MAXRTOS_INVALID_BUFFER_ID, a, &st ) ==
            MAXRTOS_ERR_INVALID_ID );
    assert( maxrtos_kernel_buffer_get_status( buf, a, NULL ) ==
            MAXRTOS_ERR_INVALID_ARG );
    assert( st.nb_message == 77U );

    printf( "test_get_status_reports_configuration_fill_and_waiters: PASS\n" );
}

int main( void )
{
    test_create_validates_arguments_and_exhausts_pool();
    test_send_rejects_wrong_message_size();
    test_single_send_receive_roundtrip_preserves_data();
    test_send_fails_when_genuinely_full();
    test_other_partition_and_bad_handles_rejected();
    test_blocked_receiver_gets_message_from_sender();
    test_blocked_sender_admitted_when_receiver_frees_slot();
    test_fifo_discipline_ignores_priority();
    test_priority_discipline_serves_by_priority_then_fifo();
    test_cannot_block_without_another_ready_process();
    test_timed_receive_expires_and_leaves_wait_list();
    test_get_status_reports_configuration_fill_and_waiters();

    printf( "all buffer tests passed\n" );

    return 0;
}
