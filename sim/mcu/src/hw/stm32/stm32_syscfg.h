/*
 * stm32_syscfg.h — STM32 SYSCFG peripheral (EXTI port selection)
 */
#ifndef STM32_SYSCFG_H
#define STM32_SYSCFG_H

#include <stdint.h>

struct stm32_gpio;
struct exti_input;

struct stm32_syscfg {
    uint32_t exticr[4];
    struct stm32_gpio *gpio;     /* array of GPIO ports */
    int ngpio;
    struct exti_input *exti_inputs;  /* array of 16 EXTI input structs */
};

void stm32_syscfg_init(struct stm32_syscfg *s, struct stm32_gpio *gpio, int ngpio, struct exti_input *exti_inputs);
uint32_t stm32_syscfg_read(void *opaque, uint32_t offset);
void stm32_syscfg_write(void *opaque, uint32_t offset, uint32_t val);

#endif
