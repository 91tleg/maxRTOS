/**
 * @file test_ipc_sampling_port.c
 * @brief SIL: sampling ports through the real SVC path, MPU and scheduler.
 *
 * Verifies
 * - A read before the first write returns MAXRTOS_ERR_QUEUE_EMPTY and
 *   copies nothing.
 * - A reader in another partition sees the latest message a writer wrote:
 *   never a torn one, never an older one than it saw before, and a read does
 *   not consume it.
 * - A write replaces the message; nothing queues behind it.
 * - The kernel never dereferences a pointer the caller could not access
 *   itself: kernel memory, another partition's memory and malformed ranges
 *   are rejected, and nothing is read, written or disturbed.
 * - A port is named by ID, never by address, and only by a partition the
 *   port lists as a writer (to write) or a reader (to read): an ID past the
 *   pool, an address, a non-member and the wrong direction are refused.
 * - A message whose size differs from the port's is rejected.
 */

#include "sil_system.h"
#include "sil_test.h"

#include "maxrtos/kernel/sampling_port.h"
#include "maxrtos/sampling_port.h"
#include "maxrtos/yield.h"

#define SA ( sil_system()->sampling_id[ 0 ] ) /* 0 writes, 1 reads; 8 bytes */
#define SB ( sil_system()->sampling_id[ 1 ] ) /* 1 writes, 0 reads; 4 bytes */

#define CANARY ( 0xC0FFEE42UL )

/* A message that shows a tear: the second word is the complement of the
 * first. */
typedef struct
{
    uint32_t value;
    uint32_t complement;
} message_t;

static void idle_yield( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        maxrtos_yield();
    }
}

static void build_system(
    void ( *p0_main )( void * ),
    void ( *p1_main )( void * ) )
{
    sil_system_spec_t spec;

    sil_spec_two_partitions( &spec, p0_main, p1_main );
    spec.partition[ 0 ].process_count = 2U;
    spec.partition[ 0 ].process[ 1 ].entry = idle_yield;
    spec.partition[ 0 ].process[ 1 ].priority = 5U;
    spec.partition[ 1 ].process_count = 2U;
    spec.partition[ 1 ].process[ 1 ].entry = idle_yield;
    spec.partition[ 1 ].process[ 1 ].priority = 5U;

    spec.sampling_count = 2U;
    spec.sampling[ 0 ].message_size = sizeof( message_t );
    spec.sampling[ 0 ].writer_mask = 0x1U;
    spec.sampling[ 0 ].reader_mask = 0x2U;
    spec.sampling[ 1 ].message_size = 4U;
    spec.sampling[ 1 ].writer_mask = 0x2U;
    spec.sampling[ 1 ].reader_mask = 0x1U;

    sil_build( &spec );
}

static size_t reads_of( maxrtos_sampling_port_id_t port )
{
    maxrtos_sampling_port_status_t status;

    SIL_REQUIRE_EQ(
        maxrtos_kernel_sampling_port_get_status(
            port, sil_system()->process_id[ 0 ][ 0 ], &status ),
        MAXRTOS_OK );

    return status.has_message ? 1U : 0U;
}

/* ------------------------------------------------------------------------ */
/* the latest message, across partitions */
/* ------------------------------------------------------------------------ */

#define WRITES ( 300U )

static volatile uint32_t s_writes_done;
static volatile uint32_t s_reads;
static volatile uint32_t s_torn;
static volatile uint32_t s_went_back;
static volatile uint32_t s_last_seen;
static volatile uint32_t s_reread_differs;
static volatile uint32_t s_final_value;
static volatile uint32_t s_done;

static void stream_writer( void * arg )
{
    message_t m;
    uint32_t n;

    ( void ) arg;

    for( n = 1U; n <= WRITES; n++ )
    {
        m.value = n;
        m.complement = ~n;
        ( void ) maxrtos_sampling_port_write( SA, &m, sizeof( m ) );
        sim_cpu_work( 40U );
        maxrtos_yield();
    }

    s_writes_done = 1U;
    idle_yield( NULL );
}

