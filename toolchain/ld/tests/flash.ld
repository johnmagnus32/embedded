/* STM32-style flash-to-RAM layout: .text runs from FLASH; .data runs in RAM but its init image is stored
 * in FLASH (AT>), for startup to copy; .bss is RAM NOBITS. Exercises MEMORY / >region / AT> / LOADADDR. */
ENTRY(_start)
MEMORY {
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}
SECTIONS {
  .text : { *(.text*) *(.rodata*) }  > FLASH
  .data : { *(.data*) }              > RAM AT > FLASH
  _data_lma = LOADADDR(.data);
  __bss_start = .;
  .bss  : { *(.bss*) *(COMMON) }     > RAM
  __bss_end = .;
  _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
