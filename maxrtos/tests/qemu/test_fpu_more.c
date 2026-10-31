/**
 * @file test_fpu_more.c
 * @brief More ways the FPU state is switched: in a privileged system
 *        partition, across timed waits, and a process that gives up the FPU.
 *
 * The control partition is a system partition (privileged); the application
 * partition is not.
 *
 * - control_main (privileged) holds a pattern across timed waits that tick
 *   wakes.
 * - control_aux (privileged) starts by clearing CONTROL.FPCA, which only
 *   privileged code can, and spinning: when the tick preempts it the hardware
 *   stacks a basic frame, which the context switch cannot save as the context
 *   every process has. It must be handled as a fault and the process
 *   restarted, not saved wrongly. From its second life it is a worker that
 *   alternates yield and timed wait.
 * - application_main holds a pattern across long loops the slot end
 *   interrupts and yields.
 * - application_aux waits a while, then makes its first FPU instruction: the
 *   registers must hold zero, whatever the others left in them.
 *
 * No process may be restarted except control_aux's first life.
 */

#include "fpu_test.h"
#include "maxrtos/arch/cortex_m7/fault_handlers.h"

#define CONTROL_FPCA ( 1UL << 2U )

enum
{
    R_A_ENTRIES,
    R_A_ITER,
    R_A_ERR,
    R_B_ENTRIES,
    R_B_ITER,
    R_B_ERR,
    R_C_ENTRIES,
    R_C_ITER,
    R_C_ERR,
    R_D_ENTRIES,
    R_D_ITER,
    R_D_ERR,
    R_D_LEAKS,
    R_STATUS_ERR,
    R_POLL_TICK,
    R_SPIN_FIRST,
    R_SPIN_LAST,
    R_FIRST_ERR_WHO,
    R_FIRST_ERR_REG,
    R_FIRST_ERR_GOT,
    R_FIRST_ERR_WANT
};

/* wait_kind: 0 yield, 1 timed wait, 2 spin and yield. */
static void worker(
    uint32_t who,
    uint32_t entries,
    uint32_t iter,
    uint32_t err,
    uint32_t wait_kind )
{
    uint32_t want[ 32 ];
    uint32_t const fpscr = ( who & 3U ) << FPSCR_RMODE_SHIFT;
    uint32_t n;

    EMU_RES[ entries ]++;

    for( n = 0U;; n++ )
    {
        maxrtos_status_t status = MAXRTOS_OK;

        fp_make_pattern( want, who, n );
        fp_load( want );
        fpscr_write( fpscr );

        switch( wait_kind )
        {
            case 0U:
                maxrtos_yield();
                break;
            case 1U:
                status = maxrtos_timed_wait( 1U + ( n % 3U ) );
                break;
            default:
                fp_spin_ticks( 1U + ( n % 2U ) );
                maxrtos_yield();
                break;
        }

        if( status != MAXRTOS_OK )
        {
            fp_note_error(
                R_FIRST_ERR_WHO,
                R_STATUS_ERR,
                who,
                200U + ( uint32_t ) status,
                0U,
                0U );
        }

        fp_check_registers( R_FIRST_ERR_WHO, err, who, want, fpscr );
        EMU_RES[ iter ]++;
    }
}

static void control_main( void * arg )
{
    ( void ) arg;
    worker( 1U, R_A_ENTRIES, R_A_ITER, R_A_ERR, 1U );
}

