/**
 * @file test_hardfault_escalation.c
 * @brief An escalated memory fault is recovered through HardFault.
 *
 * MemManage is disabled in SHCSR, so control_aux's MPU violation
 * escalates to HardFault with HFSR.FORCED set. HardFault_Handler must
 * classify the original cause from CFSR and take the same restart
 * path: the process is restarted repeatedly while every other process
 * keeps running.
 */

#include "harness.h"
#include "maxrtos/arch/cortex_m7/fault_handlers.h"

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

#define SCB_SHCSR ( *( volatile uint32_t * ) 0xE000ED24UL )
#define SHCSR_MEMFAULTENA ( 1UL << 16U )

void emu_test_setup( void )
{
    SCB_SHCSR &= ~SHCSR_MEMFAULTENA;

    EMU_CREATE( control_main, PARTITION_ID_control, control_main );
    EMU_CREATE( control_aux, PARTITION_ID_control, control_aux );
    EMU_CREATE( application_main, PARTITION_ID_application, application_main );
    EMU_CREATE( application_aux, PARTITION_ID_application, application_aux );
}

int emu_test_poll( uint32_t tick )
{
    ( void ) tick;

    if( ( g_maxrtos_arch_last_fault.hfsr & ( 1UL << 30U ) ) == 0U )
    {
        if( EMU_RES[ C_AUX_ENTRIES ] > 0U )
        {
            emu_fail_reason = "fault did not escalate to HardFault";
            return EMU_FAIL;
        }
    }

    if( ( EMU_RES[ C_AUX_ENTRIES ] >= MIN_RESTARTS ) &&
        ( EMU_RES[ C_CONTROL_MAIN ] >= MIN_PROGRESS ) &&
        ( EMU_RES[ C_APPLICATION_MAIN ] >= MIN_PROGRESS ) &&
        ( EMU_RES[ C_APPLICATION_AUX ] >= MIN_PROGRESS ) )
    {
        return EMU_PASS;
    }

    return EMU_RUNNING;
}
