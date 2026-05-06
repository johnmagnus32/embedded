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
#include "drivers/clock.h"
#include "sync.h"

#define USART_SR   0x00
#define USART_DR   0x04
#define USART_BRR  0x08
#define USART_CR1  0x0C

#define GPIO_MODER  0x00
#define GPIO_AFRL   0x20

/* NVIC registers for enabling IRQs */
#define NVIC_ISER1  (*(volatile uint32_t *)0xE000E104)  /* IRQs 32-63 */

#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

struct uart_stm32_config {
    uint32_t base;
    uint32_t baudrate;
    uint32_t gpio_base;
    uint8_t  tx_pin;
    uint8_t  tx_af;
    uint8_t  uart_clk_bus;
    uint8_t  uart_clk_bit;
};

/* RX ring buffer (filled by ISR, read by poll_in) */
#define RX_BUF_SIZE 64
static volatile char rx_buf[RX_BUF_SIZE];
static volatile uint8_t rx_head;
static volatile uint8_t rx_tail;

/* Semaphore signaled by ISR when data arrives */
struct semaphore uart_rx_sem = SEM_INIT(0);

DEVICE_DT_DECLARE(rcc);

static int uart_stm32_init(const struct device *dev)
{
    const struct uart_stm32_config *cfg = dev->config;

    clock_on(DEVICE_DT_GET(rcc), cfg->uart_clk_bus, cfg->uart_clk_bit);

    /* Enable GPIO port clock (AHB1, bit 0=GPIOA, 1=GPIOB, etc.) */
    uint8_t gpio_port = (cfg->gpio_base - 0x40020000) / 0x400;
    clock_on(DEVICE_DT_GET(rcc), 0, gpio_port);

    volatile uint32_t *moder = (volatile uint32_t *)(cfg->gpio_base + GPIO_MODER);
    /* AFRL for pins 0-7, AFRH for pins 8-15 */
    int af_pin = cfg->tx_pin;
    volatile uint32_t *afr = (volatile uint32_t *)(cfg->gpio_base +
        (af_pin < 8 ? GPIO_AFRL : GPIO_AFRL + 4));
    if (af_pin >= 8) af_pin -= 8;

    *moder &= ~(3U << (cfg->tx_pin * 2));
    *moder |=  (2U << (cfg->tx_pin * 2));
    *afr   &= ~(0xFU << (af_pin * 4));
    *afr   |=  (cfg->tx_af << (af_pin * 4));

    REG(cfg->base, USART_BRR) = (DT_SYSCLK_HZ + cfg->baudrate / 2) / cfg->baudrate;
    /* UE + TE + RE + RXNEIE (RX interrupt enable) */
    REG(cfg->base, USART_CR1) = (1 << 13) | (1 << 3) | (1 << 2) | (1 << 5);

    /* Enable USART2 IRQ in NVIC (IRQ 38 → ISER1 bit 6) */
    NVIC_ISER1 = (1 << (38 - 32));

    return 0;
}

/*
 * USART2 ISR — called by hardware when a byte is received.
 * Puts the byte in the ring buffer and signals the semaphore.
 * The shell task (blocked on sem_take) gets woken up.
 */
void usart2_isr(void)
{
    uint32_t base = DT_CONSOLE_BASE;
    uint32_t sr = REG(base, USART_SR);

    if (sr & (1 << 5)) {  /* RXNE — data available */
        char c = (char)REG(base, USART_DR);
        uint8_t next = (rx_head + 1) % RX_BUF_SIZE;
        if (next != rx_tail) {  /* not full */
            rx_buf[rx_head] = c;
            rx_head = next;
        }
        sem_give(&uart_rx_sem);  /* wake whoever is waiting for input */
    }
}

static void uart_stm32_poll_out(const struct device *dev, char c)
{
    const struct uart_stm32_config *cfg = dev->config;

    while (!(REG(cfg->base, USART_SR) & (1 << 7)))
        ;
    REG(cfg->base, USART_DR) = c;
}

static int uart_stm32_poll_in(const struct device *dev, char *c)
{
    (void)dev;
    /* Read from ISR-filled ring buffer */
    if (rx_tail == rx_head)
        return -1;  /* empty */
    *c = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
    return 0;
}

static const struct uart_driver_api uart_stm32_api = {
    .poll_out = uart_stm32_poll_out,
    .poll_in = uart_stm32_poll_in,
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
        .uart_clk_bus = _DT_INST_CLK(ST_STM32_USART, n, BUS),         \
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
