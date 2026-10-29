/**
 * @file test_fatal_hook.c
 * @brief An application can take over when the kernel cannot continue.
 *
 * control_aux executes BKPT. With no debugger that is a HardFault whose
 * CFSR names no MemManage, BusFault or UsageFault cause, so it cannot be
 * attributed to a process. HardFault_Handler must end in maxrtos_arch_fatal(),
 * and the strong definition below, which replaces the kernel's weak halt, must
 * be the one that runs.
 */

#include "harness.h"
#include "maxrtos/arch/cortex_m7/fault_handlers.h"

enum
{
    C_FATAL_CALLS,
    C_BKPT_REACHED
};

static void control_main( void * arg )
{
    ( void ) arg;
    for( ;; )
    {
        maxrtos_yield();
    }
}

static void application_main( void * arg )
{
    ( void ) arg;
    for( ;; )
    {
        maxrtos_yield();
    }
}

static void control_aux( void * arg )
{
    ( void ) arg;

    EMU_RES[ C_BKPT_REACHED ]++;
    __asm volatile( "bkpt #0" );

    for( ;; )
    {
    }
}

/* The application's own response; a real one would log and reset. It runs
 * in HardFault, which the SysTick poll cannot preempt, so it ends the test. */
void maxrtos_arch_fatal( void )
{
    EMU_RES[ C_FATAL_CALLS ]++;

    if( EMU_RES[ C_BKPT_REACHED ] != 1U )
    {
        emu_fail_reason = "fatal hook ran without the BKPT";
        emu_finish( EMU_FAIL );
    }

    if( g_maxrtos_arch_last_fault.cfsr != 0U )
    {
        emu_fail_reason =
            "the fault names a MemManage/BusFault/UsageFault cause";
        emu_finish( EMU_FAIL );
    }

    emu_finish( EMU_PASS );

    for( ;; )
    {
    }
}

void emu_test_setup( void )
{
    EMU_CREATE( control_main, PARTITION_ID_control, control_main );
    EMU_CREATE( control_aux, PARTITION_ID_control, control_aux );
    EMU_CREATE( application_main, PARTITION_ID_application, application_main );
}

int emu_test_poll( uint32_t tick )
{
    ( void ) tick;

    /* The verdict comes from the hook itself; reaching the tick limit means
     * it never ran. */
    return EMU_RUNNING;
}
