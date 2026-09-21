/**
 * @file fault_handlers.c
 * @brief ARMv7-M fault exception enable and fault handler
 *        implementation.
 *
 * Fault handlers classify the architecture-specific exception and hand
 * it to maxrtos_arch_handle_fault() (fault_dispatch.c), which asks the
 * kernel for the recovery action and enacts it.
 */

#include <stdint.h>

#include "maxrtos/arch/cortex_m7/fault_handlers.h"

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

void maxrtos_arch_fault_handlers_init( void )
{
    MAXRTOS_SCB_SHCSR |=
        MAXRTOS_SHCSR_MEMFAULTENA_BIT |
        MAXRTOS_SHCSR_BUSFAULTENA_BIT |
        MAXRTOS_SHCSR_USGFAULTENA_BIT;

    MAXRTOS_SCB_CCR |= MAXRTOS_CCR_DIVBYZERO_TRP_BIT;
}

void MemManage_Handler( void )
{
    uint32_t cfsr;

    cfsr = MAXRTOS_SCB_CFSR;

    MAXRTOS_SCB_CFSR = cfsr & MAXRTOS_CFSR_MMFSR_MASK;

    maxrtos_arch_handle_fault(
        MAXRTOS_FAULT_MEMORY_ACCESS );
}

void BusFault_Handler( void )
{
    uint32_t cfsr;

    cfsr = MAXRTOS_SCB_CFSR;

    MAXRTOS_SCB_CFSR = cfsr & MAXRTOS_CFSR_BFSR_MASK;

    maxrtos_arch_handle_fault(
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

    maxrtos_arch_handle_fault( fault_type );
}
