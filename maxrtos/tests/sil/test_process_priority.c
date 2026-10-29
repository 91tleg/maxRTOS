/**
 * @file test_process_priority.c
 * @brief SIL: process scheduling through the SVC and tick paths.
 *
 * Priority-preemptive within a partition: the running process keeps the CPU
 * until it blocks, waits, or a READY process of strictly higher priority
 * exists, and processes of equal priority are not time sliced.
 *
 * Verifies
 * - A process that never blocks or yields starves an equal-priority peer.
 * - TIMED_WAIT with a zero delay is the round-robin point: equal-priority
 *   peers alternate strictly.
 * - A higher-priority process whose timed wait expires preempts a lower one
 *   that never gives the CPU up.
 * - A higher-priority process woken by a message preempts the sender at once,
 *   before the sender's next statement; an equal-priority one does not.
 */

#include "sil_system.h"
#include "sil_test.h"

#include "maxrtos/queue_port.h"
#include "maxrtos/timing.h"
#include "maxrtos/yield.h"

#define QA ( sil_system()->port[ 0 ] ) /* member: partition 0 */

#define TRACE_LENGTH ( 600U )

static volatile uint8_t s_trace[ TRACE_LENGTH ];
static volatile uint32_t s_trace_length;
static volatile uint32_t s_count[ 4 ];

static void record( uint8_t who )
{
    if( s_trace_length < TRACE_LENGTH )
    {
        s_trace[ s_trace_length++ ] = who;
    }
}

static void reset( void )
{
    unsigned i;

    s_trace_length = 0U;

    for( i = 0U; i < 4U; i++ )
    {
        s_count[ i ] = 0U;
    }
}

static void idle_yield( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        maxrtos_yield();
    }
}

/* Partition 0 holds the two processes under test; partition 1 idles. */
static void build(
    void ( *first )( void * ),
    uint8_t first_priority,
    void ( *second )( void * ),
    uint8_t second_priority,
    bool with_port )
{
    sil_system_spec_t spec;

    sil_spec_two_partitions( &spec, first, idle_yield );
    spec.partition[ 0 ].process_count = 2U;
    spec.partition[ 0 ].process[ 0 ].priority = first_priority;
    spec.partition[ 0 ].process[ 1 ].entry = second;
    spec.partition[ 0 ].process[ 1 ].priority = second_priority;

    if( with_port )
    {
        spec.port_count = 1U;
        spec.port[ 0 ].message_size = 4U;
        spec.port[ 0 ].capacity = 4U;
        spec.port[ 0 ].member_mask = 0x1U;
    }

    sil_build( &spec );
}

/* ---- no time slicing ------------------------------------------------------
 */

static void hog( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        s_count[ 0 ]++;
        sim_cpu_work( 100U );
    }
}

static void starved_peer( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        s_count[ 1 ]++;
        maxrtos_yield();
    }
}

static void test_equal_priority_peer_is_not_time_sliced( void )
{
    reset();
    build( hog, 5U, starved_peer, 5U, false );
    SIL_REQUIRE( sim_start( 120U ) );

    SIL_EXPECT( s_count[ 0 ] > 100U );
    SIL_EXPECT_EQ( s_count[ 1 ], 0U ); /* many ticks passed; it never ran */
    SIL_EXPECT( !sim_halted() );
    sim_shutdown();
}

/* ---- TIMED_WAIT(0) is the round-robin point ----------------------------- */

static void wait_zero_a( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        record( 'A' );
        ( void ) maxrtos_timed_wait( 0U );
    }
}

static void wait_zero_b( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        record( 'B' );
        ( void ) maxrtos_timed_wait( 0U );
    }
}

static void test_timed_wait_zero_alternates_equal_priority_peers( void )
{
    uint32_t i;
    uint32_t repeats = 0U;

    reset();
    build( wait_zero_a, 5U, wait_zero_b, 5U, false );
    SIL_REQUIRE( sim_start( 200U ) );
    SIL_REQUIRE( s_trace_length >= 100U );

    /* No tick can rotate them, so a strict A B A B... is the only order. */
    for( i = 1U; i < s_trace_length; i++ )
    {
        if( s_trace[ i ] == s_trace[ i - 1U ] )
        {
            repeats++;
        }
    }

    SIL_EXPECT_EQ( repeats, 0U );
    SIL_EXPECT_EQ( s_trace[ 0 ], 'A' );
    sim_shutdown();
}

