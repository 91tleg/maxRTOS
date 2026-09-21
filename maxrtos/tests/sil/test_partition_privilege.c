/**
 * @file test_partition_privilege.c
 * @brief SIL: privilege is a property of the partition, never of a process.
 *
 * Requirements
 *   REQ-PRIV-001  Application partitions are unprivileged by default: their
 *                 processes cannot touch kernel memory.
 *   REQ-PRIV-002  A system partition runs privileged: its processes can use
 *                 kernel memory that application partitions cannot.
 *   REQ-PRIV-003  All processes of a partition share its privilege.
 *   REQ-PRIV-004  With the privileged default map disabled (deny by default),
 *                 even a privileged process cannot touch memory no region
 *                 maps, and the failure is contained like any other fault.
 *
 * The build runs with PRIVDEFENA clear (see sil_system.c), so these also show
 * that the kernel itself needs nothing beyond its explicit regions.
 */

#include "sil_system.h"
#include "sil_test.h"

#include "maxrtos/arch/cortex_m7/context_switch.h"
#include "maxrtos/yield.h"

#define UNMAPPED_ADDRESS ( 0x40000000UL ) /* outside every region */

typedef enum { ACCESS_KERNEL, ACCESS_UNMAPPED } target_t;

static volatile uint32_t s_ok[ 4 ];
static volatile uint32_t s_attempts[ 4 ];
static volatile uint32_t s_progress;

static void poke( uint32_t slot, target_t target )
{
    uint32_t address = ( target == ACCESS_KERNEL ) ? SIL_KERNEL_GUARD : UNMAPPED_ADDRESS;

    s_attempts[ slot ]++;

    if( sim_write32( address, 0x1234U ) )
    {
        s_ok[ slot ]++;
    }

    for( ;; )
    {
        maxrtos_yield();
    }
}

static void kernel_poker_0( void * arg ) { ( void ) arg; poke( 0U, ACCESS_KERNEL ); }
static void kernel_poker_1( void * arg ) { ( void ) arg; poke( 1U, ACCESS_KERNEL ); }
static void kernel_poker_2( void * arg ) { ( void ) arg; poke( 2U, ACCESS_KERNEL ); }
static void unmapped_poker( void * arg ) { ( void ) arg; poke( 3U, ACCESS_UNMAPPED ); }

static void bystander( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        s_progress++;
        sim_cpu_work( 100U );
    }
}

static void reset( void )
{
    unsigned i;

    for( i = 0U; i < 4U; i++ )
    {
        s_ok[ i ] = 0U;
        s_attempts[ i ] = 0U;
    }

    s_progress = 0U;
}

/* REQ-PRIV-001 */
static void test_application_partition_is_unprivileged_by_default( void )
{
    sil_system_spec_t spec;
    uint32_t * guard;

    reset();
    sil_spec_two_partitions( &spec, kernel_poker_0, bystander );
    sil_build( &spec );

    guard = sim_mem_ptr( SIL_KERNEL_GUARD, 4U );
    *guard = 0xC0FFEEUL;

    SIL_EXPECT( !maxrtos_arch_partition_is_privileged( 0U ) );
    SIL_EXPECT( !maxrtos_arch_partition_is_privileged( 1U ) );

    SIL_REQUIRE( sim_start( 120U ) );

    SIL_EXPECT( s_attempts[ 0 ] >= 2U );  /* faulted and was restarted */
    SIL_EXPECT_EQ( s_ok[ 0 ], 0U );
    SIL_EXPECT_EQ( *guard, 0xC0FFEEUL );
    SIL_EXPECT( s_progress > 50U );
    sim_shutdown();
}

/* REQ-PRIV-002 */
static void test_system_partition_runs_privileged( void )
{
    sil_system_spec_t spec;
    uint32_t * guard;

    reset();
    sil_spec_two_partitions( &spec, kernel_poker_0, bystander );
    spec.partition[ 0 ].system = true;
    sil_build( &spec );

    guard = sim_mem_ptr( SIL_KERNEL_GUARD, 4U );
    *guard = 0xC0FFEEUL;

    SIL_EXPECT( maxrtos_arch_partition_is_privileged( 0U ) );
    SIL_EXPECT( !maxrtos_arch_partition_is_privileged( 1U ) );

    SIL_REQUIRE( sim_start( 60U ) );

    SIL_EXPECT_EQ( s_attempts[ 0 ], 1U );  /* no fault, no restart */
    SIL_EXPECT_EQ( s_ok[ 0 ], 1U );
    SIL_EXPECT_EQ( *guard, 0x1234U );      /* the write landed */
    SIL_EXPECT( !sim_halted() );
    sim_shutdown();
}

/* REQ-PRIV-003 */
static void test_every_process_of_a_partition_shares_its_privilege( void )
{
    sil_system_spec_t spec;

    reset();
    sil_spec_two_partitions( &spec, kernel_poker_0, bystander );
    spec.partition[ 0 ].system = true;
    spec.partition[ 0 ].process_count = 2U;
    spec.partition[ 0 ].process[ 1 ].entry = kernel_poker_1;
    spec.partition[ 0 ].process[ 1 ].priority = 5U;

    /* An application partition with two processes: neither may. */
    spec.partition[ 1 ].process_count = 2U;
    spec.partition[ 1 ].process[ 0 ].entry = kernel_poker_2;
    spec.partition[ 1 ].process[ 1 ].entry = bystander;
    spec.partition[ 1 ].process[ 1 ].priority = 5U;
    sil_build( &spec );

    SIL_REQUIRE( sim_start( 160U ) );

    SIL_EXPECT_EQ( s_ok[ 0 ], 1U );  /* both system-partition processes */
    SIL_EXPECT_EQ( s_ok[ 1 ], 1U );
    SIL_EXPECT_EQ( s_ok[ 2 ], 0U );  /* application partition process */
    SIL_EXPECT( s_attempts[ 2 ] >= 2U );
    sim_shutdown();
}

/* REQ-PRIV-004 */
static void test_privileged_process_cannot_touch_unmapped_memory( void )
{
    sil_system_spec_t spec;

    reset();
    sil_spec_two_partitions( &spec, unmapped_poker, bystander );
    spec.partition[ 0 ].system = true;
    sil_build( &spec );

    SIL_REQUIRE( sim_start( 120U ) );

    /* No region maps the address, the default map is off: the privileged
     * access faults, and is restarted like any other. */
    SIL_EXPECT( s_attempts[ 3 ] >= 2U );
    SIL_EXPECT_EQ( s_ok[ 3 ], 0U );
    SIL_EXPECT( s_progress > 50U );
    SIL_EXPECT( !sim_halted() );
    sim_shutdown();
}

static sil_case_t const s_cases[] =
{
    SIL_CASE( test_application_partition_is_unprivileged_by_default, "REQ-PRIV-001", "An application partition cannot touch kernel memory" ),
    SIL_CASE( test_system_partition_runs_privileged, "REQ-PRIV-002", "A system partition can use kernel memory" ),
    SIL_CASE( test_every_process_of_a_partition_shares_its_privilege, "REQ-PRIV-003", "Privilege is per partition: all its processes share it" ),
    SIL_CASE( test_privileged_process_cannot_touch_unmapped_memory, "REQ-PRIV-004", "Deny by default: privileged code faults on unmapped memory" ),
};

int main( int argc, char ** argv )
{
    int status = sil_run_suite( "partition_privilege", s_cases,
                                sizeof( s_cases ) / sizeof( s_cases[ 0 ] ), argc, argv );

    sim_shutdown();

    return status;
}
