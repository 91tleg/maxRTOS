/**
 * @file test_simulator_selfcheck.c
 * @brief Qualifies the virtual target before anything is judged on it.
 *
 * A SIL result is only as trustworthy as the simulator under it, so the
 * simulator's own behaviour is verified against the architecture rules it
 * claims to model: PMSAv7 MPU access decisions, virtual memory mapping,
 * determinism of the virtual clock, and hand-over between contexts.
 */

#include <string.h>

#include "sil_system.h"
#include "sil_test.h"

#include "maxrtos/arch/cortex_m7/mpu_hw.h"
#include "maxrtos/scheduler.h"
#include "maxrtos/yield.h"

/* ---- MPU model (programmed through the production driver) -------------- */

static void program( uint32_t region, uint32_t base, uint32_t size,
                     maxrtos_mpu_access_t access )
{
    SIL_REQUIRE_EQ( maxrtos_arch_mpu_configure_region( region, base, size, access, false ),
                    MAXRTOS_OK );
}

static void mpu_fresh( void )
{
    sim_reset();
    maxrtos_arch_mpu_init();
}

static void test_mpu_ap_encodings( void )
{
    mpu_fresh();

    program( 0U, 0x20000000UL, 0x1000UL, MAXRTOS_MPU_ACCESS_READ_WRITE );
    program( 1U, 0x20010000UL, 0x1000UL, MAXRTOS_MPU_ACCESS_READ_ONLY );
    program( 2U, 0x20020000UL, 0x1000UL, MAXRTOS_MPU_ACCESS_PRIV_READ_WRITE );
    program( 3U, 0x20030000UL, 0x1000UL, MAXRTOS_MPU_ACCESS_PRIV_READ_ONLY );
    program( 4U, 0x20040000UL, 0x1000UL, MAXRTOS_MPU_ACCESS_NONE );

    /* full access */
    SIL_EXPECT( sim_mpu_access_ok( 0x20000000UL, 4U, true, false ) );
    SIL_EXPECT( sim_mpu_access_ok( 0x20000000UL, 4U, false, false ) );

    /* read-only for everyone */
    SIL_EXPECT( sim_mpu_access_ok( 0x20010000UL, 4U, false, false ) );
    SIL_EXPECT( !sim_mpu_access_ok( 0x20010000UL, 4U, true, false ) );
    SIL_EXPECT( !sim_mpu_access_ok( 0x20010000UL, 4U, true, true ) );

    /* privileged read/write, no user access */
    SIL_EXPECT( sim_mpu_access_ok( 0x20020000UL, 4U, true, true ) );
    SIL_EXPECT( !sim_mpu_access_ok( 0x20020000UL, 4U, false, false ) );
    SIL_EXPECT( !sim_mpu_access_ok( 0x20020000UL, 4U, true, false ) );

    /* privileged read-only */
    SIL_EXPECT( sim_mpu_access_ok( 0x20030000UL, 4U, false, true ) );
    SIL_EXPECT( !sim_mpu_access_ok( 0x20030000UL, 4U, true, true ) );
    SIL_EXPECT( !sim_mpu_access_ok( 0x20030000UL, 4U, false, false ) );

    /* no access at all */
    SIL_EXPECT( !sim_mpu_access_ok( 0x20040000UL, 4U, false, true ) );
    SIL_EXPECT( !sim_mpu_access_ok( 0x20040000UL, 4U, false, false ) );

    SIL_EXPECT_EQ( sim_model_violations(), 0U );
}

static void test_mpu_boundaries_and_straddling( void )
{
    mpu_fresh();
    program( 0U, 0x20000000UL, 0x1000UL, MAXRTOS_MPU_ACCESS_READ_WRITE );

    SIL_EXPECT( sim_mpu_access_ok( 0x20000FFCUL, 4U, true, false ) );   /* last word */
    SIL_EXPECT( !sim_mpu_access_ok( 0x20001000UL, 4U, true, false ) );  /* first word after */
    SIL_EXPECT( !sim_mpu_access_ok( 0x20000FFEUL, 4U, true, false ) );  /* straddles the end */
    SIL_EXPECT( !sim_mpu_access_ok( 0x1FFFFFFCUL, 8U, true, false ) );  /* straddles the start */
    SIL_EXPECT( !sim_mpu_access_ok( 0xFFFFFFF0UL, 0x40U, false, true ) ); /* wraps the address space */
    SIL_EXPECT( sim_mpu_access_ok( 0x20000800UL, 0U, true, false ) );   /* empty range */
}

