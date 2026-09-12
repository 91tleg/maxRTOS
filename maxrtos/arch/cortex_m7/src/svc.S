/**
 * @file svc.S
 * @brief Cortex-M7 SVC exception entry.
 *
 * Selects the stack containing the exception frame, extracts the SVC
 * immediate, and calls maxrtos_arch_svc_dispatch() with the frame pointer
 * and SVC number. The dispatcher may update stacked r0 before exception
 * return.
 *
 * @pre
 * The caller executed `svc #N`.
 */
    .syntax unified
    .cpu cortex-m7
    .thumb
    .section .text.SVC_Handler
    .global SVC_Handler
    .type SVC_Handler, %function
    .thumb_func

SVC_Handler:
    /* Preserve EXC_RETURN and keep the stack 8-byte aligned for the C call. */
    push    {r4, lr}

    /* EXC_RETURN bit 2: 0 = MSP, 1 = PSP. */
    tst     lr, #0x4
    ite     eq
    mrseq   r0, msp
    mrsne   r0, psp

    /* The stacked PC is at offset 24; SVC is the preceding halfword. */
    ldr     r1, [r0, #24]
    ldrb    r1, [r1, #-2]

    bl      maxrtos_arch_svc_dispatch

    pop     {r4, lr}
    bx      lr

    .size SVC_Handler, . - SVC_Handler
