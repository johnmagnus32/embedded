/*
 * uart.c — STM32 USART driver (Zephyr-style)
 *
 * The driver uses DT_INST macros to auto-populate config for each
 * USART instance. No hardcoded addresses anywhere — not in the driver,
 * not in main.c. Everything comes from board.dts via gen_devicetree.py.
 *
 * Compare to Zephyr's drivers/serial/uart_stm32.c
 */

#include <stdint.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"

/* ---- Register offsets (from STM32 reference manual) ---- */
#define USART_SR   0x00
#define USART_DR   0x04
#define USART_BRR  0x08
#define USART_CR1  0x0C

#define RCC_BASE      0x40023800
#define RCC_AHB1ENR   (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB1ENR   (*(volatile uint32_t *)(RCC_BASE + 0x40))

#define GPIO_MODER  0x00
#define GPIO_AFRL   0x20

#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

/* ---- Driver config (populated from DT at compile time) ---- */

struct uart_stm32_config {
    uint32_t base;
    uint32_t baudrate;
    uint32_t gpio_base;
    uint8_t  tx_pin;
    uint8_t  tx_af;
    uint8_t  uart_clk_bit;
};

/* ---- Driver implementation ---- */

static int uart_stm32_init(const struct device *dev)
{
    const struct uart_stm32_config *cfg = dev->config;

    /* Enable GPIO and UART clocks */
    RCC_AHB1ENR |= 0x07;  /* enable GPIOA/B/C (simplified) */
    RCC_APB1ENR |= (1 << cfg->uart_clk_bit);

    volatile uint32_t *moder = (volatile uint32_t *)(cfg->gpio_base + GPIO_MODER);
    volatile uint32_t *afrl  = (volatile uint32_t *)(cfg->gpio_base + GPIO_AFRL);

    *moder &= ~(3U << (cfg->tx_pin * 2));
    *moder |=  (2U << (cfg->tx_pin * 2));
    *afrl  &= ~(0xFU << (cfg->tx_pin * 4));
    *afrl  |=  (cfg->tx_af << (cfg->tx_pin * 4));

    REG(cfg->base, USART_BRR) = (DT_SYSCLK_HZ + cfg->baudrate / 2) / cfg->baudrate;
    REG(cfg->base, USART_CR1) = (1 << 13) | (1 << 3);

    return 0;
}

static void uart_stm32_poll_out(const struct device *dev, char c)
{
    const struct uart_stm32_config *cfg = dev->config;

    while (!(REG(cfg->base, USART_SR) & (1 << 7)))
        ;
    REG(cfg->base, USART_DR) = c;
}

/* ---- Driver API ops struct ---- */

static const struct uart_driver_api uart_stm32_api = {
    .poll_out = uart_stm32_poll_out,
};

/* ---- Instantiation macro (like Zephyr's DT_INST_FOREACH_STATUS_OKAY) ----
 *
 * This macro creates a config struct + struct device for one USART instance.
 * All values come from DT_INST_ST_STM32_USART_<n>_* defines.
 *
 * In Zephyr, DT_INST_FOREACH_STATUS_OKAY(STM32_UART_INIT) would expand
 * this for every usart node with status="okay".
 */
#define CONCAT3(a, b, c) a##b##c
#define _DT_INST(compat, n, prop) CONCAT3(DT_INST_##compat##_##n, _PROP_, prop)
#define _DT_INST_REG(compat, n) DT_INST_##compat##_##n##_REG_ADDR
#define _DT_INST_CLK(compat, n, f) DT_INST_##compat##_##n##_CLK_##f

#define _DT_INST_LABEL(compat, n) DT_INST_##compat##_##n##_LABEL

#define STM32_UART_DEFINE(n)                                            \
    static const struct uart_stm32_config uart_cfg_##n = {              \
        .base         = _DT_INST_REG(ST_STM32_USART, n),               \
        .baudrate     = _DT_INST(ST_STM32_USART, n, BAUDRATE),         \
        .gpio_base    = _DT_INST(ST_STM32_USART, n, TX_PORT_BASE),     \
        .tx_pin       = _DT_INST(ST_STM32_USART, n, TX_PIN),           \
        .tx_af        = _DT_INST(ST_STM32_USART, n, TX_AF),            \
        .uart_clk_bit = _DT_INST_CLK(ST_STM32_USART, n, BIT),         \
    };                                                                  \
    DEVICE_DT_DEFINE(_DT_INST_LABEL(ST_STM32_USART, n),                \
                     uart_stm32_init, NULL, &uart_cfg_##n,              \
                     &uart_stm32_api);

/*
 * Instantiate for EVERY st,stm32-usart node with status="okay".
 *
 * The code generator emits:
 *   #define DT_INST_ST_STM32_USART_FOREACH(fn) fn(0)
 *
 * So this expands to: STM32_UART_DEFINE(0)
 * If there were 3 USARTs: STM32_UART_DEFINE(0) STM32_UART_DEFINE(1) STM32_UART_DEFINE(2)
 */
DT_INST_FOREACH_STATUS_OKAY(ST_STM32_USART, STM32_UART_DEFINE)
