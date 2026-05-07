/*
 * input.c — Button input handling via GPIO interrupt API
 *
 * Registers per-pin EXTI callbacks. Latches press events into flags
 * that game code consumes via button_pressed().
 */

#include "app.h"
#include "drivers/gpio.h"
#include "drivers/uart.h"

static const uint8_t btn_pins[] = {0, 1, 2, 3, 4};  /* PC0=A, PC1=B, PC2=Left, PC3=Right, PC4=Up */
#define NUM_BUTTONS (sizeof(btn_pins) / sizeof(btn_pins[0]))

static volatile uint8_t btn_flags[NUM_BUTTONS];

static void input_handler(uint8_t pin)
{
    for (int i = 0; i < (int)NUM_BUTTONS; i++) {
        if (btn_pins[i] == pin) {
            btn_flags[i] = 1;
            if (i == 0) sfx_jump();
            if (i == 1) sfx_beep();
            return;
        }
    }
}

int button_pressed(uint8_t pin)
{
    if (pin >= NUM_BUTTONS) return 0;
    if (!btn_flags[pin]) return 0;
    btn_flags[pin] = 0;
    return 1;
}

void buttons_init(void)
{
    for (int i = 0; i < (int)NUM_BUTTONS; i++) {
        gpio_pin_configure(dev_gpioc, btn_pins[i], GPIO_INPUT | GPIO_PULL_DOWN);
        gpio_pin_register_callback(dev_gpioc, btn_pins[i], input_handler);
        gpio_pin_interrupt_configure(dev_gpioc, btn_pins[i], GPIO_INT_EDGE_RISING);
    }
}
