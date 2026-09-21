/**
 * @file test_fault_containment.c
 * @brief SIL: faults are contained by the health monitor's per-partition
 *        policy, through the production fault-recovery path.
 *
 * Faults are real exceptions on the virtual target: a process that touches
 * memory it may not is stopped by the MPU model, an injected fault is taken
 * on whatever context is running, and both go through the production
 * classification-to-recovery code (maxrtos_arch_handle_fault).
 *
 * Requirements
 *   REQ-FT-001  RESTART_PROCESS: the faulting process is restarted at its
 *               entry point; every other process and the schedule are
 *               unaffected.
 *   REQ-FT-002  HALT_PARTITION: the partition, including its healthy peers,
 *               never runs again; its slots idle; other partitions keep their
 *               exact timing.
 *   REQ-FT-003  Each partition applies its own policy.
 *   REQ-FT-004  Every fault class is handled according to its own policy.
 *   REQ-FT-005  Repeated faults (a fault storm) leave the system sound.
 *   REQ-FT-006  An asynchronous fault is charged to the partition that was
 *               running when it occurred.
 *   REQ-FT-007  A fault with no health monitor installed stops the CPU
 *               instead of continuing with unknown state.
 *   REQ-FT-008  Peers waiting on a faulted process are not stranded.
 */

#include "sil_system.h"
#include "sil_test.h"

#include "maxrtos/arch/cortex_m7/fault_handlers.h"
#include "maxrtos/queue_port.h"
#include "maxrtos/yield.h"

#define KERNEL_WORD ( SIL_KERNEL_GUARD )

static volatile uint32_t s_attempts[ SIL_MAX_PARTITIONS ];
static volatile uint32_t s_after_fault[ SIL_MAX_PARTITIONS ];
static volatile uint32_t s_fault_tick[ SIL_MAX_PARTITIONS ];
static volatile uint32_t s_progress[ 8 ];
static volatile maxrtos_fault_type_t s_fault_type;

/* A process that faults every time it starts. */
static void faulter( uint32_t partition )
{
    s_attempts[ partition ]++;

    if( s_fault_tick[ partition ] == 0U )
    {
        s_fault_tick[ partition ] = sim_tick();
    }

    if( s_fault_type == MAXRTOS_FAULT_MEMORY_ACCESS )
    {
        ( void ) sim_write32( KERNEL_WORD, 1U );
    }
    else
    {
        sim_fault( s_fault_type );
    }

    /* Reachable only if the fault was ignored. */
    s_after_fault[ partition ]++;

    for( ;; )
    {
        maxrtos_yield();
    }
}

static void faulter_0( void * arg ) { ( void ) arg; faulter( 0U ); }
static void faulter_1( void * arg ) { ( void ) arg; faulter( 1U ); }

#define WORKER( n_ )                                                   \
    static void worker_##n_( void * arg )                              \
    {                                                                  \
        ( void ) arg;                                                  \
        for( ;; ) { s_progress[ n_ ]++; sim_cpu_work( 100U ); }        \
    }

WORKER( 0 )
WORKER( 1 )
WORKER( 2 )
WORKER( 3 )

static void reset( void )
{
    unsigned i;

    for( i = 0U; i < SIL_MAX_PARTITIONS; i++ )
    {
        s_attempts[ i ] = 0U;
        s_after_fault[ i ] = 0U;
        s_fault_tick[ i ] = 0U;
    }

    for( i = 0U; i < 8U; i++ )
    {
        s_progress[ i ] = 0U;
    }

    s_fault_type = MAXRTOS_FAULT_MEMORY_ACCESS;
}

/* Partition 0: faulter + worker 0.  Partition 1: worker 1 + worker 2, or a
 * faulter + worker when both partitions fault. */
