/**
 * @file test_spatial_partitioning.c
 * @brief SIL: memory isolation between partitions, enforced by the MPU
 *        driver programmed on every context switch.
 *
 * The MPU driver under test is the production one (arch/cortex_m7/src/mpu_hw.c)
 * writing to a register-level MPU model. Simulated unprivileged code touches
 * memory through the model, so a violation raises a real fault through the
 * production recovery path.
 *
 * Requirements
 *   REQ-SP-001  A process can use its own partition's memory.
 *   REQ-SP-002  A process cannot read or write another partition's memory.
 *   REQ-SP-003  A process cannot read or write kernel memory.
 *   REQ-SP-004  An IPC port's memory is accessible only to member partitions.
 *   REQ-SP-005  The MPU maps exactly the running partition's memory and the
 *               ports it is a member of, and follows every partition switch.
 *   REQ-SP-006  Processes of one partition share that partition's memory
 *               (partition-level isolation, by design).
 *   REQ-SP-007  Flash (code and constants) is read-only for every partition.
 */

#include "sil_system.h"
#include "sil_test.h"

#include "maxrtos/yield.h"

#define CANARY ( 0xC0FFEE42UL )
#define PROBE_OFFSET ( 64U ) /* inside a stack's unused low end */

static volatile uint32_t s_attempts;
static volatile uint32_t s_returned_from_denied_access;
static volatile uint32_t s_ok_write;
static volatile uint32_t s_ok_readback;
static volatile uint32_t s_ok_read;
static volatile uint32_t s_peer_progress;
static volatile uint32_t s_done;

static void watcher( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        s_peer_progress++;
        sim_cpu_work( 100U );
    }
}

static void build_two( void ( * p0_main )( void * ) )
{
    sil_system_spec_t spec;

    sil_spec_two_partitions( &spec, p0_main, watcher );
    spec.partition[ 0 ].process_count = 2U;
    spec.partition[ 0 ].process[ 1 ].entry = watcher;
    spec.partition[ 0 ].process[ 1 ].priority = 5U;

    spec.port_count = 2U;
    spec.port[ 0 ].message_size = 4U;
    spec.port[ 0 ].capacity = 4U;
    spec.port[ 0 ].member_mask = 0x1U; /* partition 0 only */
    spec.port[ 1 ].message_size = 4U;
    spec.port[ 1 ].capacity = 4U;
    spec.port[ 1 ].member_mask = 0x2U; /* partition 1 only */

    sil_build( &spec );
}

static void reset( void )
{
    s_attempts = 0U;
    s_returned_from_denied_access = 0U;
    s_ok_write = 0U;
    s_ok_readback = 0U;
    s_ok_read = 0U;
    s_peer_progress = 0U;
    s_done = 0U;
}

/* Address of a probe word in the given partition's domain. */
static uint32_t domain_probe( uint32_t partition, uint32_t stack_index )
{
    return sil_system()->domain_base[ partition ] + ( stack_index * SIL_STACK_BYTES ) + PROBE_OFFSET;
}

/* ---- REQ-SP-001 / REQ-SP-006 ------------------------------------------- */

static void own_memory_user( void * arg )
{
    uint32_t value = 0U;

    ( void ) arg;

    s_ok_write = sim_write32( domain_probe( 0U, 0U ), 0x11112222UL ) ? 1U : 0U;
    s_ok_readback = ( sim_read32( domain_probe( 0U, 0U ), &value ) && ( value == 0x11112222UL ) ) ? 1U : 0U;

    /* The peer process's stack lives in the same domain. */
    s_ok_read = sim_write32( domain_probe( 0U, 1U ), 0x33334444UL ) ? 1U : 0U;
    s_done = 1U;

    for( ;; )
    {
        maxrtos_yield();
    }
}

static void test_process_can_use_its_own_partitions_memory( void )
{
    reset();
    build_two( own_memory_user );
    SIL_REQUIRE( sim_start( 30U ) );

    SIL_REQUIRE_EQ( s_done, 1U );
    SIL_EXPECT_EQ( s_ok_write, 1U );
    SIL_EXPECT_EQ( s_ok_readback, 1U );
    SIL_EXPECT( !sim_halted() );
    SIL_EXPECT_EQ( sim_model_violations(), 0U );
    sim_shutdown();
}

