/**
 * @file test_context_integrity.c
 * @brief Registers survive preemption by the real exception hardware.
 *
 * Four processes, two per partition, each run an assembly routine that fills
 * r1-r12 with a pattern, spins and checks the pattern. In each partition the
 * low-priority process spins for a couple of ticks and the high-priority one
 * spins briefly and then waits one tick. The wait ends on a tick, so the
 * high-priority process preempts the low-priority one in the middle of its
 * spin, every tick, and the partition slot ending cuts both. The registers go
 * through the Cortex-M7's real exception entry and return, tail-chaining, and
 * the kernel's own save and restore, together with the MPU being reprogrammed
 * for the next partition. The high-priority processes also leave and enter
 * the CPU by SVC (the wait), which takes a different route into the
 * scheduler.
 *
 * The routine returns the number of registers that changed. Any non-zero
 * count fails the test.
 */

#include "hil.h"
#include "maxrtos/timing.h"

extern uint32_t hil_register_soak(
    uint32_t seed,
    uint32_t iterations );

enum
{
    R_RUNS,
    R_BAD = 4
};

#define MIN_RUNS ( 12U )

/* The low-priority processes spin about 1.5 to 3 ticks at 64 MHz, so every
 * spin is preempted; the high-priority ones spin a tenth of that. */
#define SPIN_BASE         ( 15000U )
#define SPIN_SPREAD       ( 16384U )
#define BRIEF_SPIN_BASE   ( 1500U )
#define BRIEF_SPIN_SPREAD ( 1638U )

static void soak(
    uint32_t slot,
    bool waits )
{
    uint32_t run = 0U;

    for( ;; )
    {
        /* The seed differs per process and per run, so a register left over
         * from another process cannot match by chance. */
        uint32_t const seed = ( ( slot + 1U ) << 28U ) | ( run << 8U ) | 0x5AU;

        /* A varying length moves the preemption point across the routine. */
        uint32_t const spins =
            waits ? BRIEF_SPIN_BASE +
                        ( ( run * 2654435761UL ) % BRIEF_SPIN_SPREAD )
                  : SPIN_BASE + ( ( run * 2654435761UL ) % SPIN_SPREAD );

        EMU_RES[ R_BAD + slot ] += hil_register_soak( seed, spins );
        EMU_RES[ R_RUNS + slot ] = ++run;

        if( waits )
        {
            ( void ) maxrtos_timed_wait( 1U );
        }
    }
}

/* The module gives the *_main processes priority 2 and the *_aux ones 5. */
static void control_main( void * arg )
{
    ( void ) arg;
    soak( 0U, true );
}
static void control_aux( void * arg )
{
    ( void ) arg;
    soak( 1U, false );
}
static void application_main( void * arg )
{
    ( void ) arg;
    soak( 2U, true );
}
static void application_aux( void * arg )
{
    ( void ) arg;
    soak( 3U, false );
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
