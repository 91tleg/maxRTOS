/**
 * @file test_hm_halt_partition.c
 * @brief HALT_PARTITION stops one partition without stopping the system.
 *
 * Same fault as test_hm_restart_process, but the control partition is
 * configured to halt on a memory fault. After the fault, no process of
 * that partition may run again (not even its healthy peer), while the
 * other partition keeps running and the system keeps taking ticks.
 */

#include "harness.h"

enum { C_AUX_ENTRIES, C_CONTROL_MAIN, C_APPLICATION_MAIN, C_APPLICATION_AUX };

/* Ticks after the fault before the halted partition is sampled, so that
 * anything still in flight has settled. */
#define SETTLE_TICKS  ( 20U )
#define OBSERVE_TICKS ( 60U )
#define MIN_PROGRESS  ( 20U )

static void control_main( void * arg ) { ( void ) arg; for( ;; ) { EMU_RES[ C_CONTROL_MAIN ]++; maxrtos_yield(); } }
static void application_main( void * arg ) { ( void ) arg; for( ;; ) { EMU_RES[ C_APPLICATION_MAIN ]++; maxrtos_yield(); } }
static void application_aux( void * arg ) { ( void ) arg; for( ;; ) { EMU_RES[ C_APPLICATION_AUX ]++; maxrtos_yield(); } }

static void control_aux( void * arg )
{
    ( void ) arg;

    EMU_RES[ C_AUX_ENTRIES ]++;
    *EMU_KERNEL_ADDRESS = 0xDEADU; /* MemManage fault */

    for( ;; )
    {
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
    static uint32_t s_fault_tick;
    static uint32_t s_sampled_control_main;
    static uint32_t s_sampled_application;

    if( s_fault_tick == 0U )
    {
        if( EMU_RES[ C_AUX_ENTRIES ] > 0U )
        {
            s_fault_tick = tick;
        }

        return EMU_RUNNING;
    }

    if( EMU_RES[ C_AUX_ENTRIES ] > 1U )
    {
        return EMU_FAILED( "the faulting process was restarted despite halt_partition" );
    }

    if( tick == ( s_fault_tick + SETTLE_TICKS ) )
    {
        s_sampled_control_main = EMU_RES[ C_CONTROL_MAIN ];
        s_sampled_application = EMU_RES[ C_APPLICATION_MAIN ] +
                                EMU_RES[ C_APPLICATION_AUX ];
    }

    if( tick == ( s_fault_tick + SETTLE_TICKS + OBSERVE_TICKS ) )
    {
        if( EMU_RES[ C_CONTROL_MAIN ] != s_sampled_control_main )
        {
            return EMU_FAILED( "a process of the halted partition kept running" );
        }

        if( ( EMU_RES[ C_APPLICATION_MAIN ] + EMU_RES[ C_APPLICATION_AUX ] ) <
            ( s_sampled_application + MIN_PROGRESS ) )
        {
            return EMU_FAILED( "the other partition stopped running" );
        }

        return EMU_PASS;
    }

    return EMU_RUNNING;
}
