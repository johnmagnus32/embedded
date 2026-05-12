.syntax unified
.cpu cortex-m4
.thumb

/*
 * Vector table — must be at 0x08000000
 *
 * System exceptions (16 entries) use direct handlers.
 * All 82 peripheral IRQs use _isr_wrapper (two-level dispatch).
 * The software ISR table (_sw_isr_table) maps IRQ numbers to
 * handler+arg pairs — generated from the device tree.
 */
.section .vector_table, "a"
/* System exceptions (0x00 - 0x3C) — direct handlers */
.word _stack_top        /* 0x00: Initial SP */
.word reset_handler     /* 0x04: Reset */
.word 0                 /* 0x08: NMI */
.word 0                 /* 0x0C: HardFault */
.word memmanage_handler /* 0x10: MemManage */
.word 0                 /* 0x14: BusFault */
.word 0                 /* 0x18: UsageFault */
.word 0, 0, 0, 0       /* 0x1C-0x28: Reserved */
.word svc_handler       /* 0x2C: SVCall */
.word 0, 0             /* 0x30-0x34: Reserved */
.word pendsv_handler    /* 0x38: PendSV */
.word systick_handler   /* 0x3C: SysTick */

/* Peripheral IRQs (0x40+) — all go through two-level dispatch */
.rept 82
.word _isr_wrapper
.endr

/*
 * Weak default handlers for system exceptions
 */
.section .text
.weak pendsv_handler
.weak systick_handler
.weak memmanage_handler
.weak svc_handler
.thumb_func
pendsv_handler:
.thumb_func
systick_handler:
.thumb_func
memmanage_handler:
.thumb_func
svc_handler:
    bx lr

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
    b .
