/**
 * @file context_switch.S
 * @brief Cortex-M7 PendSV context-switch handler.
 *
 * Implements the register save and restore operations required to
 * switch between MaxRTOS processes.
 *
 * The processor automatically saves the exception stack frame on
 * exception entry. The handler explicitly saves and restores the
 * callee-saved registers r4-r11.
 *
 * The process control block stack_pointer member shall remain the
 * first structure member. The handler accesses this member at
 * offset zero.
 */

    .syntax unified
    .cpu cortex-m7
    .thumb

    .section .text.PendSV_Handler
    .global PendSV_Handler
    .type PendSV_Handler, %function
    .thumb_func

PendSV_Handler:
    cpsid   i

    /* Save current process context. */
    ldr     r0, =g_maxrtos_current_pcb
    ldr     r0, [r0]
    cbz     r0, 1f

    mrs     r1, psp
    stmdb   r1!, {r4-r11}
    str     r1, [r0]

1:
    /* Restore next process context. */
    ldr     r0, =g_maxrtos_next_pcb
    ldr     r0, [r0]

    ldr     r1, [r0]
    ldmia   r1!, {r4-r11}

    msr     psp, r1

    /* Make the selected process the current process. */
    ldr     r2, =g_maxrtos_current_pcb
    str     r0, [r2]

    cpsie   i

    ldr     lr, =0xFFFFFFFD
    bx      lr

    .size PendSV_Handler, . - PendSV_Handler
