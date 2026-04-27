/*
 * board.c — STM32 board simulation, configured from device tree
 */
#include <stdio.h>
#include <string.h>
#include "board.h"

void board_init(struct board *b, const struct dts *dt, struct chardev_table *chardevs)
{
    cpu_init(&b->cpu);
    nvic_init(&b->nvic);
    systick_init(&b->systick);
    b->nuarts = 0;
    b->sysclk_hz = dt->sysclk_hz ? dt->sysclk_hz : 16000000;

    /* Trace port — always at 0xE0000000 */
    struct chardev *trace_cd = chardevs ? chardev_find(chardevs, "trace") : NULL;
    trace_dev_init(&b->trace, 0xE0000000, trace_cd);

    for (int i = 0; i < dt->nnodes; i++) {
        const struct dts_node *n = &dt->nodes[i];

        if (strcmp(n->compatible, "st,stm32-usart") == 0 && n->has_reg) {
            if (b->nuarts < MAX_UARTS) {
                /* Find chardev by DTS label (e.g. "usart2") */
                struct chardev *cd = chardevs ? chardev_find(chardevs, n->label) : NULL;
                fprintf(stderr, "[board] UART '%s' at 0x%08X%s\n",
                        n->label, n->reg, cd ? " (chardev)" : "");
                uart_init(&b->uarts[b->nuarts++], n->reg, cd);
            }
        }
    }
}

void board_tick(struct board *b)
{
    cpu_step(&b->cpu, b->flash, b->ram);
    systick_tick(&b->systick, &b->nvic);
    nvic_update(&b->nvic, &b->cpu, b->flash, b->ram);
}
