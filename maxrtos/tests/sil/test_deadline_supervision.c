/**
 * @file test_deadline_supervision.c
 * @brief SIL: periodic release and deadline supervision (ARINC 653 PERIOD and
 *        TIME_CAPACITY), from the tick through the health monitor to the
 *        recovery the architecture layer enacts.
 *
 * The schedule is the usual 8-tick major frame: partition 0 owns ticks 0..4,
 * partition 1 owns ticks 5..7.
 *
 * Requirements
 *   REQ-DL-001  A periodic process is released exactly every period, with no
 *               jitter or drift.
 *   REQ-DL-002  A process that completes each release within its time capacity
 *               is never reported as missing.
 *   REQ-DL-003  A process that overruns its time capacity is detected at its
 *               deadline and handled by its partition's policy: restarted and
 *               re-released, without disturbing anything else.
 *   REQ-DL-004  The deadline is wall-clock time, not a CPU budget: a process
 *               whose partition is not scheduled before its deadline misses it
 *               having used no CPU.
 *   REQ-DL-005  A miss under halt_partition stops that partition and only that
 *               partition.
 *   REQ-DL-006  A process blocked on IPC when its deadline expires is restarted
 *               and leaves the wait list.
 *   REQ-DL-007  While a process waits for its next release and nothing else is
 *               ready, the CPU idles instead of running another partition's
 *               code in its slot.
 *   REQ-DL-008  Each fault class keeps its own policy: a deadline miss can be
 *               configured independently of memory faults.
 */

#include "sil_system.h"
#include "sil_test.h"

#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/queue_port.h"
#include "maxrtos/queue_port.h"
#include "maxrtos/timing.h"
#include "maxrtos/yield.h"

#define FRAME ( 8U )

static volatile uint32_t s_releases;
static volatile uint32_t s_release_tick[ 64 ];
static volatile uint32_t s_attempts;
static volatile uint32_t s_other_progress;
static volatile uint32_t s_peer_progress;

static void reset( void )
{
    unsigned i;

    s_releases = 0U;
    s_attempts = 0U;
    s_other_progress = 0U;
    s_peer_progress = 0U;

    for( i = 0U; i < 64U; i++ )
    {
        s_release_tick[ i ] = 0U;
    }
}

static void note_release( void )
{
    if( s_releases < 64U )
    {
        s_release_tick[ s_releases ] = sim_tick();
    }

    s_releases++;
}

static void bystander( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        s_other_progress++;
        sim_cpu_work( 100U );
    }
}

static maxrtos_process_control_block_t * process( unsigned partition, unsigned index )
{
    return maxrtos_process_get( sil_system()->process_id[ partition ][ index ] );
}

/* A well-behaved periodic process: a little work, then wait for the next
 * release. */
static void punctual( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        note_release();
        sim_cpu_work( SIM_CYCLES_PER_TICK / 4U );

        if( maxrtos_periodic_wait() != MAXRTOS_OK )
        {
            break;
        }
    }

    for( ;; )
    {
        maxrtos_yield();
    }
}

/* Never completes its release. */
static void overrunner( void * arg )
{
    ( void ) arg;
    s_attempts++;
    note_release();

    for( ;; )
    {
        sim_cpu_work( 100U );
    }
}

static void build_periodic( void ( * entry )( void * ), uint32_t period, uint32_t capacity )
{
    sil_system_spec_t spec;

    sil_spec_two_partitions( &spec, entry, bystander );
    spec.partition[ 0 ].process[ 0 ].period = period;
    spec.partition[ 0 ].process[ 0 ].time_capacity = capacity;
    sil_build( &spec );
}

/* REQ-DL-001 */
static void test_periodic_release_is_exact( void )
{
    uint32_t i;
    uint32_t frames = 40U;

    reset();
    build_periodic( punctual, FRAME, 5U );
    SIL_REQUIRE( sim_start( frames * FRAME ) );

    SIL_REQUIRE( s_releases >= frames - 1U );

    for( i = 0U; i < frames - 1U; i++ )
    {
        /* Released at every multiple of the period (and the partition runs at
         * the start of each frame), so the process wakes at that very tick. */
        SIL_REQUIRE_EQ( s_release_tick[ i ], i * FRAME );
    }

    SIL_EXPECT_EQ( process( 0U, 0U )->deadline_misses, 0U );
    sim_shutdown();
}

