/**
 * @file idle.c
 * @brief Cortex-M7 idle context.
 *
 * See idle.h.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "maxrtos/arch/cortex_m7/idle.h"
#include "maxrtos/arch/cortex_m7/context_switch.h"
#include "maxrtos/arch/cortex_m7/port.h"

/* Room for the initial frame (16 words) plus the exception frame taken
 * when SysTick or SVC interrupts the idle loop. */
#define MAXRTOS_IDLE_STACK_SIZE ( 256U )

static uint8_t s_idle_stack[ MAXRTOS_IDLE_STACK_SIZE ]
    __attribute__(( aligned( 8 ) ));

static maxrtos_process_control_block_t s_idle_pcb;

static void maxrtos_arch_idle_entry( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        MAXRTOS_PORT_WFI();
    }
}

void maxrtos_arch_idle_init( void )
{
    s_idle_pcb.id = MAXRTOS_INVALID_PROCESS_ID;
    s_idle_pcb.partition_id = MAXRTOS_INVALID_PARTITION_ID;
    s_idle_pcb.state = MAXRTOS_PROCESS_STATE_RUNNING;
    s_idle_pcb.priority = 0U;
    s_idle_pcb.unprivileged = false;
    s_idle_pcb.stack_base = s_idle_stack;
    s_idle_pcb.stack_size = MAXRTOS_IDLE_STACK_SIZE;
    s_idle_pcb.wake_tick = MAXRTOS_TICK_NONE;
    s_idle_pcb.waitlist = NULL;
    s_idle_pcb.entry = maxrtos_arch_idle_entry;
    s_idle_pcb.entry_arg = NULL;

    if( maxrtos_arch_init_stack( &s_idle_pcb ) != MAXRTOS_OK )
    {
        MAXRTOS_PORT_HALT();
    }
}

maxrtos_process_control_block_t * maxrtos_arch_idle_pcb( void )
{
    return &s_idle_pcb;
}

void maxrtos_arch_idle_enter_discarding_current( void )
{
    /* Re-arm the idle frame: if idle was interrupted earlier its saved
     * context is stale but valid, so only a first entry needs this. A
     * fresh frame is always safe because idle holds no state. */
    ( void ) maxrtos_arch_init_stack( &s_idle_pcb );

    maxrtos_arch_set_current_pcb( NULL );
    maxrtos_arch_set_next_pcb( &s_idle_pcb );
    maxrtos_arch_request_context_switch();
}
