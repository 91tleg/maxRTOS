/**
 * @file test_fpu_startup.c
 * @brief Whatever start-up code left in the FPU does not reach the first
 *        process.
 *
 * Before the kernel initializes, emu_test_early() grants itself the FPU (as
 * start-up code that prepares it for the C library would), fills s-registers
 * and FPSCR with values of its own, and so leaves CONTROL.FPCA set. The first
 * switch is then made from a context with live FPU state and a lazy stacking
 * pending, with nothing to save. Every process's first FPU instruction must
 * still see zero in all of s0-s31 and in FPSCR, and the processes then work as
 * in test_fpu_context.
 */

#include "fpu_test.h"

enum
{
    R_A_LEAKS,
    R_B_LEAKS,
    R_A_ITER,
    R_B_ITER,
    R_A_ERR,
    R_B_ERR,
    R_FIRST_ERR_WHO,
    R_FIRST_ERR_REG,
    R_FIRST_ERR_GOT,
    R_FIRST_ERR_WANT
};

/* Grant the FPU, then fill all of s0-s31 and FPSCR. Written in assembly:
 * a compiled function that uses the registers would save and restore s16-s31
 * around its body, with an FPU instruction in its prologue before access is
 * granted, and would undo the registers it fills. */
void __attribute__( ( naked ) ) emu_test_early( void )
{
    __asm volatile(
        "ldr r0, =0xE000ED88\n\t"
        "ldr r1, [r0]\n\t"
        "orr r1, r1, #0x00F00000\n\t"
        "str r1, [r0]\n\t"
        "dsb\n\t"
        "isb\n\t"
        "ldr r2, =0x57A27000\n\t"
        ".irp n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,"
        "24,25,26,27,28,29,30,31\n\t"
        "vmov s\\n, r2\n\t"
        ".endr\n\t"
        "ldr r3, =0x00C0001F\n\t"
        "vmsr fpscr, r3\n\t"
        "bx lr\n\t"
        ".ltorg" );
}

static void process(
    uint32_t who,
    uint32_t leaks,
    uint32_t iter,
    uint32_t err )
{
    uint32_t want[ 32 ];
    uint32_t n;

    /* The first FPU instruction of this process. */
    fp_check_zero( R_FIRST_ERR_WHO, leaks, who );

    for( n = 0U;; n++ )
    {
        fp_make_pattern( want, who, n );
        fp_load( want );
        fpscr_write( who << FPSCR_RMODE_SHIFT );
        maxrtos_yield();
        fp_check_registers(
            R_FIRST_ERR_WHO, err, who, want, who << FPSCR_RMODE_SHIFT );
        EMU_RES[ iter ]++;
    }
}

static void control_main( void * arg )
{
    ( void ) arg;
    process( 1U, R_A_LEAKS, R_A_ITER, R_A_ERR );
}

static void application_main( void * arg )
{
    ( void ) arg;
    process( 2U, R_B_LEAKS, R_B_ITER, R_B_ERR );
}

void emu_test_setup( void )
{
    EMU_CREATE( control_main, PARTITION_ID_control, control_main );
    EMU_CREATE( application_main, PARTITION_ID_application, application_main );
}

int emu_test_poll( uint32_t tick )
{
    ( void ) tick;

    if( ( EMU_RES[ R_A_LEAKS ] + EMU_RES[ R_B_LEAKS ] + EMU_RES[ R_A_ERR ] +
          EMU_RES[ R_B_ERR ] ) != 0U )
    {
        return EMU_FAILED(
            "the FPU left by start-up code reached a process, or "
            "state was lost (see R_FIRST_ERR_*)" );
    }

    if( ( EMU_RES[ R_A_ITER ] > 20U ) && ( EMU_RES[ R_B_ITER ] > 20U ) )
    {
        return EMU_PASS;
    }

    return EMU_RUNNING;
}
