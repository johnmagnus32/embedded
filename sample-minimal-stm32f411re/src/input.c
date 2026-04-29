/*
 * input.c — Button input handling
 *
 * Called from EXTI ISR via input_app_callback.
 * No register addresses — uses driver API only.
 */

#include "app.h"
#include "drivers/uart.h"

static const char * const btn_names[] = {"A", "B", "Left", "Right", "Up"};

void input_handler(uint8_t pin)
{
    if (pin < 5) {
        uart_print("[btn] ");
        uart_print(btn_names[pin]);
        uart_print("\n");
    }
    if (pin == 0) sfx_jump();
    if (pin == 1) sfx_beep();
}
