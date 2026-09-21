/**
 * @file harness.c
 * @brief Entry point, SysTick handler and semihosting exit for QEMU tests.
 */

#include <stddef.h>
#include <stdint.h>

#include "harness.h"

#define SYSTICK_CTRL ( *( volatile uint32_t * ) 0xE000E010UL )
#define SYSTICK_LOAD ( *( volatile uint32_t * ) 0xE000E014UL )
#define SYSTICK_VAL  ( *( volatile uint32_t * ) 0xE000E018UL )

/* CPU cycles per kernel tick. */
#define TICK_RELOAD ( 20000UL )

/* ARM semihosting operations. */
#define SEMIHOST_SYS_WRITE0        ( 0x04U )
#define SEMIHOST_SYS_EXIT_EXTENDED ( 0x20U )
#define SEMIHOST_ADP_APPLICATION_EXIT ( 0x20026U )

char const * emu_fail_reason = "unknown";

static uint32_t s_ticks;

static void semihost_write0( char const * text )
{
    register uint32_t op __asm( "r0" ) = SEMIHOST_SYS_WRITE0;
    register char const * arg __asm( "r1" ) = text;

    __asm volatile ( "bkpt 0xAB" : "+r" ( op ) : "r" ( arg ) : "memory" );
}

static void semihost_exit( uint32_t code )
{
    uint32_t block[ 2 ];
    register uint32_t op __asm( "r0" ) = SEMIHOST_SYS_EXIT_EXTENDED;
    register uint32_t * arg __asm( "r1" ) = block;

    block[ 0 ] = SEMIHOST_ADP_APPLICATION_EXIT;
    block[ 1 ] = code;

    __asm volatile ( "bkpt 0xAB" : "+r" ( op ) : "r" ( arg ) : "memory" );

    for( ;; )
    {
    }
}

static void write_number( uint32_t value )
{
    char digits[ 11 ];
    uint32_t i = 10U;

    digits[ i ] = '\0';

    do
    {
        digits[ --i ] = ( char ) ( '0' + ( value % 10U ) );
        value /= 10U;
    } while( value != 0U );

    semihost_write0( &digits[ i ] );
}

static void finish( int verdict )
{
    if( verdict == EMU_PASS )
    {
        semihost_write0( "EMU PASS (ticks: " );
        write_number( s_ticks );
        semihost_write0( ")\n" );
        semihost_exit( 0U );
    }

    semihost_write0( "EMU FAIL: " );
    semihost_write0( emu_fail_reason );
    semihost_write0( "\n" );
    semihost_exit( 1U );
}

void SysTick_Handler( void )
{
    int verdict;

    maxrtos_arch_systick();
    s_ticks++;

    verdict = emu_test_poll( s_ticks );

    if( verdict != EMU_RUNNING )
    {
        finish( verdict );
    }
    else if( s_ticks > EMU_TICK_LIMIT )
    {
        emu_fail_reason = "tick limit reached without a verdict";
        finish( EMU_FAIL );
    }
}

int main( void )
{
    maxrtos_config_init();

    emu_test_setup();

    /* Start the tick only now so the frame begins at tick zero. */
    SYSTICK_LOAD = TICK_RELOAD - 1U;
    SYSTICK_VAL = 0U;
    SYSTICK_CTRL = 7U; /* enable, interrupt, core clock */

    maxrtos_scheduler_start();

    for( ;; )
    {
    }
}