static void test_peer_processes_share_their_partitions_memory( void )
{
    uint32_t * peer_word;

    reset();
    build_two( own_memory_user );
    SIL_REQUIRE( sim_start( 30U ) );

    /* The write into the peer process's stack succeeded and landed:
     * isolation is per partition, not per process. */
    SIL_REQUIRE_EQ( s_done, 1U );
    SIL_EXPECT_EQ( s_ok_read, 1U );
    peer_word = sim_mem_ptr( domain_probe( 0U, 1U ), 4U );
    SIL_REQUIRE( peer_word != NULL );
    SIL_EXPECT_EQ( *peer_word, 0x33334444UL );
    sim_shutdown();
}

/* ---- Denied accesses ------------------------------------------------------ */

static volatile uint32_t s_target_address;
static volatile uint32_t s_do_write;

static void trespasser( void * arg )
{
    uint32_t value = 0U;

    ( void ) arg;
    s_attempts++;

    if( s_do_write != 0U )
    {
        ( void ) sim_write32( s_target_address, 0xBADBADUL );
    }
    else
    {
        ( void ) sim_read32( s_target_address, &value );
    }

    /* Only reachable if the access was allowed or the fault was ignored. */
    s_returned_from_denied_access++;

    for( ;; )
    {
        maxrtos_yield();
    }
}

/* Run a trespass against `address`; return the canary word (or NULL). */
static uint32_t * run_trespass( uint32_t address, uint32_t is_write )
{
    uint32_t * word;

    reset();
    build_two( trespasser );
    s_target_address = address;
    s_do_write = is_write;

    word = sim_mem_ptr( address, 4U );

    if( word != NULL )
    {
        *word = ( uint32_t ) CANARY;
    }

    SIL_REQUIRE( sim_start( 120U ) );

    return word;
}

static void expect_denied( uint32_t * word )
{
    /* The access faulted (not just failed quietly): the process was
     * restarted by the health monitor and tried again. */
    SIL_EXPECT( s_attempts >= 2U );
    SIL_EXPECT_EQ( s_returned_from_denied_access, 0U );

    if( word != NULL )
    {
        SIL_EXPECT_EQ( *word, CANARY );
    }

    /* The rest of the system carried on. */
    SIL_EXPECT( s_peer_progress > 100U );
    SIL_EXPECT( !sim_halted() );
}

static void test_foreign_partition_memory_is_inaccessible( void )
{
    uint32_t * word;

    /* Partition 0 writes, then reads, partition 1's memory. */
    word = run_trespass( domain_probe( 1U, 0U ), 1U );
    expect_denied( word );
    sim_shutdown();

    word = run_trespass( domain_probe( 1U, 0U ), 0U );
    expect_denied( word );
    sim_shutdown();
}

static void test_kernel_memory_is_inaccessible( void )
{
    uint32_t * word;

    word = run_trespass( SIL_KERNEL_GUARD, 1U );
    expect_denied( word );
    sim_shutdown();

    word = run_trespass( SIL_KERNEL_GUARD, 0U );
    expect_denied( word );
    sim_shutdown();
}

static void test_flash_is_read_only_for_partitions( void )
{
    uint32_t * word;

    /* No backing for flash in this build, so a write must fault at the MPU
     * (permission), not at the bus. Map a page so the target exists. */
    reset();
    build_two( trespasser );
    ( void ) sim_mem_map( "FLASH", 0x08000000UL, 0x1000U );
    s_target_address = 0x08000010UL;
    s_do_write = 1U;
    word = sim_mem_ptr( 0x08000010UL, 4U );
    *word = ( uint32_t ) CANARY;
    SIL_REQUIRE( sim_start( 120U ) );
    expect_denied( word );
    sim_shutdown();
}

/* ---- REQ-SP-004: ports ---------------------------------------------------- */

static void test_port_memory_is_reserved_for_member_partitions( void )
{
    sil_system_t * sys;
    uint32_t * word;

    /* Port 1 (members: partition 1 only). Partition 0 must be refused. */
    reset();
    build_two( trespasser );
    sys = sil_system();
    s_target_address = sys->port_address[ 1 ] + 1024U;
    s_do_write = 1U;
    word = sim_mem_ptr( s_target_address, 4U );
    *word = ( uint32_t ) CANARY;
    SIL_REQUIRE( sim_start( 120U ) );
    expect_denied( word );
    sim_shutdown();
}

static void member_port_user( void * arg )
{
    sil_system_t const * sys = sil_system();
    uint32_t value = 0U;

    ( void ) arg;

    /* Port 0's members: partition 0 (this one). */
    s_ok_write = sim_write32( sys->port_address[ 0 ] + 1024U, 0xAAAA5555UL ) ? 1U : 0U;
    s_ok_readback = ( sim_read32( sys->port_address[ 0 ] + 1024U, &value ) && ( value == 0xAAAA5555UL ) ) ? 1U : 0U;
    s_done = 1U;

    for( ;; )
    {
        maxrtos_yield();
    }
}