static void test_mpu_default_map_and_privdefena( void )
{
    mpu_fresh(); /* maxrtos_arch_mpu_init() sets ENABLE | PRIVDEFENA */

    SIL_EXPECT( sim_mpu_enabled() );

    /* Nothing mapped: privileged code falls back to the default map, user
     * code is denied. */
    SIL_EXPECT( sim_mpu_access_ok( 0x40000000UL, 4U, true, true ) );
    SIL_EXPECT( !sim_mpu_access_ok( 0x40000000UL, 4U, false, false ) );
}

static void test_mpu_highest_numbered_region_wins( void )
{
    mpu_fresh();

    /* A large read-only window with a small read-write hole punched in it by
     * a higher-numbered region, as the partition slot is over the background. */
    program( 0U, 0x20000000UL, 0x10000UL, MAXRTOS_MPU_ACCESS_READ_ONLY );
    program( 5U, 0x20004000UL, 0x1000UL, MAXRTOS_MPU_ACCESS_READ_WRITE );

    SIL_EXPECT( !sim_mpu_access_ok( 0x20000000UL, 4U, true, false ) );
    SIL_EXPECT( sim_mpu_access_ok( 0x20004000UL, 4U, true, false ) );
    SIL_EXPECT( !sim_mpu_access_ok( 0x20005000UL, 4U, true, false ) );

    /* A range crossing from the read-only area into the hole is judged per
     * byte: the write is denied where the read-only region applies. */
    SIL_EXPECT( !sim_mpu_access_ok( 0x20003FFCUL, 8U, true, false ) );
    SIL_EXPECT( sim_mpu_access_ok( 0x20003FFCUL, 8U, false, false ) );

    /* A lower-numbered region cannot override a higher one. */
    program( 1U, 0x20004000UL, 0x1000UL, MAXRTOS_MPU_ACCESS_NONE );
    SIL_EXPECT( sim_mpu_access_ok( 0x20004000UL, 4U, true, false ) );
}

static void test_mpu_disabled_region_is_ignored( void )
{
    mpu_fresh();
    program( 0U, 0x20000000UL, 0x1000UL, MAXRTOS_MPU_ACCESS_READ_WRITE );
    SIL_EXPECT( sim_mpu_access_ok( 0x20000000UL, 4U, true, false ) );

    /* The driver's own disable path writes RASR = 0 for the selected region. */
    maxrtos_arch_mpu_init();
    sim_mpu_write( 1U, 0U );
    sim_mpu_write( 3U, 0U );
    SIL_EXPECT( !sim_mpu_access_ok( 0x20000000UL, 4U, true, false ) );
}

static void test_mpu_misaligned_region_is_a_model_violation( void )
{
    mpu_fresh();

    /* Base 0x20000400 is not aligned to a 4 KiB size: UNPREDICTABLE on
     * silicon, so the model reports it and does not enable the region. */
    sim_mpu_write( 1U, 3U );
    sim_mpu_write( 2U, 0x20000400UL );
    sim_mpu_write( 3U, ( 11U << 1U ) | 1U ); /* SIZE = 11 -> 4 KiB, enabled */

    SIL_EXPECT_EQ( sim_model_violations(), 1U );
    SIL_EXPECT( !sim_mpu_access_ok( 0x20000400UL, 4U, false, false ) );
}

/* ---- Virtual memory ---------------------------------------------------- */

static void test_memory_map_translation( void )
{
    uint8_t * a;
    uint8_t * b;

    sim_reset();
    a = sim_mem_map( "A", 0x20000000UL, 0x1000U );
    b = sim_mem_map( "B", 0x24000000UL, 0x2000U );
    SIL_REQUIRE( ( a != NULL ) && ( b != NULL ) );

    SIL_EXPECT( sim_mem_ptr( 0x20000000UL, 4U ) == a );
    SIL_EXPECT( sim_mem_ptr( 0x24000010UL, 4U ) == b + 0x10 );
    SIL_EXPECT( sim_mem_ptr( 0x20000FFDUL, 4U ) == NULL );    /* crosses the end */
    SIL_EXPECT( sim_mem_ptr( 0x30000000UL, 1U ) == NULL );    /* unmapped */

    SIL_EXPECT_EQ( sim_ptr_to_uaddr( a + 0x20 ), 0x20000020UL );
    SIL_EXPECT_EQ( sim_ptr_to_uaddr( b + 0x1FFF ), 0x24001FFFUL );
    SIL_EXPECT_EQ( sim_ptr_to_uaddr( &a ), 0U );              /* host stack: unmapped */
}

/* ---- Virtual time and context hand-over -------------------------------- */

static volatile uint32_t s_entry_count[ 2 ];

static void counting_entry_a( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        s_entry_count[ 0 ]++;
        sim_cpu_work( 100U );
    }
}

static void counting_entry_b( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        s_entry_count[ 1 ]++;
        sim_cpu_work( 100U );
    }
}

