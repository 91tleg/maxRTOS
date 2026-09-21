/**
 * @file test_process_yield.c
 * @brief SIL: cooperative yield through the SVC path.
 *
 * Requirements
 *   REQ-YLD-001  Yield hands the CPU to another READY process of the same
 *                partition, and equal-priority processes alternate.
 *   REQ-YLD-002  Yield never crosses a partition boundary: it does not give
 *                one partition's time to another.
 *   REQ-YLD-003  A lone process can yield indefinitely without stalling.
 */

#include "sil_system.h"
#include "sil_test.h"

#include "maxrtos/yield.h"

#define TRACE_LENGTH ( 400U )

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

static void tracer_a( void * arg ) { ( void ) arg; for( ;; ) { record( 'A' ); s_count[ 0 ]++; maxrtos_yield(); } }
static void tracer_b( void * arg ) { ( void ) arg; for( ;; ) { record( 'B' ); s_count[ 1 ]++; maxrtos_yield(); } }
static void counter_c( void * arg ) { ( void ) arg; for( ;; ) { s_count[ 2 ]++; maxrtos_yield(); } }
static void counter_d( void * arg ) { ( void ) arg; for( ;; ) { s_count[ 3 ]++; maxrtos_yield(); } }

static void reset( void )
{
    unsigned i;

    s_trace_length = 0U;

    for( i = 0U; i < 4U; i++ )
    {
        s_count[ i ] = 0U;
    }
}

/* REQ-YLD-001
 *
 * A yield always hands the CPU to the peer. The one thing that can make a
 * process record twice in a row is the tick landing between its record and
 * its `svc`: it is preempted, the peer runs and yields back, and the
 * original yield then fires. That needs a tick, so such repeats are
 * bounded by the number of ticks the partition received, and a process
 * never records more than twice consecutively. */
static void test_equal_priority_processes_alternate( void )
{
    sil_system_spec_t spec;
    uint32_t i;
    uint32_t repeats = 0U;
    uint32_t longest_run = 1U;
    uint32_t run = 1U;
    uint32_t partition_0_ticks = 0U;
    uint32_t t;
    uint32_t skew;

    reset();
    sil_spec_two_partitions( &spec, tracer_a, counter_c );
    spec.partition[ 0 ].process_count = 2U;
    spec.partition[ 0 ].process[ 1 ].entry = tracer_b;
    spec.partition[ 0 ].process[ 1 ].priority = 5U;
    sil_build( &spec );

    SIL_REQUIRE( sim_start( 400U ) );
    SIL_REQUIRE( s_trace_length >= 100U );

    for( i = 1U; i < s_trace_length; i++ )
    {
        if( s_trace[ i ] == s_trace[ i - 1U ] )
        {
            repeats++;
            run++;

            if( run > longest_run )
            {
                longest_run = run;
            }
        }
        else
        {
            run = 1U;
        }
    }

    for( t = 0U; t < sim_tick(); t++ )
    {
        if( sim_partition_at_tick( t ) == 0 )
        {
            partition_0_ticks++;
        }
    }

    SIL_EXPECT( longest_run <= 2U );
    SIL_EXPECT( repeats <= partition_0_ticks );

    /* Neither process is starved: the counts differ only by the in-flight
     * iteration of each. */
    skew = ( s_count[ 0 ] > s_count[ 1 ] ) ? ( s_count[ 0 ] - s_count[ 1 ] )
                                           : ( s_count[ 1 ] - s_count[ 0 ] );
    SIL_EXPECT( skew <= 2U );
    SIL_EXPECT( s_count[ 0 ] > 1000U );
    sim_shutdown();
}

/* REQ-YLD-002 */
static void test_yield_does_not_move_time_between_partitions( void )
{
    sil_system_spec_t spec;
    uint32_t t;
    uint32_t partition_1_ticks = 0U;

    reset();
    sil_spec_two_partitions( &spec, counter_c, counter_d );
    sil_build( &spec );

    /* Partition 0 yields constantly; with nothing else READY in its own
     * partition it must keep the CPU for its whole slot. */
    SIL_REQUIRE( sim_start( 160U ) );

    for( t = 0U; t < 160U; t++ )
    {
        int32_t partition = sim_partition_at_tick( t );

        SIL_REQUIRE_EQ( partition, ( t % 8U ) < 5U ? 0 : 1 );

        if( partition == 1 )
        {
            partition_1_ticks++;
        }
    }

    SIL_EXPECT_EQ( partition_1_ticks, 60U ); /* 3 of every 8 */
    sim_shutdown();
}

/* REQ-YLD-003 */
static void test_lone_process_can_yield_indefinitely( void )
{
    sil_system_spec_t spec;

    reset();
    sil_spec_two_partitions( &spec, counter_c, counter_d );
    sil_build( &spec );

    SIL_REQUIRE( sim_start( 200U ) );

    /* Each yield costs SIM_SVC_CYCLES; a partition running 5 of 8 ticks
     * completes a predictable number of them. Both made progress and the
     * system never halted. */
    SIL_EXPECT( s_count[ 2 ] > 1000U );
    SIL_EXPECT( s_count[ 3 ] > 500U );
    SIL_EXPECT( !sim_halted() );
    SIL_EXPECT_EQ( sim_model_violations(), 0U );
    sim_shutdown();
}

static sil_case_t const s_cases[] =
{
    SIL_CASE( test_equal_priority_processes_alternate, "REQ-YLD-001", "Equal-priority peers alternate on yield; no starvation, repeats bounded by ticks" ),
    SIL_CASE( test_yield_does_not_move_time_between_partitions, "REQ-YLD-002", "Yield does not shift time across a partition boundary" ),
    SIL_CASE( test_lone_process_can_yield_indefinitely, "REQ-YLD-003", "A lone process can yield indefinitely without stalling" ),
};

int main( int argc, char ** argv )
{
    int status = sil_run_suite( "process_yield", s_cases,
                                sizeof( s_cases ) / sizeof( s_cases[ 0 ] ), argc, argv );

    sim_shutdown();

    return status;
}
