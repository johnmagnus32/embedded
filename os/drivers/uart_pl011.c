/*
 * uart_pl011.c — ARM PL011 UART driver (used by RP2040)
 *
 * Completely different register layout from STM32's USART.
 * Same driver API (uart_driver_api) — application code doesn't change.
 *
 * PL011 registers:
 *   DR   (0x00) — data register
 *   FR   (0x18) — flag register (TXFF, RXFE, BUSY)
 *   IBRD (0x24) — integer baud rate divisor
 *   FBRD (0x28) — fractional baud rate divisor
 *   LCR  (0x2C) — line control (word length, FIFO enable)
 *   CR   (0x30) — control (UART enable, TX/RX enable)
 */

#include <stdint.h>
#include "config.h"

#ifdef CONFIG_UART_PL011

#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include "sync.h"

/* Shared semaphore for shell to block on */
struct semaphore uart_rx_sem = SEM_INIT(0);

#define PL011_DR    0x00
#define PL011_FR    0x18
#define PL011_IBRD  0x24
#define PL011_FBRD  0x28
#define PL011_LCR   0x2C
#define PL011_CR    0x30
#define PL011_IMSC  0x38  /* interrupt mask */
#define PL011_ICR   0x44  /* interrupt clear */

#define FR_TXFF  (1 << 5)  /* TX FIFO full */
#define FR_RXFE  (1 << 4)  /* RX FIFO empty */

#define REG(base, off) (*(volatile uint32_t *)((base) + (off)))

struct uart_pl011_config {
    uint32_t base;
    uint32_t baudrate;
    uint8_t  tx_pin;
    uint8_t  rx_pin;
};

/*
 * RP2040 GPIO function select — each pin has a mux.
 * UART0 TX/RX = function 2 on most pins.
 * IO_BANK0 base = 0x40014000, each pin has CTRL at offset 0x04 + pin*8
 */
#define IO_BANK0_BASE 0x40014000
#define GPIO_CTRL(pin) (*(volatile uint32_t *)(IO_BANK0_BASE + 0x04 + (pin) * 8))

static int uart_pl011_init(const struct device *dev)
{
    const struct uart_pl011_config *cfg = dev->config;

    /* Configure TX/RX pins for UART function (funcsel = 2) */
    GPIO_CTRL(cfg->tx_pin) = 2;
    GPIO_CTRL(cfg->rx_pin) = 2;

    /* Disable UART while configuring */
    REG(cfg->base, PL011_CR) = 0;

    /*
     * Baud rate: IBRD = clk / (16 * baud), FBRD = frac * 64 + 0.5
     * At 125MHz, 115200 baud: IBRD = 67, FBRD = 52
     */
    uint32_t clk = DT_SYSCLK_HZ;
    uint32_t div = (8 * clk) / cfg->baudrate;
    uint32_t ibrd = div >> 7;
    uint32_t fbrd = ((div & 0x7F) + 1) / 2;

    REG(cfg->base, PL011_IBRD) = ibrd;
    REG(cfg->base, PL011_FBRD) = fbrd;

    /* 8N1, enable FIFOs */
    REG(cfg->base, PL011_LCR) = (3 << 5) | (1 << 4);  /* WLEN=8, FEN=1 */

    /* Enable UART, TX, RX */
    REG(cfg->base, PL011_CR) = (1 << 0) | (1 << 8) | (1 << 9);

    return 0;
}

static void uart_pl011_poll_out(const struct device *dev, char c)
{
    const struct uart_pl011_config *cfg = dev->config;
    while (REG(cfg->base, PL011_FR) & FR_TXFF)
        ;
    REG(cfg->base, PL011_DR) = c;
}

static int uart_pl011_poll_in(const struct device *dev, char *c)
{
    const struct uart_pl011_config *cfg = dev->config;
    if (REG(cfg->base, PL011_FR) & FR_RXFE)
        return -1;
    *c = (char)REG(cfg->base, PL011_DR);
    return 0;
}

static const struct uart_driver_api uart_pl011_api = {
    .poll_out = uart_pl011_poll_out,
    .poll_in = uart_pl011_poll_in,
};

/* ---- DT_INST instantiation ---- */

#define _PL011_LABEL(n) DT_INST_ARM_PL011_##n##_LABEL
#define _PL011_PROP(n, p) DT_INST_ARM_PL011_##n##_PROP_##p

#define PL011_UART_DEFINE(n)                                        \
    static const struct uart_pl011_config uart_pl011_cfg_##n = {    \
        .base     = DT_INST_ARM_PL011_##n##_REG_ADDR,              \
        .baudrate = _PL011_PROP(n, BAUDRATE),                       \
        .tx_pin   = _PL011_PROP(n, TX_PIN),                         \
        .rx_pin   = _PL011_PROP(n, RX_PIN),                         \
    };                                                              \
    DEVICE_DT_DEFINE(_PL011_LABEL(n),                               \
                     uart_pl011_init, NULL, &uart_pl011_cfg_##n,     \
                     &uart_pl011_api);

DT_INST_FOREACH_STATUS_OKAY(ARM_PL011, PL011_UART_DEFINE)

#endif /* CONFIG_UART_PL011 */
