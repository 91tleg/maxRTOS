/**
 * @file test_sampling_port.c
 * @brief Unit tests for the sampling port module.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/sampling_port.h"

#define STORAGE_SIZE ( 32U )
#define P0           ( UINT32_C( 1 ) << 0U )
#define P1           ( UINT32_C( 1 ) << 1U )
#define P2           ( UINT32_C( 1 ) << 2U )

static uint8_t s_storage[ MAXRTOS_MAX_SAMPLING_PORTS ][ STORAGE_SIZE ];
static uint8_t s_stacks[ 3 ][ 128 ];

static void dummy_entry( void * arg )
{
    ( void ) arg;
}

/* Processes a, b, c in partitions 0, 1, 2, and an empty port pool. */
static void make_world(
    maxrtos_process_id_t * a,
    maxrtos_process_id_t * b,
    maxrtos_process_id_t * c )
{
    maxrtos_process_id_t * const out[ 3 ] = { a, b, c };
    unsigned i;

    maxrtos_process_pool_init();
    maxrtos_sampling_port_pool_init();

    for( i = 0U; i < 3U; i++ )
    {
        assert(
            maxrtos_process_create(
                s_stacks[ i ],
                sizeof( s_stacks[ i ] ),
                ( maxrtos_partition_id_t ) i,
                5U,
                dummy_entry,
                NULL,
                out[ i ] ) == MAXRTOS_OK );
    }
}

static maxrtos_sampling_port_id_t make_port(
    size_t message_size,
    uint32_t writers,
    uint32_t readers )
{
    maxrtos_sampling_port_id_t id;

    assert(
        maxrtos_sampling_port_create(
            s_storage[ 0 ],
            sizeof( s_storage[ 0 ] ),
            message_size,
            writers,
            readers,
            &id ) == MAXRTOS_OK );

    return id;
}

static void test_create_rejects_bad_args( void )
{
    maxrtos_sampling_port_id_t id;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t c;

    make_world( &a, &b, &c );

    assert(
        maxrtos_sampling_port_create( NULL, STORAGE_SIZE, 4U, P0, P1, &id ) ==
        MAXRTOS_ERR_INVALID_ARG );
    assert(
        maxrtos_sampling_port_create(
            s_storage[ 0 ], STORAGE_SIZE, 4U, P0, P1, NULL ) ==
        MAXRTOS_ERR_INVALID_ARG );
    assert(
        maxrtos_sampling_port_create(
            s_storage[ 0 ], STORAGE_SIZE, 0U, P0, P1, &id ) ==
        MAXRTOS_ERR_INVALID_ARG );

    /* The storage must hold the message. */
    assert(
        maxrtos_sampling_port_create( s_storage[ 0 ], 3U, 4U, P0, P1, &id ) ==
        MAXRTOS_ERR_INVALID_ARG );

    /* Both ends need a partition, and only partitions that exist. */
    assert(
        maxrtos_sampling_port_create(
            s_storage[ 0 ], STORAGE_SIZE, 4U, 0U, P1, &id ) ==
        MAXRTOS_ERR_INVALID_ARG );
    assert(
        maxrtos_sampling_port_create(
            s_storage[ 0 ], STORAGE_SIZE, 4U, P0, 0U, &id ) ==
        MAXRTOS_ERR_INVALID_ARG );
    assert(
        maxrtos_sampling_port_create(
            s_storage[ 0 ],
            STORAGE_SIZE,
            4U,
            UINT32_C( 1 ) << MAXRTOS_MAX_PARTITIONS,
            P1,
            &id ) == MAXRTOS_ERR_INVALID_ARG );

    /* None of the failures used a slot. */
    assert(
        maxrtos_sampling_port_create(
            s_storage[ 0 ], STORAGE_SIZE, 4U, P0, P1, &id ) == MAXRTOS_OK );
    assert( id == 0U );

    printf( "test_create_rejects_bad_args: PASS\n" );
}

