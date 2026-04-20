/*
 * main.c — Bare-metal hello world using device tree + UART driver
 *
 * main.c knows WHAT to talk to (from devicetree.h)
 * uart.c knows HOW to talk to it (register-level driver)
 * Neither has hardcoded addresses — they meet through uart_config.
 *
 * This is the same pattern as Zephyr:
 *   devicetree.h  →  DEVICE_DT_DEFINE populates config  →  driver uses config
 */

#include "devicetree.h"       /* auto-generated from board.dts */
#include "drivers/uart.h"

/* Populate driver config from device tree — like Zephyr's DEVICE_DT_DEFINE */
static const struct uart_config console_cfg = {
    .base         = DT_USART2_BASE,
    .baudrate     = DT_USART2_BAUDRATE,
    .clk_hz       = DT_SYSCLK_HZ,
    .gpio_base    = DT_USART2_TX_PORT_BASE,
    .tx_pin       = DT_USART2_TX_PIN,
    .tx_af        = DT_USART2_TX_AF,
    .gpio_clk_bit = DT_USART2_GPIO_CLK_BIT,
    .uart_clk_bit = DT_USART2_CLK_BIT,
};

int main(void)
{
    uart_init(&console_cfg);

    uart_puts(&console_cfg, "Hello from bare metal STM32F411RE!\n");
    uart_puts(&console_cfg, "Now with device tree + UART driver.\n");

    while (1) {
        uart_puts(&console_cfg, "tick\n");
        for (volatile int i = 0; i < 1600000; i++)
            ;
    }
}
