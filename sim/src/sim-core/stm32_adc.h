#ifndef STM32_ADC_H
#define STM32_ADC_H

#include <stdint.h>

struct stm32_adc {
    uint32_t sr;
    uint32_t cr1;
    uint32_t cr2;
    uint32_t sqr3;
    uint32_t channels[16];
};

void     stm32_adc_init(struct stm32_adc *adc);
uint32_t stm32_adc_read(void *opaque, uint32_t offset);
void     stm32_adc_write(void *opaque, uint32_t offset, uint32_t val);

#endif
