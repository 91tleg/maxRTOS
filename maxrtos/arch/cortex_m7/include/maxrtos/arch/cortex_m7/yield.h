/**
 * @file yield.h
 * @brief Cortex-M7 public/partition-facing voluntary yield interface.
 */

#ifndef MAXRTOS_ARCH_CORTEX_M7_YIELD_H
#define MAXRTOS_ARCH_CORTEX_M7_YIELD_H

/**
 * @brief Voluntarily yield the CPU.
 *
 * The only yield entry point partition code should call. Issues
 * `svc #MAXRTOS_SVC_YIELD`, which is serviced in Handler mode.
 */
void maxrtos_yield( void );

#endif /* MAXRTOS_ARCH_CORTEX_M7_YIELD_H */
