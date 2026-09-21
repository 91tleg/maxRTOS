/**
 * @file test_deadline_supervision.c
 * @brief Periodic release and deadline supervision through the real SVC,
 *        PendSV and idle-context path.
 *
 * control_main is a well-behaved periodic process (period 8, deadline 5): it
 * must be released every period, never miss, and the CPU must idle in its
 * slot while it waits. control_aux overruns its deadline (3) and is restarted
 * by the health monitor over and over. The application partition must run
 * undisturbed throughout.
 */

#include "harness.h"

#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/timing.h"
#include "maxrtos/timing.h"

enum { C_RELEASES, C_WAIT_FAILURES, C_OVERRUN_ENTRIES, C_APPLICATION_MAIN, C_APPLICATION_AUX };

#define PERIOD          ( 8U )
#define MIN_RELEASES    ( 20U )
#define MIN_ENTRIES     ( 6U )
#define MIN_PROGRESS    ( 50U )

static maxrtos_process_id_t s_punctual_id;
static maxrtos_process_id_t s_overrunner_id;

static void control_main( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        EMU_RES[ C_RELEASES ]++;

        if( maxrtos_periodic_wait() != MAXRTOS_OK )
        {
            EMU_RES[ C_WAIT_FAILURES ]++;
        }
    }
}

static void control_aux( void * arg )
{
    ( void ) arg;

    EMU_RES[ C_OVERRUN_ENTRIES ]++;

    for( ;; )
    {
        /* Never completes its release. */
    }
}

static void application_main( void * arg ) { ( void ) arg; for( ;; ) { EMU_RES[ C_APPLICATION_MAIN ]++; maxrtos_yield(); } }
static void application_aux( void * arg ) { ( void ) arg; for( ;; ) { EMU_RES[ C_APPLICATION_AUX ]++; maxrtos_yield(); } }

void emu_test_setup( void )
{
    maxrtos_process_id_t id;

    EMU_CREATE( control_main, PARTITION_ID_control, control_main );
    s_punctual_id = 0U;
    EMU_CREATE( control_aux, PARTITION_ID_control, control_aux );
    s_overrunner_id = 1U;
    EMU_CREATE( application_main, PARTITION_ID_application, application_main );
    EMU_CREATE( application_aux, PARTITION_ID_application, application_aux );

    id = s_punctual_id;
    if( maxrtos_process_set_timing( id, PERIOD, 5U ) != MAXRTOS_OK )
    {
        emu_fail_reason = "set_timing failed";
        for( ;; ) { }
    }

    id = s_overrunner_id;
    if( maxrtos_process_set_timing( id, PERIOD, 3U ) != MAXRTOS_OK )
    {
        emu_fail_reason = "set_timing failed";
        for( ;; ) { }
    }
}

int emu_test_poll( uint32_t tick )
{
    ( void ) tick;

    if( EMU_RES[ C_WAIT_FAILURES ] != 0U )
    {
        return EMU_FAILED( "maxrtos_periodic_wait() returned an error" );
    }

    if( maxrtos_process_get( s_punctual_id )->deadline_misses != 0U )
    {
        return EMU_FAILED( "the punctual periodic process missed a deadline" );
    }

    if( ( EMU_RES[ C_RELEASES ] >= MIN_RELEASES ) &&
        ( EMU_RES[ C_OVERRUN_ENTRIES ] >= MIN_ENTRIES ) &&
        ( EMU_RES[ C_APPLICATION_MAIN ] >= MIN_PROGRESS ) &&
        ( EMU_RES[ C_APPLICATION_AUX ] >= MIN_PROGRESS ) )
    {
        if( maxrtos_process_get( s_overrunner_id )->deadline_misses < MIN_ENTRIES )
        {
            return EMU_FAILED( "the overrunning process's misses were not counted" );
        }

        return EMU_PASS;
    }

    return EMU_RUNNING;
}