/* ---- a timer wake-up preempts at once ----------------------------------- */

static void sleeper_high( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        ( void ) maxrtos_timed_wait( 3U );
        s_count[ 0 ]++;
    }
}

static void busy_low( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        s_count[ 1 ]++;
        sim_cpu_work( 100U );
    }
}

static void test_expired_wait_preempts_a_busy_lower_priority_process( void )
{
    reset();
    build( sleeper_high, 2U, busy_low, 5U, false );
    SIL_REQUIRE( sim_start( 80U ) );

    /* Partition 0 has 5 of every 8 ticks: about 50 in 80, a wake-up every 3
     * or more. The busy process never yields, so every wake-up is a
     * preemption. */
    SIL_EXPECT( s_count[ 0 ] >= 8U );
    SIL_EXPECT( s_count[ 1 ] > 100U ); /* it still gets the time in between */
    sim_shutdown();
}

/* ---- a message wake-up preempts the sender at once ---------------------- */

static void receiver( void * arg )
{
    uint32_t value;

    ( void ) arg;

    for( ;; )
    {
        if( maxrtos_queue_port_receive(
                QA, &value, sizeof( value ), MAXRTOS_TIMEOUT_INFINITE ) ==
            MAXRTOS_OK )
        {
            record( 'H' );
            s_count[ 0 ]++;
        }
    }
}

static void sender( void * arg )
{
    uint32_t value = 0x1234U;

    ( void ) arg;

    for( ;; )
    {
        record( 'S' );
        ( void ) maxrtos_queue_port_send( QA, &value, sizeof( value ), 0U );
        record( 'L' );
    }
}

static void test_message_wakeup_preempts_a_lower_priority_sender( void )
{
    uint32_t i;
    uint32_t sends = 0U;

    reset();
    build( receiver, 2U, sender, 5U, true );
    SIL_REQUIRE( sim_start( 60U ) );
    SIL_REQUIRE( s_count[ 0 ] >= 20U );

    /* Every send wakes the higher-priority receiver, which runs before the
     * sender's next statement. */
    for( i = 0U; ( i + 1U ) < s_trace_length; i++ )
    {
        if( s_trace[ i ] == 'S' )
        {
            sends++;
            SIL_EXPECT_EQ( s_trace[ i + 1U ], 'H' );
        }
    }

    SIL_EXPECT( sends >= 20U );
    sim_shutdown();
}

static void test_message_wakeup_of_an_equal_priority_receiver_waits( void )
{
    reset();
    build( receiver, 5U, sender, 5U, true );
    SIL_REQUIRE( sim_start( 60U ) );

    /* The receiver is READY but not outranked, so the sender, which never
     * blocks, keeps the CPU and the receiver never runs. */
    SIL_EXPECT_EQ( s_count[ 0 ], 0U );
    SIL_EXPECT( !sim_halted() );
    sim_shutdown();
}

static sil_case_t const s_cases[] = {
    SIL_CASE(
        test_equal_priority_peer_is_not_time_sliced,
        "A process that never blocks starves an equal-priority peer" ),
    SIL_CASE(
        test_timed_wait_zero_alternates_equal_priority_peers,
        "TIMED_WAIT(0) alternates equal-priority peers strictly" ),
    SIL_CASE(
        test_expired_wait_preempts_a_busy_lower_priority_process,
        "An expired timed wait preempts a busy lower-priority process" ),
    SIL_CASE(
        test_message_wakeup_preempts_a_lower_priority_sender,
        "A message wake-up preempts a lower-priority sender at once" ),
    SIL_CASE(
        test_message_wakeup_of_an_equal_priority_receiver_waits,
        "A message wake-up of an equal-priority receiver does not preempt" ),
};

int main(
    int argc,
    char ** argv )
{
    int status = sil_run_suite(
        "process_priority",
        s_cases,
        sizeof( s_cases ) / sizeof( s_cases[ 0 ] ),
        argc,
        argv );

    sim_shutdown();

    return status;
}
