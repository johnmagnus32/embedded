#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include "gpio_line.h"

struct gpio_port {
    uint32_t odr;
    uint32_t idr;
    struct gpio_line out[16];
};

void     gpio_init(struct gpio_port *g);
uint32_t gpio_read(void *opaque, uint32_t offset);
void     gpio_write(void *opaque, uint32_t offset, uint32_t val);

#endif
