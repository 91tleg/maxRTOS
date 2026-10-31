/**
 * @file test_stack_slot.c
 * @brief A process whose context is refused at a slot boundary does not take
 *        the next partition's time.
 *
 * The PendSV that ends a slot was requested to start the next partition. When
 * it finds the outgoing process's stack pointer outside its stack, the fault
 * recovery names a process of the faulting partition to run; that must not
 * replace the switch to the next partition, or the faulting partition runs in
 * the first tick of someone else's slot, and, by faulting again in it, in all
 * of it.
 *
 * application_main moves its stack pointer out of its stack in each of its
 * first ATTACKS lives, each in a slot of its own (application_aux yields to
 * it). control_main runs every tick and marks the tick it ran in. The
 * schedule is CONTROL_SLOT ticks of the control partition, then APP_SLOT of
 * the application's (module_fpu.json): the control process must mark every
 * tick of its slot, from the first, and the application's none of them.
 */

#include "fpu_test.h"

enum
{
    R_TICK,
    R_LIFE,
    R_APP_RUNS,
    R_MAP = 64
};

#define ATTACKS       ( 3U )
#define CONTROL_SLOT  ( 5U )
#define APP_SLOT      ( 3U )
#define FRAME         ( CONTROL_SLOT + APP_SLOT )
#define TICKS_WATCHED ( 80U )

#define MARK_CONTROL ( 1U )
#define MARK_APP     ( 2U )

static void mark( uint32_t bit )
{
    uint32_t const t = EMU_RES[ R_TICK ];

    if( t < TICKS_WATCHED )
    {
        EMU_RES[ R_MAP + t ] |= bit;
    }
}

static void control_main( void * arg )
{
    ( void ) arg;
    for( ;; )
    {
        mark( MARK_CONTROL );
    }
}

static void control_aux( void * arg )
{
    ( void ) arg;
    for( ;; )
    {
        maxrtos_yield();
    }
}

static void application_main( void * arg )
{
    uint32_t const life = EMU_RES[ R_LIFE ];
    uint32_t const base = ( uint32_t ) maxrtos_stack_application_main;

    ( void ) arg;

    if( life < ATTACKS )
    {
        EMU_RES[ R_LIFE ] = life + 1U;
        mark( MARK_APP );

        /* The extended frame would start at the base of the stack, and the
         * context be written below it. Never returns; the slot end preempts
         * it. */
        __asm volatile( "vmov s0, %[sp]\n\t"
                        "mov sp, %[sp]\n\t"
                        "1: b 1b"
                        :
                        : [sp] "r"( base + 104U )
                        : "s0" );
    }

    for( ;; )
    {
        mark( MARK_APP );
        maxrtos_yield();
    }
}

static void application_aux( void * arg )
{
    ( void ) arg;
    for( ;; )
    {
        mark( MARK_APP );
        EMU_RES[ R_APP_RUNS ]++;
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
    EMU_RES[ R_TICK ] = tick;

    if( tick < ( TICKS_WATCHED - 2U ) )
    {
        return EMU_RUNNING;
    }

    {
        uint32_t first = 0U;
        uint32_t t;

        while( ( first < TICKS_WATCHED ) &&
               ( ( EMU_RES[ R_MAP + first ] & MARK_CONTROL ) == 0U ) )
        {
            first++;
        }

        if( ( first >= FRAME ) || ( EMU_RES[ R_LIFE ] < ATTACKS ) )
        {
            return EMU_FAILED(
                "the control partition or the attacks never ran" );
        }

        /* The last two ticks are still being written. */
        for( t = first; t < ( TICKS_WATCHED - 2U ); t++ )
        {
            uint32_t const m = EMU_RES[ R_MAP + t ];

            if( ( ( t - first ) % FRAME ) < CONTROL_SLOT )
            {
                if( m != MARK_CONTROL )
                {
                    return EMU_FAILED(
                        "the control partition did not own a tick of its slot "
                        "(see R_MAP)" );
                }
            }
            else if( ( m & MARK_CONTROL ) != 0U )
            {
                return EMU_FAILED(
                    "the control process ran in the application's slot" );
            }
        }
    }

    if( EMU_RES[ R_APP_RUNS ] < 3U )
    {
        return EMU_FAILED( "the application partition stopped running" );
    }

    return EMU_PASS;
}
