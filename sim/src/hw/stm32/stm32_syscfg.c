/*
 * stm32_syscfg.c — STM32 SYSCFG peripheral
 *
 * Handles EXTICR registers: selects which GPIO port drives each EXTI line.
 * When firmware writes EXTICR, this rewires the GPIO→EXTI connections.
 */
#include <string.h>
#include "stm32_syscfg.h"
#include "stm32_gpio.h"
#include "stm32_exti.h"

#define SYSCFG_EXTICR1 0x08
#define SYSCFG_EXTICR4 0x14

void stm32_syscfg_init(struct stm32_syscfg *s, struct stm32_gpio *gpio, int ngpio, struct exti_input *exti_inputs)
{
    memset(s->exticr, 0, sizeof(s->exticr));
    s->gpio = gpio;
    s->ngpio = ngpio;
    s->exti_inputs = exti_inputs;
}

uint32_t stm32_syscfg_read(void *opaque, uint32_t offset)
{
    struct stm32_syscfg *s = opaque;
    if (offset >= SYSCFG_EXTICR1 && offset <= SYSCFG_EXTICR4)
        return s->exticr[(offset - SYSCFG_EXTICR1) / 4];
    return 0;
}

void stm32_syscfg_write(void *opaque, uint32_t offset, uint32_t val)
{
    struct stm32_syscfg *s = opaque;
    if (offset < SYSCFG_EXTICR1 || offset > SYSCFG_EXTICR4) return;

    int reg_idx = (offset - SYSCFG_EXTICR1) / 4;
    uint32_t old = s->exticr[reg_idx];
    s->exticr[reg_idx] = val;

    for (int i = 0; i < 4; i++) {
        int pin = reg_idx * 4 + i;
        int old_port = (old >> (i * 4)) & 0xF;
        int new_port = (val >> (i * 4)) & 0xF;
        if (old_port == new_port) continue;
        if (new_port >= s->ngpio) continue;

        /* Disconnect old port from this EXTI line */
        if (old_port < s->ngpio) {
            s->gpio[old_port].idr_change[pin].handler = NULL;
            s->gpio[old_port].idr_change[pin].opaque = NULL;
        }

        /* Connect new port → EXTI input for this line */
        s->gpio[new_port].idr_change[pin].handler = stm32_exti_input_handler;
        s->gpio[new_port].idr_change[pin].opaque = &s->exti_inputs[pin];
    }
}
