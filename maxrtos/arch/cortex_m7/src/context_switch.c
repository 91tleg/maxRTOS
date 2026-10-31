/**
 * @file context_switch.c
 * @brief Cortex-M7 context-switch support.
 *
 * Provides the C-side state management and initialization for the
 * Cortex-M7 context-switch mechanism. Register save and restore
 * operations are implemented by the PendSV handler in
 * context_switch.S.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "maxrtos/arch/cortex_m7/context_switch.h"
#include "maxrtos/arch/cortex_m7/idle.h"
#include "maxrtos/arch/cortex_m7/mpu_hw.h"

/* SCB ICSR: PendSV set-pending bit. */
#define MAXRTOS_SCB_ICSR ( *( volatile uint32_t * ) 0xE000ED04UL )

/* SHPR3: SysTick and PendSV priority fields. */
#define MAXRTOS_SCB_SHPR3 ( *( volatile uint32_t * ) 0xE000ED20UL )

#define MAXRTOS_ICSR_PENDSVSET_BIT ( UINT32_C( 1 ) << 28U )

#define MAXRTOS_CONTROL_NPRIV ( UINT32_C( 1 ) )

/* Coprocessor Access Control Register: CP10 and CP11 are the FPU. Full
 * access (both privilege levels) is 0b11 in each field. */
#define MAXRTOS_SCB_CPACR       ( *( volatile uint32_t * ) 0xE000ED88UL )
#define MAXRTOS_CPACR_CP10_CP11 ( UINT32_C( 0xF ) << 20U )

/* Floating-Point Context Control Register. ASPEN: CONTROL.FPCA follows
 * the use of the FPU. LSPEN: lazy stacking, so an exception does not copy
 * s0-s15 and FPSCR unless the handler uses the FPU. */
#define MAXRTOS_FPU_FPCCR       ( *( volatile uint32_t * ) 0xE000EF34UL )
#define MAXRTOS_FPCCR_ASPEN_BIT ( UINT32_C( 1 ) << 31U )
#define MAXRTOS_FPCCR_LSPEN_BIT ( UINT32_C( 1 ) << 30U )

/* Default FPSCR for a context's first FPU instruction: round to nearest, no
 * flags, no traps. */
#define MAXRTOS_FPU_FPDSCR ( *( volatile uint32_t * ) 0xE000EF3CUL )

/* SHPR3 priority fields.
 * ARM exception priorities use numerically larger values to represent
 * lower logical priority. PendSV shall execute at the lowest
 * configurable exception priority so that context switching does not
 * preempt higher-priority interrupt handlers. */
#define MAXRTOS_SHPR3_PENDSV_LOWEST ( UINT32_C( 0xFF ) << 16U )

#define MAXRTOS_SHPR3_SYSTICK_LOWEST ( UINT32_C( 0xFF ) << 24U )

/* Context-switch state shared with context_switch.S.
 * These objects are intentionally externally visible because the
 * PendSV handler accesses them directly. They shall only be modified
 * through the architecture context-switch interface. */
maxrtos_process_control_block_t * g_maxrtos_current_pcb = NULL;
maxrtos_process_control_block_t * g_maxrtos_next_pcb = NULL;

/* Thumb-state bit required in the stacked PC for Cortex-M. */
#define MAXRTOS_THUMB_BIT ( UINT32_C( 1 ) )

/* Initial xPSR value for a newly-created process.
 * The Thumb-state bit is set and all other fields zeroed. */
#define MAXRTOS_INITIAL_XPSR ( 0x01000000UL )

/* The PendSV handler reaches these PCB members at fixed offsets. */
_Static_assert(
    offsetof(
        maxrtos_process_control_block_t,
        stack_pointer ) == 0U,
    "context_switch.S expects stack_pointer at offset 0" );
_Static_assert(
    offsetof(
        maxrtos_process_control_block_t,
        stack_base ) == 4U,
    "context_switch.S expects stack_base at offset 4" );
_Static_assert(
    offsetof(
        maxrtos_process_control_block_t,
        stack_size ) == 8U,
    "context_switch.S expects stack_size at offset 8" );