/* REQ-DL-002 */
static void test_completing_within_the_capacity_never_misses( void )
{
    reset();
    build_periodic( punctual, FRAME, 3U );
    SIL_REQUIRE( sim_start( 200U * FRAME ) );

    SIL_EXPECT( s_releases >= 199U );
    SIL_EXPECT_EQ( process( 0U, 0U )->deadline_misses, 0U );
    SIL_EXPECT( s_other_progress > 100U );
    SIL_EXPECT( !sim_halted() );
    sim_shutdown();
}

/* REQ-DL-003 */
static void test_an_overrun_is_detected_and_the_process_restarted( void )
{
    uint32_t misses;

    reset();
    build_periodic( overrunner, FRAME, 3U );
    SIL_REQUIRE( sim_start( 20U * FRAME ) );

    misses = process( 0U, 0U )->deadline_misses;

    /* Missed on every release and restarted at its entry each time. A restart
     * begins a new release, so a miss can also fall while the process is
     * waiting in the queue for its partition's next slot: about two per frame,
     * and it runs again from its entry whenever it next gets the CPU. */
    SIL_EXPECT( misses >= 30U );
    SIL_EXPECT( s_attempts >= misses / 2U );

    /* The first release began at tick 0 with a deadline at tick 3. The tick
     * that reaches the deadline detects the miss, and the next SysTick acts
     * on it, so the restarted process runs again at tick 3. */
    SIL_EXPECT_EQ( s_release_tick[ 0 ], 0U );
    SIL_EXPECT_EQ( s_release_tick[ 1 ], 3U );

    /* The other partition and the schedule are unaffected. */
    SIL_EXPECT( s_other_progress > 100U );
    SIL_EXPECT( !sim_halted() );
    sim_shutdown();
}

/* REQ-DL-004 */
static void test_the_deadline_is_wall_clock_not_a_cpu_budget( void )
{
    sil_system_spec_t spec;

    reset();

    /* The periodic process lives in partition 1, which is not scheduled until
     * tick 5, yet its deadline is 3 ticks after each release at tick 0. */
    sil_spec_two_partitions( &spec, bystander, punctual );
    spec.partition[ 1 ].process[ 0 ].period = FRAME;
    spec.partition[ 1 ].process[ 0 ].time_capacity = 3U;
    sil_build( &spec );

    SIL_REQUIRE( sim_start( 30U * FRAME ) );

    /* It was starved of CPU and never used its 3 ticks, and still missed. */
    SIL_EXPECT( process( 1U, 0U )->deadline_misses >= 1U );
    SIL_EXPECT( s_other_progress > 100U );
    sim_shutdown();
}

/* REQ-DL-005 */
static void test_halt_on_a_miss_stops_only_that_partition( void )
{
    sil_system_spec_t spec;
    uint32_t t;
    uint32_t frozen;

    reset();
    sil_spec_two_partitions( &spec, overrunner, bystander );
    spec.partition[ 0 ].process[ 0 ].period = FRAME;
    spec.partition[ 0 ].process[ 0 ].time_capacity = 3U;
    spec.partition[ 0 ].action[ MAXRTOS_FAULT_DEADLINE_EXCEEDED ] = MAXRTOS_HM_ACTION_HALT_PARTITION;
    sil_build( &spec );

    SIL_REQUIRE( sim_start( 6U ) );
    frozen = s_attempts;
    SIL_REQUIRE( sim_run( 14U * FRAME ) );

    SIL_EXPECT_EQ( s_attempts, frozen );          /* not restarted */
    SIL_EXPECT_EQ( process( 0U, 0U )->deadline_misses, 1U );

    for( t = 8U; t < sim_tick(); t++ )
    {
        if( ( t % FRAME ) < 5U )
        {
            SIL_REQUIRE_EQ( sim_partition_at_tick( t ), SIM_IDLE );  /* halted */
        }
        else
        {
            SIL_REQUIRE_EQ( sim_partition_at_tick( t ), 1 );         /* untouched */
        }
    }

    SIL_EXPECT( s_other_progress > 100U );
    SIL_EXPECT( !sim_halted() );
    sim_shutdown();
}

/* REQ-DL-006 */
static volatile uint32_t s_wait_returned;

