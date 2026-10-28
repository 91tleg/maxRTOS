/**
 * @file test_timed_wait.c
 * @brief maxrtos_timed_wait() blocks the caller for the delay and no longer,
 *        gives the CPU to the rest of the partition meanwhile, and
 *        maxrtos_get_time() reads a counter that only moves forward.
 *
 * control_main waits a short and a long delay in turn and measures each wait
 * with maxrtos_get_time(). It must never come back early, and never later than
 * the delay plus one major frame (its partition may be off when the wait ends).
 * The short delay usually ends inside the partition's slot, so it is the one
 * that notices a wake-up a tick early; the long one spans several frames.
 * control_aux counts: if the waiter held the CPU it would starve. In the
 * application partition one process checks that a delay of 0 returns at once
 * and an infinite delay is rejected, the other that the time never goes back.
 */

#include "harness.h"
#include "maxrtos/timing.h"

enum
{
    C_WAITS,
    C_BAD_RESULT,
    C_TOO_SHORT,
    C_TOO_LONG,
    C_PEER,
    C_ZERO_WAITS,
    C_ZERO_BAD,
    C_INFINITE_REJECTED,
    C_TIME_BACKWARDS,
    C_TIME_ADVANCES
};

#define SHORT_WAIT     ( 2U )
#define LONG_WAIT      ( 20U )
#define MAJOR_FRAME    ( 8U )
#define MIN_WAITS      ( 6U )
#define MIN_PEER       ( 100U )
#define MIN_ZERO_WAITS ( 20U )

static void control_main( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        maxrtos_tick_t before;
        maxrtos_tick_t elapsed;
        maxrtos_tick_t delay;

        delay = ( ( EMU_RES[ C_WAITS ] % 2U ) == 0U ) ? SHORT_WAIT : LONG_WAIT;
        before = maxrtos_get_time();

        if( maxrtos_timed_wait( delay ) != MAXRTOS_OK )
        {
            EMU_RES[ C_BAD_RESULT ]++;
        }

        elapsed = maxrtos_get_time() - before;

        if( elapsed < delay )
        {
            EMU_RES[ C_TOO_SHORT ]++;
        }

        if( elapsed > ( delay + MAJOR_FRAME ) )
        {
            EMU_RES[ C_TOO_LONG ]++;
        }

        EMU_RES[ C_WAITS ]++;
    }
}

static void control_aux( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        EMU_RES[ C_PEER ]++;
        maxrtos_yield();
    }
}

static void application_main( void * arg )
{
    ( void ) arg;

    if( maxrtos_timed_wait( MAXRTOS_TIMEOUT_INFINITE ) ==
        MAXRTOS_ERR_INVALID_ARG )
    {
        EMU_RES[ C_INFINITE_REJECTED ] = 1U;
    }

    for( ;; )
    {
        if( maxrtos_timed_wait( 0U ) != MAXRTOS_OK )
        {
            EMU_RES[ C_ZERO_BAD ]++;
        }

        EMU_RES[ C_ZERO_WAITS ]++;
        maxrtos_yield();
    }
}

static void application_aux( void * arg )
{
    maxrtos_tick_t previous;

    ( void ) arg;
    previous = maxrtos_get_time();

    for( ;; )
    {
        maxrtos_tick_t now;

        now = maxrtos_get_time();

        if( now < previous )
        {
            EMU_RES[ C_TIME_BACKWARDS ]++;
        }
        else if( now > previous )
        {
            EMU_RES[ C_TIME_ADVANCES ]++;
        }

        previous = now;
        maxrtos_yield();
    }
}

void emu_test_setup( void )
{
    EMU_CREATE( control_main, PARTITION_ID_control, control_main );
    EMU_CREATE( control_aux, PARTITION_ID_control, control_aux );
    EMU_CREATE( application_main, PARTITION_ID_application, application_main );
    EMU_CREATE( application_aux, PARTITION_ID_application, application_aux );
}

int emu_test_poll( uint32_t tick )
{
    ( void ) tick;

    if( EMU_RES[ C_BAD_RESULT ] != 0U )
    {
        return EMU_FAILED( "maxrtos_timed_wait() did not return MAXRTOS_OK" );
    }

    if( EMU_RES[ C_TOO_SHORT ] != 0U )
    {
        return EMU_FAILED( "a timed wait returned before its delay" );
    }

    if( EMU_RES[ C_TOO_LONG ] != 0U )
    {
        return EMU_FAILED(
            "a timed wait lasted longer than its delay and a major frame" );
    }

    if( EMU_RES[ C_ZERO_BAD ] != 0U )
    {
        return EMU_FAILED(
            "maxrtos_timed_wait( 0 ) did not return MAXRTOS_OK" );
    }

    if( EMU_RES[ C_TIME_BACKWARDS ] != 0U )
    {
        return EMU_FAILED( "maxrtos_get_time() went backwards" );
    }

    if( ( EMU_RES[ C_WAITS ] >= MIN_WAITS ) &&
        ( EMU_RES[ C_PEER ] >= MIN_PEER ) &&
        ( EMU_RES[ C_ZERO_WAITS ] >= MIN_ZERO_WAITS ) &&
        ( EMU_RES[ C_INFINITE_REJECTED ] == 1U ) &&
        ( EMU_RES[ C_TIME_ADVANCES ] >= 1U ) )
    {
        return EMU_PASS;
    }

    return EMU_RUNNING;
}
