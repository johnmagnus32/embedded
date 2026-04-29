/*
 * RP2040 linker script
 *
 * RP2040 has no internal flash — code runs from external QSPI flash
 * mapped at 0x10000000 (XIP = execute in place).
 * SRAM is 264KB at 0x20000000.
 */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x10000000, LENGTH = 2048K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 264K
}

ENTRY(reset_handler)

SECTIONS
{
    .vector_table :
    {
        KEEP(*(.vector_table))
    } > FLASH

    .text :
    {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
    } > FLASH

    _data_flash = LOADADDR(.data);
    .data :
    {
        _data_start = .;
        *(.data)
        *(.data.*)
        _data_end = .;
    } > RAM AT > FLASH

    .bss :
    {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    } > RAM

    _heap_start = .;
    _heap_size = _stack_top - 2048 - _heap_start;  /* 2KB for MSP (2 cores) */

    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
