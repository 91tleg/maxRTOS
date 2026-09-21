/**
 * @file test_temporal_partitioning.c
 * @brief SIL: time-partitioned execution driven by the major frame.
 *
 * Verifies on the virtual target, with the production kernel and SysTick
 * path, that every partition receives exactly the CPU time its frame slot
 * configures, that the schedule repeats with zero jitter, and that a
 * partition that never yields cannot take time from another.
 *
 * Requirements
 *   REQ-TP-001  Each partition executes for exactly its configured number of
 *               ticks per major frame.
 *   REQ-TP-002  The partition sequence is identical in every major frame
 *               (no jitter, no drift).
 *   REQ-TP-003  A partition that never yields cannot extend its slot or
 *               delay another partition.
 *
 * The expected schedule is computed here from the slot table, independently
 * of the kernel code under test.
 */

#include "sil_system.h"
#include "sil_test.h"

#define FRAMES ( 120U )

static volatile uint32_t s_work[ SIL_MAX_PARTITIONS ];

static void work_entry_0( void * arg ) { ( void ) arg; for( ;; ) { s_work[ 0 ]++; sim_cpu_work( 100U ); } }
static void work_entry_1( void * arg ) { ( void ) arg; for( ;; ) { s_work[ 1 ]++; sim_cpu_work( 100U ); } }
static void work_entry_2( void * arg ) { ( void ) arg; for( ;; ) { s_work[ 2 ]++; sim_cpu_work( 100U ); } }

/* Independent oracle: which partition owns tick `tick` of the schedule. */
static int32_t expected_partition( sil_system_spec_t const * spec, uint32_t tick )
{
    uint32_t frame = 0U;
    uint32_t i;
    uint32_t position;

    for( i = 0U; i < spec->slot_count; i++ )
    {
        frame += spec->slot[ i ].duration_ticks;
    }

    position = tick % frame;

    for( i = 0U; i < spec->slot_count; i++ )
    {
        if( position < spec->slot[ i ].duration_ticks )
        {
            return ( int32_t ) spec->slot[ i ].partition_id;
        }

        position -= spec->slot[ i ].duration_ticks;
    }

    return SIM_IDLE;
}

static uint32_t frame_length( sil_system_spec_t const * spec )
{
    uint32_t total = 0U;
    uint32_t i;

    for( i = 0U; i < spec->slot_count; i++ )
    {
        total += spec->slot[ i ].duration_ticks;
    }

    return total;
}

static void build_two( sil_system_spec_t * spec )
{
    sil_spec_two_partitions( spec, work_entry_0, work_entry_1 );
    sil_build( spec );
}

/* REQ-TP-001 */
static void test_slot_allocation_is_exact_over_many_frames( void )
{
    sil_system_spec_t spec;
    uint32_t counted[ 2 ] = { 0U, 0U };
    uint32_t ticks;
    uint32_t t;

    build_two( &spec );
    ticks = FRAMES * frame_length( &spec );
    SIL_REQUIRE( sim_start( ticks ) );

    for( t = 0U; t < ticks; t++ )
    {
        int32_t partition = sim_partition_at_tick( t );

        SIL_REQUIRE_EQ( partition, expected_partition( &spec, t ) );
        counted[ partition ]++;
    }

    /* 5 of every 8 ticks to partition 0, 3 of 8 to partition 1: exactly. */
    SIL_EXPECT_EQ( counted[ 0 ], 5U * FRAMES );
    SIL_EXPECT_EQ( counted[ 1 ], 3U * FRAMES );
    SIL_EXPECT_EQ( sim_model_violations(), 0U );
    sim_shutdown();
}

/* REQ-TP-001: uneven, interleaved slots, three partitions. */
static void test_irregular_schedule_with_three_partitions( void )
{
    sil_system_spec_t spec;
    uint32_t counted[ 3 ] = { 0U, 0U, 0U };
    uint32_t ticks;
    uint32_t t;

    sil_spec_defaults( &spec );
    spec.partition_count = 3U;

    spec.partition[ 0 ].process_count = 1U;
    spec.partition[ 0 ].process[ 0 ].entry = work_entry_0;
    spec.partition[ 1 ].process_count = 1U;
    spec.partition[ 1 ].process[ 0 ].entry = work_entry_1;
    spec.partition[ 2 ].process_count = 1U;
    spec.partition[ 2 ].process[ 0 ].entry = work_entry_2;

    /* Partition 0 appears twice in the frame. */
    spec.slot_count = 4U;
    spec.slot[ 0 ].partition_id = 0U;
    spec.slot[ 0 ].duration_ticks = 3U;
    spec.slot[ 1 ].partition_id = 1U;
    spec.slot[ 1 ].duration_ticks = 2U;
    spec.slot[ 2 ].partition_id = 0U;
    spec.slot[ 2 ].duration_ticks = 1U;
    spec.slot[ 3 ].partition_id = 2U;
    spec.slot[ 3 ].duration_ticks = 2U;

    sil_build( &spec );
    ticks = FRAMES * frame_length( &spec );
    SIL_REQUIRE( sim_start( ticks ) );

    for( t = 0U; t < ticks; t++ )
    {
        int32_t partition = sim_partition_at_tick( t );

        SIL_REQUIRE_EQ( partition, expected_partition( &spec, t ) );
        counted[ partition ]++;
    }

    SIL_EXPECT_EQ( counted[ 0 ], 4U * FRAMES );
    SIL_EXPECT_EQ( counted[ 1 ], 2U * FRAMES );
    SIL_EXPECT_EQ( counted[ 2 ], 2U * FRAMES );
    sim_shutdown();
}

