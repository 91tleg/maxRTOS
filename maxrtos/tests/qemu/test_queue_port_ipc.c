/**
 * @file test_queue_port_ipc.c
 * @brief Queuing ports through the real SVC / PendSV path.
 *
 * Covers: blocking receive woken by a sender in the same partition and in
 * another partition; timed receive expiry; blocked sender admitted when a
 * receiver frees a slot, with FIFO order preserved; and rejection of
 * hostile pointers (kernel memory as message/buffer, a non-port as port).
 */

#include "harness.h"
#include "maxrtos/kernel/queue_port.h" /* sizeof only: the ports are opaque to applications */

#define QA ( ( maxrtos_queue_port_t * ) ( void * ) maxrtos_port_qa )
#define QB ( ( maxrtos_queue_port_t * ) ( void * ) maxrtos_port_qb )

enum
{
    R_S1_STATUS, R_S1_DATA, R_S1_DONE,
    R_S2_STATUS, R_S2_DATA, R_S2_DONE,
    R_S3_STATUS, R_S3_DONE,
    R_S4_SEND_BAD_MESSAGE, R_S4_RECEIVE_BAD_BUFFER, R_S4_SEND_BAD_PORT,
    R_S4_COUNT_BAD_PORT, R_S4_DONE,
    R_S5_SEND_STATUS, R_S5_FIRST, R_S5_DONE,
    R_S6_COUNT, R_S6_ORDER, R_S6_DONE
};

static void yield_times( uint32_t n )
{
    while( n-- > 0U )
    {
        maxrtos_yield();
    }
}

/* Runs S1 (blocking receive, woken from the same partition), S3 (timed
 * receive that nobody satisfies) and the consumer half of S5/S6. */
static void control_main( void * arg )
{
    uint32_t v = 0U;
    uint32_t x;
    uint32_t pack = 0U;
    uint32_t i;

    ( void ) arg;

    EMU_RES[ R_S1_STATUS ] = ( uint32_t ) maxrtos_queue_port_receive(
        QA, &v, sizeof( v ), MAXRTOS_TIMEOUT_INFINITE );
    EMU_RES[ R_S1_DATA ] = v;
    EMU_RES[ R_S1_DONE ] = 1U;

    EMU_RES[ R_S3_STATUS ] = ( uint32_t ) maxrtos_queue_port_receive(
        QA, &v, sizeof( v ), 5U );
    EMU_RES[ R_S3_DONE ] = 1U;

    while( ( EMU_RES[ R_S4_DONE ] == 0U ) ||
           ( maxrtos_queue_port_count( QA ) != 4U ) )
    {
        maxrtos_yield();
    }

    /* Frees a slot, which admits the sender blocked on the full queue. */
    ( void ) maxrtos_queue_port_receive( QA, &v, sizeof( v ), 0U );
    EMU_RES[ R_S5_FIRST ] = v;

    while( EMU_RES[ R_S5_DONE ] == 0U )
    {
        maxrtos_yield();
    }

    EMU_RES[ R_S6_COUNT ] = ( uint32_t ) maxrtos_queue_port_count( QA );

    for( i = 0U; i < 4U; i++ )
    {
        ( void ) maxrtos_queue_port_receive( QA, &x, sizeof( x ), 0U );
        pack = ( pack << 8U ) | ( x & 0xFFU );
    }

    EMU_RES[ R_S6_ORDER ] = pack;
    EMU_RES[ R_S6_DONE ] = 1U;

    for( ;; )
    {
        maxrtos_yield();
    }
}

/* Sends the S1 and S2 messages, probes hostile pointers (S4), then fills
 * the queue and blocks on a full send (S5). */
static void control_aux( void * arg )
{
    uint32_t m;
    uint32_t drained;

    ( void ) arg;

    yield_times( 30U );
    m = 0xCAFE0001UL;
    ( void ) maxrtos_queue_port_send( QA, &m, sizeof( m ), 0U );

    yield_times( 200U );
    m = 0xBEEF0002UL;
    ( void ) maxrtos_queue_port_send( QB, &m, sizeof( m ), 0U ); /* other partition */

    while( EMU_RES[ R_S3_DONE ] == 0U )
    {
        maxrtos_yield();
    }

    m = 1U;
    EMU_RES[ R_S4_SEND_BAD_MESSAGE ] = ( uint32_t ) maxrtos_queue_port_send(
        QA, ( void const * ) EMU_KERNEL_ADDRESS, sizeof( m ), 0U );

    ( void ) maxrtos_queue_port_send( QA, &m, sizeof( m ), 0U );
    EMU_RES[ R_S4_RECEIVE_BAD_BUFFER ] = ( uint32_t ) maxrtos_queue_port_receive(
        QA, ( void * ) EMU_KERNEL_ADDRESS, sizeof( m ), 0U );

    EMU_RES[ R_S4_SEND_BAD_PORT ] = ( uint32_t ) maxrtos_queue_port_send(
        ( maxrtos_queue_port_t * ) ( void * ) EMU_KERNEL_ADDRESS, &m, sizeof( m ), 0U );
    EMU_RES[ R_S4_COUNT_BAD_PORT ] = ( uint32_t ) maxrtos_queue_port_count(
        ( maxrtos_queue_port_t * ) ( void * ) EMU_KERNEL_ADDRESS );
    EMU_RES[ R_S4_DONE ] = 1U;

    while( maxrtos_queue_port_receive( QA, &drained, sizeof( drained ), 0U ) ==
           MAXRTOS_OK )
    {
    }

    for( m = 10U; m < 14U; m++ )
    {
        ( void ) maxrtos_queue_port_send( QA, &m, sizeof( m ), 0U ); /* capacity 4 */
    }

    m = 14U;
    EMU_RES[ R_S5_SEND_STATUS ] = ( uint32_t ) maxrtos_queue_port_send(
        QA, &m, sizeof( m ), MAXRTOS_TIMEOUT_INFINITE ); /* full: blocks */
    EMU_RES[ R_S5_DONE ] = 1U;

    for( ;; )
    {
        maxrtos_yield();
    }
}

