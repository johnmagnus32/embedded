/*
 * board.c — STM32F411RE board simulation
 *
 * One tick = one system clock cycle.
 * Steps the CPU, then each device, then the NVIC.
 * No debugger logic — just hardware.
 */
#include "board.h"

void board_init(struct board *b)
{
    cpu_init(&b->cpu);
    nvic_init(&b->nvic);
    systick_init(&b->systick);
}

void board_tick(struct board *b)
{
    cpu_step(&b->cpu, b->flash, b->ram);
    systick_tick(&b->systick, &b->nvic);
    nvic_update(&b->nvic, &b->cpu, b->flash, b->ram);
}
