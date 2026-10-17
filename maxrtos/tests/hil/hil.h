/**
 * @file hil.h
 * @brief Helpers for HIL tests that need to look at the Cortex-M7 itself.
 *
 * HIL tests use the same emu_test_setup() / emu_test_poll() interface as the
 * QEMU tests (see ../qemu/harness.h), so a QEMU test can be built for the
 * board unchanged. This header adds what only a hardware test needs.
 */

#ifndef MAXRTOS_HIL_H
#define MAXRTOS_HIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "harness.h"
#include "maxrtos/arch/cortex_m7/fault_handlers.h"

/* CFSR bits (SCB->CFSR = MMFSR | BFSR << 8 | UFSR << 16). */
#define HIL_CFSR_IACCVIOL   ( 1UL << 0U )
#define HIL_CFSR_DACCVIOL   ( 1UL << 1U )
#define HIL_CFSR_MMARVALID  ( 1UL << 7U )
#define HIL_CFSR_PRECISERR  ( 1UL << 9U )
#define HIL_CFSR_BFARVALID  ( 1UL << 15U )
#define HIL_CFSR_UNDEFINSTR ( 1UL << 16U )
#define HIL_CFSR_INVSTATE   ( 1UL << 17U )
#define HIL_CFSR_UNALIGNED  ( 1UL << 24U )
#define HIL_CFSR_DIVBYZERO  ( 1UL << 25U )

/* Core clock the harness assumes. The board runs from the reset-default HSI
 * oscillator; the timebase test checks this against wall-clock time. */
#ifndef HIL_CPU_HZ
#define HIL_CPU_HZ ( 64000000UL )
#endif

/* Kernel tick rate. */
#define HIL_TICK_HZ ( 1000UL )

/* Memory map of the NUCLEO-H753ZI as the HIL module configuration sees it
 * (retarget_module.py). */
#define HIL_FLASH_BASE ( 0x08000000UL )
#define HIL_FLASH_SIZE ( 0x00200000UL )
#define HIL_DTCM_BASE  ( 0x20000000UL )
#define HIL_DTCM_SIZE  ( 0x00020000UL )
#define HIL_SRAM_BASE  ( 0x24000000UL )
#define HIL_SRAM_SIZE  ( 0x00080000UL )
#define HIL_AHB4_BASE  ( 0x58000000UL )
#define HIL_AHB4_SIZE  ( 0x00080000UL )

/**
 * Fail with "<what> got=0x%08x want=0x%08x" (truncated to fit the verdict
 * block). Use as `return hil_fail_values( "what", got, want );`.
 */
int hil_fail_values(
    char const * what,
    uint32_t got,
    uint32_t want );

/** Text such as "rasr r9" (prefix, then n in decimal) for hil_fail_values(). */
static inline char const * hil_label(
    char const * prefix,
    uint32_t n )
{
    static char text[ 32 ];
    uint32_t div = 1000U;
    size_t len = 0U;
    bool started = false;

    while( ( *prefix != '\0' ) && ( len < 24U ) )
    {
        text[ len++ ] = *prefix++;
    }

    for( ; div != 0U; div /= 10U )
    {
        uint32_t const digit = ( n / div ) % 10U;

        if( ( digit != 0U ) || started || ( div == 1U ) )
        {
            text[ len++ ] = ( char ) ( '0' + digit );
            started = true;
        }
    }

    text[ len ] = '\0';

    return text;
}

/* Address of a peripheral register that only privileged code may touch. */
#define HIL_RCC_AHB4ENR ( ( volatile uint32_t * ) 0x580244E0UL )

#endif /* MAXRTOS_HIL_H */
