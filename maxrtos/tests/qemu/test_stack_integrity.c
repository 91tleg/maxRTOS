/**
 * @file test_stack_integrity.c
 * @brief A process cannot make the context switch write outside its stack.
 *
 * The PendSV handler saves a switched-out process's registers with privilege,
 * wherever its stack pointer says. A process that moved its stack pointer
 * could therefore make it overwrite another process's, or another partition's,
 * saved context, and with it the program counter it will resume at. The
 * handler checks the stack pointer against the stack the process was given
 * before it writes, and handles a process that failed the check as having
 * faulted.
 *
 * application_main plays the attacker. Each life it points its stack pointer
 * somewhere else, with the FPU state live, and spins until the end of the slot
 * interrupts it:
 *
 *   0  just above its stack base: the context would go below the stack, into
 *      the control partition's last stack
 *   1  just above its stack top: the exception frame would end past the top
 *   2  an address nowhere near any stack
 *   3  the very top of the address space
 *   4  eight bytes short of where the context fits at the bottom of the stack
 *   5  eight bytes past where the exception frame fits at the top
 *   6  where the context just fits at the bottom of the stack (it must be left
 *      alone)
 *   7  at the top of the stack, where the exception frame just fits (the same)
 *
 * control_main and control_aux are the victims: they hold FPU registers in
 * patterns of their own, wait, and check them. If the attack reached their
 * saved contexts they would find them changed or die. Neither may be
 * restarted, and attacks 0 to 5 must each end with the attacker restarted.
 */

#include "fpu_test.h"
#include "maxrtos/arch/cortex_m7/fault_handlers.h"

#define OBSERVE_TICKS ( 6U )

enum
{
    R_A_ENTRIES,
    R_A_ITER,
    R_A_ERR,
    R_B_ENTRIES,
    R_B_ITER,
    R_B_ERR,
    R_LIFE,
    R_POLL_TICK,
    R_SPIN,
    R_BOUNDARY_LOW_OK,
    R_BOUNDARY_HIGH_OK,
    R_FIRST_ERR_WHO,
    R_FIRST_ERR_REG,
    R_FIRST_ERR_GOT,
    R_FIRST_ERR_WANT
};

#define ATTACKS ( 8U )

/* ---- the victims ----------------------------------------------------------
 */

static void victim(
    uint32_t who,
    uint32_t entries,
    uint32_t iter,
    uint32_t err )
{
    uint32_t want[ 32 ];
    uint32_t n;
    uint32_t const fpscr = who << FPSCR_RMODE_SHIFT;

    EMU_RES[ entries ]++;

    for( n = 0U;; n++ )
    {
        fp_make_pattern( want, who, n );
        fp_load( want );
        fpscr_write( fpscr );

        if( who == 1U )
        {
            ( void ) maxrtos_timed_wait( 1U + ( n % 2U ) );
        }
        else
        {
            maxrtos_yield();
        }

        fp_check_registers( R_FIRST_ERR_WHO, err, who, want, fpscr );
        EMU_RES[ iter ]++;
    }
}

static void control_main( void * arg )
{
    ( void ) arg;
    victim( 1U, R_A_ENTRIES, R_A_ITER, R_A_ERR );
}

static void control_aux( void * arg )
{
    ( void ) arg;
    victim( 2U, R_B_ENTRIES, R_B_ITER, R_B_ERR );
}

/* ---- the attacker ----------------------------------------------------------
 */

/* Use the FPU (so the exception frame is the extended one), set the stack
 * pointer to `sp`, and run until the poll function has seen `ticks` ticks pass
 * or, if `ticks` is zero, until something stops it. When the time is up, set
 * EMU_RES[ done ] and leave through a fault, which restarts the process for its
 * next life. It uses no stack and never returns: the exception frame at the
 * stack pointer it was given may lie over the frame of the function that
 * called it, which this must not return through. */
