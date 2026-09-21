/**
 * @file test_ipc_queue_port.c
 * @brief SIL: queuing ports through the real SVC path, MPU and scheduler.
 *
 * Requirements
 *   REQ-IPC-001  A blocked receive returns exactly the message sent, whether
 *                the sender is in the same partition or another one.
 *   REQ-IPC-002  A timed wait expires no earlier than its timeout, returns
 *                MAXRTOS_ERR_TIMEOUT, and leaves the port consistent.
 *   REQ-IPC-003  A stream through a bounded queue between partitions loses,
 *                duplicates and reorders nothing, with both sides blocking.
 *   REQ-IPC-004  A full queue rejects a non-blocking send, and a blocked
 *                sender is admitted, in order, when a slot frees.
 *   REQ-IPC-005  The kernel never dereferences a pointer the caller could not
 *                access itself: kernel memory, another partition's memory,
 *                non-member ports and malformed ranges are rejected, and
 *                nothing is read, written or disturbed.
 *   REQ-IPC-006  A message whose size differs from the port's is rejected.
 */

#include "sil_system.h"
#include "sil_test.h"

#include "maxrtos/kernel/queue_port.h"
#include "maxrtos/queue_port.h"
#include "maxrtos/yield.h"

#define QA ( sil_system()->port[ 0 ] ) /* member: partition 0 */
#define QB ( sil_system()->port[ 1 ] ) /* members: partitions 0 and 1 */
#define QC ( sil_system()->port[ 2 ] ) /* member: partition 1 only */

#define CANARY ( 0xC0FFEE42UL )

static void idle_yield( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        maxrtos_yield();
    }
}

static void yield_n( uint32_t n )
{
    while( n-- > 0U )
    {
        maxrtos_yield();
    }
}

static void build_ipc_system(
    void ( * p0_main )( void * ),
    void ( * p0_aux )( void * ),
    void ( * p1_main )( void * ),
    void ( * p1_aux )( void * ) )
{
    sil_system_spec_t spec;

    sil_spec_two_partitions( &spec, p0_main, p1_main );
    spec.partition[ 0 ].process_count = 2U;
    spec.partition[ 0 ].process[ 1 ].entry = p0_aux;
    spec.partition[ 0 ].process[ 1 ].priority = 5U;
    spec.partition[ 1 ].process_count = 2U;
    spec.partition[ 1 ].process[ 1 ].entry = p1_aux;
    spec.partition[ 1 ].process[ 1 ].priority = 5U;

    spec.port_count = 3U;
    spec.port[ 0 ].message_size = 4U;
    spec.port[ 0 ].capacity = 4U;
    spec.port[ 0 ].member_mask = 0x1U;
    spec.port[ 1 ].message_size = 4U;
    spec.port[ 1 ].capacity = 4U;
    spec.port[ 1 ].member_mask = 0x3U;
    spec.port[ 2 ].message_size = 4U;
    spec.port[ 2 ].capacity = 4U;
    spec.port[ 2 ].member_mask = 0x2U;

    sil_build( &spec );
}

/* ------------------------------------------------------------------------ */
/* REQ-IPC-001: blocking receive                                             */
/* ------------------------------------------------------------------------ */

static volatile maxrtos_status_t s_status;
static volatile uint32_t s_data;
static volatile uint32_t s_done;

static void receiver_on_qa( void * arg )
{
    uint32_t value = 0U;

    ( void ) arg;
    s_status = maxrtos_queue_port_receive( QA, &value, sizeof( value ), MAXRTOS_TIMEOUT_INFINITE );
    s_data = value;
    s_done = 1U;

    idle_yield( NULL );
}

static void sender_to_qa_after_delay( void * arg )
{
    uint32_t value = 0xCAFE0001UL;

    ( void ) arg;
    yield_n( 40U );
    ( void ) maxrtos_queue_port_send( QA, &value, sizeof( value ), 0U );

    idle_yield( NULL );
}

static void test_blocked_receive_same_partition( void )
{
    s_done = 0U;
    s_status = MAXRTOS_ERR_INVALID_STATE;
    s_data = 0U;

    build_ipc_system( receiver_on_qa, sender_to_qa_after_delay, idle_yield, idle_yield );
    SIL_REQUIRE( sim_start( 200U ) );

    SIL_REQUIRE_EQ( s_done, 1U );
    SIL_EXPECT_EQ( s_status, MAXRTOS_OK );
    SIL_EXPECT_EQ( s_data, 0xCAFE0001UL );
    SIL_EXPECT_EQ( maxrtos_kernel_queue_port_count( QA ), 0U );
    SIL_EXPECT_EQ( sim_model_violations(), 0U );
    sim_shutdown();
}

