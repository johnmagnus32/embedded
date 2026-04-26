/*
 * board.c — STM32F411RE board simulation
 *
 * One tick = one system clock cycle.
 * Steps the CPU, then each device, then the NVIC.
 */
#include "board.h"

void board_init(struct board *b)
{
    cpu_init(&b->cpu);
    nvic_init(&b->nvic);
    systick_init(&b->systick);
    b->nbp = 0;
    b->bp_hit = 0;
}

void board_tick(struct board *b)
{
    /* 1. CPU executes one instruction */
    cpu_step(&b->cpu, b->flash, b->ram);

    /* 2. Devices tick */
    systick_tick(&b->systick, &b->nvic);

    /* 3. NVIC dispatches highest-priority pending interrupt to CPU */
    nvic_update(&b->nvic, &b->cpu, b->flash, b->ram);

    /* 4. Breakpoint check */
    if (b->nbp > 0) {
        uint32_t pc = b->cpu.r[REG_PC];
        for (int i = 0; i < b->nbp; i++) {
            if (pc == b->breakpoints[i]) { b->bp_hit = 1; return; }
        }
    }
}
