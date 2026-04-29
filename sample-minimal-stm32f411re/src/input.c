/*
 * input.c — Button input handling via GPIO interrupt API
 *
 * Registers per-pin callbacks through the GPIO driver.
 */

#include "app.h"
#include "drivers/gpio.h"
#include "drivers/uart.h"

static const char * const btn_names[] = {"A", "B", "Left", "Right", "Up"};

static void input_handler(uint8_t pin)
{
    if (pin < 5) {
        uart_print("[btn] ");
        uart_print(btn_names[pin]);
        uart_print("\n");
    }
    if (pin == 0) sfx_jump();
    if (pin == 1) sfx_beep();
}

void buttons_init(void)
{
    for (uint8_t i = 0; i < 5; i++) {
        gpio_pin_configure(dev_gpiob, i, GPIO_INPUT | GPIO_PULL_UP);
        gpio_pin_register_callback(dev_gpiob, i, input_handler);
        gpio_pin_interrupt_configure(dev_gpiob, i, GPIO_INT_EDGE_RISING);
    }
}
