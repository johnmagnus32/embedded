/* STM32F411RE: 512KB flash, 128KB RAM */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Entry point — CPU jumps here from vector table */
ENTRY(reset_handler)

SECTIONS
{
    /* Vector table must be at the very start of flash */
    .vector_table :
    {
        KEEP(*(.vector_table))
    } > FLASH

    /* Code */
    .text :
    {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
    } > FLASH

    /* Initialized data: stored in flash, copied to RAM at boot */
    _data_flash = LOADADDR(.data);
    .data :
    {
        _data_start = .;
        *(.data)
        *(.data.*)
        _data_end = .;
    } > RAM AT > FLASH

    /* Zero-initialized data */
    .bss :
    {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    } > RAM

    /* Top of RAM = initial stack pointer */
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
