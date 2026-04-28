/*
 * board.c — STM32 board simulation, configured from device tree
 */
#include <stdio.h>
#include <string.h>
#include "board.h"

/* RCC stub — reads return 0, writes ignored */
static uint32_t rcc_read(void *opaque, uint32_t offset)  { (void)opaque; (void)offset; return 0; }
static void     rcc_write(void *opaque, uint32_t offset, uint32_t val) { (void)opaque; (void)offset; (void)val; }

/* SPI catch-all stub for unconfigured SPI ranges */
static uint32_t spi_stub_read(void *opaque, uint32_t offset)  { (void)opaque; (void)offset; return 0; }
static void     spi_stub_write(void *opaque, uint32_t offset, uint32_t val) { (void)opaque; (void)offset; (void)val; }

void board_init(struct board *b, const struct dts *dt, struct chardev_table *chardevs)
{
    cpu_init(&b->cpu);
    nvic_init(&b->nvic);
    systick_init(&b->systick);
    b->nuarts = 0;
    b->nspis = 0;
    b->ngpio = 0;
    b->display = NULL;
    b->sysclk_hz = dt->sysclk_hz ? dt->sysclk_hz : 16000000;

    /* Flash and RAM will be set by main.c after allocation.
     * membus_init is called later in main.c after flash/ram are allocated.
     * We zero the bus here for safety. */
    memset(&b->bus, 0, sizeof(b->bus));

    /* Trace port — always at 0xE0000000 */
    struct chardev *trace_cd = chardevs ? chardev_find(chardevs, "trace") : NULL;
    trace_dev_init(&b->trace, trace_cd, &b->cpu.cycle_count);

    /* First pass: create UARTs and SPIs from DTS */
    for (int i = 0; i < dt->nnodes; i++) {
        const struct dts_node *n = &dt->nodes[i];

        if (strcmp(n->compatible, "st,stm32-usart") == 0 && n->has_reg) {
            if (b->nuarts < MAX_UARTS) {
                struct chardev *cd = chardevs ? chardev_find(chardevs, n->label) : NULL;
                fprintf(stderr, "[board] UART '%s' at 0x%08X%s\n",
                        n->label, n->reg, cd ? " (chardev)" : "");
                uart_init(&b->uarts[b->nuarts], cd);
                /* Register on membus later in board_setup_membus */
                b->nuarts++;
            }
        }

        if (strcmp(n->compatible, "st,stm32-spi") == 0 && n->has_reg) {
            if (b->nspis < MAX_SPIS) {
                fprintf(stderr, "[board] SPI '%s' at 0x%08X\n", n->label, n->reg);
                spi_init(&b->spis[b->nspis]);
                b->nspis++;
            }
        }
    }

    /* Initialize GPIO ports: GPIOA at 0x40020000, GPIOB at 0x40021000 */
    gpio_init(&b->gpio[0]);
    gpio_init(&b->gpio[1]);
    b->ngpio = 2;

    /* Second pass: attach SPI slave devices */
    int dc_gpio_pin = -1;
    for (int i = 0; i < dt->nnodes; i++) {
        const struct dts_node *n = &dt->nodes[i];

        if (strcmp(n->compatible, "ilitek,ili9341") == 0) {
            /* Find the SPI bus by matching spi_bus to DTS reg addresses */
            struct spi *bus = NULL;
            /* Re-scan DTS to find which spi index has this reg */
            int si = 0;
            for (int j = 0; j < dt->nnodes; j++) {
                if (strcmp(dt->nodes[j].compatible, "st,stm32-spi") == 0 && dt->nodes[j].has_reg) {
                    if (dt->nodes[j].reg == n->spi_bus) { bus = &b->spis[si]; break; }
                    si++;
                }
            }
            if (bus) {
                static struct ili9341 display;
                ili9341_init(&display);
                display.chardev = chardevs ? chardev_find(chardevs, "display") : NULL;
                spi_attach(bus, &display, ili9341_transfer, NULL);
                b->display = &display;
                dc_gpio_pin = n->dc_pin;
                fprintf(stderr, "[board] ILI9341 on SPI 0x%08X, DC pin %d\n", n->spi_bus, n->dc_pin);
            }
        }
    }

    /* Wire GPIO pin to ILI9341 DC line */
    if (dc_gpio_pin >= 0 && dc_gpio_pin < 16 && b->display) {
        /* Assume DC pin is on GPIOA (port 0) — same as original code */
        b->gpio[0].out[dc_gpio_pin].handler = ili9341_set_dc;
        b->gpio[0].out[dc_gpio_pin].opaque = b->display;
    }
}

/* Called from main.c after flash/ram are allocated to register all devices on the membus */
void board_init_membus(struct board *b, const struct dts *dt)
{
    membus_init(&b->bus, b->flash, b->ram);

    /* Trace dev at 0xE0000000, size 4 (single register) */
    membus_register(&b->bus, 0xE0000000, 0x04, trace_dev_read, trace_dev_write, &b->trace);

    /* UARTs — re-scan DTS to get base addresses */
    int ui = 0;
    for (int i = 0; i < dt->nnodes && ui < b->nuarts; i++) {
        const struct dts_node *n = &dt->nodes[i];
        if (strcmp(n->compatible, "st,stm32-usart") == 0 && n->has_reg) {
            membus_register(&b->bus, n->reg, 0x20, uart_read, uart_write, &b->uarts[ui]);
            ui++;
        }
    }

    /* SPIs */
    int si = 0;
    for (int i = 0; i < dt->nnodes && si < b->nspis; i++) {
        const struct dts_node *n = &dt->nodes[i];
        if (strcmp(n->compatible, "st,stm32-spi") == 0 && n->has_reg) {
            membus_register(&b->bus, n->reg, 0x24, spi_read, spi_write, &b->spis[si]);
            si++;
        }
    }

    /* SysTick at 0xE000E010, size 0x10 */
    membus_register(&b->bus, 0xE000E010, 0x10, systick_read, systick_write, &b->systick);

    /* NVIC ISER at 0xE000E100, size 0x10 */
    membus_register(&b->bus, 0xE000E100, 0x10, nvic_iser_read, nvic_iser_write, &b->nvic);

    /* NVIC SCB at 0xE000ED00, size 0xA4 (covers up to MPU TYPE at 0xED90) */
    membus_register(&b->bus, 0xE000ED00, 0xA4, nvic_scb_read, nvic_scb_write, &b->nvic);

    /* GPIO ports */
    membus_register(&b->bus, 0x40020000, 0x1000, gpio_read, gpio_write, &b->gpio[0]);
    membus_register(&b->bus, 0x40021000, 0x1000, gpio_read, gpio_write, &b->gpio[1]);

    /* RCC stub */
    membus_register(&b->bus, 0x40023800, 0x100, rcc_read, rcc_write, NULL);

    /* SPI catch-all stub for unconfigured SPI ranges */
    membus_register(&b->bus, 0x40013000, 0x100, spi_stub_read, spi_stub_write, NULL);
}

void board_tick(struct board *b)
{
    cpu_step(&b->cpu, &b->bus);
    systick_tick(&b->systick, &b->nvic);
    nvic_update(&b->nvic, &b->cpu, &b->bus);
}
