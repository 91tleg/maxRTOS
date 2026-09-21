/**
 * @file svc.h
 * @brief SVC syscall gate for unprivileged partitions.
 *
 * Unprivileged code uses SVC to enter kernel services that require
 * privileged access. SVC handlers execute in privileged Handler mode.
 */

#ifndef MAXRTOS_ARCH_CORTEX_M7_SVC_H
#define MAXRTOS_ARCH_CORTEX_M7_SVC_H

#include <stdint.h>

/**
 * @brief SVC syscall numbers.
 *
 * MAXRTOS_SVC_COUNT is a sizing sentinel and is not a valid SVC number.
 * Add new SVC numbers before it.
 */
typedef enum
{
    MAXRTOS_SVC_YIELD = 0U,
    MAXRTOS_SVC_QUEUE_SEND,
    MAXRTOS_SVC_QUEUE_RECEIVE,
    MAXRTOS_SVC_QUEUE_COUNT,

    MAXRTOS_SVC_COUNT
} maxrtos_svc_number_t;

/**
 * @brief Dispatch an SVC request.
 *
 * Called from SVC_Handler in privileged Handler mode.
 *
 * @param[in,out] stacked_args
 *     Pointer to the caller's stacked r0-r3. stacked_args[0] may be
 *     updated with the syscall return value.
 *
 * @param[in] svc_number
 *     Immediate operand of the executed SVC instruction.
 */
void maxrtos_arch_svc_dispatch(
    uint32_t * stacked_args,
    uint8_t svc_number );

#endif /* MAXRTOS_ARCH_CORTEX_M7_SVC_H */