/* REQ-TP-002 */
static void test_schedule_repeats_identically_every_major_frame( void )
{
    sil_system_spec_t spec;
    uint32_t frame;
    uint32_t t;
    uint32_t f;

    build_two( &spec );
    frame = frame_length( &spec );
    SIL_REQUIRE( sim_start( FRAMES * frame ) );

    for( f = 1U; f < FRAMES; f++ )
    {
        for( t = 0U; t < frame; t++ )
        {
            SIL_REQUIRE_EQ( sim_partition_at_tick( f * frame + t ),
                            sim_partition_at_tick( t ) );
        }
    }

    sim_shutdown();
}

/* REQ-TP-002: the slice boundaries do not drift, at the very first frame
 * and after thousands of ticks. */
static void test_slot_boundaries_do_not_drift( void )
{
    sil_system_spec_t spec;
    uint32_t frame;
    uint32_t f;

    build_two( &spec );
    frame = frame_length( &spec );
    SIL_REQUIRE( sim_start( 5000U * frame / frame ) );

    for( f = 0U; f < 5000U / frame; f++ )
    {
        /* First tick of each slot must belong to that slot's partition. */
        SIL_REQUIRE_EQ( sim_partition_at_tick( f * frame + 0U ), 0 );
        SIL_REQUIRE_EQ( sim_partition_at_tick( f * frame + 4U ), 0 );
        SIL_REQUIRE_EQ( sim_partition_at_tick( f * frame + 5U ), 1 );
        SIL_REQUIRE_EQ( sim_partition_at_tick( f * frame + 7U ), 1 );
    }

    sim_shutdown();
}

/* REQ-TP-003 */
static volatile uint32_t s_hog_iterations;

static void hog_entry( void * arg )
{
    ( void ) arg;

    /* Never yields, never blocks: only the tick can take the CPU away. */
    for( ;; )
    {
        s_hog_iterations++;
        sim_cpu_work( 10U );
    }
}

static void test_hogging_partition_cannot_take_time_from_another( void )
{
    sil_system_spec_t spec;
    uint32_t frame;
    uint32_t ticks;
    uint32_t t;
    uint32_t expected_victim_iterations;

    s_work[ 1 ] = 0U;
    s_hog_iterations = 0U;

    sil_spec_two_partitions( &spec, hog_entry, work_entry_1 );
    sil_build( &spec );

    frame = frame_length( &spec );
    ticks = FRAMES * frame;
    SIL_REQUIRE( sim_start( ticks ) );

    for( t = 0U; t < ticks; t++ )
    {
        SIL_REQUIRE_EQ( sim_partition_at_tick( t ), expected_partition( &spec, t ) );
    }

    /* The victim ran for 3 ticks of every frame, each tick holding 10
     * iterations of 100 cycles; allow the one iteration cut off per slot. */
    expected_victim_iterations = FRAMES * 3U * 10U;
    SIL_EXPECT( s_work[ 1 ] >= expected_victim_iterations - FRAMES );
    SIL_EXPECT( s_work[ 1 ] <= expected_victim_iterations + FRAMES );
    SIL_EXPECT( s_hog_iterations > 0U );
    sim_shutdown();
}

static sil_case_t const s_cases[] =
{
    SIL_CASE( test_slot_allocation_is_exact_over_many_frames, "REQ-TP-001", "5:3 schedule allocates exactly 5 and 3 ticks per frame over 120 frames" ),
    SIL_CASE( test_irregular_schedule_with_three_partitions, "REQ-TP-001", "Interleaved uneven slots for three partitions match the oracle every tick" ),
    SIL_CASE( test_schedule_repeats_identically_every_major_frame, "REQ-TP-002", "Every major frame repeats the first with zero jitter" ),
    SIL_CASE( test_slot_boundaries_do_not_drift, "REQ-TP-002", "Slot boundaries stay fixed after thousands of ticks" ),
    SIL_CASE( test_hogging_partition_cannot_take_time_from_another, "REQ-TP-003", "A partition that never yields cannot steal another's slot" ),
};

int main( int argc, char ** argv )
{
    int status = sil_run_suite( "temporal_partitioning", s_cases,
                                sizeof( s_cases ) / sizeof( s_cases[ 0 ] ), argc, argv );

    sim_shutdown();

    return status;
}
