ENTRY(Reset_Handler)

/* Top of the main stack in D1 SRAM */
__StackTop = ORIGIN(RAM_D1) + LENGTH(RAM_D1);

_Min_Heap_Size = 0x200;
_Min_Stack_Size = 0x400;

MEMORY
{
    FLASH  (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
    RAM_D1 (xrw) : ORIGIN = 0x24000000, LENGTH = 512K
}

/* Output sections */
SECTIONS
{
    /* Interrupt vector table at the beginning of FLASH */
    .isr_vector :
    {
        . = ALIGN(4);
        __isr_vector_start = .;
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    /* Load address of initialized data in FLASH.
     * startup.s copies this data into RAM_D1. */
    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >RAM_D1 AT>FLASH

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;

        *(.bss)
        *(.bss*)
        *(COMMON)

        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >RAM_D1

    /* Reserve minimum heap and stack space. */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);

        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;

        . = ALIGN(8);
    } >RAM_D1

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
