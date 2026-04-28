#ifndef STM32F411_H
#define STM32F411_H

#include "cpu.h"
#include "membus.h"
#include "nvic.h"
#include "systick.h"
#include "uart.h"
#include "spi.h"
#include "gpio.h"

#define STM32F411_NUM_USARTS 3
#define STM32F411_NUM_SPIS   2
#define STM32F411_NUM_GPIO   3

struct stm32f411 {
    struct cpu_state cpu;
    struct membus    bus;
    struct nvic      nvic;
    struct systick   systick;
    struct uart      usarts[STM32F411_NUM_USARTS];
    struct spi       spis[STM32F411_NUM_SPIS];
    struct gpio_port gpio[STM32F411_NUM_GPIO];
    uint8_t         *flash;
    uint8_t         *ram;
    uint32_t         sysclk_hz;
};

void stm32f411_init(struct stm32f411 *soc);
void stm32f411_tick(struct stm32f411 *soc);

#endif
