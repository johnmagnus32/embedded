#ifndef DRIVERS_BUTTONS_H
#define DRIVERS_BUTTONS_H

#include "device.h"
#include <stdbool.h>

enum button_id {
    BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B, BTN_COUNT,
};

struct buttons_driver_api {
    bool (*is_pressed)(const struct device *dev, enum button_id btn);
};

static inline bool button_is_pressed(const struct device *dev, enum button_id btn)
{
    const struct buttons_driver_api *api = dev->api;
    return api->is_pressed(dev, btn);
}

#endif
