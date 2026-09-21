/**
 * @file test_scheduler_startup.c
 * @brief SIL: maxrtos_scheduler_start() and the first dispatch.
 *
 * Requirements
 *   REQ-BOOT-001  Every READY process created before start is registered with
 *                 its partition and gets to run.
 *   REQ-BOOT-002  Processes that are not READY (suspended) are never run.
 *   REQ-BOOT-003  The major frame starts at tick zero with the first slot's
 *                 partition, and that partition's memory is mapped before
 *                 any of its code executes.
 *   REQ-BOOT-004  Starting with no runnable process stops safely and says why.
 */

#include "sil_system.h"
#include "sil_test.h"

#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/tick.h"

static volatile uint32_t s_ran[ 8 ];
static volatile uint32_t s_first_region_valid;

#define RUNNER( n_ )                                            \
    static void runner_##n_( void * arg )                       \
    {                                                           \
        ( void ) arg;                                           \
        for( ;; ) { s_ran[ n_ ]++; maxrtos_yield(); }           \
    }

#include "maxrtos/yield.h"

RUNNER( 0 )
RUNNER( 1 )
RUNNER( 2 )
RUNNER( 3 )

static void reset_counters( void )
{
    unsigned i;

    for( i = 0U; i < 8U; i++ )
    {
        s_ran[ i ] = 0U;
    }
}

/* REQ-BOOT-001 */
static void test_all_created_processes_are_registered_and_run( void )
{
    sil_system_spec_t spec;

    reset_counters();
    sil_spec_two_partitions( &spec, runner_0, runner_2 );
    spec.partition[ 0 ].process_count = 2U;
    spec.partition[ 0 ].process[ 1 ].entry = runner_1;
    spec.partition[ 0 ].process[ 1 ].priority = 5U;
    spec.partition[ 1 ].process_count = 2U;
    spec.partition[ 1 ].process[ 1 ].entry = runner_3;
    spec.partition[ 1 ].process[ 1 ].priority = 5U;

    sil_build( &spec );
    SIL_REQUIRE( sim_start( 200U ) );

    SIL_EXPECT( s_ran[ 0 ] > 0U );
    SIL_EXPECT( s_ran[ 1 ] > 0U );
    SIL_EXPECT( s_ran[ 2 ] > 0U );
    SIL_EXPECT( s_ran[ 3 ] > 0U );
    SIL_EXPECT_EQ( sim_model_violations(), 0U );
    sim_shutdown();
}

/* REQ-BOOT-002 */
static void test_suspended_process_is_never_run( void )
{
    sil_system_spec_t spec;
    sil_system_t * sys;

    reset_counters();
    sil_spec_two_partitions( &spec, runner_0, runner_2 );
    spec.partition[ 0 ].process_count = 2U;
    spec.partition[ 0 ].process[ 1 ].entry = runner_1;
    spec.partition[ 0 ].process[ 1 ].priority = 5U;

    sil_build( &spec );
    sys = sil_system();

    /* Created but administratively stopped before the scheduler starts. */
    SIL_REQUIRE_EQ( maxrtos_process_set_state( sys->process_id[ 0 ][ 1 ],
                                               MAXRTOS_PROCESS_STATE_SUSPENDED ),
                    MAXRTOS_OK );

    SIL_REQUIRE( sim_start( 200U ) );

    SIL_EXPECT( s_ran[ 0 ] > 0U );
    SIL_EXPECT( s_ran[ 2 ] > 0U );
    SIL_EXPECT_EQ( s_ran[ 1 ], 0U );
    sim_shutdown();
}

/* REQ-BOOT-003 */
static void probe_entry( void * arg )
{
    ( void ) arg;

    /* Runs in partition 0's first slot: its own memory must already be
     * accessible. Reads through the MPU as unprivileged code. */
    {
        sil_system_t * sys = sil_system();
        uint32_t value;

        s_first_region_valid = sim_read32( sys->domain_base[ 0 ], &value ) ? 1U : 2U;
    }

    for( ;; )
    {
        maxrtos_yield();
    }
}

static void test_first_dispatch_maps_the_first_partitions_memory( void )
{
    sil_system_spec_t spec;
    sil_system_t * sys;
    uint32_t base;
    uint32_t size;
    uint32_t access;

    s_first_region_valid = 0U;
    sil_spec_two_partitions( &spec, probe_entry, runner_2 );
    sil_build( &spec );
    sys = sil_system();

    SIL_REQUIRE( sim_start( 3U ) );

    /* The very first code that ran in partition 0 could read its domain. */
    SIL_EXPECT_EQ( s_first_region_valid, 1U );

    /* The partition slot (MPU region 8) describes partition 0's domain. */
    SIL_REQUIRE( sim_mpu_region_info( 8U, &base, &size, &access ) );
    SIL_EXPECT_EQ( base, sys->domain_base[ 0 ] );
    SIL_EXPECT_EQ( size, sys->domain_size[ 0 ] );
    sim_shutdown();
}

static void test_frame_begins_at_tick_zero_with_the_first_slot( void )
{
    sil_system_spec_t spec;

    reset_counters();
    sil_spec_two_partitions( &spec, runner_0, runner_2 );
    sil_build( &spec );

    /* Time spent before the scheduler starts does not count. */
    maxrtos_kernel_tick_reset();
    SIL_REQUIRE( sim_start( 20U ) );

    SIL_EXPECT_EQ( sim_partition_at_tick( 0U ), 0 );   /* first slot: partition 0 */
    SIL_EXPECT_EQ( sim_partition_at_tick( 4U ), 0 );
    SIL_EXPECT_EQ( sim_partition_at_tick( 5U ), 1 );
    SIL_EXPECT_EQ( maxrtos_kernel_tick_now(), sim_tick() + 1U ); /* tick 0 was processed by start */
    sim_shutdown();
}

/* REQ-BOOT-004 */
static void test_start_without_runnable_processes_stops_safely( void )
{
    sil_system_spec_t spec;

    sil_spec_two_partitions( &spec, runner_0, runner_2 );
    spec.partition[ 0 ].process_count = 0U;
    spec.partition[ 1 ].process_count = 0U;
    sil_build( &spec );

    SIL_EXPECT( !sim_start( 10U ) );
    SIL_EXPECT( sim_halted() );
    SIL_EXPECT( sim_halt_reason() != NULL );
    SIL_EXPECT_EQ( sim_tick(), 0U ); /* it never got as far as running */
    sim_shutdown();
}

static sil_case_t const s_cases[] =
{
    SIL_CASE( test_all_created_processes_are_registered_and_run, "REQ-BOOT-001", "All created READY processes are registered and run" ),
    SIL_CASE( test_suspended_process_is_never_run, "REQ-BOOT-002", "A suspended process is not registered and never runs" ),
    SIL_CASE( test_first_dispatch_maps_the_first_partitions_memory, "REQ-BOOT-003", "The first partition's memory is mapped before its code runs" ),
    SIL_CASE( test_frame_begins_at_tick_zero_with_the_first_slot, "REQ-BOOT-003", "The major frame starts at tick zero with the first slot" ),
    SIL_CASE( test_start_without_runnable_processes_stops_safely, "REQ-BOOT-004", "No runnable process: safe stop with a reason" ),
};

int main( int argc, char ** argv )
{
    int status = sil_run_suite( "scheduler_startup", s_cases,
                                sizeof( s_cases ) / sizeof( s_cases[ 0 ] ), argc, argv );

    sim_shutdown();

    return status;
}
