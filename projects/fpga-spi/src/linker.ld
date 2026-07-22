/* STM32F411RE: 512KB flash, 128KB RAM.
 * RTOS-app layout (matches projects/gameboy-v2): the .device_area section holds
 * the device-model descriptors that device_init_all() walks at boot. The FPGA
 * bitstream is embedded as a C array (src/bitstream.h -> .rodata), so no
 * separate .fpga_bitstream section is needed here. */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

ENTRY(reset_handler)

SECTIONS
{
    .vector_table : { KEEP(*(.vector_table)) } > FLASH

    .text :
    {
        *(.text) *(.text.*)
        *(.rodata) *(.rodata.*)
    } > FLASH

    /* Device descriptors — iterated at boot by device_init_all() */
    .device_area :
    {
        _device_start = .;
        KEEP(*(SORT_BY_NAME(device_area.*)))
        _device_end = .;
    } > FLASH

    _data_flash = LOADADDR(.data);
    .data :
    {
        _data_start = .;
        *(.data) *(.data.*)
        _data_end = .;
    } > RAM AT > FLASH

    .bss :
    {
        _bss_start = .;
        *(.bss) *(.bss.*) *(COMMON)
        _bss_end = .;
    } > RAM

    /* Heap symbols (unused without CONFIG_HEAP, but harmless to define) */
    _heap_start = .;
    _heap_size = _stack_top - 1024 - _heap_start;

    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