static void receiver_on_qb( void * arg )
{
    uint32_t value = 0U;

    ( void ) arg;
    s_status = maxrtos_queue_port_receive( QB, &value, sizeof( value ), MAXRTOS_TIMEOUT_INFINITE );
    s_data = value;
    s_done = 1U;

    idle_yield( NULL );
}

static void sender_to_qb_after_delay( void * arg )
{
    uint32_t value = 0xBEEF0002UL;

    ( void ) arg;
    yield_n( 40U );
    ( void ) maxrtos_queue_port_send( QB, &value, sizeof( value ), 0U );

    idle_yield( NULL );
}

static void test_blocked_receive_across_partitions( void )
{
    s_done = 0U;
    s_status = MAXRTOS_ERR_INVALID_STATE;
    s_data = 0U;

    /* Receiver in partition 1, sender in partition 0. */
    build_ipc_system( idle_yield, sender_to_qb_after_delay, receiver_on_qb, idle_yield );
    SIL_REQUIRE( sim_start( 200U ) );

    SIL_REQUIRE_EQ( s_done, 1U );
    SIL_EXPECT_EQ( s_status, MAXRTOS_OK );
    SIL_EXPECT_EQ( s_data, 0xBEEF0002UL );
    sim_shutdown();
}

/* ------------------------------------------------------------------------ */
/* REQ-IPC-002: timed wait                                                   */
/* ------------------------------------------------------------------------ */

#define TIMEOUT_TICKS ( 7U )

static volatile uint32_t s_wait_started;
static volatile uint32_t s_wait_ended;

static void timed_receiver( void * arg )
{
    uint32_t value = 0U;

    ( void ) arg;
    s_wait_started = sim_tick();
    s_status = maxrtos_queue_port_receive( QA, &value, sizeof( value ), TIMEOUT_TICKS );
    s_wait_ended = sim_tick();
    s_done = 1U;

    idle_yield( NULL );
}

static void test_timed_receive_expires_and_leaves_port_consistent( void )
{
    s_done = 0U;
    s_status = MAXRTOS_OK;

    build_ipc_system( timed_receiver, idle_yield, idle_yield, idle_yield );
    SIL_REQUIRE( sim_start( 120U ) );

    SIL_REQUIRE_EQ( s_done, 1U );
    SIL_EXPECT_EQ( s_status, MAXRTOS_ERR_TIMEOUT );

    /* Never early; and it resumes within its own partition's next chance
     * (at most one major frame, 8 ticks, after the deadline). */
    SIL_EXPECT( s_wait_ended >= s_wait_started + TIMEOUT_TICKS );
    SIL_EXPECT( s_wait_ended <= s_wait_started + TIMEOUT_TICKS + 8U );

    /* The port is intact: nothing queued, and it still works. */
    SIL_EXPECT_EQ( maxrtos_kernel_queue_port_count( QA ), 0U );
    SIL_EXPECT_EQ( QA->waiting_receivers.count, 0U );
    sim_shutdown();
}

/* ------------------------------------------------------------------------ */
/* REQ-IPC-003: stream between partitions                                    */
/* ------------------------------------------------------------------------ */

#define STREAM_LENGTH ( 500U )

static volatile uint32_t s_received;
static volatile uint32_t s_stream_errors;
static volatile uint32_t s_done_tick;

static void stream_producer( void * arg )
{
    uint32_t i;

    ( void ) arg;

    for( i = 0U; i < STREAM_LENGTH; i++ )
    {
        uint32_t value = i;

        if( maxrtos_queue_port_send( QB, &value, sizeof( value ), MAXRTOS_TIMEOUT_INFINITE ) != MAXRTOS_OK )
        {
            s_stream_errors++;
        }
    }

    idle_yield( NULL );
}

static void stream_consumer( void * arg )
{
    uint32_t expected = 0U;

    ( void ) arg;

    while( expected < STREAM_LENGTH )
    {
        uint32_t value = 0xFFFFFFFFUL;

        if( maxrtos_queue_port_receive( QB, &value, sizeof( value ), MAXRTOS_TIMEOUT_INFINITE ) != MAXRTOS_OK )
        {
            s_stream_errors++;
        }
        else if( value != expected )
        {
            s_stream_errors++; /* lost, duplicated or reordered */
        }

        expected++;
        s_received = expected;
    }

    s_done_tick = sim_tick();
    idle_yield( NULL );
}