static void build( maxrtos_hm_action_t action_0, maxrtos_hm_action_t action_1, bool p1_faults )
{
    sil_system_spec_t spec;
    uint32_t f;

    sil_spec_two_partitions( &spec, faulter_0, p1_faults ? faulter_1 : worker_1 );
    spec.partition[ 0 ].process_count = 2U;
    spec.partition[ 0 ].process[ 1 ].entry = worker_0;
    spec.partition[ 0 ].process[ 1 ].priority = 5U;
    spec.partition[ 1 ].process_count = 2U;
    spec.partition[ 1 ].process[ 1 ].entry = p1_faults ? worker_2 : worker_2;
    spec.partition[ 1 ].process[ 1 ].priority = 5U;

    for( f = 0U; f < ( uint32_t ) MAXRTOS_FAULT_COUNT; f++ )
    {
        spec.partition[ 0 ].action[ f ] = action_0;
        spec.partition[ 1 ].action[ f ] = action_1;
    }

    sil_build( &spec );
}

static int32_t oracle( uint32_t tick )
{
    return ( ( tick % 8U ) < 5U ) ? 0 : 1;
}

/* ---- REQ-FT-001 ------------------------------------------------------------- */

static void test_restart_contains_a_faulting_process( void )
{
    uint32_t t;
    uint32_t * guard;

    reset();
    build( MAXRTOS_HM_ACTION_RESTART_PROCESS, MAXRTOS_HM_ACTION_RESTART_PROCESS, false );
    guard = sim_mem_ptr( KERNEL_WORD, 4U );
    *guard = 0xC0FFEEUL;

    SIL_REQUIRE( sim_start( 400U ) );

    /* Restarted again and again, always at its entry: it never got past the
     * faulting access. */
    SIL_EXPECT( s_attempts[ 0 ] >= 20U );
    SIL_EXPECT_EQ( s_after_fault[ 0 ], 0U );

    /* Its healthy peer and the other partition keep running... */
    SIL_EXPECT( s_progress[ 0 ] > 100U );
    SIL_EXPECT( s_progress[ 1 ] > 100U );
    SIL_EXPECT( s_progress[ 2 ] > 100U );

    /* ...the schedule is exactly the configured one, tick for tick... */
    for( t = 0U; t < 400U; t++ )
    {
        SIL_REQUIRE_EQ( sim_partition_at_tick( t ), oracle( t ) );
    }

    /* ...and the kernel word the faulter kept trying to write is intact. */
    SIL_EXPECT_EQ( *guard, 0xC0FFEEUL );
    SIL_EXPECT( !sim_halted() );
    SIL_EXPECT_EQ( sim_model_violations(), 0U );
    sim_shutdown();
}

/* ---- REQ-FT-002 ------------------------------------------------------------- */

static void test_halt_stops_the_whole_partition_only( void )
{
    uint32_t t;
    uint32_t frozen;

    reset();
    build( MAXRTOS_HM_ACTION_HALT_PARTITION, MAXRTOS_HM_ACTION_RESTART_PROCESS, false );

    SIL_REQUIRE( sim_start( 60U ) );
    frozen = s_progress[ 0 ];
    SIL_REQUIRE( sim_run( 340U ) );

    SIL_EXPECT_EQ( s_attempts[ 0 ], 1U );           /* not restarted */
    SIL_EXPECT_EQ( s_after_fault[ 0 ], 0U );

    /* Partition 0's healthy peer stopped with it. */
    SIL_EXPECT_EQ( s_progress[ 0 ], frozen );

    /* The other partition never noticed. */
    SIL_EXPECT( s_progress[ 1 ] > 100U );
    SIL_EXPECT( s_progress[ 2 ] > 100U );

    /* Partition 0's slots idle; partition 1's keep their exact timing. */
    for( t = 60U; t < 400U; t++ )
    {
        if( oracle( t ) == 0 )
        {
            SIL_REQUIRE_EQ( sim_partition_at_tick( t ), SIM_IDLE );
        }
        else
        {
            SIL_REQUIRE_EQ( sim_partition_at_tick( t ), 1 );
        }
    }

    SIL_EXPECT( !sim_halted() ); /* the system as a whole is alive */
    sim_shutdown();
}

/* ---- REQ-FT-003 ------------------------------------------------------------- */

