/*
 * gpio.c — GPIO port device
 */
#include <string.h>
#include "gpio.h"

void gpio_init(struct gpio_port *g)
{
    memset(g, 0, sizeof(*g));
}

uint32_t gpio_read(void *opaque, uint32_t offset)
{
    struct gpio_port *g = (struct gpio_port *)opaque;
    switch (offset) {
    case 0x10: return g->idr;
    case 0x14: return g->odr;
    default:   return 0;
    }
}

void gpio_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct gpio_port *g = (struct gpio_port *)opaque;
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
