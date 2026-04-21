/*
 * startup_m0plus.s — Cortex-M0+ startup (RP2040)
 *
 * Differences from Cortex-M4 startup:
 *   - M0+ only supports Thumb (16-bit) instructions
 *   - No stmdb/ldmia with arbitrary register lists
 *   - No BASEPRI register (only PRIMASK for interrupt masking)
 *   - Simpler vector table (same layout, fewer optional entries)
 *
 * RP2040 boot: the boot ROM loads the first 256 bytes from
 * external flash into SRAM, then jumps to the vector table.
 */

.syntax unified
.cpu cortex-m0plus
.thumb

.section .vector_table, "a"
.word _stack_top
.word reset_handler
.word 0                 /* NMI */
.word 0                 /* HardFault */
.word 0, 0, 0, 0       /* reserved */
.word 0, 0, 0           /* reserved */
.word 0                 /* SVCall */
.word 0, 0              /* reserved */
.word pendsv_handler    /* PendSV */
.word systick_handler   /* SysTick */
/* IRQs 0-25 (RP2040 has 26 peripheral IRQs) */
.word 0, 0, 0, 0, 0, 0, 0, 0   /* IRQ 0-7 */
.word 0, 0, 0, 0, 0, 0, 0, 0   /* IRQ 8-15 */
.word 0, 0, 0, 0                /* IRQ 16-19 */
.word uart0_isr                 /* IRQ 20: UART0 */
.word 0, 0, 0, 0, 0            /* IRQ 21-25 */

/* Weak defaults */
.section .text
.weak pendsv_handler
.weak systick_handler
.weak uart0_isr
.thumb_func
pendsv_handler:
.thumb_func
systick_handler:
.thumb_func
uart0_isr:
    bx lr

.global reset_handler
.thumb_func
reset_handler:
    /* Copy .data */
    ldr r0, =_data_start
    ldr r1, =_data_end
    ldr r2, =_data_flash
copy_data:
    cmp r0, r1
    bge done_data
    ldm r2!, {r3}
    stm r0!, {r3}
    b copy_data
done_data:

    /* Zero .bss */
    ldr r0, =_bss_start
    ldr r1, =_bss_end
    movs r3, #0
zero_bss:
    cmp r0, r1
    bge done_bss
    stm r0!, {r3}
    b zero_bss
done_bss:

    bl main

hang:
    b hang
