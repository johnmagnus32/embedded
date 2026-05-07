/*
 * stm32_gpio.c — STM32 GPIO port device
 */
#include <string.h>
#include "stm32_gpio.h"

void stm32_gpio_init(struct stm32_gpio *g)
{
    memset(g, 0, sizeof(*g));
}

uint32_t stm32_gpio_read(void *opaque, uint32_t offset)
{
    struct stm32_gpio *g = (struct stm32_gpio *)opaque;
    switch (offset) {
    case 0x10: return g->idr;
    case 0x14: return g->odr;
    default:   return 0;
    }
}

void stm32_gpio_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct stm32_gpio *g = (struct stm32_gpio *)opaque;
    switch (offset) {
    case 0x14: /* ODR */
        for (int i = 0; i < 16; i++) {
            int new_bit = (val >> i) & 1;
            int old_bit = (g->odr >> i) & 1;
            if (new_bit != old_bit)
                gpio_set(&g->out[i], new_bit);
        }
        g->odr = val;
        break;
    case 0x18: /* BSRR */
        for (int i = 0; i < 16; i++) {
            if (val & (1u << i)) {       /* set */
                if (!((g->odr >> i) & 1))
                    gpio_set(&g->out[i], 1);
                g->odr |= (1u << i);
            }
            if (val & (1u << (i + 16))) { /* reset */
                if ((g->odr >> i) & 1)
                    gpio_set(&g->out[i], 0);
                g->odr &= ~(1u << i);
            }
        }
        break;
    }
}

void stm32_gpio_set_input(struct stm32_gpio *g, int pin, int level)
{
    int old = (g->idr >> pin) & 1;
    if (level) g->idr |= (1u << pin);
    else       g->idr &= ~(1u << pin);
    if (level != old)
        gpio_set(&g->idr_change[pin], level);
}