static void spin_at(
    uint32_t sp,
    uint32_t ticks,
    uint32_t done )
{
    __asm volatile( "vmov s0, %[sp]\n\t"
                    "mov sp, %[sp]\n\t"
                    "ldr r5, [%[res], %[tick_at]]\n\t"
                    "add r5, r5, %[ticks]\n\t"
                    "1:\n\t"
                    "ldr r6, [%[res], %[spin_at]]\n\t"
                    "add r6, r6, #1\n\t"
                    "str r6, [%[res], %[spin_at]]\n\t"
                    "cmp %[ticks], #0\n\t"
                    "beq 1b\n\t"
                    "ldr r6, [%[res], %[tick_at]]\n\t"
                    "cmp r6, r5\n\t"
                    "blo 1b\n\t"
                    "mov r6, #1\n\t"
                    "str r6, [%[res], %[done]]\n\t"
                    "ldr r5, =g_emu_kernel_word\n\t"
                    "str r6, [r5]\n\t"
                    "2: b 2b"
                    :
                    : [sp] "r"( sp ),
                      [res] "r"( EMU_RES ),
                      [ticks] "r"( ticks ),
                      [done] "r"( done * 4U ),
                      [tick_at] "i"( R_POLL_TICK * 4U ),
                      [spin_at] "i"( R_SPIN * 4U )
                    : "r5", "r6", "s0", "cc", "memory" );
}

static void application_main( void * arg )
{
    uint32_t const life = EMU_RES[ R_LIFE ];
    uint32_t const base = ( uint32_t ) maxrtos_stack_application_main;
    uint32_t const top = base + PROCESS_STACK_SIZE_application_main;

    ( void ) arg;

    if( life < ATTACKS )
    {
        EMU_RES[ R_LIFE ] = life + 1U;

        switch( life )
        {
            case 0U:
                /* The extended frame starts at the base, so the context
                 * would be written 96 bytes below it. */
                spin_at( base + 104U, 0U, R_SPIN );
                break;
            case 1U:
                /* The extended frame would end 64 bytes past the top. */
                spin_at( top + 64U, 0U, R_SPIN );
                break;
            case 2U:
                spin_at( 0x00000040UL, 0U, R_SPIN );
                break;
            case 3U:
                spin_at( 0xFFFFFFF0UL, 0U, R_SPIN );
                break;
            case 4U:
                /* The extended frame at base + 88: its context is eight bytes
                 * below the stack. */
                spin_at( base + 192U, 0U, R_SPIN );
                break;
            case 5U:
                /* The frame would end eight bytes past the top. */
                spin_at( top + 8U, 0U, R_SPIN );
                break;
            case 6U:
                /* The frame at the base and the context 96 bytes above it:
                 * exactly what fits. Must run, undisturbed. */
                spin_at( base + 200U, OBSERVE_TICKS, R_BOUNDARY_LOW_OK );
                break;
            default:
                /* A stack pointer at the very top: the frame ends at the top.
                 */
                spin_at( top, OBSERVE_TICKS, R_BOUNDARY_HIGH_OK );
                break;
        }
    }

    for( ;; )
    {
        maxrtos_yield();
    }
}

static void application_aux( void * arg )
{
    ( void ) arg;

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
        ( EMU_RES[ R_A_ERR ] + EMU_RES[ R_B_ERR ] ) == 0U,
        "a victim's saved context was changed (see R_FIRST_ERR_*)" );
    CHECK(
        ( EMU_RES[ R_A_ENTRIES ] <= 1U ) && ( EMU_RES[ R_B_ENTRIES ] <= 1U ),
        "a victim was restarted: the attacker's fault was charged to it" );

    if( ( EMU_RES[ R_LIFE ] < ATTACKS ) ||
        ( EMU_RES[ R_BOUNDARY_HIGH_OK ] == 0U ) )
    {
        return EMU_RUNNING;
    }

    CHECK(
        EMU_RES[ R_BOUNDARY_LOW_OK ] == 1U,
        "a stack pointer where the context just fits was treated as a fault" );
    CHECK(
        ( EMU_RES[ R_A_ITER ] > 10U ) && ( EMU_RES[ R_B_ITER ] > 10U ),
        "a victim stopped running" );

    return EMU_PASS;
}
