/**
 * @file yield.h
 * @brief Internal declaration of the privileged yield implementation.
 */

#ifndef MAXRTOS_ARCH_CORTEX_M7_INTERNAL_YIELD_H
#define MAXRTOS_ARCH_CORTEX_M7_INTERNAL_YIELD_H

/**
 * @brief Privileged yield implementation.
 *
 * Touches privileged-only kernel state (the current/next PCB
 * globals, ICSR) via the kernel yield evaluation logic. Callable
 * only from svc, which always runs privileged.
 */
void maxrtos_arch_yield( void );

#endif /* MAXRTOS_ARCH_CORTEX_M7_INTERNAL_YIELD_H */
