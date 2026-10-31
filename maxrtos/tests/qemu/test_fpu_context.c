/**
 * @file test_fpu_context.c
 * @brief The FPU state of a process survives every kind of context switch,
 *        and never reaches another process.
 *
 * Needs this file built with -mfpu=fpv5-d16 -mfloat-abi=softfp (the CMake
 * targets do).
 *
 * Four processes:
 *
 * - control_main and application_main load all of s0-s31 and FPSCR with a
 *   pattern of their own, then keep running through yields, SVCs and long
 *   stretches that the end of the partition's slot interrupts at an arbitrary
 *   instruction, and check every register and FPSCR afterwards. The
 *   application one also does double-precision arithmetic the compiler
 *   allocates registers for and checks the exact result.
 * - control_aux loads a pattern and blocks in a queue receive. It is woken
 *   by control_main from another context switch, which also writes the
 *   result of the call into its saved frame; the registers, FPSCR, status and
 *   message must all be right.
 * - application_aux checks, as the first FPU instruction of each of its
 *   lives, that the registers hold zero and FPSCR is zero, whatever the
 *   others left. It then dirties them and faults, in lives that alternate
 *   between a deep stack (which also yields while holding the state) and a
 *   shallow one. The health monitor restarts it, which discards a context
 *   that still has FPU state and a pending lazy stacking; the next life must
 *   find zero again and run from a clean entry.
 */

#include "fpu_test.h"
#include "maxrtos/queue_port.h"
#include "maxrtos/arch/cortex_m7/fault_handlers.h"

#define QA ( PORT_ID_qa )

#define MIN_WORKER_ITERATIONS ( 40U )
#define MIN_RECEIVES          ( 30U )
#define MIN_DOUBLE_RUNS       ( 8U )
#define OBSERVER_LIVES        ( 20U )

#define DOUBLE_TERMS ( 2048U )
/* sum of i * 0.5 for i < 2048 */
#define DOUBLE_EXPECTED ( 1048064.0 )

enum
{
    R_A_ITER,
    R_A_ERR,
    R_A_SENT,
    R_B_ITER,
    R_B_ERR,
    R_B_STATUS_ERR,
    R_C_ITER,
    R_C_ERR,
    R_C_DOUBLE_RUNS,
    R_C_DOUBLE_ERR,
    R_D_LIVES,
    R_D_LEAKS,
    R_D_ERR,
    R_D_FAULTS,
    R_FIRST_ERR_WHO,
    R_FIRST_ERR_REG,
    R_FIRST_ERR_GOT,
    R_FIRST_ERR_WANT
};

/* Exact double-precision arithmetic that the compiler allocates registers
 * for, with calls in the middle of it that switch contexts. */
static uint32_t double_workload( void )
{
    double acc = 0.0;
    uint32_t i;

    for( i = 0U; i < DOUBLE_TERMS; i++ )
    {
        acc += ( double ) i * 0.5;

        if( ( i % 128U ) == 0U )
        {
            maxrtos_yield();
        }
    }

    return ( acc == DOUBLE_EXPECTED ) ? 1U : 0U;
}

/* ---- the processes --------------------------------------------------------
 */

/* Worker with the rounding mode "toward plus infinity". Also the sender for
 * control_aux. */
static void control_main( void * arg )
{
    uint32_t want[ 32 ];
    uint32_t const fpscr = 1UL << FPSCR_RMODE_SHIFT;
    uint32_t n;

    ( void ) arg;

    for( n = 0U;; n++ )
    {
        fp_make_pattern( want, 1U, n );
        fp_load( want );
        fpscr_write( fpscr );

        switch( n % 3U )
        {
            case 0U:
                fp_spin_ticks( 2U );
                break;
            case 1U:
                maxrtos_yield();
                break;
            default:
                maxrtos_yield();
                maxrtos_yield();
                break;
        }

        fp_check_registers( R_FIRST_ERR_WHO, R_A_ERR, 1U, want, fpscr );

        /* Wake control_aux with the next token if it has room. */
        if( ( EMU_RES[ R_A_SENT ] - EMU_RES[ R_B_ITER ] ) < 3U )
        {
            uint32_t token = EMU_RES[ R_A_SENT ];

            if( maxrtos_queue_port_send( QA, &token, sizeof( token ), 0U ) ==
                MAXRTOS_OK )
            {
                EMU_RES[ R_A_SENT ]++;
            }
        }

        fp_check_registers( R_FIRST_ERR_WHO, R_A_ERR, 1U, want, fpscr );
        EMU_RES[ R_A_ITER ]++;
    }
}

