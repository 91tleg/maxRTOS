/**
 * @file fpu_test.h
 * @brief Helpers for the tests that use the FPU.
 *
 * A test using these must be built with -mfpu=fpv5-d16 -mfloat-abi=softfp
 * (see the CMakeLists.txt in this directory).
 *
 * The FPU instructions are only in the always-inline helpers below, so nothing
 * the compiler generates between them touches the FPU registers. They must be
 * inline: s16-s31 are callee-saved, so a helper that is a function of its own
 * would restore the caller's values on return and undo the very registers a
 * test loads.
 *
 * Mismatches are counted in EMU_RES[ err_slot ] and the first one is recorded
 * in four words from EMU_RES[ first_slot ] (who, register, got, want), where
 * a debugger can read it.
 */

#ifndef MAXRTOS_EMU_FPU_TEST_H
#define MAXRTOS_EMU_FPU_TEST_H

#include <stdint.h>

#include "harness.h"
#include "maxrtos/timing.h"

#define FP_INLINE static inline __attribute__( ( always_inline ) )

#define FPSCR_RMODE_SHIFT ( 22U )

#define FP_ALL_S_REGISTERS                                                     \
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11",  \
        "s12", "s13", "s14", "s15", "s16", "s17", "s18", "s19", "s20", "s21",  \
        "s22", "s23", "s24", "s25", "s26", "s27", "s28", "s29", "s30", "s31"

FP_INLINE void fp_load( uint32_t const * words )
{
    __asm volatile( "vldmia %0, {s0-s31}"
                    :
                    : "r"( words )
                    : "memory", FP_ALL_S_REGISTERS );
}

FP_INLINE void fp_store( uint32_t * words )
{
    __asm volatile( "vstmia %0, {s0-s31}" : : "r"( words ) : "memory" );
}

FP_INLINE void fpscr_write( uint32_t value )
{
    __asm volatile( "vmsr fpscr, %0" : : "r"( value ) : "cc" );
}

FP_INLINE uint32_t fpscr_read( void )
{
    uint32_t value;

    __asm volatile( "vmrs %0, fpscr" : "=r"( value ) );

    return value;
}

/* A pattern no other process or iteration shares. */
static inline void fp_make_pattern(
    uint32_t * words,
    uint32_t who,
    uint32_t n )
{
    uint32_t i;

    for( i = 0U; i < 32U; i++ )
    {
        words[ i ] = 0xA5000000UL ^ ( who << 20U ) ^ ( n << 8U ) ^
                     ( i * 0x01010101UL ) ^ ( who * 0x9E3779B9UL );
    }
}

static inline void fp_note_error(
    uint32_t first_slot,
    uint32_t err_slot,
    uint32_t who,
    uint32_t reg,
    uint32_t got,
    uint32_t want )
{
    if( EMU_RES[ first_slot ] == 0U )
    {
        EMU_RES[ first_slot + 1U ] = reg;
        EMU_RES[ first_slot + 2U ] = got;
        EMU_RES[ first_slot + 3U ] = want;
        EMU_RES[ first_slot ] = who;
    }

    EMU_RES[ err_slot ]++;
}

/* Compare the registers and FPSCR with what was loaded. */
static inline void fp_check_registers(
    uint32_t first_slot,
    uint32_t err_slot,
    uint32_t who,
    uint32_t const * want,
    uint32_t want_fpscr )
{
    uint32_t got[ 32 ];
    uint32_t got_fpscr;
    uint32_t i;

    fp_store( got );
    got_fpscr = fpscr_read();

    for( i = 0U; i < 32U; i++ )
    {
        if( got[ i ] != want[ i ] )
        {
            fp_note_error( first_slot, err_slot, who, i, got[ i ], want[ i ] );
        }
    }

    if( got_fpscr != want_fpscr )
    {
        fp_note_error( first_slot, err_slot, who, 100U, got_fpscr, want_fpscr );
    }
}

/* All of s0-s31 and FPSCR are zero. Used as the first FPU instruction of a
 * process: whatever else ran before, it must start from zeros. */
static inline void fp_check_zero(
    uint32_t first_slot,
    uint32_t err_slot,
    uint32_t who )
{
    static uint32_t const zeros[ 32 ] = { 0U };

    fp_check_registers( first_slot, err_slot, who, zeros, 0U );
}

/* Run for at least `ticks` ticks of the kernel without yielding, so the end of
 * the slot interrupts it, reading the time (an SVC) as it goes. */
static inline void fp_spin_ticks( uint32_t ticks )
{
    maxrtos_tick_t const start = maxrtos_get_time();

    while( ( maxrtos_get_time() - start ) < ticks )
    {
    }
}

#endif /* MAXRTOS_EMU_FPU_TEST_H */
