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

#include "maxrtos/arch/cortex_m7/context_switch.h"

/* ICSR controls system exception state, including the PendSV pending bit. */
#define MAXRTOS_SCB_ICSR \
    ( *( volatile uint32_t * ) 0xE000ED04UL )

/* SHPR3 contains the configurable priority fields for SysTick and PendSV. */
#define MAXRTOS_SCB_SHPR3 \
    ( *( volatile uint32_t * ) 0xE000ED20UL )

#define MAXRTOS_ICSR_PENDSVSET_BIT \
    ( 1UL << 28U )

/* SHPR3 priority fields.
 * ARM exception priorities use numerically larger values to represent
 * lower logical priority. PendSV shall execute at the lowest
 * configurable exception priority so that context switching does not
 * preempt higher-priority interrupt handlers. */
#define MAXRTOS_SHPR3_PENDSV_LOWEST \
    ( 0xFFUL << 16U )

#define MAXRTOS_SHPR3_SYSTICK_LOWEST \
    ( 0xFFUL << 24U )

/* Context-switch state shared with context_switch.S.
 * These objects are intentionally externally visible because the
 * PendSV handler accesses them directly. They shall only be modified
 * through the architecture context-switch interface. */
maxrtos_process_control_block_t * g_maxrtos_current_pcb = NULL;
maxrtos_process_control_block_t * g_maxrtos_next_pcb = NULL;

/* Cortex-M requires the Thumb state bit to be set in an exception
 * return address. */
#define MAXRTOS_THUMB_BIT \
    ( 0x1UL )

/* Initial xPSR value for a newly-created process.
 * The Thumb-state bit is set and all other fields zeroed. */
#define MAXRTOS_INITIAL_XPSR \
    ( 0x01000000UL )

/* Number of 32-bit words in the constructed initial stack frame.
 * 8 software-saved regs (r4-r11) + 8 hardware-stacked regs
 * (r0-r3, r12, LR, PC, xPSR). */
#define MAXRTOS_INITIAL_FRAME_WORDS \
    ( 16U )

#define MAXRTOS_ARCH_STACK_ALIGNMENT \
    ( 8U )

#define MAXRTOS_ARCH_MIN_STACK_SIZE \
    ( ( MAXRTOS_INITIAL_FRAME_WORDS * sizeof( uint32_t ) ) + \
      ( MAXRTOS_ARCH_STACK_ALIGNMENT - 1U ) )

/* Handle unexpected return from a process entry function.
 * Process entry functions shall not return to their caller. If an
 * entry function returns, interrupts are disabled and execution is
 * halted. */
static void maxrtos_arch_process_return_trap( void )
{
    __asm volatile ( "cpsid i" );

    for( ;; )
    {
        __asm volatile ( "bkpt #0" );
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

    if( ( pcb != NULL ) &&
        ( pcb->stack_base != NULL ) &&
        ( pcb->stack_size >= MAXRTOS_ARCH_MIN_STACK_SIZE ) &&
        ( pcb->entry != NULL ) )
    {
        frame_top = ( uint32_t * )
            ( ( ( uintptr_t ) pcb->stack_base +
                ( uintptr_t ) pcb->stack_size ) &
              ~( ( uintptr_t ) 0x7U ) );

        sp = frame_top - MAXRTOS_INITIAL_FRAME_WORDS;

        /* Software-saved registers r4-r11. */
        sp[ 0 ] = 0UL;
        sp[ 1 ] = 0UL;
        sp[ 2 ] = 0UL;
        sp[ 3 ] = 0UL;
        sp[ 4 ] = 0UL;
        sp[ 5 ] = 0UL;
        sp[ 6 ] = 0UL;
        sp[ 7 ] = 0UL;

        /* Hardware-stacked exception frame. */
        sp[ 8 ]  = ( uint32_t ) ( uintptr_t ) pcb->entry_arg;
        sp[ 9 ]  = 0UL;
        sp[ 10 ] = 0UL;
        sp[ 11 ] = 0UL;
        sp[ 12 ] = 0UL;
        sp[ 13 ] =
            ( uint32_t ) ( uintptr_t ) maxrtos_arch_process_return_trap;
        sp[ 14 ] =
            ( ( uint32_t ) ( uintptr_t ) pcb->entry ) |
            MAXRTOS_THUMB_BIT;
        sp[ 15 ] = MAXRTOS_INITIAL_XPSR;

        pcb->stack_pointer = ( void * ) sp;
        status = MAXRTOS_OK;
    }

    return status;
}

void maxrtos_arch_set_current_pcb(
    maxrtos_process_control_block_t * pcb )
{
    g_maxrtos_current_pcb = pcb;
}

void maxrtos_arch_set_next_pcb(
    maxrtos_process_control_block_t * pcb )
{
    g_maxrtos_next_pcb = pcb;
}

maxrtos_process_control_block_t * maxrtos_arch_get_current_pcb( void )
{
    return g_maxrtos_current_pcb;
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

    __asm volatile ( "dsb" );
    __asm volatile ( "isb" );
}

void maxrtos_arch_start_first_process(
    maxrtos_process_control_block_t * pcb )
{
    g_maxrtos_current_pcb = NULL;
    g_maxrtos_next_pcb = pcb;

    maxrtos_arch_request_context_switch();

    /* Reaching this point indicates that the requested context switch
     * did not transfer execution to the first process. */
    for( ;; )
    {
        __asm volatile ( "bkpt #0" );
    }
}
