/*
 * buttons.c — gpio-keys driver (Linux-style child nodes)
 *
 * Each button is a child node in the device tree with:
 *   gpios = <&gpioc pin>;
 *   code = <keycode>;
 *
 * The driver reads pin numbers from the generated DT child defines.
 */

#include <stdint.h>
#include <stdbool.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/gpio.h"
#include "drivers/buttons.h"

/* Key codes (matching Linux input.h) */
#define KEY_UP    103
#define KEY_DOWN  108
#define KEY_LEFT  105
#define KEY_RIGHT 106
#define KEY_A     30
#define KEY_B     48

/*
 * Build a pin lookup table from the DT child nodes.
 * We match child CODE to our button enum.
 */
struct button_def {
    uint8_t pin;
    uint8_t code;
};

#define CHILD(n) { \
    .pin = DT_INST_GPIO_KEYS_0_CHILD_##n##_PIN, \
    .code = DT_INST_GPIO_KEYS_0_CHILD_##n##_CODE, \
}

static const struct button_def all_buttons[] = {
    CHILD(0), CHILD(1), CHILD(2), CHILD(3), CHILD(4), CHILD(5),
};
#define NUM_BUTTONS (sizeof(all_buttons) / sizeof(all_buttons[0]))

/* Map keycode → button enum */
static const uint8_t code_to_btn[] = {
    [KEY_UP]    = BTN_UP,
    [KEY_DOWN]  = BTN_DOWN,
    [KEY_LEFT]  = BTN_LEFT,
    [KEY_RIGHT] = BTN_RIGHT,
    [KEY_A]     = BTN_A,
    [KEY_B]     = BTN_B,
};

/* Pin for each button enum value (filled at init) */
static uint8_t btn_pins[BTN_COUNT];

DEVICE_DT_DECLARE(gpioc);

static int buttons_init(const struct device *dev)
{
    (void)dev;
    const struct device *gpio = DEVICE_DT_GET(gpioc);

    for (int i = 0; i < (int)NUM_BUTTONS; i++) {
        uint8_t pin = all_buttons[i].pin;
        uint8_t code = all_buttons[i].code;

        gpio_pin_configure(gpio, pin, GPIO_INPUT | GPIO_PULL_UP);

        if (code < sizeof(code_to_btn)) {
            btn_pins[code_to_btn[code]] = pin;
        }
    }
    return 0;
}

static bool buttons_is_pressed(const struct device *dev, enum button_id btn)
{
    (void)dev;
    const struct device *gpio = DEVICE_DT_GET(gpioc);

    if (btn >= BTN_COUNT) return false;
    return gpio_pin_get(gpio, btn_pins[btn]) == 0;  /* active low */
}

static const struct buttons_driver_api buttons_api = {
    .is_pressed = buttons_is_pressed,
};

/* ---- Instantiation ---- */

#define _BTN_INST_LABEL(n) DT_INST_GPIO_KEYS_##n##_LABEL

#define GPIO_KEYS_DEFINE(n) \
    DEVICE_DT_DEFINE(_BTN_INST_LABEL(n), \
                     buttons_init, NULL, NULL, &buttons_api);

DT_INST_FOREACH_STATUS_OKAY(GPIO_KEYS, GPIO_KEYS_DEFINE)
