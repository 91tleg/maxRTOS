/**
 * @file context_switch.S
 * @brief Cortex-M7 PendSV context-switch implementation.
 *
 * @details
 * Performs the processor-specific context save, MPU update, and
 * context restore required to switch between MaxRTOS processes.
 *
 * On exception entry, the processor automatically saves the hardware
 * exception frame. This handler additionally saves and restores
 * registers r4-r11, which comprise the software-saved process context.
 *
 * The process control block shall store stack_pointer as its first
 * member. This handler accesses that member at offset zero.
 *
 * The current process pointer may be NULL during the initial context
 * switch. In that case, no outgoing process context is saved.
 *
 * The MPU shall be configured for the incoming process before its
 * context is restored and execution resumes. This prevents the
 * incoming process from executing with the outgoing process's MPU
 * configuration.
 *
 * The EXC_RETURN value supplied by the processor is preserved across
 * the handler. Bits 3 and 2 are set before exception return to select
 * Thread mode and PSP. Other EXC_RETURN attributes are preserved.
 *
 * The handler maintains the stack alignment required by the ARM
 * procedure call standard before calling the C MPU configuration
 * function.
 *
 * @pre
 * g_maxrtos_next_pcb shall reference a valid process control block
 * when a context switch is requested.
 *
 * @post
 * The MPU is configured for the incoming process, PSP references its
 * restored exception frame, and g_maxrtos_current_pcb references the
 * incoming process before exception return.
 */

    .syntax unified
    .cpu cortex-m7
    .thumb

    .section .text.PendSV_Handler
    .global PendSV_Handler
    .type PendSV_Handler, %function
    .thumb_func

PendSV_Handler:
    /* Preserve the entry EXC_RETURN value and maintain 8-byte stack
     * alignment for the C function call below. */
    push    {r0, lr}

    /* Prevent a concurrent interrupt from observing partially updated
     * process context or scheduler state. */
    cpsid   i

    /* Save the outgoing process's software-saved registers. */
    ldr     r0, =g_maxrtos_current_pcb
    ldr     r0, [r0]              /* r0 = current PCB */
    cbz     r0, 1f                /* No outgoing process during first switch. */

    mrs     r1, psp               /* r1 = outgoing process PSP */
    stmdb   r1!, {r4-r11}         /* Save software-saved registers. */
    str     r1, [r0]              /* pcb->stack_pointer = saved context. */

1:
    /* Configure the incoming process's MPU before restoring its
     * context and allowing it to execute. */
    ldr     r0, =g_maxrtos_next_pcb
    ldr     r0, [r0]              /* r0 = incoming PCB */
    bl      maxrtos_arch_mpu_configure_for_next_pcb

    /* r0-r3 and r12 are caller-saved by the function call above.
     * Reload the incoming PCB rather than relying on r0. */
    ldr     r0, =g_maxrtos_next_pcb
    ldr     r0, [r0]

    /* Restore the incoming process's software-saved registers. */
    ldr     r1, [r0]              /* r1 = incoming saved stack pointer */
    ldmia   r1!, {r4-r11}         /* Restore software-saved registers. */
    msr     psp, r1               /* PSP now references the hardware frame. */

    /* Publish the incoming process as the current process before
     * returning to Thread mode. */
    ldr     r2, =g_maxrtos_current_pcb
    str     r0, [r2]

    /* Re-enable interrupts before exception return. */
    cpsie   i

    /* Restore EXC_RETURN. Select Thread mode and PSP while preserving
     * all other processor-supplied EXC_RETURN attributes. */
    pop     {r0, lr}
    orr     lr, lr, #0x0C
    bx      lr

    .size PendSV_Handler, . - PendSV_Handler
