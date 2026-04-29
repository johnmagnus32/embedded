/*
 * stm32_exti.c — STM32 External Interrupt/Event Controller
 */
#include <string.h>
#include "stm32_exti.h"
#include "armv7m_nvic.h"

void stm32_exti_init(struct stm32_exti *e)
{
    memset(e, 0, sizeof(*e));
}

uint32_t stm32_exti_read(void *opaque, uint32_t offset)
{
    struct stm32_exti *e = (struct stm32_exti *)opaque;
    switch (offset) {
    case 0x00: return e->imr;
    case 0x04: return e->emr;
    case 0x08: return e->rtsr;
    case 0x0C: return e->ftsr;
    case 0x10: return e->swier;
    case 0x14: return e->pr;
    default:   return 0;
    }
}

void stm32_exti_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct stm32_exti *e = (struct stm32_exti *)opaque;
    switch (offset) {
    case 0x00: e->imr = val; break;
    case 0x04: e->emr = val; break;
    case 0x08: e->rtsr = val; break;
    case 0x0C: e->ftsr = val; break;
    case 0x10: /* SWIER: setting a bit sets PR and fires IRQ */
        for (int i = 0; i < 16; i++) {
            if ((val >> i) & 1) {
                e->pr |= (1u << i);
                if ((e->imr >> i) & 1)
                    gpio_set(&e->irq_out[i], 1);
            }
        }
        e->swier |= val;
        break;
    case 0x14: /* PR: write-1-to-clear */
        e->pr &= ~val;
        e->swier &= ~val;
        break;
    }
}

/* Called when a GPIO input changes — opaque is &exti_input[line] */
void stm32_exti_input_handler(void *opaque, int level)
{
    struct exti_input *in = (struct exti_input *)opaque;
    struct stm32_exti *e = in->exti;
    int line = in->line;
    uint32_t mask = 1u << line;

    int rising  = level && (e->rtsr & mask);
    int falling = !level && (e->ftsr & mask);

    if (rising || falling) {
        e->pr |= mask;
        if (e->imr & mask)
            gpio_set(&e->irq_out[line], 1);
    }
}

/* Called when EXTI fires an IRQ line — opaque is &nvic_irq_line */
void nvic_irq_line_handler(void *opaque, int level)
{
    struct nvic_irq_line *n = (struct nvic_irq_line *)opaque;
    (void)level;
    armv7m_nvic_set_pending(n->nvic, 16 + n->irq);
}