static void stream_reader( void * arg )
{
    message_t m = { 0U, 0U };
    message_t again = { 0U, 0U };

    ( void ) arg;

    for( ;; )
    {
        if( maxrtos_sampling_port_read( SA, &m, sizeof( m ) ) == MAXRTOS_OK )
        {
            s_reads++;

            if( m.complement != ~m.value )
            {
                s_torn++;
            }

            if( m.value < s_last_seen )
            {
                s_went_back++;
            }

            s_last_seen = m.value;

            /* A read leaves the message, so the next read sees it again
             * unless the writer replaced it. */
            if( ( maxrtos_sampling_port_read( SA, &again, sizeof( again ) ) ==
                  MAXRTOS_OK ) &&
                ( again.value < m.value ) )
            {
                s_reread_differs++;
            }
        }

        if( s_writes_done != 0U )
        {
            ( void ) maxrtos_sampling_port_read( SA, &m, sizeof( m ) );
            s_final_value = m.value;
            s_done = 1U;
            idle_yield( NULL );
        }

        sim_cpu_work( 25U );
        maxrtos_yield();
    }
}

static void test_reader_sees_the_latest_message_across_partitions( void )
{
    s_writes_done = 0U;
    s_reads = 0U;
    s_torn = 0U;
    s_went_back = 0U;
    s_last_seen = 0U;
    s_reread_differs = 0U;
    s_final_value = 0U;
    s_done = 0U;

    build_system( stream_writer, stream_reader );
    SIL_REQUIRE( sim_start( 800U ) );
    SIL_REQUIRE_EQ( s_done, 1U );

    SIL_EXPECT( s_reads > 20U );
    SIL_EXPECT_EQ( s_torn, 0U );
    SIL_EXPECT_EQ( s_went_back, 0U );
    SIL_EXPECT_EQ( s_reread_differs, 0U );

    /* The last write is the one that stays. */
    SIL_EXPECT_EQ( s_final_value, WRITES );
    SIL_EXPECT_EQ( reads_of( SA ), 1U );
    SIL_EXPECT( !sim_halted() );
    SIL_EXPECT_EQ( sim_model_violations(), 0U );

    sim_shutdown();
}

/* ------------------------------------------------------------------------ */
/* nothing written yet */
/* ------------------------------------------------------------------------ */

static volatile maxrtos_status_t s_empty_read;
static volatile uint32_t s_empty_buffer;

static void empty_reader( void * arg )
{
    /* On the stack, so inside the partition's domain. */
    uint32_t buffer = 0xFFFFFFFFU;

    ( void ) arg;

    s_empty_read = maxrtos_sampling_port_read( SB, &buffer, sizeof( buffer ) );
    s_empty_buffer = buffer;
    s_done = 1U;

    idle_yield( NULL );
}

static void test_read_before_the_first_write_is_empty( void )
{
    maxrtos_sampling_port_status_t status;

    s_done = 0U;
    s_empty_read = MAXRTOS_OK;

    /* Partition 0 reads SB, which partition 1 never writes. */
    build_system( empty_reader, idle_yield );
    SIL_REQUIRE( sim_start( 30U ) );
    SIL_REQUIRE_EQ( s_done, 1U );

    SIL_EXPECT_EQ( s_empty_read, MAXRTOS_ERR_QUEUE_EMPTY );

    /* Nothing was copied out, and the port still says it is empty. */
    SIL_EXPECT_EQ( s_empty_buffer, 0xFFFFFFFFU );
    SIL_REQUIRE_EQ(
        maxrtos_kernel_sampling_port_get_status(
            SB, sil_system()->process_id[ 0 ][ 0 ], &status ),
        MAXRTOS_OK );
    SIL_EXPECT( !status.has_message );

    sim_shutdown();
}

/* ------------------------------------------------------------------------ */
/* hostile calls */
/* ------------------------------------------------------------------------ */

enum
{
    H_WRITE_KERNEL_MESSAGE,
    H_READ_KERNEL_BUFFER,
    H_BUFFER_STRADDLES_DOMAIN_END,
    H_BUFFER_IN_OTHER_PARTITION,
    H_SIZE_WRAPS_ADDRESS_SPACE,
    H_SIZE_ZERO,
    H_ID_OUT_OF_RANGE,
    H_ID_INVALID,
    H_ID_IS_AN_ADDRESS,
    H_READ_ON_WRITE_ONLY_PORT,
    H_WRITE_WRONG_SIZE_SMALL,
    H_WRITE_WRONG_SIZE_LARGE,
    H_LEGITIMATE_WRITE,
    H_COUNT
};

