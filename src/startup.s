.syntax unified
.cpu cortex-m4
.thumb

/*
 * Vector table — must be at 0x08000000
 *
 * Cortex-M vector table layout:
 *   Offset 0x00: Initial SP
 *   Offset 0x04: Reset
 *   Offset 0x08: NMI
 *   Offset 0x0C: HardFault
 *   Offset 0x10: MemManage
 *   Offset 0x14: BusFault
 *   Offset 0x18: UsageFault
 *   Offset 0x1C-0x28: Reserved
 *   Offset 0x2C: SVCall
 *   Offset 0x30-0x34: Reserved
 *   Offset 0x38: PendSV        ← context switch happens here
 *   Offset 0x3C: SysTick       ← timer interrupt
 */
.section .vector_table, "a"
.word _stack_top        /* 0x00: Initial stack pointer */
.word reset_handler     /* 0x04: Reset */
.word 0                 /* 0x08: NMI */
.word 0                 /* 0x0C: HardFault */
.word 0                 /* 0x10: MemManage */
.word 0                 /* 0x14: BusFault */
.word 0                 /* 0x18: UsageFault */
.word 0                 /* 0x1C: Reserved */
.word 0                 /* 0x20: Reserved */
.word 0                 /* 0x24: Reserved */
.word 0                 /* 0x28: Reserved */
.word 0                 /* 0x2C: SVCall */
.word 0                 /* 0x30: Reserved */
.word 0                 /* 0x34: Reserved */
.word pendsv_handler    /* 0x38: PendSV — context switch */
.word systick_handler   /* 0x3C: SysTick — timer tick */

.section .text
.global reset_handler
.thumb_func
reset_handler:
    /* 1. Copy .data from flash to RAM */
    ldr r0, =_data_start
    ldr r1, =_data_end
    ldr r2, =_data_flash
copy_data:
    cmp r0, r1
    bge done_data
    ldr r3, [r2], #4
    str r3, [r0], #4
    b copy_data
done_data:

    /* 2. Zero .bss */
    ldr r0, =_bss_start
    ldr r1, =_bss_end
    movs r3, #0
zero_bss:
    cmp r0, r1
    bge done_bss
    str r3, [r0], #4
    b zero_bss
done_bss:

    /* 3. Call main */
    bl main

    /* If main returns, loop forever */
hang:
    b hang
