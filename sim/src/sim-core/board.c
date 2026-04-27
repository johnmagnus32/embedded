/*
 * board.c — STM32 board simulation, configured from device tree
 */
#include <stdio.h>
#include <string.h>
#include "board.h"

void board_init(struct board *b, const struct dts *dt)
{
    cpu_init(&b->cpu);
    nvic_init(&b->nvic);
    systick_init(&b->systick);
    b->nuarts = 0;
    b->sysclk_hz = dt->sysclk_hz ? dt->sysclk_hz : 16000000;

    /* Create devices from device tree */
    for (int i = 0; i < dt->nnodes; i++) {
        const struct dts_node *n = &dt->nodes[i];

        if (strcmp(n->compatible, "st,stm32-usart") == 0 && n->has_reg) {
            if (b->nuarts < MAX_UARTS) {
                fprintf(stderr, "[board] UART '%s' at 0x%08X\n", n->label, n->reg);
                uart_init(&b->uarts[b->nuarts++], n->reg);
            }
        }
        /* Future: st,stm32-spi, st,stm32-gpio, etc. */
    }
}

void board_tick(struct board *b)
{
    cpu_step(&b->cpu, b->flash, b->ram);
    systick_tick(&b->systick, &b->nvic);
    nvic_update(&b->nvic, &b->cpu, b->flash, b->ram);
}