static volatile uint32_t s_probe[ H_COUNT ];
static volatile uint32_t s_p1_write_on_read_only;
static volatile uint32_t s_p1_read_on_write_only_port;
static volatile uint32_t s_probes_done;
static volatile uint32_t s_p1_done;

/* Partition 0: writes SA, reads SB. */
static void hostile( void * arg )
{
    sil_system_t const * sys = sil_system();
    message_t good = { 0x600DU, ~0x600DU };

    ( void ) arg;

    s_probe[ H_WRITE_KERNEL_MESSAGE ] = ( uint32_t ) sim_sampling_write_raw(
        SA, SIL_KERNEL_GUARD, sizeof( good ) );

    s_probe[ H_READ_KERNEL_BUFFER ] =
        ( uint32_t ) sim_sampling_read_raw( SB, SIL_KERNEL_GUARD, 4U );

    s_probe[ H_BUFFER_STRADDLES_DOMAIN_END ] =
        ( uint32_t ) sim_sampling_read_raw(
            SB, sys->domain_base[ 0 ] + sys->domain_size[ 0 ] - 2U, 4U );

    s_probe[ H_BUFFER_IN_OTHER_PARTITION ] = ( uint32_t ) sim_sampling_read_raw(
        SB, sys->domain_base[ 1 ] + 16U, 4U );

    s_probe[ H_SIZE_WRAPS_ADDRESS_SPACE ] = ( uint32_t ) sim_sampling_write_raw(
        SA, sim_ptr_to_uaddr( &good ), 0xFFFFFFF0U );

    s_probe[ H_SIZE_ZERO ] = ( uint32_t ) sim_sampling_write_raw(
        SA, sim_ptr_to_uaddr( &good ), 0U );

    /* An ID past the pool, the "no port" sentinel, and an address the caller
     * could once have used as a port: none names a port. */
    s_probe[ H_ID_OUT_OF_RANGE ] = ( uint32_t ) sim_sampling_write_raw(
        MAXRTOS_MAX_SAMPLING_PORTS, sim_ptr_to_uaddr( &good ), sizeof( good ) );
    s_probe[ H_ID_INVALID ] = ( uint32_t ) sim_sampling_write_raw(
        MAXRTOS_INVALID_SAMPLING_PORT_ID,
        sim_ptr_to_uaddr( &good ),
        sizeof( good ) );
    s_probe[ H_ID_IS_AN_ADDRESS ] = ( uint32_t ) sim_sampling_write_raw(
        sys->domain_base[ 0 ] + 64U,
        sim_ptr_to_uaddr( &good ),
        sizeof( good ) );

    /* SA: partition 0 only writes. Reading it is refused. */
    s_probe[ H_READ_ON_WRITE_ONLY_PORT ] = ( uint32_t ) sim_sampling_read_raw(
        SA, sim_ptr_to_uaddr( &good ), sizeof( good ) );

    s_probe[ H_WRITE_WRONG_SIZE_SMALL ] =
        ( uint32_t ) maxrtos_sampling_port_write(
            SA, &good, sizeof( good ) - 1U );
    s_probe[ H_WRITE_WRONG_SIZE_LARGE ] =
        ( uint32_t ) maxrtos_sampling_port_write(
            SA, &good, sizeof( good ) + 1U );

    /* Rejection is not blanket: a legitimate write still works. */
    s_probe[ H_LEGITIMATE_WRITE ] =
        ( uint32_t ) maxrtos_sampling_port_write( SA, &good, sizeof( good ) );

    s_probes_done = 1U;
    idle_yield( NULL );
}

/* Partition 1: reads SA, writes SB; it may not write SA or read SB... except
 * that it is SB's writer, so only the wrong directions are probed. */
static void hostile_p1( void * arg )
{
    message_t m = { 1U, ~1U };
    uint32_t word = 0U;

    ( void ) arg;

    s_p1_write_on_read_only =
        ( uint32_t ) maxrtos_sampling_port_write( SA, &m, sizeof( m ) );
    s_p1_read_on_write_only_port =
        ( uint32_t ) maxrtos_sampling_port_read( SB, &word, sizeof( word ) );
    s_p1_done = 1U;

    idle_yield( NULL );
}

