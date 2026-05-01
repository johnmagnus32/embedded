/*
 * input.c — Button input handling via GPIO interrupt API
 *
 * Registers per-pin EXTI callbacks. Latches press events into flags
 * that game code consumes via button_pressed().
 */

#include "app.h"
#include "drivers/gpio.h"
#include "drivers/uart.h"

static volatile uint8_t btn_flags[5];

static void input_handler(uint8_t pin)
{
    if (pin < 5)
        btn_flags[pin] = 1;
    if (pin == 0) sfx_jump();
    if (pin == 1) sfx_beep();
}

int button_pressed(uint8_t pin)
{
    if (pin >= 5) return 0;
    if (!btn_flags[pin]) return 0;
    btn_flags[pin] = 0;
    return 1;
}

void buttons_init(void)
{
    for (uint8_t i = 0; i < 5; i++) {
        gpio_pin_configure(dev_gpiob, i, GPIO_INPUT | GPIO_PULL_UP);
        gpio_pin_register_callback(dev_gpiob, i, input_handler);
        gpio_pin_interrupt_configure(dev_gpiob, i, GPIO_INT_EDGE_RISING);
    }
}
