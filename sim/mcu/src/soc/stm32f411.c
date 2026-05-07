/*
 * stm32f411.c — STM32F411RE SoC model
 *
 * Creates all on-chip peripherals at fixed addresses.
 * No external devices, no chardevs, no DTS.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stm32f411.h"

/* RCC stub — reads return 0, writes ignored */
/* Minimal RCC model — tracks clock enable registers */
static struct {
    uint32_t ahb1enr;  /* offset 0x30 */
    uint32_t apb1enr;  /* offset 0x40 */
    uint32_t apb2enr;  /* offset 0x44 */
} rcc_state;

static uint32_t rcc_read(void *opaque, uint32_t offset)
{
    (void)opaque;
    switch (offset) {
    case 0x30: return rcc_state.ahb1enr;
    case 0x40: return rcc_state.apb1enr;
    case 0x44: return rcc_state.apb2enr;
    default: return 0;
    }
}

static void rcc_write(void *opaque, uint32_t offset, uint32_t val)
{
    (void)opaque;
    switch (offset) {
    case 0x30: rcc_state.ahb1enr |= val; break;
    case 0x40: rcc_state.apb1enr |= val; break;
    case 0x44: rcc_state.apb2enr |= val; break;
    }
}

int rcc_is_clock_enabled(int bus, int bit)
{
    switch (bus) {
    case 0: return (rcc_state.ahb1enr >> bit) & 1;
    case 1: return (rcc_state.apb1enr >> bit) & 1;
    case 2: return (rcc_state.apb2enr >> bit) & 1;
    default: return 0;
    }
}

/* DWT stub — exposes cpu->cycle_count as CYCCNT */
static uint32_t dwt_read(void *opaque, uint32_t offset)
{
    struct armv7m_cpu *cpu = (struct armv7m_cpu *)opaque;
    if (offset == 0x04) return (uint32_t)cpu->cycle_count;
    if (offset == 0x00) return 1; /* CTRL: CYCCNTENA */
    return 0;
}
static void dwt_write(void *opaque, uint32_t offset, uint32_t val)
{ (void)opaque; (void)offset; (void)val; }

/* DEMCR stub */
static uint32_t demcr_val;
static uint32_t demcr_read(void *opaque, uint32_t offset)  { (void)opaque; (void)offset; return demcr_val; }
static void     demcr_write(void *opaque, uint32_t offset, uint32_t val) { (void)opaque; (void)offset; demcr_val = val; }