static void blocked_receiver( void * arg )
{
    uint32_t value = 0U;

    ( void ) arg;
    s_attempts++;

    /* Waits for a message that never comes. */
    if( maxrtos_queue_port_receive( sil_system()->port[ 0 ], &value, sizeof( value ),
                                    MAXRTOS_TIMEOUT_INFINITE ) == MAXRTOS_OK )
    {
        s_wait_returned++;
    }

    for( ;; )
    {
        maxrtos_yield();
    }
}

static void peer( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        s_peer_progress++;
        maxrtos_yield();
    }
}

static void test_a_blocked_process_that_misses_is_restarted_off_the_wait_list( void )
{
    sil_system_spec_t spec;

    reset();
    s_wait_returned = 0U;
    sil_spec_two_partitions( &spec, blocked_receiver, bystander );
    spec.partition[ 0 ].process_count = 2U;
    spec.partition[ 0 ].process[ 0 ].time_capacity = 6U; /* aperiodic, 6-tick deadline */
    spec.partition[ 0 ].process[ 1 ].entry = peer;
    spec.partition[ 0 ].process[ 1 ].priority = 5U;
    spec.port_count = 1U;
    spec.port[ 0 ].message_size = 4U;
    spec.port[ 0 ].capacity = 4U;
    spec.port[ 0 ].member_mask = 0x3U;
    sil_build( &spec );

    SIL_REQUIRE( sim_start( 10U * FRAME ) );

    /* It blocked, missed its deadline while blocked, and was restarted at its
     * entry (and blocked again, since the restart re-arms a 6-tick deadline). */
    SIL_EXPECT( process( 0U, 0U )->deadline_misses >= 2U );
    SIL_EXPECT( s_attempts >= 2U );
    SIL_EXPECT_EQ( s_wait_returned, 0U );

    /* Never on the wait list twice: at most the one current wait. */
    SIL_EXPECT( sil_system()->port[ 0 ]->waiting_receivers.count <= 1U );
    SIL_EXPECT( s_peer_progress > 50U );
    sim_shutdown();
}

/* REQ-DL-007 */
static void test_the_cpu_idles_while_a_lone_process_waits_for_its_release( void )
{
    uint32_t frame;
    uint32_t t;
    uint32_t idle_ticks = 0U;

    reset();
    build_periodic( punctual, FRAME, 5U );
    SIL_REQUIRE( sim_start( 30U * FRAME ) );

    for( frame = 1U; frame < 29U; frame++ )
    {
        /* Partition 0's slot is ticks 0..4. The process runs a quarter tick
         * at the start of the frame, then waits: nothing else is READY in
         * its partition, so the rest of the slot is idle, not partition 1's
         * (or anyone else's) code. */
        for( t = 1U; t < 5U; t++ )
        {
            SIL_REQUIRE_EQ( sim_partition_at_tick( frame * FRAME + t ), SIM_IDLE );
            idle_ticks++;
        }

        SIL_REQUIRE_EQ( sim_partition_at_tick( frame * FRAME ), 0 );
        SIL_REQUIRE_EQ( sim_partition_at_tick( frame * FRAME + 5U ), 1 );
    }

    SIL_EXPECT( idle_ticks > 100U );
    SIL_EXPECT( s_other_progress > 100U );
    sim_shutdown();
}

/* REQ-DL-007 */
static void test_a_slot_with_nothing_ready_does_not_run_the_previous_partition( void )
{
    sil_system_spec_t spec;
    uint32_t t;
    uint32_t checked = 0U;

    reset();

    /* The lone periodic process is in partition 1 (slot: ticks 5..7) with a
     * 16-tick period, so on alternate frames it is still waiting when its
     * slot arrives, while partition 0's process is running at that moment. */
    sil_spec_two_partitions( &spec, bystander, punctual );
    spec.partition[ 1 ].process[ 0 ].period = 2U * FRAME;
    spec.partition[ 1 ].process[ 0 ].time_capacity = 2U * FRAME;
    sil_build( &spec );

    SIL_REQUIRE( sim_start( 20U * FRAME ) );

    /* The process is released at ticks 15, 31, 47, ... (kernel ticks 16, 32,
     * ...): it runs at the end of every other slot and waits through the
     * whole slot in between. Ticks 21..23, then every 32 ticks, are such a
     * slot: partition 0's process was running until tick 20, and nothing may
     * run in partition 1's slot but the idle context. */
    for( t = 21U; ( t + 2U ) < sim_tick(); t += 4U * FRAME )
    {
        SIL_REQUIRE_EQ( sim_partition_at_tick( t ), SIM_IDLE );
        SIL_REQUIRE_EQ( sim_partition_at_tick( t + 1U ), SIM_IDLE );
        SIL_REQUIRE_EQ( sim_partition_at_tick( t + 2U ), SIM_IDLE );
        checked++;
    }

    SIL_EXPECT( checked >= 4U );
    SIL_EXPECT( s_releases >= 8U );
    sim_shutdown();
}

