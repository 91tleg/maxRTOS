/**
 * @file test_fault_status.c
 * @brief The fault status registers the Cortex-M7 leaves, and the fault class
 *        the kernel maps them to.
 *
 * An unprivileged process provokes one fault per stage: an undefined
 * instruction, an integer divide by zero, an unaligned LDRD, a branch to ARM
 * state, and a read from an address with no memory behind it (the bus fault).
 * For each, the hardware must leave exactly the CFSR bits the ARMv7-M manual
 * lists, the fault must not escalate to HardFault, and the health monitor
 * must restart the process with the rest of the system running.
 *
 * The bus fault needs an MPU region that lets the access through to the bus;
 * the CMake target adds region 4 over 0x00100000 for it. That address lies
 * past the 64 KiB of ITCM and is reserved on the STM32H753.
 */

#include "hil_stages.h"

#define BUS_FAULT_ADDRESS ( 0x00100000UL )

typedef enum
{
    ACT_UNDEFINED,
    ACT_DIVIDE_BY_ZERO,
    ACT_UNALIGNED_LDRD,
    ACT_ARM_STATE,
    ACT_BUS_READ
} action_t;

static action_t const k_actions[] = {
    ACT_UNDEFINED,
    ACT_DIVIDE_BY_ZERO,
    ACT_UNALIGNED_LDRD,
    ACT_ARM_STATE,
    ACT_BUS_READ,
};

#define STAGES ( sizeof( k_actions ) / sizeof( k_actions[ 0 ] ) )

static hs_expect_t const k_expect[ STAGES ] = {
    { HIL_CFSR_UNDEFINSTR, 0U, 0U },
    { HIL_CFSR_DIVBYZERO, 0U, 0U },
    { HIL_CFSR_UNALIGNED, 0U, 0U },
    { HIL_CFSR_INVSTATE, 0U, 0U },
    { HIL_CFSR_PRECISERR | HIL_CFSR_BFARVALID, 0U, BUS_FAULT_ADDRESS },
};

static void arm_state_target( void )
{
}

static void inject( action_t action )
{
    switch( action )
    {
        case ACT_UNDEFINED:
            __asm volatile( "udf #0" );
            break;

        case ACT_DIVIDE_BY_ZERO:
        {
            register uint32_t numerator __asm( "r0" ) = 1U;
            register uint32_t divisor __asm( "r1" ) = 0U;

            __asm volatile( "udiv r0, r0, r1"
                            : "+r"( numerator )
                            : "r"( divisor ) );
            break;
        }

        case ACT_UNALIGNED_LDRD:
        {
            /* LDRD faults on an address that is not word aligned whatever
             * CCR.UNALIGN_TRP says. Aim it at the process's own stack. */
            register uint32_t address __asm( "r2" ) = ( uint32_t ) &action | 1U;
            register uint32_t low __asm( "r0" );
            register uint32_t high __asm( "r1" );

            __asm volatile( "ldrd %0, %1, [%2]"
                            : "=r"( low ), "=r"( high )
                            : "r"( address ) );
            break;
        }

        case ACT_ARM_STATE:
            /* Bit 0 clear selects ARM state, which the Cortex-M7 lacks. */
            ( ( void ( * )( void ) )(
                ( uint32_t ) arm_state_target & ~1UL ) )();
            break;

        case ACT_BUS_READ:
            EMU_RES[ HS_FIRST_FREE ] =
                *( volatile uint32_t * ) BUS_FAULT_ADDRESS;
            break;

        default:
            break;
    }
}

static void control_aux( void * arg )
{
    ( void ) arg;

    EMU_RES[ HS_ENTRIES_CONTROL ]++;

    for( ;; )
    {
        uint32_t const stage = EMU_RES[ HS_STAGE ];

        if( ( stage < STAGES ) && hs_claim( stage ) )
        {
            inject( k_actions[ stage ] );
            hs_escaped( stage );
        }

        maxrtos_yield();
    }
}

static void control_main( void * arg )
{
    ( void ) arg;
    for( ;; )
    {
        EMU_RES[ HS_BEAT_CONTROL ]++;
        maxrtos_yield();
    }
}
static void application_main( void * arg )
{
    ( void ) arg;
    for( ;; )
    {
        EMU_RES[ HS_BEAT_APPLICATION ]++;
        maxrtos_yield();
    }
}
static void application_aux( void * arg )
{
    ( void ) arg;
    for( ;; )
    {
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
    int result;

    result = hs_poll( k_expect, STAGES, NULL );

    if( result == HS_DONE )
    {
        result = hs_check_containment( tick, STAGES, 0U );
    }
    else if( result == HS_RUNNING )
    {
        result = EMU_RUNNING;
    }
    else
    {
        /* EMU_FAIL */
    }

    return result;
}