static void control_aux( void * arg )
{
    uint32_t control;

    ( void ) arg;

    if( EMU_RES[ R_B_ENTRIES ] == 0U )
    {
        EMU_RES[ R_B_ENTRIES ] = 1U;

        /* Use the FPU, then give up the state: FPCA clear. The exception
         * that preempts this loop stacks a basic frame. The loop records the
         * tick it last ran at, so the test can see how soon the process was
         * stopped. Its stack pointer is in the middle of its stack, where a
         * basic frame is no reason for the stack bounds to refuse it. */
        EMU_RES[ R_SPIN_FIRST ] = EMU_RES[ R_POLL_TICK ];
        __asm volatile( "vmov s0, %1\n\t"
                        "mov sp, %5\n\t"
                        "mrs %0, control\n\t"
                        "bic %0, %0, %2\n\t"
                        "msr control, %0\n\t"
                        "isb\n\t"
                        "1: ldr %0, [%3]\n\t"
                        "str %0, [%4]\n\t"
                        "b 1b"
                        : "=&r"( control )
                        : "r"( 1U ),
                          "i"( CONTROL_FPCA ),
                          "r"( &EMU_RES[R_POLL_TICK] ),
                          "r"( &EMU_RES[R_SPIN_LAST] ),
                          "r"( ( uint32_t ) maxrtos_stack_control_aux + 512U )
                        : "s0", "cc", "memory" );
    }

    EMU_RES[ R_B_ENTRIES ]++;
    worker( 2U, R_B_ENTRIES, R_B_ITER, R_B_ERR, 0U );
}

static void application_main( void * arg )
{
    ( void ) arg;
    worker( 3U, R_C_ENTRIES, R_C_ITER, R_C_ERR, 2U );
}

static void application_aux( void * arg )
{
    ( void ) arg;

    ( void ) maxrtos_timed_wait( 9U );

    /* The first FPU instruction of this process, long after the others began
     * to fill the registers. */
    fp_check_zero( R_FIRST_ERR_WHO, R_D_LEAKS, 4U );

    worker( 4U, R_D_ENTRIES, R_D_ITER, R_D_ERR, 1U );
}

void emu_test_setup( void )
{
    EMU_CREATE( control_main, PARTITION_ID_control, control_main );
    EMU_CREATE( control_aux, PARTITION_ID_control, control_aux );
    EMU_CREATE( application_main, PARTITION_ID_application, application_main );
    EMU_CREATE( application_aux, PARTITION_ID_application, application_aux );
}

#define CHECK( cond_, why_ )                                                   \
    do                                                                         \
    {                                                                          \
        if( !( cond_ ) )                                                       \
        {                                                                      \
            return EMU_FAILED( why_ );                                         \
        }                                                                      \
    } while( 0 )

int emu_test_poll( uint32_t tick )
{
    EMU_RES[ R_POLL_TICK ] = tick;

    CHECK(
        ( EMU_RES[ R_A_ERR ] + EMU_RES[ R_B_ERR ] + EMU_RES[ R_C_ERR ] +
          EMU_RES[ R_D_ERR ] + EMU_RES[ R_D_LEAKS ] +
          EMU_RES[ R_STATUS_ERR ] ) == 0U,
        "FPU state corrupted or leaked, or a bad status (see R_FIRST_ERR_*)" );

    /* Only control_aux's first life ends in a restart. The fault is the
     * context switch's own, not an exception the fault handlers record. */
    CHECK(
        ( EMU_RES[ R_A_ENTRIES ] <= 1U ) && ( EMU_RES[ R_C_ENTRIES ] <= 1U ) &&
            ( EMU_RES[ R_D_ENTRIES ] <= 1U ),
        "a process that did nothing wrong was restarted" );
    CHECK(
        g_maxrtos_arch_last_fault.count == 0U,
        "an exception the fault handlers saw" );

    if( ( EMU_RES[ R_A_ITER ] > 30U ) && ( EMU_RES[ R_B_ITER ] > 30U ) &&
        ( EMU_RES[ R_C_ITER ] > 30U ) && ( EMU_RES[ R_D_ITER ] > 15U ) )
    {
        /* control_aux ran its first life, was restarted, and ran again. */
        CHECK(
            EMU_RES[ R_B_ENTRIES ] >= 2U,
            "the process that cleared FPCA was not restarted" );

        /* It was stopped when the tick first preempted it, not after it had
         * been resumed with a frame of the wrong size: the control partition's
         * slot is five ticks. */
        CHECK(
            ( EMU_RES[ R_SPIN_LAST ] - EMU_RES[ R_SPIN_FIRST ] ) <= 5U,
            "the process that cleared FPCA was resumed" );

        return EMU_PASS;
    }

    return EMU_RUNNING;
}