/* Blocks in a receive holding its registers. */
static void control_aux( void * arg )
{
    uint32_t want[ 32 ];
    uint32_t const fpscr = 3UL << FPSCR_RMODE_SHIFT;
    uint32_t token;
    uint32_t n;

    ( void ) arg;

    for( n = 0U;; n++ )
    {
        maxrtos_status_t status;

        fp_make_pattern( want, 2U, n );
        fp_load( want );
        fpscr_write( fpscr );

        token = 0xFFFFFFFFU;
        status = maxrtos_queue_port_receive(
            QA, &token, sizeof( token ), MAXRTOS_TIMEOUT_INFINITE );

        if( ( status != MAXRTOS_OK ) || ( token != n ) )
        {
            fp_note_error(
                R_FIRST_ERR_WHO,
                R_B_STATUS_ERR,
                2U,
                200U + ( uint32_t ) status,
                token,
                n );
        }

        fp_check_registers( R_FIRST_ERR_WHO, R_B_ERR, 2U, want, fpscr );
        EMU_RES[ R_B_ITER ]++;
    }
}

/* Worker with rounding mode "toward minus infinity", and the arithmetic. */
static void application_main( void * arg )
{
    uint32_t want[ 32 ];
    uint32_t const fpscr = 2UL << FPSCR_RMODE_SHIFT;
    uint32_t n;

    ( void ) arg;

    for( n = 0U;; n++ )
    {
        fp_make_pattern( want, 3U, n );
        fp_load( want );
        fpscr_write( fpscr );

        if( ( n % 4U ) == 3U )
        {
            /* The compiler's own use of the registers runs between the
             * load and the check, so it must put them back. It cannot
             * assume they hold the pattern, and the pattern is checked
             * after, so reload first. */
            if( double_workload() == 1U )
            {
                EMU_RES[ R_C_DOUBLE_RUNS ]++;
            }
            else
            {
                EMU_RES[ R_C_DOUBLE_ERR ]++;
            }

            fp_load( want );
            fpscr_write( fpscr );
        }

        switch( n % 3U )
        {
            case 0U:
                fp_spin_ticks( 1U );
                break;
            case 1U:
                maxrtos_yield();
                break;
            default:
                fp_spin_ticks( 3U );
                break;
        }

        fp_check_registers( R_FIRST_ERR_WHO, R_C_ERR, 3U, want, fpscr );
        EMU_RES[ R_C_ITER ]++;
    }
}

/* A life with a deep stack: all of s0-s31 checked and dirtied, a switch away
 * while holding them, then a fault. */
static void __attribute__( ( noinline ) ) deep_life( uint32_t life )
{
    uint32_t seen[ 32 ];
    uint32_t seen_fpscr;
    uint32_t dirt[ 32 ];
    uint32_t i;

    /* Whatever else the other processes keep in the registers, this is a
     * process that has never used the FPU. */
    fp_store( seen );
    seen_fpscr = fpscr_read();

    for( i = 0U; i < 32U; i++ )
    {
        if( seen[ i ] != 0U )
        {
            fp_note_error( R_FIRST_ERR_WHO, R_D_LEAKS, 4U, i, seen[ i ], 0U );
        }
    }

    if( seen_fpscr != 0U )
    {
        fp_note_error( R_FIRST_ERR_WHO, R_D_LEAKS, 4U, 100U, seen_fpscr, 0U );
    }

    EMU_RES[ R_D_LIVES ] = life + 1U;

    /* Dirty it all, including a rounding mode and flags, and be switched out
     * with it. */
    fp_make_pattern( dirt, 4U, life );
    fp_load( dirt );
    fpscr_write( ( 1UL << FPSCR_RMODE_SHIFT ) | 0x1FU );
    maxrtos_yield();
    maxrtos_yield();
    fp_check_registers(
        R_FIRST_ERR_WHO,
        R_D_ERR,
        4U,
        dirt,
        ( 1UL << FPSCR_RMODE_SHIFT ) | 0x1FU );

    EMU_RES[ R_D_FAULTS ]++;
    *EMU_KERNEL_ADDRESS = 0xDEADU;
}

/* The argument the observer is created with: it is in the new context's r0,
 * at the top of the stack. */
#define OBSERVER_ARG ( ( void * ) 0xA5C3E10FUL )

/* Each life: the first FPU instruction must see zeros. Then dirty the state
 * and fault with it live. Lives alternate between a deep stack and a shallow
 * one.
 *
 * The shallow life is inline here and moves the stack pointer to the very top
 * before it faults: the exception frame (extended, with the room reserved for
 * the lazy stacking of s0-s15) lies exactly where the restarted process's new
 * initial frame is built. A pending lazy stacking that was not cancelled
 * would write s0-s15 over that new frame, and the argument in r0 would
 * change. */
