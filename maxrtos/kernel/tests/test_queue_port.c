/**
 * @file test_queue_port.c
 * @brief Unit tests for the queuing port module.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "maxrtos/kernel/queue_port.h"

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

    assert( maxrtos_queue_port_count( &port ) == 0U );

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
        maxrtos_queue_port_receive(
            &port,
            buf,
            sizeof( buf ) ) == MAXRTOS_ERR_QUEUE_EMPTY );

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
        maxrtos_queue_port_send(
            &port,
            &message,
            3U ) == MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_queue_port_send(
            &port,
            &message,
            8U ) == MAXRTOS_ERR_INVALID_ARG );

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
        maxrtos_queue_port_send(
            &port,
            &message,
            4U ) == MAXRTOS_OK );

    assert(
        maxrtos_queue_port_receive(
            &port,
            small_buf,
            sizeof( small_buf ) )
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
        maxrtos_queue_port_send(
            &port,
            &sent_value,
            sizeof( sent_value ) ) == MAXRTOS_OK );

    assert( maxrtos_queue_port_count( &port ) == 1U );

    assert(
        maxrtos_queue_port_receive(
            &port,
            &received_value,
            sizeof( received_value ) ) == MAXRTOS_OK );

    assert( received_value == sent_value );
    assert( maxrtos_queue_port_count( &port ) == 0U );

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
            maxrtos_queue_port_send(
                &port,
                &i,
                sizeof( i ) ) == MAXRTOS_OK );
    }

    for( i = 0U; i < 5U; i++ )
    {
        assert(
            maxrtos_queue_port_receive(
                &port,
                &out,
                sizeof( out ) ) == MAXRTOS_OK );

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
            maxrtos_queue_port_send(
                &port,
                &value,
                sizeof( value ) ) == MAXRTOS_OK );
    }

    assert( maxrtos_queue_port_count( &port ) == 4U );

    value = 999U;

    assert(
        maxrtos_queue_port_send(
            &port,
            &value,
            sizeof( value ) ) == MAXRTOS_ERR_QUEUE_FULL );

    assert( maxrtos_queue_port_count( &port ) == 4U );

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
        maxrtos_queue_port_send(
            &port,
            &value,
            sizeof( value ) ) == MAXRTOS_OK );

    value = 2U;
    assert(
        maxrtos_queue_port_send(
            &port,
            &value,
            sizeof( value ) ) == MAXRTOS_OK );

    value = 3U;
    assert(
        maxrtos_queue_port_send(
            &port,
            &value,
            sizeof( value ) ) == MAXRTOS_OK );

    assert(
        maxrtos_queue_port_receive(
            &port,
            &out,
            sizeof( out ) ) == MAXRTOS_OK );

    assert( out == 1U );

    assert(
        maxrtos_queue_port_receive(
            &port,
            &out,
            sizeof( out ) ) == MAXRTOS_OK );

    assert( out == 2U );

    /* Head has advanced to slot 2 and one message remains.
     * Send three additional messages so the tail wraps around. */
    value = 4U;
    assert(
        maxrtos_queue_port_send(
            &port,
            &value,
            sizeof( value ) ) == MAXRTOS_OK );

    value = 5U;
    assert(
        maxrtos_queue_port_send(
            &port,
            &value,
            sizeof( value ) ) == MAXRTOS_OK );

    value = 6U;
    assert(
        maxrtos_queue_port_send(
            &port,
            &value,
            sizeof( value ) ) == MAXRTOS_OK );

    assert( maxrtos_queue_port_count( &port ) == 4U );

    assert(
        maxrtos_queue_port_receive(
            &port,
            &out,
            sizeof( out ) ) == MAXRTOS_OK );

    assert( out == 3U );

    assert(
        maxrtos_queue_port_receive(
            &port,
            &out,
            sizeof( out ) ) == MAXRTOS_OK );

    assert( out == 4U );

    assert(
        maxrtos_queue_port_receive(
            &port,
            &out,
            sizeof( out ) ) == MAXRTOS_OK );

    assert( out == 5U );

    assert(
        maxrtos_queue_port_receive(
            &port,
            &out,
            sizeof( out ) ) == MAXRTOS_OK );

    assert( out == 6U );

    assert(
        maxrtos_queue_port_receive(
            &port,
            &out,
            sizeof( out ) ) == MAXRTOS_ERR_QUEUE_EMPTY );

    printf(
        "test_wraparound_preserves_fifo_order: PASS\n" );
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

    printf( "all queue port tests passed\n" );

    return 0;
}
