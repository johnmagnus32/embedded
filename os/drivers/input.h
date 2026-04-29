/*
 * input.h — Button input driver API (EXTI-based)
 */

#ifndef DRIVERS_INPUT_H
#define DRIVERS_INPUT_H

#include "device.h"
#include <stdint.h>

typedef void (*input_callback_t)(uint8_t pin);

struct input_driver_api {
    int (*is_pressed)(const struct device *dev, uint8_t pin);
};

static inline int input_is_pressed(const struct device *dev, uint8_t pin)
{
    const struct input_driver_api *api = dev->api;
    return api->is_pressed(dev, pin);
}

/* Set by application, called from ISR */
extern input_callback_t input_app_callback;

#endif
