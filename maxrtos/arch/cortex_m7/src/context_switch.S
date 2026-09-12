/**
 * @file context_switch.S
 * @brief Cortex-M7 PendSV context switch.
 *
 * Saves and restores the software-saved process context around the
 * hardware exception frame, configures the incoming process MPU and
 * privilege state, and returns to it using PSP.
 *
 * The PCB's stack_pointer member shall be at offset zero.
 *
 * g_maxrtos_current_pcb may be NULL during the first context switch.
 *
 * @pre
 * g_maxrtos_next_pcb references a valid process control block.
 *
 * @post
 * The incoming process is configured and restored, and
 * g_maxrtos_current_pcb references it before exception return.
 */

    .syntax unified
    .cpu cortex-m7
    .thumb

    .section .text.PendSV_Handler
    .global PendSV_Handler
    .type PendSV_Handler, %function
    .thumb_func

PendSV_Handler:
    /* Preserve EXC_RETURN and keep the stack 8-byte aligned for C calls. */
    push    {r0, lr}

    /* Protect scheduler state and process context during the switch. */
    cpsid   i

    /* Save the outgoing software-saved context. */
    ldr     r0, =g_maxrtos_current_pcb
    ldr     r0, [r0]
    cbz     r0, 1f

    mrs     r1, psp
    stmdb   r1!, {r4-r11}
    str     r1, [r0]              /* pcb->stack_pointer */

1:
    /* Configure the incoming process before restoring its context. */
    ldr     r0, =g_maxrtos_next_pcb
    ldr     r0, [r0]
    bl      maxrtos_arch_mpu_configure_for_next_pcb

    cmp     r0, #0
    beq     2f
    b       maxrtos_mpu_configure_failed

2:
    ldr     r0, =g_maxrtos_next_pcb
    ldr     r0, [r0]
    bl      maxrtos_arch_apply_privilege_for_next_pcb

    ldr     r0, =g_maxrtos_next_pcb
    ldr     r0, [r0]

    /* Restore the incoming software-saved context. */
    ldr     r1, [r0]              /* pcb->stack_pointer */
    ldmia   r1!, {r4-r11}
    msr     psp, r1

    /* Publish the incoming process before returning to Thread mode. */
    ldr     r2, =g_maxrtos_current_pcb
    str     r0, [r2]

    cpsie   i

    /* Return to Thread mode using PSP, preserving other EXC_RETURN bits. */
    pop     {r0, lr}
    orr     lr, lr, #0x0C
    bx      lr

    .size PendSV_Handler, . - PendSV_Handler

/* MPU configuration failure is fatal. */
    .section .text.maxrtos_mpu_configure_failed
    .type maxrtos_mpu_configure_failed, %function
    .thumb_func
maxrtos_mpu_configure_failed:
    bkpt    #0
    b       maxrtos_mpu_configure_failed
    .size maxrtos_mpu_configure_failed, . - maxrtos_mpu_configure_failed
