.syntax unified
.cpu cortex-m7
.thumb

.global Reset_Handler
.global Default_Handler
.global __StackTop

.extern main
.extern MemManage_Handler
.extern BusFault_Handler
.extern UsageFault_Handler
.extern PendSV_Handler
.extern SysTick_Handler

.section .isr_vector, "a", %progbits
.align 2

.word __StackTop
.word Reset_Handler
.word Default_Handler
.word Default_Handler
.word MemManage_Handler
.word BusFault_Handler
.word UsageFault_Handler
.word 0
.word 0
.word 0
.word 0
.word Default_Handler
.word Default_Handler
.word 0
.word PendSV_Handler
.word SysTick_Handler

.section .text.Reset_Handler
.type Reset_Handler, %function

Reset_Handler:
    /* Load vector table base into VTOR. */
    ldr    r0, =__isr_vector_start
    ldr    r1, =0xE000ED08
    str    r0, [r1]

    dsb
    isb

    /* Zero init .bss. */
    ldr    r0, =_sbss
    ldr    r1, =_ebss
    movs   r2, #0

1:
    cmp    r0, r1
    bcs    2f

    str    r2, [r0]
    adds   r0, r0, #4
    b      1b

2:
    ldr    r0, =_sidata
    ldr    r1, =_sdata
    ldr    r2, =_edata

3:
    cmp    r1, r2
    bcs    4f

    ldr    r3, [r0]
    str    r3, [r1]

    adds   r0, r0, #4
    adds   r1, r1, #4

    b      3b

4:
    bl     main

5:
    b      5b

.size Reset_Handler, .-Reset_Handler

.section .text.Default_Handler
.type Default_Handler, %function

Default_Handler:
6:
    b      6b

.size Default_Handler, .-Default_Handler