static void test_kernel_rejects_hostile_calls( void )
{
    uint32_t * guard;
    uint32_t * victim;
    sil_system_t * sys;
    message_t seen = { 0U, 0U };

    s_probes_done = 0U;
    s_p1_done = 0U;

    build_system( hostile, hostile_p1 );
    sys = sil_system();

    /* Canaries the probes would corrupt if the kernel followed the pointers. */
    guard = sim_mem_ptr( SIL_KERNEL_GUARD, 4U );
    victim = sim_mem_ptr( sys->domain_base[ 1 ] + 16U, 4U );
    SIL_REQUIRE( ( guard != NULL ) && ( victim != NULL ) );
    *guard = CANARY;
    *victim = CANARY;

    SIL_REQUIRE( sim_start( 60U ) );
    SIL_REQUIRE_EQ( s_probes_done, 1U );
    SIL_REQUIRE_EQ( s_p1_done, 1U );

    SIL_EXPECT_EQ( s_probe[ H_WRITE_KERNEL_MESSAGE ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ( s_probe[ H_READ_KERNEL_BUFFER ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ(
        s_probe[ H_BUFFER_STRADDLES_DOMAIN_END ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ(
        s_probe[ H_BUFFER_IN_OTHER_PARTITION ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ(
        s_probe[ H_SIZE_WRAPS_ADDRESS_SPACE ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ( s_probe[ H_SIZE_ZERO ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ( s_probe[ H_ID_OUT_OF_RANGE ], MAXRTOS_ERR_INVALID_ID );
    SIL_EXPECT_EQ( s_probe[ H_ID_INVALID ], MAXRTOS_ERR_INVALID_ID );
    SIL_EXPECT_EQ( s_probe[ H_ID_IS_AN_ADDRESS ], MAXRTOS_ERR_INVALID_ID );
    SIL_EXPECT_EQ(
        s_probe[ H_READ_ON_WRITE_ONLY_PORT ], MAXRTOS_ERR_INVALID_ID );
    SIL_EXPECT_EQ(
        s_probe[ H_WRITE_WRONG_SIZE_SMALL ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ(
        s_probe[ H_WRITE_WRONG_SIZE_LARGE ], MAXRTOS_ERR_INVALID_ARG );
    SIL_EXPECT_EQ( s_probe[ H_LEGITIMATE_WRITE ], MAXRTOS_OK );

    /* Partition 1 reads SA and writes SB: the other directions are refused. */
    SIL_EXPECT_EQ( s_p1_write_on_read_only, MAXRTOS_ERR_INVALID_ID );
    SIL_EXPECT_EQ( s_p1_read_on_write_only_port, MAXRTOS_ERR_INVALID_ID );

    /* Nothing was read or written through a hostile pointer... */
    SIL_EXPECT_EQ( *guard, CANARY );
    SIL_EXPECT_EQ( *victim, CANARY );

    /* ...only the legitimate write reached the port... */
    SIL_REQUIRE_EQ(
        maxrtos_kernel_sampling_port_read(
            SA, &seen, sizeof( seen ), sys->process_id[ 1 ][ 0 ] ),
        MAXRTOS_OK );
    SIL_EXPECT_EQ( seen.value, 0x600DU );
    SIL_EXPECT_EQ( seen.complement, ~0x600DU );

    /* ...and nothing faulted. */
    SIL_EXPECT( !sim_halted() );

    sim_shutdown();
}

static sil_case_t const s_cases[] = {
    SIL_CASE(
        test_read_before_the_first_write_is_empty,
        "A read before the first write returns QUEUE_EMPTY and copies "
        "nothing" ),
    SIL_CASE(
        test_reader_sees_the_latest_message_across_partitions,
        "A reader in another partition sees the latest message, never a torn "
        "or older one, and reading does not consume it" ),
    SIL_CASE(
        test_kernel_rejects_hostile_calls,
        "Hostile pointers, unknown IDs, non-members and the wrong direction "
        "are refused and nothing is disturbed" ),
};

int main(
    int argc,
    char ** argv )
{
    int status = sil_run_suite(
        "ipc_sampling_port",
        s_cases,
        sizeof( s_cases ) / sizeof( s_cases[ 0 ] ),
        argc,
        argv );

    sim_shutdown();

    return status;
}
