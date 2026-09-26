/**
 * @file test_mutex_release_on_restart.c
 * @brief A process restarted while holding a mutex releases it.
 *
 * control_aux takes the mutex and then faults. Health monitoring
 * restarts it; without the release, control_main would wait for the
 * mutex forever and the restarted control_aux would find it still
 * "owned" by itself. Both must keep making progress.
 */

#include "harness.h"
#include "maxrtos/mutex.h"
#include "maxrtos/kernel/mutex.h"

enum { C_AUX_ENTRIES, C_MAIN_SECTIONS, C_BAD_STATUS, C_APPLICATION_MAIN, C_APPLICATION_AUX };

#define MUTEX_ID     ( 0U )
#define MIN_RESTARTS ( 5U )
#define MIN_SECTIONS ( 20U )

static void control_main( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        if( maxrtos_mutex_lock( MUTEX_ID, MAXRTOS_TIMEOUT_INFINITE ) == MAXRTOS_OK )
        {
            EMU_RES[ C_MAIN_SECTIONS ]++;
            ( void ) maxrtos_mutex_unlock( MUTEX_ID );
        }

        maxrtos_yield();
    }
}

static void control_aux( void * arg )
{
    ( void ) arg;

    EMU_RES[ C_AUX_ENTRIES ]++;

    /* After a restart the mutex must be free again, so this succeeds every
     * time (a stale ownership would fail with INVALID_STATE or time out). */
    if( maxrtos_mutex_lock( MUTEX_ID, 50U ) != MAXRTOS_OK )
    {
        EMU_RES[ C_BAD_STATUS ]++;
    }

    *EMU_KERNEL_ADDRESS = 0xDEADU; /* MemManage fault while holding it */

    for( ;; )
    {
    }
}

static void application_main( void * arg ) { ( void ) arg; for( ;; ) { EMU_RES[ C_APPLICATION_MAIN ]++; maxrtos_yield(); } }
static void application_aux( void * arg ) { ( void ) arg; for( ;; ) { EMU_RES[ C_APPLICATION_AUX ]++; maxrtos_yield(); } }

void emu_test_setup( void )
{
    maxrtos_mutex_id_t id;

    if( ( maxrtos_mutex_create( PARTITION_ID_control, &id ) != MAXRTOS_OK ) ||
        ( id != MUTEX_ID ) )
    {
        emu_fail_reason = "mutex create failed";
        for( ;; ) { }
    }

    EMU_CREATE( control_main, PARTITION_ID_control, control_main );
    EMU_CREATE( control_aux, PARTITION_ID_control, control_aux );
    EMU_CREATE( application_main, PARTITION_ID_application, application_main );
    EMU_CREATE( application_aux, PARTITION_ID_application, application_aux );
}

int emu_test_poll( uint32_t tick )
{
    ( void ) tick;

    if( EMU_RES[ C_BAD_STATUS ] != 0U )
    {
        return EMU_FAILED( "restarted process could not take the mutex" );
    }

    if( ( EMU_RES[ C_AUX_ENTRIES ] >= MIN_RESTARTS ) &&
        ( EMU_RES[ C_MAIN_SECTIONS ] >= MIN_SECTIONS ) &&
        ( EMU_RES[ C_APPLICATION_MAIN ] >= MIN_SECTIONS ) )
    {
        return EMU_PASS;
    }

    return EMU_RUNNING;
}
