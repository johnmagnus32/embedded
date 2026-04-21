/* STM32H745 M4 core: flash at 0x08100000 (second bank), RAM at 0x30000000 (128KB) */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08100000, LENGTH = 1024K
    RAM   (rwx) : ORIGIN = 0x30000000, LENGTH = 128K
}
ENTRY(reset_handler)
SECTIONS
{
    .vector_table : { KEEP(*(.vector_table)) } > FLASH
    .text : { *(.text) *(.text.*) *(.rodata) *(.rodata.*) } > FLASH
    _data_flash = LOADADDR(.data);
    .data : { _data_start = .; *(.data) *(.data.*) _data_end = .; } > RAM AT > FLASH
    .bss : { _bss_start = .; *(.bss) *(.bss.*) *(COMMON) _bss_end = .; } > RAM
    _heap_start = .;
    _heap_size = _stack_top - 1024 - _heap_start;
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
