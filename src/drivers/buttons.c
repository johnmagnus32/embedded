/*
 * buttons.c — GPIO-keys button driver
 *
 * Reads button state through the GPIO driver. Config (which pins)
 * comes from device tree. This driver depends on the GPIO driver —
 * like how a filesystem driver depends on the block layer.
 */

#include <stdint.h>
#include <stdbool.h>
#include "devicetree.h"
#include "device.h"
#include "drivers/gpio.h"
#include "drivers/buttons.h"

struct buttons_config {
    uint8_t pins[BTN_COUNT];
};

/* Reference to the GPIO port device this button set is on */
DEVICE_DT_DECLARE(gpioc);

static int buttons_init(const struct device *dev)
{
    const struct buttons_config *cfg = dev->config;
    const struct device *gpio = DEVICE_DT_GET(gpioc);

    for (int i = 0; i < BTN_COUNT; i++) {
        gpio_pin_configure(gpio, cfg->pins[i], GPIO_INPUT | GPIO_PULL_UP);
    }
    return 0;
}

static bool buttons_is_pressed(const struct device *dev, enum button_id btn)
{
    const struct buttons_config *cfg = dev->config;
    const struct device *gpio = DEVICE_DT_GET(gpioc);

    if (btn >= BTN_COUNT) return false;

    /* Active low: pressed = pin reads 0 */
    return gpio_pin_get(gpio, cfg->pins[btn]) == 0;
}

static const struct buttons_driver_api buttons_api = {
    .is_pressed = buttons_is_pressed,
};

/* ---- DT_INST instantiation ---- */

#define _BTN_INST_PROP(n, prop) DT_INST_GPIO_KEYS_##n##_PROP_##prop
#define _BTN_INST_LABEL(n) DT_INST_GPIO_KEYS_##n##_LABEL

#define GPIO_KEYS_DEFINE(n)                                         \
    static const struct buttons_config buttons_cfg_##n = {          \
        .pins = {                                                   \
            _BTN_INST_PROP(n, PIN_UP),                              \
            _BTN_INST_PROP(n, PIN_DOWN),                            \
            _BTN_INST_PROP(n, PIN_LEFT),                            \
            _BTN_INST_PROP(n, PIN_RIGHT),                           \
            _BTN_INST_PROP(n, PIN_A),                               \
            _BTN_INST_PROP(n, PIN_B),                               \
        },                                                          \
    };                                                              \
    DEVICE_DT_DEFINE(_BTN_INST_LABEL(n),                            \
                     buttons_init, NULL, &buttons_cfg_##n,           \
                     &buttons_api);

DT_INST_FOREACH_STATUS_OKAY(GPIO_KEYS, GPIO_KEYS_DEFINE)
