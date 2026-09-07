/**
 * @file fault_handlers.c
 * @brief ARMv7-M fault exception enable and fault handler
 *        implementation.
 *
 * Fault handlers classify the architecture-specific exception and
 * delegate recovery policy and recovery mechanics to the kernel.
 */

#include <stdint.h>
#include <stdnoreturn.h>

#include "maxrtos/arch/cortex_m7/fault_handlers.h"
#include "maxrtos/arch/cortex_m7/context_switch.h"
#include "maxrtos/kernel/fault_recovery.h"

/* System Control Block configuration and fault-status registers. */
#define MAXRTOS_SCB_CCR   ( *( volatile uint32_t * ) 0xE000ED14UL )
#define MAXRTOS_SCB_SHCSR ( *( volatile uint32_t * ) 0xE000ED24UL )
#define MAXRTOS_SCB_CFSR  ( *( volatile uint32_t * ) 0xE000ED28UL )

/* CCR.DIV_0_TRP: enable UsageFault generation for integer
 * divide-by-zero operations. */
#define MAXRTOS_CCR_DIVBYZERO_TRP_BIT   ( 1UL << 4U )

/* SHCSR fault-handler enable bits. */
#define MAXRTOS_SHCSR_MEMFAULTENA_BIT   ( 1UL << 16U )
#define MAXRTOS_SHCSR_BUSFAULTENA_BIT   ( 1UL << 17U )
#define MAXRTOS_SHCSR_USGFAULTENA_BIT   ( 1UL << 18U )

/* CFSR.UFSR.DIVBYZERO: indicates that an integer divide-by-zero
 * operation caused the UsageFault exception. */
#define MAXRTOS_CFSR_UFSR_DIVBYZERO_BIT ( 1UL << 25U )

/* CFSR sub-register masks. */
#define MAXRTOS_CFSR_MMFSR_MASK         ( 0x000000FFUL )
#define MAXRTOS_CFSR_BFSR_MASK          ( 0x0000FF00UL )
#define MAXRTOS_CFSR_UFSR_MASK          ( 0xFFFF0000UL )

static maxrtos_health_monitor_t const * s_hm = NULL;
static maxrtos_partition_table_t * s_table = NULL;

static noreturn void maxrtos_arch_halt( void )
{
    __asm volatile ( "cpsid i" );

    for( ;; )
    {
        __asm volatile ( "bkpt #0" );
    }
}

void maxrtos_arch_fault_handlers_init( void )
{
    MAXRTOS_SCB_SHCSR |=
        MAXRTOS_SHCSR_MEMFAULTENA_BIT |
        MAXRTOS_SHCSR_BUSFAULTENA_BIT |
        MAXRTOS_SHCSR_USGFAULTENA_BIT;

    MAXRTOS_SCB_CCR |= MAXRTOS_CCR_DIVBYZERO_TRP_BIT;
}

void maxrtos_arch_set_health_monitor(
    maxrtos_health_monitor_t const * hm )
{
    s_hm = hm;
}

void maxrtos_arch_set_partition_table(
    maxrtos_partition_table_t * table )
{
    s_table = table;
}

maxrtos_partition_table_t * maxrtos_arch_get_partition_table( void )
{
    return s_table;
}

/* Handle a fault after architecture-specific classification.
 * Recovery policy is selected by the kernel health monitor. The
 * selected action is then enacted here because process context and
 * context-switch operations are architecture-specific. */
static void maxrtos_arch_handle_classified_fault(
    maxrtos_fault_type_t fault_type )
{
    maxrtos_process_control_block_t * faulting_pcb;
    maxrtos_hm_action_t action;
    maxrtos_status_t status;

    if( ( s_hm == NULL ) || ( s_table == NULL ) )
    {
        maxrtos_arch_halt();
    }

    faulting_pcb = maxrtos_arch_get_current_pcb();

    if( faulting_pcb == NULL )
    {
        maxrtos_arch_halt();
    }

    status = maxrtos_fault_recovery_handle(
                 s_hm,
                 s_table,
                 faulting_pcb->id,
                 fault_type,
                 &action );

    if( status != MAXRTOS_OK )
    {
        maxrtos_arch_halt();
    }

    switch( action )
    {
        case MAXRTOS_HM_ACTION_IGNORE:
        {
            /* Returning from the exception resumes the faulting
             * instruction. This is only useful for fault classes
             * where retrying the instruction is known to be valid. */
            break;
        }

        case MAXRTOS_HM_ACTION_RESTART_PROCESS:
        {
            maxrtos_process_id_t next_id;
            maxrtos_process_control_block_t * next_pcb;
            maxrtos_status_t dispatch_status;

            status = maxrtos_arch_init_stack( faulting_pcb );

            if( status != MAXRTOS_OK )
            {
                maxrtos_arch_halt();
            }

            dispatch_status = maxrtos_partition_dispatch(
                                  s_table,
                                  faulting_pcb->partition_id,
                                  &next_id );

            if( dispatch_status != MAXRTOS_OK )
            {
                maxrtos_arch_halt();
            }

            next_pcb = maxrtos_process_get( next_id );

            if( next_pcb == NULL )
            {
                maxrtos_arch_halt();
            }

            /* The faulting context must not be saved. Its stack now contains
             * the fresh initial frame prepared by maxrtos_arch_init_stack(). */
            maxrtos_arch_set_current_pcb( NULL );
            maxrtos_arch_set_next_pcb( next_pcb );
            maxrtos_arch_request_context_switch();

            break;
        }

        case MAXRTOS_HM_ACTION_HALT_PARTITION:
        {
            /* The partition has been marked halted by the recovery layer.
             * Do not return from the fault handler because that would resume
             * the faulting context. Sleep until an interrupt transfers control
             * to the scheduler or another exception handler. */
            {
                __asm volatile ( "wfi" );
            }

            break;
        }

        default:
        {
            maxrtos_arch_halt();
            break;
        }
    }
}

void MemManage_Handler( void )
{
    uint32_t cfsr;

    cfsr = MAXRTOS_SCB_CFSR;

    MAXRTOS_SCB_CFSR = cfsr & MAXRTOS_CFSR_MMFSR_MASK;

    maxrtos_arch_handle_classified_fault(
        MAXRTOS_FAULT_MEMORY_ACCESS );
}

void BusFault_Handler( void )
{
    uint32_t cfsr;

    cfsr = MAXRTOS_SCB_CFSR;

    MAXRTOS_SCB_CFSR = cfsr & MAXRTOS_CFSR_BFSR_MASK;

    maxrtos_arch_handle_classified_fault(
        MAXRTOS_FAULT_BUS_ERROR );
}

void UsageFault_Handler( void )
{
    uint32_t cfsr;
    maxrtos_fault_type_t fault_type;

    cfsr = MAXRTOS_SCB_CFSR;

    if( ( cfsr & MAXRTOS_CFSR_UFSR_DIVBYZERO_BIT ) != 0U )
    {
        fault_type = MAXRTOS_FAULT_DIVIDE_BY_ZERO;
    }
    else
    {
        fault_type = MAXRTOS_FAULT_ILLEGAL_INSTRUCTION;
    }

    /* CFSR fault-status bits are write-one-to-clear. Clear the
     * UsageFault status bits that were observed before recovery. */
    MAXRTOS_SCB_CFSR = cfsr & MAXRTOS_CFSR_UFSR_MASK;

    maxrtos_arch_handle_classified_fault( fault_type );
}
