#ifndef STM32_EXTI_H
#define STM32_EXTI_H

#include <stdint.h>
#include "gpio_line.h"

struct armv7m_nvic;

struct exti_input {
    struct stm32_exti *exti;
    int line;
};

struct nvic_irq_line {
    struct armv7m_nvic *nvic;
    int irq;
};

struct stm32_exti {
    uint32_t imr;
    uint32_t emr;
    uint32_t rtsr;
    uint32_t ftsr;
    uint32_t swier;
    uint32_t pr;
    struct gpio_line irq_out[16];
};

void     stm32_exti_init(struct stm32_exti *e);
uint32_t stm32_exti_read(void *opaque, uint32_t offset);
void     stm32_exti_write(void *opaque, uint32_t offset, uint32_t val);

void stm32_exti_input_handler(void *opaque, int level);
void nvic_irq_line_handler(void *opaque, int level);

#endif