static void test_stream_between_partitions_loses_nothing( void )
{
    s_received = 0U;
    s_stream_errors = 0U;
    s_done_tick = 0U;

    build_ipc_system( stream_producer, idle_yield, stream_consumer, idle_yield );
    SIL_REQUIRE( sim_start( 6000U ) );

    SIL_EXPECT_EQ( s_received, STREAM_LENGTH );
    SIL_EXPECT_EQ( s_stream_errors, 0U );
    SIL_EXPECT( s_done_tick != 0U );

    /* Everything drained; no process left parked on the port. */
    SIL_EXPECT_EQ( maxrtos_kernel_queue_port_count( QB ), 0U );
    SIL_EXPECT_EQ( QB->waiting_senders.count, 0U );
    SIL_EXPECT_EQ( sim_model_violations(), 0U );
    sim_shutdown();
}

/* ------------------------------------------------------------------------ */
/* REQ-IPC-004: capacity and blocked senders                                 */
/* ------------------------------------------------------------------------ */

static volatile maxrtos_status_t s_fill_status[ 4 ];
static volatile maxrtos_status_t s_full_status;
static volatile maxrtos_status_t s_blocked_send_status;
static volatile uint32_t s_drained[ 5 ];
static volatile uint32_t s_drained_count;

static void filler( void * arg )
{
    uint32_t value;

    ( void ) arg;

    for( value = 1U; value <= 4U; value++ )
    {
        s_fill_status[ value - 1U ] = maxrtos_queue_port_send( QA, &value, sizeof( value ), 0U );
    }

    value = 5U;
    s_full_status = maxrtos_queue_port_send( QA, &value, sizeof( value ), 0U );

    /* Blocks: the queue is full and its peer will make room. */
    value = 99U;
    s_blocked_send_status = maxrtos_queue_port_send( QA, &value, sizeof( value ), MAXRTOS_TIMEOUT_INFINITE );
    s_done = 1U;

    idle_yield( NULL );
}

static void drainer( void * arg )
{
    uint32_t value;

    ( void ) arg;

    /* Wait until the filler has filled the queue and blocked. */
    while( maxrtos_queue_port_count( QA ) < 4U )
    {
        maxrtos_yield();
    }

    yield_n( 20U );

    while( s_drained_count < 5U )
    {
        if( maxrtos_queue_port_receive( QA, &value, sizeof( value ), 0U ) == MAXRTOS_OK )
        {
            s_drained[ s_drained_count++ ] = value;
        }
        else
        {
            maxrtos_yield();
        }
    }

    idle_yield( NULL );
}

static void test_full_queue_rejects_and_blocked_sender_is_admitted_in_order( void )
{
    unsigned i;

    s_done = 0U;
    s_drained_count = 0U;
    s_full_status = MAXRTOS_OK;
    s_blocked_send_status = MAXRTOS_ERR_INVALID_STATE;

    build_ipc_system( filler, drainer, idle_yield, idle_yield );
    SIL_REQUIRE( sim_start( 300U ) );

    for( i = 0U; i < 4U; i++ )
    {
        SIL_EXPECT_EQ( s_fill_status[ i ], MAXRTOS_OK );
    }

    SIL_EXPECT_EQ( s_full_status, MAXRTOS_ERR_QUEUE_FULL );
    SIL_REQUIRE_EQ( s_done, 1U );
    SIL_EXPECT_EQ( s_blocked_send_status, MAXRTOS_OK );

    /* FIFO order, with the blocked sender's message last. */
    SIL_REQUIRE_EQ( s_drained_count, 5U );
    SIL_EXPECT_EQ( s_drained[ 0 ], 1U );
    SIL_EXPECT_EQ( s_drained[ 1 ], 2U );
    SIL_EXPECT_EQ( s_drained[ 2 ], 3U );
    SIL_EXPECT_EQ( s_drained[ 3 ], 4U );
    SIL_EXPECT_EQ( s_drained[ 4 ], 99U );
    sim_shutdown();
}

/* ------------------------------------------------------------------------ */
/* REQ-IPC-005: hostile pointers                                             */
/* ------------------------------------------------------------------------ */

enum
{
    H_SEND_KERNEL_MESSAGE,
    H_RECEIVE_KERNEL_BUFFER,
    H_BUFFER_STRADDLES_DOMAIN_END,
    H_BUFFER_IN_OTHER_PARTITION,
    H_PORT_IS_NOT_A_PORT,
    H_PORT_NOT_A_MEMBER_SEND,
    H_COUNT_NOT_A_MEMBER,
    H_SIZE_WRAPS_ADDRESS_SPACE,
    H_SIZE_ZERO,
    H_PORT_UNMAPPED,
    H_MESSAGE_IN_PORT_MEMORY_OF_NON_MEMBER,
    H_LEGITIMATE_SEND,
    H_COUNT
};