/* Every process has the same saved context, in words from its stack pointer:
 *
 *    0-15   s16-s31, saved by the PendSV handler
 *   16-23   r4-r11, saved by the PendSV handler
 *   24-31   r0-r3, r12, lr, pc, xPSR  } the extended exception frame the
 *   32-47   s0-s15                    } hardware stacks for a process that
 *   48      FPSCR                     } can use the FPU, which is every one
 *   49      reserved                  }
 *
 * Because every context has the whole FPU state, a new or restarted process
 * starts from zeros, which is also what keeps one process's FPU values from
 * reaching another. */
#define MAXRTOS_INITIAL_FRAME_WORDS ( 50U )

#define MAXRTOS_FRAME_R0   ( 24U )
#define MAXRTOS_FRAME_LR   ( 29U )
#define MAXRTOS_FRAME_PC   ( 30U )
#define MAXRTOS_FRAME_XPSR ( 31U )

#define MAXRTOS_ARCH_STACK_ALIGNMENT ( 8U )

#define MAXRTOS_ARCH_MIN_STACK_SIZE                                            \
    ( ( MAXRTOS_INITIAL_FRAME_WORDS * sizeof( uint32_t ) ) +                   \
      ( MAXRTOS_ARCH_STACK_ALIGNMENT - 1U ) )

/* Handle unexpected return from a process entry function.
 * Process entry functions shall not return to their caller. If an
 * entry function returns, interrupts are disabled and execution is
 * halted. */
static void maxrtos_arch_process_return_trap( void )
{
    __asm volatile( "cpsid i" );

    for( ;; )
    {
        __asm volatile( "bkpt #0" );
    }
}

maxrtos_status_t maxrtos_arch_init_stack(
    maxrtos_process_control_block_t * pcb )
{
    maxrtos_status_t status;
    uint32_t * frame_top;
    uint32_t * sp;

    status = MAXRTOS_ERR_INVALID_ARG;
    frame_top = NULL;
    sp = NULL;

    if( ( pcb != NULL ) && ( pcb->stack_base != NULL ) &&
        ( pcb->stack_size >= MAXRTOS_ARCH_MIN_STACK_SIZE ) &&
        ( pcb->entry != NULL ) )
    {
        frame_top = ( uint32_t * ) ( ( ( uintptr_t ) pcb->stack_base +
                                       ( uintptr_t ) pcb->stack_size ) &
                                     ~( ( uintptr_t ) 0x7U ) );

        sp = frame_top - MAXRTOS_INITIAL_FRAME_WORDS;

        /* Everything starts as zero: the registers, the FPU registers and
         * FPSCR. Only the entry state is set. */
        for( size_t i = 0U; i < MAXRTOS_INITIAL_FRAME_WORDS; i++ )
        {
            sp[ i ] = 0UL;
        }

        sp[ MAXRTOS_FRAME_R0 ] = ( uint32_t ) ( uintptr_t ) pcb->entry_arg;
        sp[ MAXRTOS_FRAME_LR ] =
            ( uint32_t ) ( uintptr_t ) maxrtos_arch_process_return_trap;
        sp[ MAXRTOS_FRAME_PC ] =
            ( ( uint32_t ) ( uintptr_t ) pcb->entry ) | MAXRTOS_THUMB_BIT;
        sp[ MAXRTOS_FRAME_XPSR ] = MAXRTOS_INITIAL_XPSR;

        pcb->stack_pointer = ( void * ) sp;

        /* A fresh context has no blocked operation waiting to complete. */
        pcb->ipc_result_pending = false;

        status = MAXRTOS_OK;
    }

    return status;
}

void maxrtos_arch_set_current_pcb( maxrtos_process_control_block_t * pcb )
{
    g_maxrtos_current_pcb = pcb;
}

void maxrtos_arch_set_next_pcb( maxrtos_process_control_block_t * pcb )
{
    g_maxrtos_next_pcb = pcb;
}

maxrtos_process_control_block_t * maxrtos_arch_get_current_pcb( void )
{
    return g_maxrtos_current_pcb;
}

