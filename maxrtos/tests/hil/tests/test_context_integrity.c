/**
 * @file test_context_integrity.c
 * @brief Registers survive preemption by the real exception hardware.
 *
 * Four processes, two per partition, each run an assembly routine that fills
 * r1-r12 with a pattern, spins for a couple of ticks and checks the pattern.
 * Every spin is cut by the tick (an equal-priority peer takes over) and by
 * the partition slot ending, so the registers go through the Cortex-M7's real
 * exception entry and return, tail-chaining, and the kernel's own save and
 * restore, together with the MPU being reprogrammed for the next partition.
 * Two of the processes also yield by SVC between spins, which takes a
 * different route into the scheduler.
 *
 * The routine returns the number of registers that changed. Any non-zero
 * count fails the test.
 */

#include "hil.h"

extern uint32_t hil_register_soak(
    uint32_t seed,
    uint32_t iterations );

enum
{
    R_RUNS,
    R_BAD = 4
};

#define MIN_RUNS ( 12U )

/* About 1.5 to 3 ticks per spin at 64 MHz, so every spin is preempted. */
#define SPIN_BASE   ( 15000U )
#define SPIN_SPREAD ( 16384U )

static void soak(
    uint32_t slot,
    bool yields )
{
    uint32_t run = 0U;

    for( ;; )
    {
        /* The seed differs per process and per run, so a register left over
         * from another process cannot match by chance. */
        uint32_t const seed = ( ( slot + 1U ) << 28U ) | ( run << 8U ) | 0x5AU;

        /* A varying length moves the preemption point across the routine. */
        uint32_t const spins =
            SPIN_BASE + ( ( run * 2654435761UL ) % SPIN_SPREAD );

        EMU_RES[ R_BAD + slot ] += hil_register_soak( seed, spins );
        EMU_RES[ R_RUNS + slot ] = ++run;

        if( yields )
        {
            maxrtos_yield();
        }
    }
}

static void control_main( void * arg )
{
    ( void ) arg;
    soak( 0U, false );
}
static void control_aux( void * arg )
{
    ( void ) arg;
    soak( 1U, true );
}
static void application_main( void * arg )
{
    ( void ) arg;
    soak( 2U, false );
}
static void application_aux( void * arg )
{
    ( void ) arg;
    soak( 3U, true );
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
    uint32_t slot;
    bool done = true;

    ( void ) tick;

    for( slot = 0U; slot < 4U; slot++ )
    {
        if( EMU_RES[ R_BAD + slot ] != 0U )
        {
            return hil_fail_values(
                hil_label( "registers changed, proc ", slot ),
                EMU_RES[ R_BAD + slot ],
                0U );
        }

        if( EMU_RES[ R_RUNS + slot ] < MIN_RUNS )
        {
            done = false;
        }
    }

    return done ? EMU_PASS : EMU_RUNNING;
}
