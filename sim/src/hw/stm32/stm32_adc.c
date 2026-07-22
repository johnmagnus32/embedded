#include <string.h>
#include "stm32_adc.h"

#define ADC_SR_EOC (1 << 1)
#define ADC_CR2_SWSTART (1 << 30)

void stm32_adc_init(struct stm32_adc *adc)
{
    memset(adc, 0, sizeof(*adc));
    /* Default channels to mid-range (12-bit ADC) so analog inputs
     * like volume knobs don't read as zero. */
    for (int i = 0; i < 16; i++)
        adc->channels[i] = 2048;
}

uint32_t stm32_adc_read(void *opaque, uint32_t offset)
{
    struct stm32_adc *adc = opaque;
    switch (offset) {
    case 0x00: return adc->sr;
    case 0x04: return adc->cr1;
    case 0x08: return adc->cr2;
    case 0x34: return adc->sqr3;
    case 0x4C: { /* DR — return channel data, clear EOC */
        uint32_t ch = adc->sqr3 & 0xF;
        adc->sr &= ~ADC_SR_EOC;
        return adc->channels[ch];
    }
    default: return 0;
    }
}

void stm32_adc_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct stm32_adc *adc = opaque;
    switch (offset) {
    case 0x00: adc->sr = val; break;
    case 0x04: adc->cr1 = val; break;
    case 0x08:
        adc->cr2 = val;
        if (val & ADC_CR2_SWSTART)
            adc->sr |= ADC_SR_EOC;
        break;
    case 0x34: adc->sqr3 = val; break;
    default: break;
    }
}
