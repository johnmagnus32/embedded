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

    cpu_init(&soc->cpu);
    nvic_init(&soc->nvic);
    systick_init(&soc->systick);

    /* UARTs: USART1 0x40011000, USART2 0x40004400, USART6 0x40011400 */
    uart_init(&soc->usarts[0], NULL);
    uart_init(&soc->usarts[1], NULL);
    uart_init(&soc->usarts[2], NULL);

    /* SPIs: SPI1 0x40013000, SPI2 0x40003800 */
    spi_init(&soc->spis[0]);
    spi_init(&soc->spis[1]);

    /* GPIO: GPIOA 0x40020000, GPIOB 0x40020400, GPIOC 0x40020800 */
    gpio_init(&soc->gpio[0]);
    gpio_init(&soc->gpio[1]);
    gpio_init(&soc->gpio[2]);

    /* Memory bus */
    membus_init(&soc->bus);
    membus_register_ram(&soc->bus, FLASH_BASE, FLASH_SIZE, soc->flash, 1);
    membus_register_ram(&soc->bus, RAM_BASE, RAM_SIZE, soc->ram, 0);

    /* Cortex-M system peripherals */
    membus_register(&soc->bus, 0xE000E010, 0x10, systick_read, systick_write, &soc->systick);
    membus_register(&soc->bus, 0xE000E100, 0x10, nvic_iser_read, nvic_iser_write, &soc->nvic);
    membus_register(&soc->bus, 0xE000ED00, 0xA4, nvic_scb_read, nvic_scb_write, &soc->nvic);

    /* UARTs */
    membus_register(&soc->bus, 0x40011000, 0x20, uart_read, uart_write, &soc->usarts[0]);
    membus_register(&soc->bus, 0x40004400, 0x20, uart_read, uart_write, &soc->usarts[1]);
    membus_register(&soc->bus, 0x40011400, 0x20, uart_read, uart_write, &soc->usarts[2]);

    /* SPIs */
    membus_register(&soc->bus, 0x40013000, 0x24, spi_read, spi_write, &soc->spis[0]);
    membus_register(&soc->bus, 0x40003800, 0x24, spi_read, spi_write, &soc->spis[1]);

    /* GPIO */
    membus_register(&soc->bus, 0x40020000, 0x0400, gpio_read, gpio_write, &soc->gpio[0]);
    membus_register(&soc->bus, 0x40020400, 0x0400, gpio_read, gpio_write, &soc->gpio[1]);
    membus_register(&soc->bus, 0x40020800, 0x0400, gpio_read, gpio_write, &soc->gpio[2]);

    /* RCC */
    membus_register(&soc->bus, 0x40023800, 0x100, rcc_read, rcc_write, NULL);
}

void stm32f411_tick(struct stm32f411 *soc)
{
    cpu_step(&soc->cpu, &soc->bus);
    systick_tick(&soc->systick, &soc->nvic);
    nvic_update(&soc->nvic, &soc->cpu, &soc->bus);
}
