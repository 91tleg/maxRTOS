/**
 * @file yield.h
 * @brief Public application-facing yield interface.
 */

#ifndef MAXRTOS_YIELD_H
#define MAXRTOS_YIELD_H

/**
 * @brief Voluntarily yield the CPU.
 *
 * Gives the CPU to a READY process of the same partition that has the same
 * or a higher priority; the caller goes behind its equal-priority peers. This
 * is the round-robin point, the same as maxrtos_timed_wait( 0 ). Processes
 * are not time sliced: without a yield, a wait or a blocking call, a process
 * keeps the CPU until a
 * higher-priority process becomes READY. A READY process of lower priority
 * never takes the CPU this way. If none qualifies the caller keeps running.
 * The call enters the privileged kernel through the architecture-specific
 * system-call mechanism.
 */
void maxrtos_yield( void );

#endif /* MAXRTOS_YIELD_H */
