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
    b->nspis = 0;
    b->display = NULL;
    b->dc_gpio_base = 0;
    b->dc_gpio_pin = -1;
    b->sysclk_hz = dt->sysclk_hz ? dt->sysclk_hz : 16000000;

    /* Trace port — always at 0xE0000000 */
    struct chardev *trace_cd = chardevs ? chardev_find(chardevs, "trace") : NULL;
    trace_dev_init(&b->trace, 0xE0000000, trace_cd);

    for (int i = 0; i < dt->nnodes; i++) {
        const struct dts_node *n = &dt->nodes[i];

        if (strcmp(n->compatible, "st,stm32-usart") == 0 && n->has_reg) {
            if (b->nuarts < MAX_UARTS) {
                struct chardev *cd = chardevs ? chardev_find(chardevs, n->label) : NULL;
                fprintf(stderr, "[board] UART '%s' at 0x%08X%s\n",
                        n->label, n->reg, cd ? " (chardev)" : "");
                uart_init(&b->uarts[b->nuarts++], n->reg, cd);
            }
        }

        if (strcmp(n->compatible, "st,stm32-spi") == 0 && n->has_reg) {
            if (b->nspis < MAX_SPIS) {
                fprintf(stderr, "[board] SPI '%s' at 0x%08X\n", n->label, n->reg);
                spi_init(&b->spis[b->nspis++], n->reg);
            }
        }
    }

    /* Second pass: attach SPI slave devices */
    for (int i = 0; i < dt->nnodes; i++) {
        const struct dts_node *n = &dt->nodes[i];

        if (strcmp(n->compatible, "ilitek,ili9341") == 0) {
            /* Find the SPI bus */
            struct spi *bus = NULL;
            for (int s = 0; s < b->nspis; s++) {
                if (b->spis[s].base == n->spi_bus) { bus = &b->spis[s]; break; }
            }
            if (bus) {
                static struct ili9341 display;
                ili9341_init(&display);
                /* Attach display chardev for framebuffer streaming */
                display.chardev = chardevs ? chardev_find(chardevs, "display") : NULL;
                spi_attach(bus, &display, ili9341_transfer, NULL);
                b->display = &display;
                if (n->dc_pin >= 0) b->dc_gpio_pin = n->dc_pin;
                fprintf(stderr, "[board] ILI9341 on SPI 0x%08X, DC pin %d\n", n->spi_bus, n->dc_pin);
            }
        }
    }
}

void board_tick(struct board *b)
{
    cpu_step(&b->cpu, b->flash, b->ram);
    systick_tick(&b->systick, &b->nvic);
    nvic_update(&b->nvic, &b->cpu, b->flash, b->ram);
    /* Flush display ~30Hz (every 500K cycles at 16MHz) */
    if (b->display && (b->cpu.cycle_count & 0x7FFFF) == 0)
        ili9341_flush(b->display);
}
