/**
 * @file test_timebase.c
 * @brief The kernel tick runs at the rate the harness programmed.
 *
 * QEMU's time follows executed instructions, so it cannot say whether
 * SysTick's reload matches the real clock. Here the core cycle counter (DWT)
 * is read on every tick: the tick period must be reload + 1 core cycles, with
 * no tick lost or doubled over the run. runner.py then compares the tick
 * count against wall-clock time on the host, which checks that the core clock
 * is the HIL_CPU_HZ the harness assumed.
 */

#include "hil.h"

#define RUN_TICKS ( 5000UL )

#define NOMINAL_CYCLES ( HIL_CPU_HZ / HIL_TICK_HZ )

#define DEMCR      ( *( volatile uint32_t * ) 0xE000EDFCUL )
#define DWT_CTRL   ( *( volatile uint32_t * ) 0xE0001000UL )
#define DWT_CYCCNT ( *( volatile uint32_t * ) 0xE0001004UL )
#define DWT_LAR    ( *( volatile uint32_t * ) 0xE0001FB0UL )

static uint32_t s_first;
static uint32_t s_previous;

static void beat_control( void * arg )
{
    ( void ) arg;
    for( ;; )
    {
        EMU_RES[ 0 ]++;
        maxrtos_yield();
    }
}
static void beat_application( void * arg )
{
    ( void ) arg;
    for( ;; )
    {
        EMU_RES[ 1 ]++;
        maxrtos_yield();
    }
}

void emu_test_setup( void )
{
    EMU_CREATE( control_main, PARTITION_ID_control, beat_control );
    EMU_CREATE( control_aux, PARTITION_ID_control, beat_control );
    EMU_CREATE( application_main, PARTITION_ID_application, beat_application );
    EMU_CREATE( application_aux, PARTITION_ID_application, beat_application );

    DEMCR |= 1UL << 24U; /* TRCENA */
    DWT_LAR = 0xC5ACCE55UL;
    DWT_CYCCNT = 0U;
    DWT_CTRL |= 1UL; /* CYCCNTENA */
}

int emu_test_poll( uint32_t tick )
{
    uint32_t const now = DWT_CYCCNT;
    int result = EMU_RUNNING;

    if( tick == 1U )
    {
        s_first = now;
    }
    else
    {
        uint32_t const delta = now - s_previous;

        /* A tick can be late by the length of a context switch (SysTick and
         * PendSV share the lowest priority), but not by a tenth of a tick. */
        if( ( delta > ( NOMINAL_CYCLES + ( NOMINAL_CYCLES / 10U ) ) ) ||
            ( delta < ( NOMINAL_CYCLES - ( NOMINAL_CYCLES / 10U ) ) ) )
        {
            result =
                hil_fail_values( "cycles in one tick", delta, NOMINAL_CYCLES );
        }
    }

    s_previous = now;

    if( ( result == EMU_RUNNING ) && ( tick >= RUN_TICKS ) )
    {
        uint32_t const elapsed = now - s_first;
        uint32_t const expected =
            ( uint32_t ) ( ( RUN_TICKS - 1U ) * NOMINAL_CYCLES );
        uint32_t const slack = NOMINAL_CYCLES / 4U;

        if( ( elapsed > ( expected + slack ) ) ||
            ( elapsed < ( expected - slack ) ) )
        {
            result = hil_fail_values( "cycles over run", elapsed, expected );
        }
        else
        {
            result = EMU_PASS;
        }
    }

    return result;
}
