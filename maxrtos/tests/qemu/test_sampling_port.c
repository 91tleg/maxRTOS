/**
 * @file test_sampling_port.c
 * @brief Sampling ports through the real SVC path.
 *
 * Covers: a read before the first write; a write in one partition seen by a
 * reader in another, whole and latest; a read that leaves the message; a
 * write that replaces it; and refusal of hostile pointers (kernel memory as
 * message or buffer), of IDs that name no port, and of partitions and
 * directions the port does not allow.
 *
 * SA: control writes, application reads (8 bytes). SB: application writes,
 * control reads (4 bytes).
 */

#include "harness.h"
#include "maxrtos/sampling_port.h"

#define SA ( SAMPLING_PORT_ID_sa )
#define SB ( SAMPLING_PORT_ID_sb )

typedef struct
{
    uint32_t value;
    uint32_t complement;
} message_t;

enum
{
    R_EMPTY_READ,
    R_EMPTY_UNTOUCHED,
    R_WRITE_1,
    R_WRITE_2,
    R_READ_1_STATUS,
    R_READ_1_VALUE,
    R_READ_1_AGAIN,
    R_READ_2_VALUE,
    R_BAD_MESSAGE,
    R_BAD_BUFFER,
    R_BAD_ID,
    R_ID_IS_AN_ADDRESS,
    R_READ_ON_WRITE_ONLY,
    R_WRONG_SIZE,
    R_APP_WRITE_ON_READ_ONLY,
    R_APP_WRITE_SB,
    R_CONTROL_READ_SB,
    R_APP_DONE,
    R_CONTROL_DONE
};

static void control_main( void * arg )
{
    message_t m;
    uint32_t word;

    ( void ) arg;

    /* Nothing has been written to SB yet. */
    word = 0xFFFFFFFFU;
    EMU_RES[ R_EMPTY_READ ] =
        ( uint32_t ) maxrtos_sampling_port_read( SB, &word, sizeof( word ) );
    EMU_RES[ R_EMPTY_UNTOUCHED ] = ( word == 0xFFFFFFFFU ) ? 1U : 0U;

    /* Two writes; the second replaces the first. */
    m.value = 0x1111U;
    m.complement = ~m.value;
    EMU_RES[ R_WRITE_1 ] =
        ( uint32_t ) maxrtos_sampling_port_write( SA, &m, sizeof( m ) );

    while( EMU_RES[ R_READ_1_AGAIN ] == 0U )
    {
        maxrtos_yield();
    }

    m.value = 0x2222U;
    m.complement = ~m.value;
    EMU_RES[ R_WRITE_2 ] =
        ( uint32_t ) maxrtos_sampling_port_write( SA, &m, sizeof( m ) );

    /* Hostile calls. */
    EMU_RES[ R_BAD_MESSAGE ] = ( uint32_t ) maxrtos_sampling_port_write(
        SA, ( void const * ) EMU_KERNEL_ADDRESS, sizeof( m ) );
    EMU_RES[ R_BAD_BUFFER ] = ( uint32_t ) maxrtos_sampling_port_read(
        SB, ( void * ) EMU_KERNEL_ADDRESS, sizeof( word ) );
    EMU_RES[ R_BAD_ID ] = ( uint32_t ) maxrtos_sampling_port_write(
        ( maxrtos_sampling_port_id_t ) MAXRTOS_MAX_SAMPLING_PORTS,
        &m,
        sizeof( m ) );
    EMU_RES[ R_ID_IS_AN_ADDRESS ] = ( uint32_t ) maxrtos_sampling_port_write(
        ( maxrtos_sampling_port_id_t ) ( uint32_t ) EMU_KERNEL_ADDRESS,
        &m,
        sizeof( m ) );
    EMU_RES[ R_READ_ON_WRITE_ONLY ] =
        ( uint32_t ) maxrtos_sampling_port_read( SA, &m, sizeof( m ) );
    EMU_RES[ R_WRONG_SIZE ] =
        ( uint32_t ) maxrtos_sampling_port_write( SA, &m, sizeof( m ) - 1U );

    /* Wait for the application's SB write, then read it. */
    while( EMU_RES[ R_APP_WRITE_SB ] == 0xFFFFFFFFU )
    {
        maxrtos_yield();
    }

    word = 0U;
    EMU_RES[ R_CONTROL_READ_SB ] =
        ( uint32_t ) maxrtos_sampling_port_read( SB, &word, sizeof( word ) );
    EMU_RES[ R_CONTROL_DONE ] = word;

    for( ;; )
    {
        maxrtos_yield();
    }
}

