/**
 * @file harness.c
 * @brief Entry point, SysTick handler and verdict for the HIL tests.
 *
 * Implements the interface in ../qemu/harness.h for the NUCLEO-H753ZI. It
 * differs from the QEMU harness in how the verdict leaves the chip: there is
 * no semihosting, so the verdict is stored in g_hil_result, which runner.py
 * reads through the debug port. The LEDs show it too: LD2 (yellow) while the
 * test runs, then LD1 (green) for a pass or LD3 (red) for a fail.
 */

#include <stddef.h>
#include <stdint.h>

#include "hil.h"
#include "hil_result.h"

#define SYSTICK_CTRL ( *( volatile uint32_t * ) 0xE000E010UL )
#define SYSTICK_LOAD ( *( volatile uint32_t * ) 0xE000E014UL )
#define SYSTICK_VAL  ( *( volatile uint32_t * ) 0xE000E018UL )

#define RCC_AHB4ENR ( *( volatile uint32_t * ) 0x580244E0UL )
#define GPIOB_MODER ( *( volatile uint32_t * ) 0x58020400UL )
#define GPIOB_BSRR  ( *( volatile uint32_t * ) 0x58020418UL )
#define GPIOE_MODER ( *( volatile uint32_t * ) 0x58021000UL )
#define GPIOE_BSRR  ( *( volatile uint32_t * ) 0x58021018UL )

#define LED_GREEN_PIN  ( 0U )  /* LD1, PB0 */
#define LED_YELLOW_PIN ( 1U )  /* LD2, PE1 */
#define LED_RED_PIN    ( 14U ) /* LD3, PB14 */

/* The run is cut off after this many ticks. A test that needs longer sets
 * HIL_TICK_LIMIT when it is built. */
#ifndef HIL_TICK_LIMIT
#define HIL_TICK_LIMIT EMU_TICK_LIMIT
#endif

char const * emu_fail_reason = "unknown";

volatile hil_result_t g_hil_result;

static uint32_t s_ticks;

int hil_fail_values(
    char const * what,
    uint32_t got,
    uint32_t want )
{
    static char text[ HIL_REASON_SIZE ];
    static char const digits[] = "0123456789abcdef";
    size_t n = 0U;
    size_t i;
    char const * const parts[ 2 ] = { " got=0x", " want=0x" };
    uint32_t const values[ 2 ] = { got, want };
    size_t part;

    for( i = 0U; ( what[ i ] != '\0' ) && ( n < 30U ); i++ )
    {
        text[ n++ ] = what[ i ];
    }

    for( part = 0U; part < 2U; part++ )
    {
        int shift;

        for( i = 0U; parts[ part ][ i ] != '\0'; i++ )
        {
            text[ n++ ] = parts[ part ][ i ];
        }

        for( shift = 28; shift >= 0; shift -= 4 )
        {
            text[ n++ ] =
                digits[ ( values[ part ] >> ( uint32_t ) shift ) & 0xFU ];
        }
    }

    text[ n ] = '\0';
    emu_fail_reason = text;

    return EMU_FAIL;
}

static void leds_init( void )
{
    RCC_AHB4ENR |= ( 1UL << 1U ) | ( 1UL << 4U ); /* GPIOB, GPIOE */

    GPIOB_MODER &= ~(
        ( 3UL << ( LED_GREEN_PIN * 2U ) ) | ( 3UL << ( LED_RED_PIN * 2U ) ) );
    GPIOB_MODER |=
        ( 1UL << ( LED_GREEN_PIN * 2U ) ) | ( 1UL << ( LED_RED_PIN * 2U ) );

    GPIOE_MODER &= ~( 3UL << ( LED_YELLOW_PIN * 2U ) );
    GPIOE_MODER |= ( 1UL << ( LED_YELLOW_PIN * 2U ) );

    GPIOE_BSRR = 1UL << LED_YELLOW_PIN;
}

static void finish( int verdict )
{
    size_t i;

    /* The test is over: stop the tick so the numbers stay put. */
    SYSTICK_CTRL = 0U;

    if( verdict == EMU_PASS )
    {
        GPIOE_BSRR = 1UL << ( LED_YELLOW_PIN + 16U );
        GPIOB_BSRR = 1UL << LED_GREEN_PIN;
        g_hil_result.state = HIL_STATE_PASS;
    }
    else
    {
        for( i = 0U; i < ( HIL_REASON_SIZE - 1U ); i++ )
        {
            if( emu_fail_reason[ i ] == '\0' )
            {
                break;
            }

            g_hil_result.reason[ i ] = emu_fail_reason[ i ];
        }

        g_hil_result.reason[ i ] = '\0';

        GPIOE_BSRR = 1UL << ( LED_YELLOW_PIN + 16U );
        GPIOB_BSRR = 1UL << LED_RED_PIN;
        __asm volatile( "dmb" ::: "memory" );
        g_hil_result.state = HIL_STATE_FAIL;
    }
}

void SysTick_Handler( void )
{
    int verdict;

    maxrtos_arch_systick();
    s_ticks++;
    g_hil_result.ticks = s_ticks;

    verdict = emu_test_poll( s_ticks );

    if( verdict != EMU_RUNNING )
    {
        finish( verdict );
    }
    else if( s_ticks > HIL_TICK_LIMIT )
    {
        emu_fail_reason = "tick limit reached without a verdict";
        finish( EMU_FAIL );
    }
}

/* The QEMU tests start from zeroed RAM. A reset on the board keeps SRAM, so
 * the results the previous test left in the port would still be there. The
 * port memory is not part of .bss, so clear the result words by hand. */
static void clear_results( void )
{
    volatile uint32_t * word = EMU_RES;
    size_t const words = ( PORT_SIZE_results - 1024U ) / sizeof( uint32_t );
    size_t i;

    for( i = 0U; i < words; i++ )
    {
        word[ i ] = 0U;
    }
}

int main( void )
{
    /* Before the MPU is on: the peripheral region is privileged-only. */
    leds_init();

    maxrtos_config_init();
    clear_results();

    emu_test_setup();

    g_hil_result.tick_hz = HIL_TICK_HZ;
    g_hil_result.ticks = 0U;
    g_hil_result.state = HIL_STATE_RUNNING;
    g_hil_result.magic = HIL_MAGIC;

    /* Start the tick only now so the frame begins at tick zero. */
    SYSTICK_LOAD = ( HIL_CPU_HZ / HIL_TICK_HZ ) - 1U;
    SYSTICK_VAL = 0U;
    SYSTICK_CTRL = 7U; /* enable, interrupt, core clock */

    maxrtos_scheduler_start();

    for( ;; )
    {
    }
}
