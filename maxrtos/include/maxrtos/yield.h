/**
 * @file yield.h
 * @brief Public application-facing yield interface.
 */

#ifndef MAXRTOS_YIELD_H
#define MAXRTOS_YIELD_H

/**
 * @brief Voluntarily yield the CPU.
 *
 * Requests that the scheduler give another eligible process an
 * opportunity to execute. The call enters the privileged kernel
 * through the architecture-specific system-call mechanism.
 */
void maxrtos_yield( void );

#endif /* MAXRTOS_YIELD_H */