/* S2: blocking receive in the application partition, woken by a sender in
 * the control partition. */
static void application_main( void * arg )
{
    uint32_t v = 0U;

    ( void ) arg;

    EMU_RES[ R_S2_STATUS ] = ( uint32_t ) maxrtos_queue_port_receive(
        QB, &v, sizeof( v ), MAXRTOS_TIMEOUT_INFINITE );
    EMU_RES[ R_S2_DATA ] = v;
    EMU_RES[ R_S2_DONE ] = 1U;

    for( ;; )
    {
        maxrtos_yield();
    }
}

/* Gives application_main a peer to run while it is blocked. */
static void application_aux( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        maxrtos_yield();
    }
}

void emu_test_setup( void )
{
    _Static_assert( sizeof( maxrtos_queue_port_t ) <= 1024U,
                    "results are stored past the queue header" );

    EMU_CREATE( control_main, PARTITION_ID_control, control_main );
    EMU_CREATE( control_aux, PARTITION_ID_control, control_aux );
    EMU_CREATE( application_main, PARTITION_ID_application, application_main );
    EMU_CREATE( application_aux, PARTITION_ID_application, application_aux );
}

#define CHECK( cond_, why_ ) \
    do { if( !( cond_ ) ) { return EMU_FAILED( why_ ); } } while( 0 )

int emu_test_poll( uint32_t tick )
{
    ( void ) tick;

    if( ( EMU_RES[ R_S6_DONE ] == 0U ) || ( EMU_RES[ R_S2_DONE ] == 0U ) )
    {
        return EMU_RUNNING;
    }

    CHECK( EMU_RES[ R_S1_STATUS ] == ( uint32_t ) MAXRTOS_OK,
           "S1: blocked receive (same partition) did not return OK" );
    CHECK( EMU_RES[ R_S1_DATA ] == 0xCAFE0001UL,
           "S1: blocked receive got the wrong data" );

    CHECK( EMU_RES[ R_S2_STATUS ] == ( uint32_t ) MAXRTOS_OK,
           "S2: blocked receive (cross partition) did not return OK" );
    CHECK( EMU_RES[ R_S2_DATA ] == 0xBEEF0002UL,
           "S2: blocked receive (cross partition) got the wrong data" );

    CHECK( EMU_RES[ R_S3_STATUS ] == ( uint32_t ) MAXRTOS_ERR_TIMEOUT,
           "S3: timed receive did not time out" );

    CHECK( EMU_RES[ R_S4_SEND_BAD_MESSAGE ] == ( uint32_t ) MAXRTOS_ERR_INVALID_ARG,
           "S4: kernel memory accepted as a message" );
    CHECK( EMU_RES[ R_S4_RECEIVE_BAD_BUFFER ] == ( uint32_t ) MAXRTOS_ERR_INVALID_ARG,
           "S4: kernel memory accepted as a receive buffer" );
    CHECK( EMU_RES[ R_S4_SEND_BAD_PORT ] == ( uint32_t ) MAXRTOS_ERR_INVALID_ARG,
           "S4: a non-port accepted as a port" );
    CHECK( EMU_RES[ R_S4_COUNT_BAD_PORT ] == 0U,
           "S4: count on a non-port did not report 0" );

    CHECK( EMU_RES[ R_S5_SEND_STATUS ] == ( uint32_t ) MAXRTOS_OK,
           "S5: blocked send did not complete OK" );
    CHECK( EMU_RES[ R_S5_FIRST ] == 10U, "S5: first message was not 10" );
    CHECK( EMU_RES[ R_S6_COUNT ] == 4U,
           "S6: queue does not hold 4 messages after the blocked send" );
    CHECK( EMU_RES[ R_S6_ORDER ] == 0x0B0C0D0EUL,
           "S6: FIFO order not preserved (expected 11,12,13,14)" );

    return EMU_PASS;
}