static void test_the_pool_fills_in_order_and_empties_on_init( void )
{
    maxrtos_sampling_port_id_t id;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t c;
    uint32_t v;
    size_t i;

    make_world( &a, &b, &c );

    for( i = 0U; i < MAXRTOS_MAX_SAMPLING_PORTS; i++ )
    {
        assert(
            maxrtos_sampling_port_create(
                s_storage[ i ], STORAGE_SIZE, 4U, P0, P1, &id ) == MAXRTOS_OK );
        assert( id == ( maxrtos_sampling_port_id_t ) i );
    }

    assert(
        maxrtos_sampling_port_create(
            s_storage[ 0 ], STORAGE_SIZE, 4U, P0, P1, &id ) ==
        MAXRTOS_ERR_POOL_FULL );

    maxrtos_sampling_port_pool_init();
    v = 1U;
    assert(
        maxrtos_kernel_sampling_port_write( 0U, &v, 4U, a ) ==
        MAXRTOS_ERR_INVALID_ID );

    printf( "test_the_pool_fills_in_order_and_empties_on_init: PASS\n" );
}

/* A read before the first write fails; a read returns the latest message and
 * leaves it; a write replaces it. */
static void test_write_replaces_and_read_leaves_the_message( void )
{
    maxrtos_sampling_port_id_t id;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t c;
    maxrtos_sampling_port_status_t st;
    uint32_t v;
    uint32_t out;

    make_world( &a, &b, &c );
    id = make_port( 4U, P0, P1 );

    assert(
        maxrtos_kernel_sampling_port_get_status( id, a, &st ) == MAXRTOS_OK );
    assert( st.message_size == 4U );
    assert( st.has_message == false );

    out = 0xFFFFFFFFU;
    assert(
        maxrtos_kernel_sampling_port_read( id, &out, 4U, b ) ==
        MAXRTOS_ERR_QUEUE_EMPTY );
    assert( out == 0xFFFFFFFFU );

    v = 0xDEADBEEFU;
    assert( maxrtos_kernel_sampling_port_write( id, &v, 4U, a ) == MAXRTOS_OK );
    assert(
        maxrtos_kernel_sampling_port_get_status( id, b, &st ) == MAXRTOS_OK );
    assert( st.has_message == true );

    /* A read does not consume the message. */
    out = 0U;
    assert(
        maxrtos_kernel_sampling_port_read( id, &out, 4U, b ) == MAXRTOS_OK );
    assert( out == 0xDEADBEEFU );
    out = 0U;
    assert(
        maxrtos_kernel_sampling_port_read( id, &out, 4U, b ) == MAXRTOS_OK );
    assert( out == 0xDEADBEEFU );

    /* A second write replaces the first, with nothing queued behind it. */
    v = 0x12345678U;
    assert( maxrtos_kernel_sampling_port_write( id, &v, 4U, a ) == MAXRTOS_OK );
    out = 0U;
    assert(
        maxrtos_kernel_sampling_port_read( id, &out, 4U, b ) == MAXRTOS_OK );
    assert( out == 0x12345678U );

    printf( "test_write_replaces_and_read_leaves_the_message: PASS\n" );
}

/* The message goes in and out whole and only that many bytes are touched. */
static void test_a_message_round_trips_and_nothing_else_is_written( void )
{
    maxrtos_sampling_port_id_t id;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t c;
    uint8_t in[ 6 ] = { 1U, 2U, 3U, 4U, 5U, 6U };
    uint8_t out[ 10 ];
    unsigned i;

    make_world( &a, &b, &c );
    id = make_port( 6U, P0, P1 );

    assert(
        maxrtos_kernel_sampling_port_write( id, in, sizeof( in ), a ) ==
        MAXRTOS_OK );

    ( void ) memset( out, 0xAA, sizeof( out ) );
    assert(
        maxrtos_kernel_sampling_port_read( id, out, sizeof( out ), b ) ==
        MAXRTOS_OK );

    assert( memcmp( out, in, sizeof( in ) ) == 0 );

    for( i = sizeof( in ); i < sizeof( out ); i++ )
    {
        assert( out[ i ] == 0xAAU );
    }

    printf( "test_a_message_round_trips_and_nothing_else_is_written: PASS\n" );
}

