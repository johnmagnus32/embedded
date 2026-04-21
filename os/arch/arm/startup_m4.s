.syntax unified
.cpu cortex-m4
.thumb

/*
 * Vector table — must be at 0x08000000
 *
 * System exceptions (16 entries) + peripheral IRQs.
 * USART2 = IRQ 38 (offset 0x40 + 38*4 = 0xD8)
 */
.section .vector_table, "a"
/* System exceptions (0x00 - 0x3C) */
.word _stack_top        /* 0x00: Initial SP */
.word reset_handler     /* 0x04: Reset */
.word 0                 /* 0x08: NMI */
.word 0                 /* 0x0C: HardFault */
.word memmanage_handler /* 0x10: MemManage ← MPU fault */
.word 0                 /* 0x14: BusFault */
.word 0                 /* 0x18: UsageFault */
.word 0                 /* 0x1C: Reserved */
.word 0                 /* 0x20: Reserved */
.word 0                 /* 0x24: Reserved */
.word 0                 /* 0x28: Reserved */
.word svc_handler       /* 0x2C: SVCall ← syscall entry */
.word 0                 /* 0x30: Reserved */
.word 0                 /* 0x34: Reserved */
.word pendsv_handler    /* 0x38: PendSV */
.word systick_handler   /* 0x3C: SysTick */

/* Peripheral IRQs (0x40+) — IRQ 0 through 38 */
.word 0                 /* IRQ 0:  WWDG */
.word 0                 /* IRQ 1:  EXTI16/PVD */
.word 0                 /* IRQ 2:  EXTI21/TAMP_STAMP */
.word 0                 /* IRQ 3:  EXTI22/RTC_WKUP */
.word 0                 /* IRQ 4:  FLASH */
.word 0                 /* IRQ 5:  RCC */
.word 0                 /* IRQ 6:  EXTI0 */
.word 0                 /* IRQ 7:  EXTI1 */
.word 0                 /* IRQ 8:  EXTI2 */
.word 0                 /* IRQ 9:  EXTI3 */
.word 0                 /* IRQ 10: EXTI4 */
.word 0                 /* IRQ 11: DMA1_Stream0 */
.word 0                 /* IRQ 12: DMA1_Stream1 */
.word 0                 /* IRQ 13: DMA1_Stream2 */
.word 0                 /* IRQ 14: DMA1_Stream3 */
.word 0                 /* IRQ 15: DMA1_Stream4 */
.word 0                 /* IRQ 16: DMA1_Stream5 */
.word 0                 /* IRQ 17: DMA1_Stream6 */
.word 0                 /* IRQ 18: ADC */
.word 0                 /* IRQ 19-22: reserved */
.word 0
.word 0
.word 0
.word 0                 /* IRQ 23: EXTI9_5 */
.word 0                 /* IRQ 24: TIM1_BRK_TIM9 */
.word 0                 /* IRQ 25: TIM1_UP_TIM10 */
.word 0                 /* IRQ 26: TIM1_TRG_COM_TIM11 */
.word 0                 /* IRQ 27: TIM1_CC */
.word 0                 /* IRQ 28: TIM2 */
.word 0                 /* IRQ 29: TIM3 */
.word 0                 /* IRQ 30: TIM4 */
.word 0                 /* IRQ 31: I2C1_EV */
.word 0                 /* IRQ 32: I2C1_ER */
.word 0                 /* IRQ 33: I2C2_EV */
.word 0                 /* IRQ 34: I2C2_ER */
.word 0                 /* IRQ 35: SPI1 */
.word 0                 /* IRQ 36: SPI2 */
.word 0                 /* IRQ 37: USART1 */
.word usart2_isr        /* IRQ 38: USART2 ← our console */

/*
 * Weak default handlers
 */
.section .text
.weak pendsv_handler
.weak systick_handler
.weak usart2_isr
.weak memmanage_handler
.weak svc_handler
.thumb_func
pendsv_handler:
.thumb_func
systick_handler:
.thumb_func
usart2_isr:
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

hang:
    b hang
