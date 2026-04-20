/*
 * uart.c — STM32 USART driver
 *
 * All hardware addresses come from the uart_config struct, which is
 * populated from devicetree.h defines. The driver itself has no
 * hardcoded addresses — it works for any STM32 USART instance.
 *
 * Compare to Zephyr's drivers/serial/uart_stm32.c which does the
 * same thing but gets config from DEVICE_DT_DEFINE().
 */

#include "uart.h"
#include <stdint.h>

/* Register offsets within a USART peripheral (from reference manual) */
#define USART_SR   0x00
#define USART_DR   0x04
#define USART_BRR  0x08
#define USART_CR1  0x0C

/* Register offsets for RCC */
#define RCC_BASE       0x40023800
#define RCC_AHB1ENR    (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB1ENR    (*(volatile uint32_t *)(RCC_BASE + 0x40))

/* GPIO register offsets */
#define GPIO_MODER  0x00
#define GPIO_AFRL   0x20

/* Helper to access a register at base + offset */
#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

void uart_init(const struct uart_config *cfg)
{
    /* 1. Enable clocks */
    RCC_AHB1ENR |= (1 << cfg->gpio_clk_bit);   /* GPIO port clock */
    RCC_APB1ENR |= (1 << cfg->uart_clk_bit);    /* USART clock */

    /* 2. Configure TX pin as alternate function */
    volatile uint32_t *moder = (volatile uint32_t *)(cfg->gpio_base + GPIO_MODER);
    volatile uint32_t *afrl  = (volatile uint32_t *)(cfg->gpio_base + GPIO_AFRL);

    *moder &= ~(3U << (cfg->tx_pin * 2));       /* clear mode bits */
    *moder |=  (2U << (cfg->tx_pin * 2));        /* AF mode (0b10) */
    *afrl  &= ~(0xFU << (cfg->tx_pin * 4));      /* clear AF bits */
    *afrl  |=  (cfg->tx_af << (cfg->tx_pin * 4)); /* set AF number */

    /* 3. Set baud rate: BRR = fclk / baudrate */
    REG(cfg->base, USART_BRR) = (cfg->clk_hz + cfg->baudrate / 2) / cfg->baudrate;

    /* 4. Enable USART: UE (bit 13) + TE (bit 3) */
    REG(cfg->base, USART_CR1) = (1 << 13) | (1 << 3);
}

void uart_putc(const struct uart_config *cfg, char c)
{
    while (!(REG(cfg->base, USART_SR) & (1 << 7)))
        ;  /* wait for TXE */
    REG(cfg->base, USART_DR) = c;
}

void uart_puts(const struct uart_config *cfg, const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_putc(cfg, '\r');
        uart_putc(cfg, *s++);
    }
}
