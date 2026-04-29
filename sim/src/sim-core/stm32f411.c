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
static uint32_t rcc_read(void *opaque, uint32_t offset)  { (void)opaque; (void)offset; return 0; }
static void     rcc_write(void *opaque, uint32_t offset, uint32_t val) { (void)opaque; (void)offset; (void)val; }

void stm32f411_init(struct stm32f411 *soc)
{
    soc->flash = calloc(1, FLASH_SIZE);
    soc->ram   = calloc(1, RAM_SIZE);
    soc->sysclk_hz = 16000000;

    armv7m_cpu_init(&soc->cpu);
    armv7m_nvic_init(&soc->nvic);
    armv7m_systick_init(&soc->systick);

    /* UARTs: USART1 0x40011000, USART2 0x40004400, USART6 0x40011400 */
    stm32_uart_init(&soc->usarts[0], NULL);
    stm32_uart_init(&soc->usarts[1], NULL);
    stm32_uart_init(&soc->usarts[2], NULL);

    /* SPIs: SPI1 0x40013000, SPI2 0x40003800 */
    stm32_spi_init(&soc->spis[0]);
    stm32_spi_init(&soc->spis[1]);

    /* GPIO: GPIOA 0x40020000, GPIOB 0x40020400, GPIOC 0x40020800 */
    stm32_gpio_init(&soc->gpio[0]);
    stm32_gpio_init(&soc->gpio[1]);
    stm32_gpio_init(&soc->gpio[2]);

    /* EXTI */
    stm32_exti_init(&soc->exti);

    /* Wire EXTI outputs 0-4 → NVIC IRQ 6-10 */
    for (int i = 0; i < 5; i++) {
        soc->exti_nvic[i].nvic = &soc->nvic;
        soc->exti_nvic[i].irq = 6 + i;
        soc->exti.irq_out[i].handler = nvic_irq_line_handler;
        soc->exti.irq_out[i].opaque = &soc->exti_nvic[i];
    }

    /* Wire GPIOA idr_change 0-15 → EXTI inputs (default mux) */
    for (int i = 0; i < 16; i++) {
        soc->exti_inputs[i].exti = &soc->exti;
        soc->exti_inputs[i].line = i;
        soc->gpio[0].idr_change[i].handler = stm32_exti_input_handler;
        soc->gpio[0].idr_change[i].opaque = &soc->exti_inputs[i];
    }

    /* Memory bus */
    membus_init(&soc->bus);
    membus_register_ram(&soc->bus, FLASH_BASE, FLASH_SIZE, soc->flash, 1);
    membus_register_ram(&soc->bus, RAM_BASE, RAM_SIZE, soc->ram, 0);

    /* Cortex-M system peripherals */
    membus_register(&soc->bus, 0xE000E010, 0x10, armv7m_systick_read, armv7m_systick_write, &soc->systick);
    membus_register(&soc->bus, 0xE000E100, 0x10, armv7m_nvic_iser_read, armv7m_nvic_iser_write, &soc->nvic);
    membus_register(&soc->bus, 0xE000ED00, 0xA4, armv7m_nvic_scb_read, armv7m_nvic_scb_write, &soc->nvic);

    /* UARTs */
    membus_register(&soc->bus, 0x40011000, 0x20, stm32_uart_read, stm32_uart_write, &soc->usarts[0]);
    membus_register(&soc->bus, 0x40004400, 0x20, stm32_uart_read, stm32_uart_write, &soc->usarts[1]);
    membus_register(&soc->bus, 0x40011400, 0x20, stm32_uart_read, stm32_uart_write, &soc->usarts[2]);

    /* SPIs */
    membus_register(&soc->bus, 0x40013000, 0x24, stm32_spi_read, stm32_spi_write, &soc->spis[0]);
    membus_register(&soc->bus, 0x40003800, 0x24, stm32_spi_read, stm32_spi_write, &soc->spis[1]);

    /* GPIO */
    membus_register(&soc->bus, 0x40020000, 0x0400, stm32_gpio_read, stm32_gpio_write, &soc->gpio[0]);
    membus_register(&soc->bus, 0x40020400, 0x0400, stm32_gpio_read, stm32_gpio_write, &soc->gpio[1]);
    membus_register(&soc->bus, 0x40020800, 0x0400, stm32_gpio_read, stm32_gpio_write, &soc->gpio[2]);

    /* RCC */
    membus_register(&soc->bus, 0x40023800, 0x100, rcc_read, rcc_write, NULL);

    /* EXTI */
    membus_register(&soc->bus, 0x40013C00, 0x18, stm32_exti_read, stm32_exti_write, &soc->exti);
}

void stm32f411_tick(struct stm32f411 *soc)
{
    armv7m_cpu_step(&soc->cpu, &soc->bus);
    armv7m_systick_tick(&soc->systick, &soc->nvic);
    armv7m_nvic_update(&soc->nvic, &soc->cpu, &soc->bus);
}
