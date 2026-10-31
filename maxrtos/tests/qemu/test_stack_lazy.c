/**
 * @file test_stack_lazy.c
 * @brief A refused context does not leave a lazy FPU save to land on the
 *        stack the process is restarted on.
 *
 * With the FPU in use, the hardware stacks s0-s15 and FPSCR into the exception
 * frame only at the next FPU instruction, at an address it fixed when the
 * exception began. A context switch that refuses the context never executes
 * the FPU instruction that would complete it, so it must cancel the pending
 * save; otherwise the first FPU instruction of whatever runs next writes 17
 * words at the frame address the refused process chose.
 *
 * application_aux fills the words where that save would land with a canary,
 * loads the FPU registers and points its stack pointer so that the frame is
 * in the results block, which every partition can access (the save is made
 * with whatever MPU region the next process runs under, so a frame in the
 * stack of the refused process could not be reached after a switch to
 * another partition) and outside its own stack. When the switch refuses it
 * and the processes after it use the FPU, the canary must be as it was left.
 */

#include "fpu_test.h"

enum
{
    R_TICK,
    R_LIFE,
    R_ENTRIES,
    R_A,
    R_B,
    R_C
};

#define CANARY_WORDS ( 17U )

/* The exception frame the hardware stacks lies at FRAME_AT; s0 is 32 bytes
 * into it. The stack pointer at the exception is above it by the frame's
 * size. */
#define FRAME_WORD   ( 256U )
#define FRAME_SP     ( ( uint32_t ) &EMU_RES[ FRAME_WORD ] + 104U )
#define CANARY( i_ ) ( 0xC0FFEE00UL + ( i_ ) )

static void control_main( void * arg )
{
    ( void ) arg;
    for( ;; )
    {
        EMU_RES[ R_A ]++;
        maxrtos_yield();
    }
}

static void control_aux( void * arg )
{
    ( void ) arg;
    for( ;; )
    {
        EMU_RES[ R_B ]++;
        maxrtos_yield();
    }
}

static void application_main( void * arg )
{
    ( void ) arg;
    for( ;; )
    {
        EMU_RES[ R_C ]++;
        maxrtos_yield();
    }
}

static void application_aux( void * arg )
{
    volatile uint32_t * const canary = &EMU_RES[ FRAME_WORD + 8U ];
    uint32_t pattern[ 32 ];
    uint32_t i;

    ( void ) arg;

    EMU_RES[ R_ENTRIES ]++;

    if( EMU_RES[ R_LIFE ] < 1U )
    {
        EMU_RES[ R_LIFE ]++;

        for( i = 0U; i < CANARY_WORDS; i++ )
        {
            canary[ i ] = CANARY( i );
        }

        fp_make_pattern( pattern, 1U, 0U );
        fp_load( pattern );

        /* s0-s15 and FPSCR would be saved over the canary. Never returns;
         * the slot end preempts it. */
        __asm volatile( "mov sp, %[sp]\n\t"
                        "1: b 1b"
                        :
                        : [sp] "r"( FRAME_SP ) );
    }

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
    EMU_RES[ R_TICK ] = tick;

    if( ( EMU_RES[ R_ENTRIES ] >= 2U ) && ( EMU_RES[ R_A ] > 10U ) &&
        ( EMU_RES[ R_C ] > 10U ) )
    {
        volatile uint32_t const * const canary = &EMU_RES[ FRAME_WORD + 8U ];
        uint32_t i;

        for( i = 0U; i < CANARY_WORDS; i++ )
        {
            if( canary[ i ] != CANARY( i ) )
            {
                return EMU_FAILED(
                    "a lazy FPU save landed at the frame of a refused "
                    "context" );
            }
        }

        return EMU_PASS;
    }

    return EMU_RUNNING;
}