/* REQ-DL-008 */
static void memory_faulter( void * arg )
{
    ( void ) arg;
    s_attempts++;
    ( void ) sim_write32( SIL_KERNEL_GUARD, 1U );

    for( ;; )
    {
        maxrtos_yield();
    }
}

static void test_a_deadline_miss_has_its_own_policy( void )
{
    sil_system_spec_t spec;

    /* Deadline misses restart; memory faults halt. A process that overruns is
     * restarted, not halted. */
    reset();
    sil_spec_two_partitions( &spec, overrunner, bystander );
    spec.partition[ 0 ].process[ 0 ].period = FRAME;
    spec.partition[ 0 ].process[ 0 ].time_capacity = 3U;
    spec.partition[ 0 ].action[ MAXRTOS_FAULT_MEMORY_ACCESS ] = MAXRTOS_HM_ACTION_HALT_PARTITION;
    spec.partition[ 0 ].action[ MAXRTOS_FAULT_DEADLINE_EXCEEDED ] = MAXRTOS_HM_ACTION_RESTART_PROCESS;
    sil_build( &spec );

    SIL_REQUIRE( sim_start( 12U * FRAME ) );
    SIL_EXPECT( s_attempts >= 8U );      /* restarted repeatedly, never halted */
    sim_shutdown();

    /* And the converse: a memory fault halts even though deadline misses
     * would restart. */
    reset();
    sil_spec_two_partitions( &spec, memory_faulter, bystander );
    spec.partition[ 0 ].process[ 0 ].period = FRAME;
    spec.partition[ 0 ].process[ 0 ].time_capacity = 3U;
    spec.partition[ 0 ].action[ MAXRTOS_FAULT_MEMORY_ACCESS ] = MAXRTOS_HM_ACTION_HALT_PARTITION;
    sil_build( &spec );

    SIL_REQUIRE( sim_start( 12U * FRAME ) );
    SIL_EXPECT_EQ( s_attempts, 1U );
    sim_shutdown();
}

static sil_case_t const s_cases[] =
{
    SIL_CASE( test_periodic_release_is_exact, "REQ-DL-001", "A periodic process is released exactly every period" ),
    SIL_CASE( test_completing_within_the_capacity_never_misses, "REQ-DL-002", "Completing within the time capacity never reports a miss" ),
    SIL_CASE( test_an_overrun_is_detected_and_the_process_restarted, "REQ-DL-003", "An overrun is detected at its deadline; the process is restarted and re-released" ),
    SIL_CASE( test_the_deadline_is_wall_clock_not_a_cpu_budget, "REQ-DL-004", "A starved process misses its deadline having used no CPU" ),
    SIL_CASE( test_halt_on_a_miss_stops_only_that_partition, "REQ-DL-005", "halt_partition on a miss stops only that partition" ),
    SIL_CASE( test_a_blocked_process_that_misses_is_restarted_off_the_wait_list, "REQ-DL-006", "A process blocked on IPC that misses is restarted off the wait list" ),
    SIL_CASE( test_the_cpu_idles_while_a_lone_process_waits_for_its_release, "REQ-DL-007", "The CPU idles while a lone process waits for its release" ),
    SIL_CASE( test_a_slot_with_nothing_ready_does_not_run_the_previous_partition, "REQ-DL-007", "A slot with nothing ready idles instead of running the previous partition" ),
    SIL_CASE( test_a_deadline_miss_has_its_own_policy, "REQ-DL-008", "A deadline miss follows its own policy, independent of memory faults" ),
};

int main( int argc, char ** argv )
{
    int status = sil_run_suite( "deadline_supervision", s_cases,
                                sizeof( s_cases ) / sizeof( s_cases[ 0 ] ), argc, argv );

    sim_shutdown();

    return status;
}
