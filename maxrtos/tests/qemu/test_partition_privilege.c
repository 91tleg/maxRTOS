/**
 * @file test_partition_privilege.c
 * @brief Privilege follows the partition, through the real CONTROL.nPRIV
 *        programming in the context switch.
 *
 * The control partition is a system partition (module_system.json), the
 * application partition is not. Both try to write a word of kernel memory
 * (privileged-only DTCM). The system partition's process must succeed; the
 * application partition's process must fault and be restarted, leaving the
 * word untouched. Runs with PRIVDEFENA clear.
 */

#include "harness.h"

enum { C_SYSTEM_ATTEMPTS, C_APPLICATION_ATTEMPTS, C_SYSTEM_PEER, C_APPLICATION_PEER };

#define SYSTEM_VALUE      ( 0x5157U )
#define APPLICATION_VALUE ( 0xBADD00UL )
#define MIN_ATTEMPTS      ( 4U )
#define MIN_PROGRESS      ( 30U )

static void control_main( void * arg )
{
    ( void ) arg;

    EMU_RES[ C_SYSTEM_ATTEMPTS ]++;
    *EMU_KERNEL_ADDRESS = SYSTEM_VALUE; /* privileged: allowed */

    for( ;; )
    {
        maxrtos_yield();
    }
}

static void control_aux( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        EMU_RES[ C_SYSTEM_PEER ]++;
        maxrtos_yield();
    }
}

static void application_main( void * arg )
{
    ( void ) arg;

    EMU_RES[ C_APPLICATION_ATTEMPTS ]++;
    *EMU_KERNEL_ADDRESS = APPLICATION_VALUE; /* unprivileged: MemManage */

    for( ;; )
    {
    }
}

static void application_aux( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        EMU_RES[ C_APPLICATION_PEER ]++;
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

    if( EMU_RES[ C_SYSTEM_ATTEMPTS ] > 1U )
    {
        return EMU_FAILED( "the system partition's process faulted" );
    }

    if( ( EMU_RES[ C_SYSTEM_ATTEMPTS ] == 1U ) &&
        ( EMU_RES[ C_APPLICATION_ATTEMPTS ] >= MIN_ATTEMPTS ) &&
        ( EMU_RES[ C_SYSTEM_PEER ] >= MIN_PROGRESS ) &&
        ( EMU_RES[ C_APPLICATION_PEER ] >= MIN_PROGRESS ) )
    {
        if( *EMU_KERNEL_ADDRESS != SYSTEM_VALUE )
        {
            return EMU_FAILED( "the application partition altered kernel memory" );
        }

        return EMU_PASS;
    }

    return EMU_RUNNING;
}
