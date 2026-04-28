#ifndef STM32F411_H
#define STM32F411_H

#include "cpu.h"
#include "membus.h"
#include "armv7m_nvic.h"
#include "armv7m_systick.h"
#include "stm32_uart.h"
#include "stm32_spi.h"
#include "stm32_gpio.h"
#include "stm32_exti.h"

#define STM32F411_NUM_USARTS 3
#define STM32F411_NUM_SPIS   2
#define STM32F411_NUM_GPIO   3

struct stm32f411 {
    struct cpu_state      cpu;
    struct membus         bus;
    struct armv7m_nvic    nvic;
    struct armv7m_systick systick;
    struct stm32_uart     usarts[STM32F411_NUM_USARTS];
    struct stm32_spi      spis[STM32F411_NUM_SPIS];
    struct stm32_gpio     gpio[STM32F411_NUM_GPIO];
    struct stm32_exti     exti;
    struct exti_input     exti_inputs[16];
    struct nvic_irq_line  exti_nvic[5];
    uint8_t              *flash;
    uint8_t              *ram;
    uint32_t              sysclk_hz;
};

void stm32f411_init(struct stm32f411 *soc);
void stm32f411_tick(struct stm32f411 *soc);

#endif
