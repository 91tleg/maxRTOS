/**
 * @file test_mutex.c
 * @brief Mutex mutual exclusion, blocking hand-off and partition binding.
 *
 * control_main and control_aux contend for one mutex. Each holds it
 * across a yield, which forces the other to run and try to lock: the
 * contender must block (not enter the critical section) until the
 * holder unlocks. A shared flag records any overlap. A process of the
 * other partition must be refused the mutex outright.
 */

#include "harness.h"
#include "maxrtos/mutex.h"
#include "maxrtos/kernel/mutex.h"

enum
{
    C_MAIN_SECTIONS,
    C_AUX_SECTIONS,
    C_IN_CRITICAL,
    C_VIOLATIONS,
    C_BAD_STATUS,
    C_FOREIGN_STATUS,
    C_FOREIGN_DONE,
    C_AUX_TRYLOCK_FAILS,
    C_APPLICATION_MAIN,
    C_APPLICATION_AUX
};

#define MUTEX_ID     ( 0U )
#define MIN_SECTIONS ( 20U )
#define NOT_YET      ( 0xFFFFFFFFU )

static void section( uint32_t counter, maxrtos_tick_t timeout )
{
    maxrtos_status_t status;

    status = maxrtos_mutex_lock( MUTEX_ID, timeout );

    if( status == MAXRTOS_ERR_TIMEOUT )
    {
        EMU_RES[ C_AUX_TRYLOCK_FAILS ]++;
        maxrtos_yield();
        return;
    }

    if( status != MAXRTOS_OK )
    {
        EMU_RES[ C_BAD_STATUS ]++;
        return;
    }

    if( EMU_RES[ C_IN_CRITICAL ] != 0U )
    {
        EMU_RES[ C_VIOLATIONS ]++;
    }

    EMU_RES[ C_IN_CRITICAL ] = 1U;

    /* Force the peer to run while the lock is held. */
    maxrtos_yield();

    EMU_RES[ C_IN_CRITICAL ] = 0U;
    EMU_RES[ counter ]++;

    if( maxrtos_mutex_unlock( MUTEX_ID ) != MAXRTOS_OK )
    {
        EMU_RES[ C_BAD_STATUS ]++;
    }
}

static void control_main( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        section( C_MAIN_SECTIONS, MAXRTOS_TIMEOUT_INFINITE );
    }
}

static void control_aux( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        /* A bounded wait: it may time out, which must be harmless. */
        section( C_AUX_SECTIONS, 50U );
    }
}

static void application_main( void * arg )
{
    ( void ) arg;

    /* The mutex belongs to the control partition. */
    EMU_RES[ C_FOREIGN_STATUS ] = ( uint32_t ) maxrtos_mutex_lock( MUTEX_ID, 0U );
    EMU_RES[ C_FOREIGN_DONE ] = 1U;

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
    maxrtos_mutex_id_t id;

    if( ( maxrtos_mutex_create( PARTITION_ID_control, &id ) != MAXRTOS_OK ) ||
        ( id != MUTEX_ID ) )
    {
        emu_fail_reason = "mutex create failed";
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
    ( void ) tick;

    if( EMU_RES[ C_VIOLATIONS ] != 0U )
    {
        return EMU_FAILED( "two processes were in the critical section" );
    }

    if( EMU_RES[ C_BAD_STATUS ] != 0U )
    {
        return EMU_FAILED( "unexpected lock/unlock status" );
    }

    if( ( EMU_RES[ C_FOREIGN_DONE ] != 0U ) &&
        ( EMU_RES[ C_FOREIGN_STATUS ] != ( uint32_t ) MAXRTOS_ERR_INVALID_ID ) )
    {
        return EMU_FAILED( "other partition was not refused the mutex" );
    }

    if( ( EMU_RES[ C_MAIN_SECTIONS ] >= MIN_SECTIONS ) &&
        ( EMU_RES[ C_AUX_SECTIONS ] >= MIN_SECTIONS ) &&
        ( EMU_RES[ C_FOREIGN_DONE ] != 0U ) &&
        ( EMU_RES[ C_APPLICATION_MAIN ] >= MIN_SECTIONS ) )
    {
        return EMU_PASS;
    }

    return EMU_RUNNING;
}