static void application_main( void * arg )
{
    message_t m;
    message_t again;
    uint32_t word;

    ( void ) arg;

    /* Wait for the first write, read it twice (a read leaves it). */
    do
    {
        m.value = 0U;
        EMU_RES[ R_READ_1_STATUS ] =
            ( uint32_t ) maxrtos_sampling_port_read( SA, &m, sizeof( m ) );
        maxrtos_yield();
    } while( EMU_RES[ R_READ_1_STATUS ] != ( uint32_t ) MAXRTOS_OK );

    EMU_RES[ R_READ_1_VALUE ] = m.value;
    again.value = 0U;
    ( void ) maxrtos_sampling_port_read( SA, &again, sizeof( again ) );
    EMU_RES[ R_READ_1_AGAIN ] = again.value;

    /* Wait for the second write: the reader then sees only that one. */
    do
    {
        ( void ) maxrtos_sampling_port_read( SA, &m, sizeof( m ) );
        maxrtos_yield();
    } while( m.value != 0x2222U );

    EMU_RES[ R_READ_2_VALUE ] = ( m.complement == ~m.value ) ? m.value : 0U;

    /* The application may not write SA, and writes SB. */
    EMU_RES[ R_APP_WRITE_ON_READ_ONLY ] =
        ( uint32_t ) maxrtos_sampling_port_write( SA, &m, sizeof( m ) );

    word = 0xCAFE0001UL;
    EMU_RES[ R_APP_WRITE_SB ] =
        ( uint32_t ) maxrtos_sampling_port_write( SB, &word, sizeof( word ) );
    EMU_RES[ R_APP_DONE ] = 1U;

    for( ;; )
    {
        maxrtos_yield();
    }
}

/* A peer in each partition so a yield in the others is never the only
 * runnable process. */
static void idle_process( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        maxrtos_yield();
    }
}

void emu_test_setup( void )
{
    EMU_RES[ R_APP_WRITE_SB ] = 0xFFFFFFFFU;

    EMU_CREATE( control_main, PARTITION_ID_control, control_main );
    EMU_CREATE( control_aux, PARTITION_ID_control, idle_process );
    EMU_CREATE( application_main, PARTITION_ID_application, application_main );
    EMU_CREATE( application_aux, PARTITION_ID_application, idle_process );
}

#define CHECK( cond_, why_ )                                                   \
    do                                                                         \
    {                                                                          \
        if( !( cond_ ) )                                                       \
        {                                                                      \
            return EMU_FAILED( why_ );                                         \
        }                                                                      \
    } while( 0 )

int emu_test_poll( uint32_t tick )
{
    ( void ) tick;

    if( ( EMU_RES[ R_APP_DONE ] == 0U ) || ( EMU_RES[ R_CONTROL_DONE ] == 0U ) )
    {
        return EMU_RUNNING;
    }

    CHECK(
        EMU_RES[ R_EMPTY_READ ] == ( uint32_t ) MAXRTOS_ERR_QUEUE_EMPTY,
        "a read before the first write was not QUEUE_EMPTY" );
    CHECK(
        EMU_RES[ R_EMPTY_UNTOUCHED ] == 1U,
        "a read of an empty port wrote the buffer" );

    CHECK(
        EMU_RES[ R_WRITE_1 ] == ( uint32_t ) MAXRTOS_OK &&
            EMU_RES[ R_WRITE_2 ] == ( uint32_t ) MAXRTOS_OK,
        "a write did not return OK" );
    CHECK(
        EMU_RES[ R_READ_1_VALUE ] == 0x1111U,
        "the reader did not see the first message" );
    CHECK(
        EMU_RES[ R_READ_1_AGAIN ] == 0x1111U, "a read consumed the message" );
    CHECK(
        EMU_RES[ R_READ_2_VALUE ] == 0x2222U,
        "the second write did not replace the first, whole" );

    CHECK(
        EMU_RES[ R_BAD_MESSAGE ] == ( uint32_t ) MAXRTOS_ERR_INVALID_ARG,
        "kernel memory accepted as a message" );
    CHECK(
        EMU_RES[ R_BAD_BUFFER ] == ( uint32_t ) MAXRTOS_ERR_INVALID_ARG,
        "kernel memory accepted as a read buffer" );
    CHECK(
        EMU_RES[ R_BAD_ID ] == ( uint32_t ) MAXRTOS_ERR_INVALID_ID,
        "an ID past the pool accepted" );
    CHECK(
        EMU_RES[ R_ID_IS_AN_ADDRESS ] == ( uint32_t ) MAXRTOS_ERR_INVALID_ID,
        "an address accepted as a port" );
    CHECK(
        EMU_RES[ R_READ_ON_WRITE_ONLY ] == ( uint32_t ) MAXRTOS_ERR_INVALID_ID,
        "a write-only partition read the port" );
    CHECK(
        EMU_RES[ R_WRONG_SIZE ] == ( uint32_t ) MAXRTOS_ERR_INVALID_ARG,
        "a message of the wrong size accepted" );
    CHECK(
        EMU_RES[ R_APP_WRITE_ON_READ_ONLY ] ==
            ( uint32_t ) MAXRTOS_ERR_INVALID_ID,
        "a read-only partition wrote the port" );

    CHECK(
        EMU_RES[ R_APP_WRITE_SB ] == ( uint32_t ) MAXRTOS_OK,
        "the application's write to SB did not return OK" );
    CHECK(
        EMU_RES[ R_CONTROL_READ_SB ] == ( uint32_t ) MAXRTOS_OK &&
            EMU_RES[ R_CONTROL_DONE ] == 0xCAFE0001UL,
        "control did not read the application's SB message" );

    return EMU_PASS;
}
