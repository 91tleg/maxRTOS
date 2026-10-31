/**
 * @file context_switch.S
 * @brief Cortex-M7 PendSV context switch.
 *
 * Saves and restores the software-saved process context around the
 * hardware exception frame, prepares the incoming process (MPU, privilege
 * and pending blocking-call result) with one call to
 * maxrtos_arch_prepare_next_pcb(), and returns to it using PSP.
 *
 * Every process may use the FPU, so every process has the same saved context,
 * from its stack_pointer upwards (see context_switch.c):
 *
 *   s16-s31
 *   r4-r11
 *   extended exception frame: r0-r3, r12, lr, pc, xPSR, s0-s15, FPSCR, pad
 *
 * The hardware stacks the exception frame (s0-s15 and FPSCR lazily, at the
 * first FPU instruction after the exception begins, which on the usual path
 * is the save of s16-s31 here); this handler saves and restores the rest. A new or restarted
 * process has a context of zeros, so it starts from cleared registers and
 * FPSCR, and a context switch cannot carry one process's FPU values to
 * another: the whole FPU state is replaced each time.
 *
 * The handler writes the outgoing context with privilege, outside the MPU.
 * Before it does, it checks that the process's stack pointer leaves the
 * context inside the stack the process was given (stack_base, stack_size),
 * and that the hardware stacked an extended frame. A process that moved its
 * stack pointer elsewhere would otherwise make this handler overwrite another
 * process's, or another partition's, memory. Such a process is handled as
 * having faulted (maxrtos_arch_context_invalid()) and its context is not
 * saved.
 *
 * The PCB's stack_pointer, stack_base and stack_size members shall be at
 * offsets 0, 4 and 8.
 *
 * g_maxrtos_current_pcb may be NULL during the first context switch, or when
 * a faulting process's context was discarded.
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
    .fpu fpv5-d16
    .thumb

#define FPCCR 0xE000EF34

/* The context the handler writes below the hardware frame: r4-r11 and
 * s16-s31 (24 words), and the frame the hardware stacked: an extended one,
 * 26 words. The word of alignment padding the hardware may add lies between
 * the frame and the stack pointer the process had, not above the frame, so a
 * process that has used none of its stack, with its stack pointer at the top,
 * leaves the frame ending exactly at the top. */
#define SOFT_CONTEXT_BYTES 96
#define HARD_FRAME_BYTES   104

    .section .text.PendSV_Handler
    .global PendSV_Handler
    .type PendSV_Handler, %function
    .thumb_func

PendSV_Handler:
    /* Preserve EXC_RETURN and keep the stack 8-byte aligned for C calls. */
    push    {r0, lr}

    /* Protect scheduler state and process context during the switch. */
    cpsid   i

    ldr     r0, =g_maxrtos_current_pcb
    ldr     r0, [r0]
    cmp     r0, #0
    beq.w   1f

    /* The process must have been running in Thread mode on PSP with an
     * extended frame stacked (EXC_RETURN 0xFFFFFFED); every process runs with
     * FPCA set. Anything else, such as a privileged process that gave up the
     * FPU state or moved to MSP, has no PSP frame this handler can save. */
    mvn     r2, #0x12
    cmp     lr, r2
    bne.w   7f

    /* The process's stack pointer must leave room for the context below the
     * frame and the frame above it, inside its own stack. The comparisons
     * are unsigned, and the upper one is made first so that the addition
     * cannot wrap. */
    mrs     r1, psp
    ldr     r2, [r0, #4]          /* pcb->stack_base */
    ldr     r3, [r0, #8]          /* pcb->stack_size */
    add     r3, r2, r3            /* top of the stack */
    sub     r3, r3, #HARD_FRAME_BYTES
    cmp     r1, r3
    bhi.w   7f
    add     r2, r2, #SOFT_CONTEXT_BYTES
    cmp     r1, r2
    blo.w   7f

    /* Save the outgoing software-saved context. The vstmdb is the first FPU
     * instruction since the exception began: it is what makes the hardware
     * stack s0-s15 and FPSCR into the frame. */
    stmdb   r1!, {r4-r11}
    vstmdb  r1!, {s16-s31}
    str     r1, [r0]              /* pcb->stack_pointer */
    b       4f

7:
    /* The context cannot be saved. Handle the process as faulted; this
     * discards its context and picks the next process. */
    bl      maxrtos_arch_context_invalid

1:
    /* No context to save: the first switch, or a context that was discarded.
     * A discarded process may still owe the lazy stacking of s0-s15 into a
     * frame that is no longer there (FPCCR.LSPACT), and the first FPU
     * instruction below would write it into the stack the replacement context
     * is built on. Cancel it. */
    ldr     r2, [sp, #4]          /* the EXC_RETURN pushed on entry */
    tst     r2, #0x10
    bne     4f

    ldr     r1, =FPCCR
    ldr     r2, [r1]
    bic     r2, r2, #1
    str     r2, [r1]
    dsb
    isb

4:
    /* Configure the incoming process before restoring its context: MPU,
     * privilege, and the result of a completed blocking call. r4 is free
     * here (the outgoing r4-r11 are saved, the incoming ones not yet
     * restored) and survives the call, so the process is loaded once. */
    ldr     r4, =g_maxrtos_next_pcb
    ldr     r4, [r4]
    mov     r0, r4
    bl      maxrtos_arch_prepare_next_pcb

    cmp     r0, #0
    beq     2f
    b       maxrtos_mpu_configure_failed

2:
    mov     r0, r4

    /* Restore the incoming software-saved context. */
    ldr     r1, [r0]              /* pcb->stack_pointer */
    vldmia  r1!, {s16-s31}
    ldmia   r1!, {r4-r11}
    msr     psp, r1

    /* Publish the incoming process before returning to Thread mode. */
    ldr     r3, =g_maxrtos_current_pcb
    str     r0, [r3]

    cpsie   i

    /* Return to Thread mode using PSP with the extended frame: EXC_RETURN
     * bit 4 clear, the other bits as the exception was entered. */
    pop     {r0, lr}
    orr     lr, lr, #0x0C
    bic     lr, lr, #0x10
    bx      lr

    .size PendSV_Handler, . - PendSV_Handler

/* MPU configuration failure is fatal. Call the hook directly: a BKPT only
 * reaches the HardFault handler (and so the hook) when no debug monitor is
 * enabled. Handler mode is privileged, so the hook may run here. */
    .section .text.maxrtos_mpu_configure_failed
    .type maxrtos_mpu_configure_failed, %function
    .thumb_func
maxrtos_mpu_configure_failed:
    bl      maxrtos_arch_fatal
    b       maxrtos_mpu_configure_failed
    .size maxrtos_mpu_configure_failed, . - maxrtos_mpu_configure_failed