static void maxrtos_arch_apply_privilege_for_next_pcb(
    maxrtos_process_control_block_t const * pcb )
{
    if( pcb != NULL )
    {
        uint32_t current;
        uint32_t wanted;

        __asm volatile( "mrs %0, control" : "=r"( current ) : : "memory" );

        if( maxrtos_arch_partition_is_privileged( pcb->partition_id ) == false )
        {
            wanted = current | MAXRTOS_CONTROL_NPRIV; /* CONTROL.nPRIV = 1 */
        }
        else
        {
            wanted = current & ~MAXRTOS_CONTROL_NPRIV; /* CONTROL.nPRIV = 0 */
        }

        /* Most switches stay inside one partition, so the level is usually
         * already right; skip the write and its barrier then. The live
         * register is compared, not a remembered value, so a level changed
         * by anything else is still corrected. */
        if( wanted != current )
        {
            __asm volatile( "msr control, %0" : : "r"( wanted ) : "memory" );

            __asm volatile( "isb" );
        }
    }
}

static void maxrtos_arch_apply_resume_result(
    maxrtos_process_control_block_t * pcb )
{
    if( ( pcb != NULL ) && ( pcb->ipc_result_pending == true ) &&
        ( pcb->stack_pointer != NULL ) )
    {
        uint32_t * frame;

        /* stack_pointer addresses the saved context (see the layout above);
         * r0 of the exception frame is the value the process's SVC wrapper
         * returns. */
        frame = ( uint32_t * ) pcb->stack_pointer;
        frame[ MAXRTOS_FRAME_R0 ] = ( uint32_t ) pcb->ipc_result;

        pcb->ipc_result_pending = false;
    }
}

maxrtos_status_t maxrtos_arch_prepare_next_pcb(
    maxrtos_process_control_block_t * pcb )
{
    maxrtos_status_t status;

    status = maxrtos_arch_mpu_configure_for_next_pcb( pcb );

    if( status == MAXRTOS_OK )
    {
        maxrtos_arch_apply_privilege_for_next_pcb( pcb );
        maxrtos_arch_apply_resume_result( pcb );
    }

    return status;
}

void maxrtos_arch_request_context_switch( void )
{
    MAXRTOS_SCB_ICSR = MAXRTOS_ICSR_PENDSVSET_BIT;
}

void maxrtos_arch_context_switch_init( void )
{
    uint32_t shpr3;

    /* Configure PendSV and SysTick to the lowest configurable
     * exception priority. */
    shpr3 = MAXRTOS_SCB_SHPR3;

    shpr3 &= ~( 0xFFUL << 16U );
    shpr3 &= ~( 0xFFUL << 24U );

    shpr3 |= MAXRTOS_SHPR3_PENDSV_LOWEST;
    shpr3 |= MAXRTOS_SHPR3_SYSTICK_LOWEST;

    MAXRTOS_SCB_SHPR3 = shpr3;

    /* Let processes use the FPU: both privilege levels, automatic state
     * (ASPEN) and lazy stacking (LSPEN), so an exception does not copy
     * s0-s15 and FPSCR until something uses them. The PendSV handler saves
     * and restores what a process leaves in the FPU. */
    MAXRTOS_SCB_CPACR |= MAXRTOS_CPACR_CP10_CP11;
    MAXRTOS_FPU_FPCCR |= MAXRTOS_FPCCR_ASPEN_BIT | MAXRTOS_FPCCR_LSPEN_BIT;
    MAXRTOS_FPU_FPDSCR = 0U;

    __asm volatile( "dsb" );
    __asm volatile( "isb" );

    maxrtos_arch_idle_init();
}

void maxrtos_arch_start_first_process( maxrtos_process_control_block_t * pcb )
{
    g_maxrtos_current_pcb = NULL;
    g_maxrtos_next_pcb = pcb;

    maxrtos_arch_request_context_switch();

    /* Make sure the pended PendSV is taken before the next instruction
     * executes. Without the barriers the CPU can reach the breakpoint
     * below first. */
    __asm volatile( "dsb" ::: "memory" );
    __asm volatile( "isb" ::: "memory" );

    /* Reaching this point indicates that the requested context switch
     * did not transfer execution to the first process. */
    for( ;; )
    {
        __asm volatile( "bkpt #0" );
    }
}
