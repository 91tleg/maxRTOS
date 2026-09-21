/**
 * @file test_hm_restart_process.c
 * @brief RESTART_PROCESS (the default policy) contains a memory fault.
 *
 * control_aux writes to kernel memory, which its partition may not
 * touch. The MPU faults, the health monitor restarts that process, and it
 * faults again. Meanwhile every other process, including the faulting
 * process's own partition peer and the other partition, must keep
 * running.
 */

#include "harness.h"

enum { C_AUX_ENTRIES, C_CONTROL_MAIN, C_APPLICATION_MAIN, C_APPLICATION_AUX };

#define MIN_RESTARTS ( 5U )
#define MIN_PROGRESS ( 50U )

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
    ( void ) tick;

    if( ( EMU_RES[ C_AUX_ENTRIES ] >= MIN_RESTARTS ) &&
        ( EMU_RES[ C_CONTROL_MAIN ] >= MIN_PROGRESS ) &&
        ( EMU_RES[ C_APPLICATION_MAIN ] >= MIN_PROGRESS ) &&
        ( EMU_RES[ C_APPLICATION_AUX ] >= MIN_PROGRESS ) )
    {
        return EMU_PASS;
    }

    return EMU_RUNNING;
}