static void application_aux( void * arg )
{
    uint32_t const life = EMU_RES[ R_D_LIVES ];

    if( arg != OBSERVER_ARG )
    {
        fp_note_error(
            R_FIRST_ERR_WHO,
            R_D_ERR,
            5U,
            300U,
            ( uint32_t ) arg,
            0xA5C3E10FUL );
    }

    if( life < OBSERVER_LIVES )
    {
        if( ( life % 2U ) == 0U )
        {
            deep_life( life );
        }
        else
        {
            uint32_t acc;
            uint32_t t;

            __asm volatile( "vmov %0, s0\n\t"
                            "vmov %1, s15\n\t"
                            "orr %0, %0, %1\n\t"
                            "vmov %1, s16\n\t"
                            "orr %0, %0, %1\n\t"
                            "vmov %1, s31\n\t"
                            "orr %0, %0, %1\n\t"
                            "vmrs %1, fpscr\n\t"
                            "orr %0, %0, %1"
                            : "=&r"( acc ), "=&r"( t )
                            :
                            : "cc" );

            if( acc != 0U )
            {
                fp_note_error( R_FIRST_ERR_WHO, R_D_LEAKS, 5U, 0U, acc, 0U );
            }

            EMU_RES[ R_D_LIVES ] = life + 1U;
            EMU_RES[ R_D_FAULTS ]++;

            /* Move the stack pointer to the very top first: this is the
             * shallowest a process can fault, which no compiled function can
             * reach on its own. The asm does not return. */
            __asm volatile( "vmov s0, %0\n\t"
                            "vmov s15, %0\n\t"
                            "vmov s16, %0\n\t"
                            "vmov s31, %0\n\t"
                            "vmsr fpscr, %0\n\t"
                            "mov sp, %2\n\t"
                            "str %0, [%1]"
                            :
                            : "r"( 0xD1D1D1D1UL ),
                              "r"( EMU_KERNEL_ADDRESS ),
                              "r"( ( uint32_t ) maxrtos_stack_application_aux +
                                   PROCESS_STACK_SIZE_application_aux )
                            : "s0", "s15", "s16", "s31", "cc", "memory" );
        }
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

    /* The observer is created with an argument, to see that a restart leaves
     * its new initial frame intact. */
    {
        maxrtos_process_id_t id;

        if( maxrtos_process_create(
                maxrtos_stack_application_aux,
                PROCESS_STACK_SIZE_application_aux,
                PARTITION_ID_application,
                PROCESS_PRIORITY_application_aux,
                application_aux,
                OBSERVER_ARG,
                &id ) != MAXRTOS_OK )
        {
            /* As EMU_CREATE does: the test cannot run, and the tick limit
             * reports it. */
            emu_fail_reason = "process create failed";
            for( ;; )
            {
            }
        }
    }
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
    uint32_t const errors = EMU_RES[ R_A_ERR ] + EMU_RES[ R_B_ERR ] +
                            EMU_RES[ R_B_STATUS_ERR ] + EMU_RES[ R_C_ERR ] +
                            EMU_RES[ R_C_DOUBLE_ERR ] + EMU_RES[ R_D_ERR ] +
                            EMU_RES[ R_D_LEAKS ];

    ( void ) tick;

    /* The first error ends the test, with who and which register in the
     * results for the debugger. */
    CHECK( errors == 0U, "FPU state corrupted or leaked (see R_FIRST_ERR_*)" );

    /* Every fault is the observer's own. */
    CHECK(
        g_maxrtos_arch_last_fault.count == EMU_RES[ R_D_FAULTS ],
        "a fault other than the observer's (FPU disabled? stack?)" );

    if( ( EMU_RES[ R_A_ITER ] < MIN_WORKER_ITERATIONS ) ||
        ( EMU_RES[ R_B_ITER ] < MIN_RECEIVES ) ||
        ( EMU_RES[ R_C_ITER ] < MIN_WORKER_ITERATIONS ) ||
        ( EMU_RES[ R_C_DOUBLE_RUNS ] < MIN_DOUBLE_RUNS ) ||
        ( EMU_RES[ R_D_LIVES ] < OBSERVER_LIVES ) )
    {
        return EMU_RUNNING;
    }

    /* The last life has faulted and been restarted, and everyone kept going. */
    CHECK(
        EMU_RES[ R_D_FAULTS ] == OBSERVER_LIVES,
        "the observer did not fault once per life" );

    return EMU_PASS;
}
