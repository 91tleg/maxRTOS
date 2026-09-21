/**
 * @file port.h
 * @brief Hardware access points of the Cortex-M7 architecture layer.
 *
 * Everything in the architecture layer that touches the CPU or memory
 * mapped registers directly is reached through the macros below, so that
 * the otherwise portable logic (SVC dispatch, MPU programming, fault
 * recovery enactment, scheduler start-up) can be compiled and run
 * unchanged against a simulated target.
 *
 * On the target the defaults apply. A host build (see tests/sil) predefines
 * any of these macros, using a force-included header, to redirect them to a
 * virtual target. Defaults must never be changed to suit a simulator.
 */

#ifndef MAXRTOS_ARCH_CORTEX_M7_PORT_H
#define MAXRTOS_ARCH_CORTEX_M7_PORT_H

#include <stdint.h>

/** Stop execution: interrupts off, then break forever. Does not return. */
#ifndef MAXRTOS_PORT_HALT
#define MAXRTOS_PORT_HALT()                                    \
    do                                                         \
    {                                                          \
        __asm volatile ( "cpsid i" );                          \
        for( ;; )                                              \
        {                                                      \
            __asm volatile ( "bkpt #0" );                      \
        }                                                      \
    } while( 0 )
#endif

/** Wait for interrupt. */
#ifndef MAXRTOS_PORT_WFI
#define MAXRTOS_PORT_WFI() __asm volatile ( "wfi" )
#endif

/** Data and instruction synchronization barrier. */
#ifndef MAXRTOS_PORT_BARRIER
#define MAXRTOS_PORT_BARRIER()                                 \
    do                                                         \
    {                                                          \
        __asm volatile ( "dsb" );                              \
        __asm volatile ( "isb" );                              \
    } while( 0 )
#endif

/**
 * Convert an address a process passed through a system call (a 32-bit
 * value taken from its stacked registers) into a pointer the kernel can
 * dereference. On a 32-bit target this is the identity. The address must
 * already have been validated against the caller's partition.
 */
#ifndef MAXRTOS_PORT_UADDR_TO_PTR
#define MAXRTOS_PORT_UADDR_TO_PTR( addr_ ) \
    ( ( void * ) ( uintptr_t ) ( addr_ ) )
#endif

/* MPU registers. The default is direct register access. */
#ifndef MAXRTOS_PORT_MPU_REG_WRITE
#define MAXRTOS_PORT_MPU_CTRL ( *( volatile uint32_t * ) 0xE000ED94UL )
#define MAXRTOS_PORT_MPU_RNR  ( *( volatile uint32_t * ) 0xE000ED98UL )
#define MAXRTOS_PORT_MPU_RBAR ( *( volatile uint32_t * ) 0xE000ED9CUL )
#define MAXRTOS_PORT_MPU_RASR ( *( volatile uint32_t * ) 0xE000EDA0UL )
#define MAXRTOS_PORT_MPU_REG_WRITE( reg_, value_ ) \
    ( ( reg_ ) = ( value_ ) )
#endif

#endif /* MAXRTOS_ARCH_CORTEX_M7_PORT_H */