void stm32f411_init(struct stm32f411 *soc, uint32_t sysclk_hz)
{
    soc->flash = calloc(1, FLASH_SIZE);
    soc->ram   = calloc(1, RAM_SIZE);
    soc->sysclk_hz = sysclk_hz;

    armv7m_cpu_init(&soc->cpu);
    armv7m_nvic_init(&soc->nvic);
    armv7m_systick_init(&soc->systick);

    /* UARTs: USART1 0x40011000, USART2 0x40004400, USART6 0x40011400 */
    stm32_uart_init(&soc->usarts[0], NULL);
    stm32_uart_init(&soc->usarts[1], NULL);
    stm32_uart_init(&soc->usarts[2], NULL);

    /* SPIs: SPI1 0x40013000, SPI2 0x40003800, SPI3 0x40003C00 */
    stm32_spi_init(&soc->spis[0]);
    stm32_spi_init(&soc->spis[1]);
    stm32_spi_init(&soc->spis[2]);

    /* GPIO: GPIOA 0x40020000, GPIOB 0x40020400, GPIOC 0x40020800 */
    stm32_gpio_init(&soc->gpio[0]);
    stm32_gpio_init(&soc->gpio[1]);
    stm32_gpio_init(&soc->gpio[2]);

    /* EXTI */
    stm32_exti_init(&soc->exti);

    /* ADC1 */
    stm32_adc_init(&soc->adc);

    /* DMA controllers */
    stm32_dma_init(&soc->dma1, &soc->nvic, &soc->bus);
    stm32_dma_init(&soc->dma2, &soc->nvic, &soc->bus);

    /* Wire SPI2 (spis[1]) DMA TX → DMA1 Stream 4 */
    soc->spis[1].dma_tx = &soc->dma1.streams[4];

    /* Wire SPI1 (spis[0]) DMA TX → DMA2 Stream 3 */
    soc->spis[0].dma_tx = &soc->dma2.streams[3];

    /* Wire EXTI outputs → NVIC:
     * EXTI0-4: dedicated IRQs 6-10
     * EXTI5-9: shared IRQ 23 (EXTI9_5)
     * EXTI10-15: shared IRQ 40 (EXTI15_10) */
    for (int i = 0; i < 16; i++) {
        soc->exti_nvic[i].nvic = &soc->nvic;
        if (i < 5)       soc->exti_nvic[i].irq = 6 + i;
        else if (i <= 9) soc->exti_nvic[i].irq = 23;
        else             soc->exti_nvic[i].irq = 40;
        soc->exti.irq_out[i].handler = nvic_irq_line_handler;
        soc->exti.irq_out[i].opaque = &soc->exti_nvic[i];
    }

    /* Wire GPIOA idr_change 0-15 → EXTI inputs (default after reset: port A) */
    for (int i = 0; i < 16; i++) {
        soc->exti_inputs[i].exti = &soc->exti;
        soc->exti_inputs[i].line = i;
        soc->gpio[0].idr_change[i].handler = stm32_exti_input_handler;
        soc->gpio[0].idr_change[i].opaque = &soc->exti_inputs[i];
    }

    /* SYSCFG — handles EXTICR writes to remap GPIO→EXTI */
    stm32_syscfg_init(&soc->syscfg, soc->gpio, STM32F411_NUM_GPIO, soc->exti_inputs);

    /* Memory bus */
    membus_init(&soc->bus);
    membus_register_ram(&soc->bus, FLASH_BASE, FLASH_SIZE, soc->flash, 1);
    membus_register_ram(&soc->bus, RAM_BASE, RAM_SIZE, soc->ram, 0);

    /* Cortex-M system peripherals */
    membus_register(&soc->bus, 0xE000E010, 0x10, armv7m_systick_read, armv7m_systick_write, soc);
    membus_register(&soc->bus, 0xE000E100, 0x10, armv7m_nvic_iser_read, armv7m_nvic_iser_write, &soc->nvic);
    membus_register(&soc->bus, 0xE000ED00, 0xA4, armv7m_nvic_scb_read, armv7m_nvic_scb_write, &soc->nvic);

    /* DWT (Data Watchpoint and Trace) — cycle counter */
    membus_register(&soc->bus, 0xE0001000, 0x100, dwt_read, dwt_write, &soc->cpu);

    /* DEMCR (Debug Exception and Monitor Control Register) */
    membus_register(&soc->bus, 0xE000EDF0, 0x10, demcr_read, demcr_write, NULL);

    /* UARTs */
    membus_register(&soc->bus, 0x40011000, 0x20, stm32_uart_read, stm32_uart_write, &soc->usarts[0]);
    membus_register(&soc->bus, 0x40004400, 0x20, stm32_uart_read, stm32_uart_write, &soc->usarts[1]);
    membus_register(&soc->bus, 0x40011400, 0x20, stm32_uart_read, stm32_uart_write, &soc->usarts[2]);

    /* SPIs */
    membus_register(&soc->bus, 0x40013000, 0x24, stm32_spi_read, stm32_spi_write, &soc->spis[0]);
    membus_register(&soc->bus, 0x40003800, 0x24, stm32_spi_read, stm32_spi_write, &soc->spis[1]);
    membus_register(&soc->bus, 0x40003C00, 0x24, stm32_spi_read, stm32_spi_write, &soc->spis[2]);

    /* GPIO */
    membus_register(&soc->bus, 0x40020000, 0x0400, stm32_gpio_read, stm32_gpio_write, &soc->gpio[0]);
    membus_register(&soc->bus, 0x40020400, 0x0400, stm32_gpio_read, stm32_gpio_write, &soc->gpio[1]);
    membus_register(&soc->bus, 0x40020800, 0x0400, stm32_gpio_read, stm32_gpio_write, &soc->gpio[2]);

    /* RCC */
    membus_register(&soc->bus, 0x40023800, 0x100, rcc_read, rcc_write, NULL);

    /* EXTI */
    membus_register(&soc->bus, 0x40013C00, 0x18, stm32_exti_read, stm32_exti_write, &soc->exti);

    /* SYSCFG */
    membus_register(&soc->bus, 0x40013800, 0x18, stm32_syscfg_read, stm32_syscfg_write, &soc->syscfg);

    /* ADC1 */
    membus_register(&soc->bus, 0x40012000, 0x400, stm32_adc_read, stm32_adc_write, &soc->adc);

    /* DMA1 0x40026000, DMA2 0x40026400 */
    membus_register(&soc->bus, 0x40026000, 0x400, stm32_dma_read, stm32_dma_write, &soc->dma1);
    membus_register(&soc->bus, 0x40026400, 0x400, stm32_dma_read, stm32_dma_write, &soc->dma2);

    /* Event queue */
    event_queue_init(&soc->eq);

    /* Wire event queue and cycle count pointers to SPIs */
    soc->spis[0].eq = &soc->eq;
    soc->spis[0].cycle_ptr = &soc->cpu.cycle_count;
    soc->spis[0].spi_evt_id = EVT_SPI0_TXE;
    soc->spis[1].eq = &soc->eq;
    soc->spis[1].cycle_ptr = &soc->cpu.cycle_count;
    soc->spis[1].spi_evt_id = EVT_SPI1_TXE;
    soc->spis[2].eq = &soc->eq;
    soc->spis[2].cycle_ptr = &soc->cpu.cycle_count;
    soc->spis[2].spi_evt_id = EVT_SPI2_TXE;
}

/* ---- Event callbacks ---- */

void systick_event_cb(void *opaque)
{
    struct stm32f411 *soc = (struct stm32f411 *)opaque;
    if (soc->systick.csr & 2)
        armv7m_nvic_set_pending(&soc->nvic, 15 /* IRQ_VEC_SYSTICK */);
    /* Reschedule */
    if ((soc->systick.csr & 1) && soc->systick.rvr > 0)
        event_schedule(&soc->eq, EVT_SYSTICK,
                       soc->cpu.cycle_count + soc->systick.rvr,
                       systick_event_cb, soc);
}

int stm32f411_tick(struct stm32f411 *soc)
{
    /* CPU_OK (0) normally, or CPU_SEMIHOST_EXIT | code on BKPT #0xAB */
    int r = armv7m_cpu_step(&soc->cpu, &soc->bus);

    /* One comparison replaces all per-device checks */
    if (__builtin_expect(soc->cpu.cycle_count >= soc->eq.next_event, 0))
        event_process(&soc->eq, soc->cpu.cycle_count);

    /* DMA still uses per-tick (request_pending driven by SPI events) */
    stm32_dma_tick(&soc->dma1);
    stm32_dma_tick(&soc->dma2);

    if (__builtin_expect(soc->nvic.needs_update, 0))
        armv7m_nvic_update(&soc->nvic, &soc->cpu, &soc->bus);
    return r;
}