static void test_sizes_and_null_are_checked( void )
{
    maxrtos_sampling_port_id_t id;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t c;
    uint8_t buf[ 8 ] = { 0 };

    make_world( &a, &b, &c );
    id = make_port( 4U, P0, P1 );

    /* A write must be exactly the port's size. */
    assert(
        maxrtos_kernel_sampling_port_write( id, buf, 3U, a ) ==
        MAXRTOS_ERR_INVALID_ARG );
    assert(
        maxrtos_kernel_sampling_port_write( id, buf, 8U, a ) ==
        MAXRTOS_ERR_INVALID_ARG );
    assert(
        maxrtos_kernel_sampling_port_write( id, NULL, 4U, a ) ==
        MAXRTOS_ERR_INVALID_ARG );

    /* A refused write left the port empty. */
    assert(
        maxrtos_kernel_sampling_port_read( id, buf, sizeof( buf ), b ) ==
        MAXRTOS_ERR_QUEUE_EMPTY );

    assert(
        maxrtos_kernel_sampling_port_write( id, buf, 4U, a ) == MAXRTOS_OK );

    /* A read buffer must hold a message. */
    assert(
        maxrtos_kernel_sampling_port_read( id, buf, 3U, b ) ==
        MAXRTOS_ERR_INVALID_ARG );
    assert(
        maxrtos_kernel_sampling_port_read( id, NULL, 4U, b ) ==
        MAXRTOS_ERR_INVALID_ARG );
    assert(
        maxrtos_kernel_sampling_port_get_status( id, a, NULL ) ==
        MAXRTOS_ERR_INVALID_ARG );

    printf( "test_sizes_and_null_are_checked: PASS\n" );
}

/* Only writers write, only readers read, and a partition on neither does
 * nothing and sees nothing. A refused call changes nothing. */
static void test_only_writers_write_and_only_readers_read( void )
{
    maxrtos_sampling_port_id_t id;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t c;
    maxrtos_sampling_port_status_t st;
    uint32_t v;
    uint32_t out;

    /* 0 writes, 1 reads, 2 is not on the port. */
    make_world( &a, &b, &c );
    id = make_port( 4U, P0, P1 );

    v = 11U;
    assert( maxrtos_kernel_sampling_port_write( id, &v, 4U, a ) == MAXRTOS_OK );

    /* The writer may not read. */
    out = 0U;
    assert(
        maxrtos_kernel_sampling_port_read( id, &out, 4U, a ) ==
        MAXRTOS_ERR_INVALID_ID );
    assert( out == 0U );

    /* The reader may not write, and the attempt leaves the message alone. */
    v = 22U;
    assert(
        maxrtos_kernel_sampling_port_write( id, &v, 4U, b ) ==
        MAXRTOS_ERR_INVALID_ID );

    /* A partition that is neither does nothing and sees nothing. */
    assert(
        maxrtos_kernel_sampling_port_write( id, &v, 4U, c ) ==
        MAXRTOS_ERR_INVALID_ID );
    assert(
        maxrtos_kernel_sampling_port_read( id, &out, 4U, c ) ==
        MAXRTOS_ERR_INVALID_ID );
    assert(
        maxrtos_kernel_sampling_port_get_status( id, c, &st ) ==
        MAXRTOS_ERR_INVALID_ID );

    out = 0U;
    assert(
        maxrtos_kernel_sampling_port_read( id, &out, 4U, b ) == MAXRTOS_OK );
    assert( out == 11U );

    /* A partition may be both writer and reader. */
    maxrtos_sampling_port_pool_init();
    id = make_port( 4U, P0 | P1, P0 | P1 );
    v = 33U;
    assert( maxrtos_kernel_sampling_port_write( id, &v, 4U, b ) == MAXRTOS_OK );
    assert(
        maxrtos_kernel_sampling_port_read( id, &out, 4U, b ) == MAXRTOS_OK );
    assert( out == 33U );

    printf( "test_only_writers_write_and_only_readers_read: PASS\n" );
}