static void test_each_partition_applies_its_own_policy( void )
{
    uint32_t t;

    reset();
    build( MAXRTOS_HM_ACTION_HALT_PARTITION, MAXRTOS_HM_ACTION_RESTART_PROCESS, true );

    SIL_REQUIRE( sim_start( 400U ) );

    SIL_EXPECT_EQ( s_attempts[ 0 ], 1U );           /* partition 0: halted */
    SIL_EXPECT( s_attempts[ 1 ] >= 20U );           /* partition 1: restarted */
    SIL_EXPECT_EQ( s_after_fault[ 1 ], 0U );
    SIL_EXPECT( s_progress[ 2 ] > 100U );           /* partition 1's peer runs */

    for( t = 100U; t < 400U; t++ )
    {
        if( oracle( t ) == 0 )
        {
            SIL_REQUIRE_EQ( sim_partition_at_tick( t ), SIM_IDLE );
        }
        else
        {
            SIL_REQUIRE_EQ( sim_partition_at_tick( t ), 1 );
        }
    }

    sim_shutdown();
}

/* ---- REQ-FT-004 ------------------------------------------------------------- */

typedef struct
{
    maxrtos_fault_type_t type;
    char const * name;
} fault_class_t;

static fault_class_t const s_classes[] =
{
    { MAXRTOS_FAULT_MEMORY_ACCESS, "memory access" },
    { MAXRTOS_FAULT_BUS_ERROR, "bus error" },
    { MAXRTOS_FAULT_ILLEGAL_INSTRUCTION, "illegal instruction" },
    { MAXRTOS_FAULT_DIVIDE_BY_ZERO, "divide by zero" },
};

static void test_every_fault_class_follows_its_configured_action( void )
{
    unsigned c;
    unsigned action;

    for( c = 0U; c < sizeof( s_classes ) / sizeof( s_classes[ 0 ] ); c++ )
    {
        for( action = 0U; action < 2U; action++ )
        {
            maxrtos_hm_action_t configured =
                ( action == 0U ) ? MAXRTOS_HM_ACTION_RESTART_PROCESS
                                 : MAXRTOS_HM_ACTION_HALT_PARTITION;

            reset();
            s_fault_type = s_classes[ c ].type;
            build( configured, MAXRTOS_HM_ACTION_RESTART_PROCESS, false );
            SIL_REQUIRE( sim_start( 120U ) );

            if( configured == MAXRTOS_HM_ACTION_RESTART_PROCESS )
            {
                SIL_EXPECT( s_attempts[ 0 ] >= 3U );
                SIL_EXPECT( s_progress[ 0 ] > 20U );
            }
            else
            {
                SIL_EXPECT_EQ( s_attempts[ 0 ], 1U );
            }

            SIL_EXPECT( !sim_halted() );
            sim_shutdown();
        }
    }
}

static void test_a_fault_class_uses_only_its_own_policy( void )
{
    sil_system_spec_t spec;

    reset();
    s_fault_type = MAXRTOS_FAULT_DIVIDE_BY_ZERO;

    /* Memory faults halt the partition; divide-by-zero restarts the process. */
    sil_spec_two_partitions( &spec, faulter_0, worker_1 );
    spec.partition[ 0 ].process_count = 2U;
    spec.partition[ 0 ].process[ 1 ].entry = worker_0;
    spec.partition[ 0 ].process[ 1 ].priority = 5U;
    spec.partition[ 0 ].action[ MAXRTOS_FAULT_MEMORY_ACCESS ] = MAXRTOS_HM_ACTION_HALT_PARTITION;
    sil_build( &spec );

    SIL_REQUIRE( sim_start( 200U ) );

    SIL_EXPECT( s_attempts[ 0 ] >= 10U );  /* restarted, not halted */
    SIL_EXPECT( s_progress[ 0 ] > 50U );
    sim_shutdown();
}

/* ---- REQ-FT-005 ------------------------------------------------------------- */