static void test_members_can_use_their_port_memory( void )
{
    reset();
    build_two( member_port_user );
    SIL_REQUIRE( sim_start( 30U ) );

    SIL_REQUIRE_EQ( s_done, 1U );
    SIL_EXPECT_EQ( s_ok_write, 1U );
    SIL_EXPECT_EQ( s_ok_readback, 1U );
    SIL_EXPECT( !sim_halted() );
    sim_shutdown();
}

/* ---- REQ-SP-005: the MPU follows the running partition ---------------------- */

static void idle_process( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        sim_cpu_work( 100U );
    }
}

static void test_mpu_maps_exactly_the_running_partition( void )
{
    sil_system_spec_t spec;
    sil_system_t * sys;
    uint32_t i;

    sil_spec_two_partitions( &spec, idle_process, idle_process );
    spec.port_count = 3U;
    spec.port[ 0 ].message_size = 4U;
    spec.port[ 0 ].capacity = 2U;
    spec.port[ 0 ].member_mask = 0x1U;
    spec.port[ 1 ].message_size = 4U;
    spec.port[ 1 ].capacity = 2U;
    spec.port[ 1 ].member_mask = 0x2U;
    spec.port[ 2 ].message_size = 4U;
    spec.port[ 2 ].capacity = 2U;
    spec.port[ 2 ].member_mask = 0x3U;
    sil_build( &spec );
    sys = sil_system();

    SIL_REQUIRE( sim_start( 1U ) );

    for( i = 1U; i < 64U; i++ )
    {
        uint32_t base;
        uint32_t size;
        uint32_t access;
        int32_t running = sim_partition_at_tick( sim_tick() );
        uint32_t p;
        uint32_t port;

        SIL_REQUIRE( ( running == 0 ) || ( running == 1 ) );
        p = ( uint32_t ) running;

        /* The partition slot holds the running partition's domain. */
        SIL_REQUIRE( sim_mpu_region_info( 8U, &base, &size, &access ) );
        SIL_REQUIRE_EQ( base, sys->domain_base[ p ] );
        SIL_REQUIRE_EQ( size, sys->domain_size[ p ] );
        SIL_REQUIRE_EQ( access, 3U ); /* read/write for user and privileged */

        /* Port regions 9.. are enabled exactly for member partitions. */
        for( port = 0U; port < 3U; port++ )
        {
            uint32_t port_base;
            bool mapped = sim_mpu_region_info( 9U + port, &port_base, &size, &access );
            bool member = ( ( ( uint32_t ) spec.port[ port ].member_mask >> p ) & 1U ) != 0U;

            SIL_REQUIRE( mapped == member );

            if( mapped )
            {
                SIL_REQUIRE_EQ( port_base, sys->port_address[ port ] );
            }
        }

        SIL_REQUIRE( sim_run( 1U ) );
    }

    SIL_EXPECT_EQ( sim_model_violations(), 0U );
    sim_shutdown();
}

static sil_case_t const s_cases[] =
{
    SIL_CASE( test_process_can_use_its_own_partitions_memory, "REQ-SP-001", "A process reads and writes its own partition's memory" ),
    SIL_CASE( test_foreign_partition_memory_is_inaccessible, "REQ-SP-002", "Read and write of another partition's memory fault; memory unchanged" ),
    SIL_CASE( test_kernel_memory_is_inaccessible, "REQ-SP-003", "Read and write of kernel memory fault; memory unchanged" ),
    SIL_CASE( test_port_memory_is_reserved_for_member_partitions, "REQ-SP-004", "A non-member cannot touch a port's memory" ),
    SIL_CASE( test_members_can_use_their_port_memory, "REQ-SP-004", "A member can use its port's memory" ),
    SIL_CASE( test_mpu_maps_exactly_the_running_partition, "REQ-SP-005", "The MPU maps exactly the running partition and its ports on every switch" ),
    SIL_CASE( test_peer_processes_share_their_partitions_memory, "REQ-SP-006", "Peer processes share their partition's memory domain" ),
    SIL_CASE( test_flash_is_read_only_for_partitions, "REQ-SP-007", "Flash is read-only for every partition" ),
};

int main( int argc, char ** argv )
{
    int status = sil_run_suite( "spatial_partitioning", s_cases,
                                sizeof( s_cases ) / sizeof( s_cases[ 0 ] ), argc, argv );

    sim_shutdown();

    return status;
}
