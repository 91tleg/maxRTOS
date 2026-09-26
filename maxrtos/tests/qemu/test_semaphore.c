/**
 * @file test_semaphore.c
 * @brief Counting semaphore: producer/consumer, maximum, partition binding.
 *
 * control_main signals a semaphore (maximum 4) and control_aux waits on it.
 * Every unit taken must have been given (consumed never exceeds produced),
 * the count must never pass its maximum (a signal at the maximum must fail
 * with OVERFLOW and be observed), and a process of the other partition must be refused.
 */

#include "harness.h"
#include "maxrtos/semaphore.h"
#include "maxrtos/kernel/semaphore.h"

enum
{
    C_PRODUCED,
    C_CONSUMED,
    C_OVERFLOWS,
    C_BAD_STATUS,
    C_FOREIGN_STATUS,
    C_FOREIGN_DONE,
    C_APPLICATION_MAIN,
    C_APPLICATION_AUX,
    C_STATUS_CHECKS,
    C_STATUS_BAD,
    C_STATUS_FOREIGN,
    C_STATUS_BADPTR
};

#define SEM_ID       ( 0U )
#define SEM_MAX      ( 4U )
#define MIN_UNITS    ( 30U )
#define NOT_YET      ( 0xFFFFFFFFU )

static void control_main( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        uint32_t i;

        /* A burst larger than the maximum, so the top is actually reached
         * before the consumer gets to run. */
        for( i = 0U; i < ( SEM_MAX + 2U ); i++ )
        {
            maxrtos_status_t status;

            status = maxrtos_semaphore_signal( SEM_ID );

            if( status == MAXRTOS_OK )
            {
                EMU_RES[ C_PRODUCED ]++;
            }
            else if( status == MAXRTOS_ERR_OVERFLOW )
            {
                EMU_RES[ C_OVERFLOWS ]++;
            }
            else
            {
                EMU_RES[ C_BAD_STATUS ]++;
            }
        }

        maxrtos_yield();
    }
}

/* The status snapshot must always be self-consistent. */
static void check_status( void )
{
    maxrtos_semaphore_status_t st;

    if( maxrtos_semaphore_get_status( SEM_ID, &st ) != MAXRTOS_OK )
    {
        EMU_RES[ C_STATUS_BAD ]++;
    }
    else
    {
        if( ( st.maximum_value != SEM_MAX ) ||
            ( st.current_value > st.maximum_value ) ||
            ( st.waiting > 1U ) )
        {
            EMU_RES[ C_STATUS_BAD ]++;
        }

        EMU_RES[ C_STATUS_CHECKS ]++;
    }

    /* A destination outside the caller's partition must be refused, not
     * written by the kernel. */
    if( maxrtos_semaphore_get_status(
            SEM_ID, ( maxrtos_semaphore_status_t * ) ( void * ) EMU_KERNEL_ADDRESS ) !=
        MAXRTOS_ERR_INVALID_ARG )
    {
        EMU_RES[ C_STATUS_BADPTR ]++;
    }
}

static void control_aux( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        maxrtos_status_t status;

        check_status();

        /* A bounded wait: expiring must be harmless. */
        status = maxrtos_semaphore_wait( SEM_ID, 50U );

        if( status == MAXRTOS_OK )
        {
            EMU_RES[ C_CONSUMED ]++;
        }
        else if( status != MAXRTOS_ERR_TIMEOUT )
        {
            EMU_RES[ C_BAD_STATUS ]++;
        }
    }
}

static void application_main( void * arg )
{
    ( void ) arg;

    /* The semaphore belongs to the control partition. */
    EMU_RES[ C_FOREIGN_STATUS ] =
        ( uint32_t ) maxrtos_semaphore_wait( SEM_ID, 0U );
    EMU_RES[ C_FOREIGN_DONE ] = 1U;

    {
        maxrtos_semaphore_status_t st;

        EMU_RES[ C_STATUS_FOREIGN ] =
            ( uint32_t ) maxrtos_semaphore_get_status( SEM_ID, &st );
    }

    for( ;; )
    {
        EMU_RES[ C_APPLICATION_MAIN ]++;
        maxrtos_yield();
    }
}

static void application_aux( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        EMU_RES[ C_APPLICATION_AUX ]++;
        maxrtos_yield();
    }
}

void emu_test_setup( void )
{
    maxrtos_semaphore_id_t id;

    if( ( maxrtos_semaphore_create(
              PARTITION_ID_control, 0U, SEM_MAX, &id ) != MAXRTOS_OK ) ||
        ( id != SEM_ID ) )
    {
        emu_fail_reason = "semaphore create failed";
        for( ;; ) { }
    }

    EMU_RES[ C_FOREIGN_STATUS ] = NOT_YET;

    EMU_CREATE( control_main, PARTITION_ID_control, control_main );
    EMU_CREATE( control_aux, PARTITION_ID_control, control_aux );
    EMU_CREATE( application_main, PARTITION_ID_application, application_main );
    EMU_CREATE( application_aux, PARTITION_ID_application, application_aux );
}

int emu_test_poll( uint32_t tick )
{
    uint32_t produced;
    uint32_t consumed;

    ( void ) tick;

    /* Read consumed first: it can only trail produced. */
    consumed = EMU_RES[ C_CONSUMED ];
    produced = EMU_RES[ C_PRODUCED ];

    if( EMU_RES[ C_BAD_STATUS ] != 0U )
    {
        return EMU_FAILED( "unexpected wait/signal status" );
    }

    if( consumed > produced )
    {
        return EMU_FAILED( "a unit was taken that was never given" );
    }

    /* SEM_MAX units can sit in the count, plus the one the consumer has
     * been handed but not yet counted. */
    if( ( produced - consumed ) > ( SEM_MAX + 1U ) )
    {
        return EMU_FAILED( "the count passed its maximum" );
    }

    if( ( EMU_RES[ C_FOREIGN_DONE ] != 0U ) &&
        ( EMU_RES[ C_FOREIGN_STATUS ] != ( uint32_t ) MAXRTOS_ERR_INVALID_ID ) )
    {
        return EMU_FAILED( "other partition was not refused the semaphore" );
    }

    if( ( EMU_RES[ C_STATUS_BAD ] != 0U ) || ( EMU_RES[ C_STATUS_BADPTR ] != 0U ) )
    {
        return EMU_FAILED( "status call inconsistent or wrote outside the partition" );
    }

    if( ( EMU_RES[ C_FOREIGN_DONE ] != 0U ) &&
        ( EMU_RES[ C_STATUS_FOREIGN ] != ( uint32_t ) MAXRTOS_ERR_INVALID_ID ) )
    {
        return EMU_FAILED( "other partition could read the semaphore status" );
    }

    if( ( consumed >= MIN_UNITS ) &&
        ( EMU_RES[ C_STATUS_CHECKS ] >= MIN_UNITS ) &&
        ( EMU_RES[ C_OVERFLOWS ] > 0U ) &&
        ( EMU_RES[ C_FOREIGN_DONE ] != 0U ) &&
        ( EMU_RES[ C_APPLICATION_MAIN ] >= MIN_UNITS ) )
    {
        return EMU_PASS;
    }

    return EMU_RUNNING;
}