static void test_fault_storm_leaves_the_system_sound( void )
{
    uint32_t t;
    uint32_t horizon;
    maxrtos_process_id_t ids_before[ 2 ];
    sil_system_t * sys;

    reset();
    build( MAXRTOS_HM_ACTION_RESTART_PROCESS, MAXRTOS_HM_ACTION_RESTART_PROCESS, false );
    sys = sil_system();
    ids_before[ 0 ] = sys->process_id[ 0 ][ 0 ];
    ids_before[ 1 ] = sys->process_id[ 1 ][ 0 ];

    horizon = 4000U;
    SIL_REQUIRE( sim_start( horizon ) );

    SIL_EXPECT( s_attempts[ 0 ] >= 2000U );
    SIL_EXPECT_EQ( s_after_fault[ 0 ], 0U );

    /* Identity and schedule intact after thousands of recoveries. */
    SIL_EXPECT_EQ( sys->process_id[ 0 ][ 0 ], ids_before[ 0 ] );
    SIL_EXPECT_EQ( sys->process_id[ 1 ][ 0 ], ids_before[ 1 ] );

    for( t = 0U; t < horizon; t++ )
    {
        SIL_REQUIRE_EQ( sim_partition_at_tick( t ), oracle( t ) );
    }

    SIL_EXPECT( s_progress[ 1 ] > 1000U );
    SIL_EXPECT( !sim_halted() );
    SIL_EXPECT_EQ( sim_model_violations(), 0U );
    sim_shutdown();
}

/* ---- REQ-FT-006 ------------------------------------------------------------- */

static void test_async_fault_is_charged_to_the_running_partition( void )
{
    unsigned p;

    for( p = 0U; p < 2U; p++ )
    {
        sil_system_spec_t spec;
        uint32_t tick_in_partition;
        uint32_t t;

        reset();
        sil_spec_two_partitions( &spec, worker_0, worker_1 );
        spec.partition[ 0 ].process_count = 2U;
        spec.partition[ 0 ].process[ 1 ].entry = worker_2;
        spec.partition[ 0 ].process[ 1 ].priority = 5U;
        spec.partition[ 1 ].process_count = 2U;
        spec.partition[ 1 ].process[ 1 ].entry = worker_3;
        spec.partition[ 1 ].process[ 1 ].priority = 5U;

        /* Both partitions halt on a memory fault; only one will get one. */
        spec.partition[ 0 ].action[ MAXRTOS_FAULT_MEMORY_ACCESS ] = MAXRTOS_HM_ACTION_HALT_PARTITION;
        spec.partition[ 1 ].action[ MAXRTOS_FAULT_MEMORY_ACCESS ] = MAXRTOS_HM_ACTION_HALT_PARTITION;
        sil_build( &spec );

        /* A tick in the middle of partition p's slot. */
        tick_in_partition = ( p == 0U ) ? 2U : 6U;
        sim_inject_fault_at_tick( 8U * 3U + tick_in_partition, MAXRTOS_FAULT_MEMORY_ACCESS );

        SIL_REQUIRE( sim_start( 200U ) );

        /* Partition p is halted from the fault on... */
        for( t = 8U * 3U + tick_in_partition + 1U; t < 200U; t++ )
        {
            if( oracle( t ) == ( int32_t ) p )
            {
                SIL_REQUIRE_EQ( sim_partition_at_tick( t ), SIM_IDLE );
            }
            else
            {
                SIL_REQUIRE_EQ( sim_partition_at_tick( t ), oracle( t ) );
            }
        }

        sim_shutdown();
    }
}

/* ---- REQ-FT-007 ------------------------------------------------------------- */

static void test_fault_without_a_health_monitor_stops_the_cpu( void )
{
    reset();
    build( MAXRTOS_HM_ACTION_RESTART_PROCESS, MAXRTOS_HM_ACTION_RESTART_PROCESS, false );

    /* Un-install the health monitor after configuration. */
    maxrtos_arch_set_health_monitor( NULL );

    SIL_EXPECT( !sim_start( 100U ) );
    SIL_EXPECT( sim_halted() );
    SIL_EXPECT( sim_halt_reason() != NULL );
    SIL_EXPECT_EQ( s_after_fault[ 0 ], 0U ); /* no code ran past the fault */
    sim_shutdown();
}