static volatile uint32_t s_probe[ H_COUNT ];
static volatile uint32_t s_probes_done;

static void hostile( void * arg )
{
    sil_system_t const * sys = sil_system();
    uint32_t good = 0x600DU;

    ( void ) arg;

    /* Something for the receive probes to (try to) take. */
    ( void ) maxrtos_queue_port_send( QA, &good, sizeof( good ), 0U );

    s_probe[ H_SEND_KERNEL_MESSAGE ] = ( uint32_t ) sim_queue_send_raw(
        sys->port_address[ 0 ], SIL_KERNEL_GUARD, 4U, 0U );

    s_probe[ H_RECEIVE_KERNEL_BUFFER ] = ( uint32_t ) sim_queue_receive_raw(
        sys->port_address[ 0 ], SIL_KERNEL_GUARD, 4U, 0U );

    s_probe[ H_BUFFER_STRADDLES_DOMAIN_END ] = ( uint32_t ) sim_queue_receive_raw(
        sys->port_address[ 0 ], sys->domain_base[ 0 ] + sys->domain_size[ 0 ] - 2U, 4U, 0U );

    s_probe[ H_BUFFER_IN_OTHER_PARTITION ] = ( uint32_t ) sim_queue_receive_raw(
        sys->port_address[ 0 ], sys->domain_base[ 1 ] + 16U, 4U, 0U );

    /* An address inside the caller's own domain is not a port. */
    s_probe[ H_PORT_IS_NOT_A_PORT ] = ( uint32_t ) sim_queue_send_raw(
        sys->domain_base[ 0 ] + 64U, ( uint32_t ) 0U + sim_ptr_to_uaddr( &good ), 4U, 0U );

    /* Port QC exists but partition 0 is not a member. */
    s_probe[ H_PORT_NOT_A_MEMBER_SEND ] = ( uint32_t ) sim_queue_send_raw(
        sys->port_address[ 2 ], sim_ptr_to_uaddr( &good ), 4U, 0U );
    s_probe[ H_COUNT_NOT_A_MEMBER ] = sim_queue_count_raw( sys->port_address[ 2 ] );

    s_probe[ H_SIZE_WRAPS_ADDRESS_SPACE ] = ( uint32_t ) sim_queue_send_raw(
        sys->port_address[ 0 ], sim_ptr_to_uaddr( &good ), 0xFFFFFFF0U, 0U );

    s_probe[ H_SIZE_ZERO ] = ( uint32_t ) sim_queue_send_raw(
        sys->port_address[ 0 ], sim_ptr_to_uaddr( &good ), 0U, 0U );

    s_probe[ H_PORT_UNMAPPED ] = ( uint32_t ) sim_queue_send_raw(
        0U, sim_ptr_to_uaddr( &good ), 4U, 0U );

    /* The message points into a port region this partition may not access. */
    s_probe[ H_MESSAGE_IN_PORT_MEMORY_OF_NON_MEMBER ] = ( uint32_t ) sim_queue_send_raw(
        sys->port_address[ 0 ], sys->port_address[ 2 ] + 1024U, 4U, 0U );

    /* Rejection is not blanket: a legitimate call still works. */
    s_probe[ H_LEGITIMATE_SEND ] = ( uint32_t ) maxrtos_queue_port_send( QA, &good, sizeof( good ), 0U );

    s_probes_done = 1U;
    idle_yield( NULL );
}