static void test_virtual_clock_is_exact_and_deterministic( void )
{
    sil_system_spec_t spec;
    uint32_t first_run[ 2 ];
    unsigned run;

    for( run = 0U; run < 2U; run++ )
    {
        s_entry_count[ 0 ] = 0U;
        s_entry_count[ 1 ] = 0U;

        sil_spec_two_partitions( &spec, counting_entry_a, counting_entry_b );
        sil_build( &spec );
        SIL_REQUIRE( sim_start( 80U ) );

        SIL_EXPECT_EQ( sim_tick(), 80U );

        /* Each tick is SIM_CYCLES_PER_TICK cycles and each loop iteration
         * costs 100, so a partition running for T ticks executed T * 10
         * iterations (give or take the iteration in flight at the ends). */
        if( run == 0U )
        {
            first_run[ 0 ] = s_entry_count[ 0 ];
            first_run[ 1 ] = s_entry_count[ 1 ];
            SIL_EXPECT( s_entry_count[ 0 ] > 0U );
            SIL_EXPECT( s_entry_count[ 1 ] > 0U );
        }
        else
        {
            /* Same inputs, same virtual time, bit-identical outcome. */
            SIL_EXPECT_EQ( s_entry_count[ 0 ], first_run[ 0 ] );
            SIL_EXPECT_EQ( s_entry_count[ 1 ], first_run[ 1 ] );
        }
    }

    SIL_EXPECT_EQ( sim_model_violations(), 0U );
    sim_shutdown();
}

static void test_run_can_be_resumed_in_slices( void )
{
    sil_system_spec_t spec;
    uint32_t whole;

    s_entry_count[ 0 ] = 0U;
    s_entry_count[ 1 ] = 0U;
    sil_spec_two_partitions( &spec, counting_entry_a, counting_entry_b );
    sil_build( &spec );
    SIL_REQUIRE( sim_start( 64U ) );
    whole = s_entry_count[ 0 ] + s_entry_count[ 1 ];
    sim_shutdown();

    s_entry_count[ 0 ] = 0U;
    s_entry_count[ 1 ] = 0U;
    sil_build( &spec );
    SIL_REQUIRE( sim_start( 10U ) );
    SIL_REQUIRE( sim_run( 14U ) );
    SIL_REQUIRE( sim_run( 40U ) );
    SIL_EXPECT_EQ( sim_tick(), 64U );

    /* Slicing a run must not change what happens in it. */
    SIL_EXPECT_EQ( s_entry_count[ 0 ] + s_entry_count[ 1 ], whole );
    sim_shutdown();
}

static void test_scheduler_start_without_processes_halts( void )
{
    sil_system_spec_t spec;

    sil_spec_two_partitions( &spec, counting_entry_a, counting_entry_b );
    spec.partition[ 0 ].process_count = 0U;
    spec.partition[ 1 ].process_count = 0U;
    sil_build( &spec );

    SIL_EXPECT( !sim_start( 5U ) );
    SIL_EXPECT( sim_halted() );
    SIL_EXPECT( sim_halt_reason() != NULL );
    sim_shutdown();
}

static sil_case_t const s_cases[] =
{
    SIL_CASE( test_mpu_ap_encodings, "SIM-001", "MPU AP encodings decide access as PMSAv7 specifies" ),
    SIL_CASE( test_mpu_boundaries_and_straddling, "SIM-001", "MPU region boundaries, straddling and wrap-around" ),
    SIL_CASE( test_mpu_default_map_and_privdefena, "SIM-001", "PRIVDEFENA gives privileged code the default map only" ),
    SIL_CASE( test_mpu_highest_numbered_region_wins, "SIM-001", "Overlapping regions: the highest number wins" ),
    SIL_CASE( test_mpu_disabled_region_is_ignored, "SIM-001", "A disabled region maps nothing" ),
    SIL_CASE( test_mpu_misaligned_region_is_a_model_violation, "SIM-001", "An illegally programmed region is reported" ),
    SIL_CASE( test_memory_map_translation, "SIM-002", "Virtual/host address translation" ),
    SIL_CASE( test_virtual_clock_is_exact_and_deterministic, "SIM-003", "Virtual time is exact and runs are bit-identical" ),
    SIL_CASE( test_run_can_be_resumed_in_slices, "SIM-003", "Slicing a run does not change its outcome" ),
    SIL_CASE( test_scheduler_start_without_processes_halts, "SIM-004", "Start-up with nothing to run halts and is reported" ),
};

int main( int argc, char ** argv )
{
    int status = sil_run_suite( "simulator_selfcheck", s_cases,
                                sizeof( s_cases ) / sizeof( s_cases[ 0 ] ), argc, argv );

    sim_shutdown();

    return status;
}
