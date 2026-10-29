/*
 * uint32_t hil_register_soak(uint32_t seed, uint32_t iterations);
 *
 * Loads r1-r12 with seed + n (register rn holds seed + n), spins for
 * `iterations` loops, then returns how many of those registers changed.
 * While it spins the process is preempted by SysTick and PendSV, so r1-r3 and
 * r12 must come back through the hardware exception frame and r4-r11 through
 * the saved context, exactly as they were. The loop keeps its counter in
 * memory and uses r0 as scratch, because every other register is under test.
 *
 * iterations must not be zero. Preserves r4-r11 for the caller.
 */

    .syntax unified
    .cpu cortex-m7
    .thumb

    .global hil_register_soak
    .type hil_register_soak, %function
    .text

    .macro CHECK reg, n
    ldr    r0, [sp, #0]
    add    r0, r0, #\n
    cmp    \reg, r0
    beq    1f
    ldr    r0, [sp, #4]
    add    r0, r0, #1
    str    r0, [sp, #4]
1:
    .endm

hil_register_soak:
    push   {r4-r11, lr}
    sub    sp, sp, #12            @ [sp] seed, [sp+4] mismatches, [sp+8] count
    str    r0, [sp, #0]
    str    r1, [sp, #8]
    movs   r2, #0
    str    r2, [sp, #4]

    add    r1, r0, #1
    add    r2, r0, #2
    add    r3, r0, #3
    add    r4, r0, #4
    add    r5, r0, #5
    add    r6, r0, #6
    add    r7, r0, #7
    add    r8, r0, #8
    add    r9, r0, #9
    add    r10, r0, #10
    add    r11, r0, #11
    add    r12, r0, #12

    /* The loop is 8 bytes. Aligned to 8 it cannot straddle a 32-byte flash
     * line; one that does costs an extra line fetch on every iteration with
     * the caches off, which made a spin ten times slower in one build. */
    .balign 8
spin:
    ldr    r0, [sp, #8]
    subs   r0, r0, #1
    str    r0, [sp, #8]
    bne    spin

    CHECK  r1, 1
    CHECK  r2, 2
    CHECK  r3, 3
    CHECK  r4, 4
    CHECK  r5, 5
    CHECK  r6, 6
    CHECK  r7, 7
    CHECK  r8, 8
    CHECK  r9, 9
    CHECK  r10, 10
    CHECK  r11, 11
    CHECK  r12, 12

    ldr    r0, [sp, #4]
    add    sp, sp, #12
    pop    {r4-r11, pc}

    .size hil_register_soak, . - hil_register_soak
