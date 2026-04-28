#ifndef STM32_GPIO_H
#define STM32_GPIO_H

#include <stdint.h>
#include "gpio_line.h"

struct stm32_gpio {
    uint32_t odr;
    uint32_t idr;
    struct gpio_line out[16];
    struct gpio_line idr_change[16];
};

void     stm32_gpio_init(struct stm32_gpio *g);
uint32_t stm32_gpio_read(void *opaque, uint32_t offset);
void     stm32_gpio_write(void *opaque, uint32_t offset, uint32_t val);
void     stm32_gpio_set_input(struct stm32_gpio *g, int pin, int level);

#endif