/* ---- REQ-FT-008 ------------------------------------------------------------- */

static volatile maxrtos_status_t s_wait_status;
static volatile uint32_t s_wait_done;

static void waiter_for_faulty_sender( void * arg )
{
    uint32_t value = 0U;

    ( void ) arg;
    s_wait_status = maxrtos_queue_port_receive( sil_system()->port[ 0 ], &value, sizeof( value ), 40U );
    s_wait_done = 1U;

    for( ;; )
    {
        maxrtos_yield();
    }
}

static void test_waiting_peer_is_released_when_its_sender_faults( void )
{
    sil_system_spec_t spec;

    reset();
    s_wait_done = 0U;
    s_wait_status = MAXRTOS_OK;

    /* Partition 0's main would have sent, but faults instead; the receiver
     * in partition 1 waits with a timeout. */
    sil_spec_two_partitions( &spec, faulter_0, waiter_for_faulty_sender );
    spec.partition[ 0 ].process_count = 2U;
    spec.partition[ 0 ].process[ 1 ].entry = worker_0;
    spec.partition[ 0 ].process[ 1 ].priority = 5U;
    spec.partition[ 1 ].process_count = 2U;
    spec.partition[ 1 ].process[ 1 ].entry = worker_2;
    spec.partition[ 1 ].process[ 1 ].priority = 5U;
    spec.partition[ 0 ].action[ MAXRTOS_FAULT_MEMORY_ACCESS ] = MAXRTOS_HM_ACTION_HALT_PARTITION;

    spec.port_count = 1U;
    spec.port[ 0 ].message_size = 4U;
    spec.port[ 0 ].capacity = 4U;
    spec.port[ 0 ].member_mask = 0x3U;
    sil_build( &spec );

    SIL_REQUIRE( sim_start( 200U ) );

    SIL_REQUIRE_EQ( s_wait_done, 1U );
    SIL_EXPECT_EQ( s_wait_status, MAXRTOS_ERR_TIMEOUT );
    SIL_EXPECT( s_progress[ 2 ] > 10U );
    sim_shutdown();
}

static sil_case_t const s_cases[] =
{
    SIL_CASE( test_restart_contains_a_faulting_process, "REQ-FT-001", "Restart contains the fault; peers, other partitions and the schedule are unaffected" ),
    SIL_CASE( test_halt_stops_the_whole_partition_only, "REQ-FT-002", "Halt stops the whole partition, idles its slots, spares the others" ),
    SIL_CASE( test_each_partition_applies_its_own_policy, "REQ-FT-003", "One partition halts while another restarts" ),
    SIL_CASE( test_every_fault_class_follows_its_configured_action, "REQ-FT-004", "Each fault class x action gives the configured outcome" ),
    SIL_CASE( test_a_fault_class_uses_only_its_own_policy, "REQ-FT-004", "A fault class is not affected by another class's policy" ),
    SIL_CASE( test_fault_storm_leaves_the_system_sound, "REQ-FT-005", "2000+ restarts leave identity, schedule and peers intact" ),
    SIL_CASE( test_async_fault_is_charged_to_the_running_partition, "REQ-FT-006", "An injected fault halts the partition that was running" ),
    SIL_CASE( test_fault_without_a_health_monitor_stops_the_cpu, "REQ-FT-007", "No health monitor: the CPU stops safely" ),
    SIL_CASE( test_waiting_peer_is_released_when_its_sender_faults, "REQ-FT-008", "A receiver waiting on a faulted sender is released by its timeout" ),
};

int main( int argc, char ** argv )
{
    int status = sil_run_suite( "fault_containment", s_cases,
                                sizeof( s_cases ) / sizeof( s_cases[ 0 ] ), argc, argv );

    sim_shutdown();

    return status;
}
