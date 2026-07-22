/*
 * input.c — Button input handling via GPIO interrupt API
 *
 * Registers per-pin EXTI callbacks. Latches press events into flags
 * that game code consumes via button_pressed().
 */

#include "board.h"
#include "buttons.h"
#include "audio.h"
#include "drivers/gpio.h"
#include "drivers/uart.h"

static const uint8_t btn_pins[] = {0, 1, 2, 3, 4, 5, 6, 7};  /* PC0=A, PC1=B, PC2=Left, PC3=Right, PC4=Up, PC5=Down, PC6=Start, PC7=Select */
#define NUM_BUTTONS (sizeof(btn_pins) / sizeof(btn_pins[0]))

static volatile uint8_t btn_flags[NUM_BUTTONS];

static void button_press_handler(uint8_t pin)
{
    for (int i = 0; i < (int)NUM_BUTTONS; i++) {
        if (btn_pins[i] == pin) {
            btn_flags[i] = 1;
            if (i == BUTTON_A) sfx_jump();
            if (i == BUTTON_B) sfx_jump();
            return;
        }
    }
}

int button_pressed(uint8_t button)
{
    if (button >= NUM_BUTTONS) return 0;
    if (!btn_flags[button]) return 0;
    btn_flags[button] = 0;
    return 1;
}

void buttons_init(void)
{
    for (int i = 0; i < (int)NUM_BUTTONS; i++) {
        gpio_pin_configure(dev_gpioc, btn_pins[i], GPIO_INPUT | GPIO_PULL_DOWN);
        gpio_pin_register_callback(dev_gpioc, btn_pins[i], button_press_handler);
        gpio_pin_interrupt_configure(dev_gpioc, btn_pins[i], GPIO_INT_EDGE_RISING);
    }
}

void wait_for_button_press(uint8_t button)
{
    extern void sched_sleep_ms(uint32_t ms);
    button_pressed(button);  /* clear any pending press */
    while (!button_pressed(button)) sched_sleep_ms(50);
}