static void test_kernel_rejects_hostile_pointers( void )
{
    uint32_t * guard;
    uint32_t * victim;
    uint32_t canary_before;
    unsigned i;
    sil_system_t * sys;

    s_probes_done = 0U;

    build_ipc_system( hostile, idle_yield, idle_yield, idle_yield );
    sys = sil_system();

    /* Canaries the probes would corrupt if the kernel followed the pointers. */
    guard = sim_mem_ptr( SIL_KERNEL_GUARD, 4U );
    victim = sim_mem_ptr( sys->domain_base[ 1 ] + 16U, 4U );
    SIL_REQUIRE( ( guard != NULL ) && ( victim != NULL ) );
    *guard = CANARY;
    *victim = CANARY;
    canary_before = CANARY;

    SIL_REQUIRE( sim_start( 60U ) );
    SIL_REQUIRE_EQ( s_probes_done, 1U );

    SIL_EXPECT_EQ( s_probe[ H_SEND_KERNEL_MESSAGE ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ( s_probe[ H_RECEIVE_KERNEL_BUFFER ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ( s_probe[ H_BUFFER_STRADDLES_DOMAIN_END ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ( s_probe[ H_BUFFER_IN_OTHER_PARTITION ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ( s_probe[ H_PORT_IS_NOT_A_PORT ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ( s_probe[ H_PORT_NOT_A_MEMBER_SEND ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ( s_probe[ H_COUNT_NOT_A_MEMBER ], 0U );
    SIL_EXPECT_EQ( s_probe[ H_SIZE_WRAPS_ADDRESS_SPACE ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ( s_probe[ H_SIZE_ZERO ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ( s_probe[ H_PORT_UNMAPPED ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ( s_probe[ H_MESSAGE_IN_PORT_MEMORY_OF_NON_MEMBER ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ( s_probe[ H_LEGITIMATE_SEND ], MAXRTOS_OK );

    /* Nothing was read or written through a hostile pointer... */
    SIL_EXPECT_EQ( *guard, canary_before );
    SIL_EXPECT_EQ( *victim, canary_before );

    /* ...the good messages are the only ones queued, and nothing faulted. */
    SIL_EXPECT_EQ( maxrtos_kernel_queue_port_count( QA ), 2U );
    SIL_EXPECT_EQ( maxrtos_kernel_queue_port_count( QC ), 0U );
    SIL_EXPECT( !sim_halted() );

    for( i = 0U; i < 8U; i++ )
    {
        SIL_EXPECT( sys->hm.policy[ 0 ][ 0 ] == MAXRTOS_HM_ACTION_RESTART_PROCESS );
    }

    sim_shutdown();
}

/* ------------------------------------------------------------------------ */
/* REQ-IPC-006: message size                                                 */
/* ------------------------------------------------------------------------ */

static volatile maxrtos_status_t s_size_status[ 3 ];

static void size_prober( void * arg )
{
    uint8_t buffer[ 16 ] = { 0U };

    ( void ) arg;
    s_size_status[ 0 ] = maxrtos_queue_port_send( QA, buffer, 3U, 0U );  /* too small */
    s_size_status[ 1 ] = maxrtos_queue_port_send( QA, buffer, 8U, 0U );  /* too large */
    s_size_status[ 2 ] = maxrtos_queue_port_send( QA, buffer, 4U, 0U );  /* exact */
    s_done = 1U;

    idle_yield( NULL );
}

static void test_wrong_message_size_is_rejected( void )
{
    s_done = 0U;
    build_ipc_system( size_prober, idle_yield, idle_yield, idle_yield );
    SIL_REQUIRE( sim_start( 30U ) );

    SIL_REQUIRE_EQ( s_done, 1U );
    SIL_EXPECT_EQ( s_size_status[ 0 ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ( s_size_status[ 1 ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ( s_size_status[ 2 ], MAXRTOS_OK );
    SIL_EXPECT_EQ( maxrtos_kernel_queue_port_count( QA ), 1U );
    sim_shutdown();
}

static sil_case_t const s_cases[] =
{
    SIL_CASE( test_blocked_receive_same_partition, "REQ-IPC-001", "Blocked receive returns the message (sender in the same partition)" ),
    SIL_CASE( test_blocked_receive_across_partitions, "REQ-IPC-001", "Blocked receive returns the message (sender in another partition)" ),
    SIL_CASE( test_timed_receive_expires_and_leaves_port_consistent, "REQ-IPC-002", "A timed receive expires on time with TIMEOUT and leaves the port intact" ),
    SIL_CASE( test_stream_between_partitions_loses_nothing, "REQ-IPC-003", "500 messages across partitions: none lost, duplicated or reordered" ),
    SIL_CASE( test_full_queue_rejects_and_blocked_sender_is_admitted_in_order, "REQ-IPC-004", "Full queue rejects; a blocked sender is admitted in FIFO order" ),
    SIL_CASE( test_kernel_rejects_hostile_pointers, "REQ-IPC-005", "Hostile pointers are rejected and nothing is touched" ),
    SIL_CASE( test_wrong_message_size_is_rejected, "REQ-IPC-006", "A message of the wrong size is rejected" ),
};

int main( int argc, char ** argv )
{
    int status = sil_run_suite( "ipc_queue_port", s_cases,
                                sizeof( s_cases ) / sizeof( s_cases[ 0 ] ), argc, argv );

    sim_shutdown();

    return status;
}
