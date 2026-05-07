#ifndef STM32F411_H
#define STM32F411_H

#include "armv7m_cpu.h"
#include "membus.h"
#include "armv7m_nvic.h"
#include "armv7m_systick.h"
#include "stm32_uart.h"
#include "stm32_spi.h"
#include "stm32_gpio.h"
#include "stm32_exti.h"
#include "stm32_adc.h"
#include "stm32_dma.h"
#include "stm32_syscfg.h"
#include "event_queue.h"

#define STM32F411_NUM_USARTS 3
#define STM32F411_NUM_SPIS   3
#define STM32F411_NUM_GPIO   3

struct stm32f411 {
    struct armv7m_cpu      cpu;
    struct membus         bus;
    struct armv7m_nvic    nvic;
    struct armv7m_systick systick;
    struct stm32_uart     usarts[STM32F411_NUM_USARTS];
    struct stm32_spi      spis[STM32F411_NUM_SPIS];
    struct stm32_gpio     gpio[STM32F411_NUM_GPIO];
    struct stm32_exti     exti;
    struct stm32_adc      adc;
    struct stm32_dma      dma1;
    struct stm32_dma      dma2;
    struct stm32_syscfg   syscfg;
    struct exti_input     exti_inputs[16];
    struct nvic_irq_line  exti_nvic[16];
    uint8_t              *flash;
    uint8_t              *ram;
    uint32_t              sysclk_hz;
    struct event_queue    eq;
};

void stm32f411_init(struct stm32f411 *soc, uint32_t sysclk_hz);
int  stm32f411_tick(struct stm32f411 *soc);

#endif