static void test_unknown_ids_and_processes_are_refused( void )
{
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t c;
    maxrtos_sampling_port_id_t id;
    maxrtos_sampling_port_status_t st;
    uint32_t v;

    make_world( &a, &b, &c );
    id = make_port( 4U, P0 | P1, P0 | P1 );
    v = 1U;

    /* Only port 0 exists. */
    assert(
        maxrtos_kernel_sampling_port_write( 1U, &v, 4U, a ) ==
        MAXRTOS_ERR_INVALID_ID );
    assert(
        maxrtos_kernel_sampling_port_write(
            ( maxrtos_sampling_port_id_t ) MAXRTOS_MAX_SAMPLING_PORTS,
            &v,
            4U,
            a ) == MAXRTOS_ERR_INVALID_ID );
    assert(
        maxrtos_kernel_sampling_port_read(
            MAXRTOS_INVALID_SAMPLING_PORT_ID, &v, 4U, a ) ==
        MAXRTOS_ERR_INVALID_ID );
    assert(
        maxrtos_kernel_sampling_port_get_status( 1U, a, &st ) ==
        MAXRTOS_ERR_INVALID_ID );

    /* No such process. */
    assert(
        maxrtos_kernel_sampling_port_write( id, &v, 4U, 7U ) ==
        MAXRTOS_ERR_INVALID_ID );
    assert(
        maxrtos_kernel_sampling_port_read(
            id, &v, 4U, MAXRTOS_INVALID_PROCESS_ID ) ==
        MAXRTOS_ERR_INVALID_ID );

    printf( "test_unknown_ids_and_processes_are_refused: PASS\n" );
}

/* Ports are independent: each holds its own message of its own size. */
static void test_ports_do_not_share_messages( void )
{
    maxrtos_sampling_port_id_t first;
    maxrtos_sampling_port_id_t second;
    maxrtos_process_id_t a;
    maxrtos_process_id_t b;
    maxrtos_process_id_t c;
    uint32_t wide;
    uint8_t narrow;
    uint32_t out_wide;
    uint8_t out_narrow;

    make_world( &a, &b, &c );

    assert(
        maxrtos_sampling_port_create(
            s_storage[ 0 ], STORAGE_SIZE, 4U, P0, P1, &first ) == MAXRTOS_OK );
    assert(
        maxrtos_sampling_port_create(
            s_storage[ 1 ], STORAGE_SIZE, 1U, P0, P1, &second ) == MAXRTOS_OK );

    wide = 0xA1B2C3D4U;
    narrow = 0x5AU;
    assert(
        maxrtos_kernel_sampling_port_write( first, &wide, 4U, a ) ==
        MAXRTOS_OK );
    assert(
        maxrtos_kernel_sampling_port_write( second, &narrow, 1U, a ) ==
        MAXRTOS_OK );

    out_wide = 0U;
    out_narrow = 0U;
    assert(
        maxrtos_kernel_sampling_port_read( first, &out_wide, 4U, b ) ==
        MAXRTOS_OK );
    assert(
        maxrtos_kernel_sampling_port_read( second, &out_narrow, 1U, b ) ==
        MAXRTOS_OK );
    assert( out_wide == 0xA1B2C3D4U );
    assert( out_narrow == 0x5AU );

    printf( "test_ports_do_not_share_messages: PASS\n" );
}

int main( void )
{
    test_create_rejects_bad_args();
    test_the_pool_fills_in_order_and_empties_on_init();
    test_write_replaces_and_read_leaves_the_message();
    test_a_message_round_trips_and_nothing_else_is_written();
    test_sizes_and_null_are_checked();
    test_only_writers_write_and_only_readers_read();
    test_unknown_ids_and_processes_are_refused();
    test_ports_do_not_share_messages();

    printf( "all sampling port tests passed\n" );

    return 0;
}
