/*
 * fpga_test.c - Minimal STM32 + iCE40UP5K for FPGA device tests.
 *
 * Generic index-based wiring:
 *   FPGA input ports  -> driven by MCU GPIOA output pins (by discovery order)
 *   FPGA output ports -> drive MCU GPIOB input pins (by discovery order)
 *
 * The FPGA runs free at its own clock rate relative to the MCU, just like
 * real hardware. Firmware uses NOP delay loops to wait for FPGA to settle.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fpga_test.h"
#include "machine.h"
#include "stm32_gpio.h"

struct fpga_test_ctx {
    struct fpga_test *board;
    int fpga_input_pins[32];
    int fpga_output_pins[32];
    int num_inputs;
    int num_outputs;
};

static struct fpga_test_ctx ctx;

/* MCU GPIOA output changed -> drive corresponding FPGA input pin */
static void mcu_to_fpga_handler(void *opaque, int level)
{
    int fpga_pin_idx = (int)(intptr_t)opaque;
    ice40up5k_set_pin(&ctx.board->fpga, fpga_pin_idx, level);
}

/* FPGA output pin changed -> set MCU GPIOB input */
static void fpga_to_mcu_handler(void *opaque, int level)
{
    int mcu_pin = (int)(intptr_t)opaque;
    stm32_gpio_set_input(&ctx.board->soc.gpio[1], mcu_pin, level);
}

static void wire_fpga(struct fpga_test *b)
{
    ctx.board = b;
    ctx.num_inputs = 0;
    ctx.num_outputs = 0;

    for (int i = 0; i < b->fpga.num_pins; i++) {
        if (b->fpga.pins[i].direction == 0) {
            int mcu_pin = ctx.num_inputs;
            ctx.fpga_input_pins[ctx.num_inputs++] = i;
            b->soc.gpio[0].out[mcu_pin].handler = mcu_to_fpga_handler;
            b->soc.gpio[0].out[mcu_pin].opaque = (void *)(intptr_t)i;
        } else {
            int mcu_pin = ctx.num_outputs;
            ctx.fpga_output_pins[ctx.num_outputs++] = i;
            b->fpga.pins[i].out.handler = fpga_to_mcu_handler;
            b->fpga.pins[i].out.opaque = (void *)(intptr_t)mcu_pin;
        }
    }

    fprintf(stderr, "[fpga-test] Wired: %d inputs (GPIOA), %d outputs (GPIOB)\n",
            ctx.num_inputs, ctx.num_outputs);

    /* Sync current FPGA output pin levels to MCU GPIOB */
    for (int i = 0; i < ctx.num_outputs; i++) {
        int fpga_pin = ctx.fpga_output_pins[i];
        stm32_gpio_set_input(&b->soc.gpio[1], i, b->fpga.pins[fpga_pin].level);
    }
}

void fpga_test_init(struct fpga_test *b, struct chardev_table *chardevs)
{
    stm32f411_init(&b->soc, FPGA_TEST_SYSCLK_HZ);
    b->fpga_accum = 0;

    /* Trace port for semihosting */
    struct chardev *trace_cd = chardevs ? chardev_find(chardevs, "trace") : NULL;
    trace_dev_init(&b->trace, trace_cd, &b->soc.cpu.cycle_count);
    membus_register(&b->soc.bus, 0xE0000000, 0x04, trace_dev_read, trace_dev_write, &b->trace);
}

int fpga_test_tick(struct fpga_test *b)
{
    int r = stm32f411_tick(&b->soc);

    /* Tick FPGA at correct clock ratio (48MHz FPGA / 16MHz MCU = 3:1) */
    if (b->fpga.fpga) {
        b->fpga_accum += FPGA_TEST_FPGA_HZ;
        while (b->fpga_accum >= FPGA_TEST_SYSCLK_HZ) {
            ice40up5k_tick(&b->fpga);
            b->fpga_accum -= FPGA_TEST_SYSCLK_HZ;
        }
    }

    return r;
}

static int fpga_test_load_device(void *board, const char *name, const char *path)
{
    struct fpga_test *b = (struct fpga_test *)board;
    if (strcmp(name, "fpga0") == 0) {
        ice40up5k_init(&b->fpga, path);
        wire_fpga(b);
        return 0;
    }
    fprintf(stderr, "[fpga-test] Unknown device: %s\n", name);
    return -1;
}

/* Machine descriptor */
static void fpga_test_init_wrap(void *board, struct chardev_table *cd)
{ fpga_test_init((struct fpga_test *)board, cd); }

static int fpga_test_tick_wrap(void *board)
{ return fpga_test_tick((struct fpga_test *)board); }

static struct armv7m_cpu *fpga_test_get_cpu(void *board)
{ return &((struct fpga_test *)board)->soc.cpu; }

static struct membus *fpga_test_get_bus(void *board)
{ return &((struct fpga_test *)board)->soc.bus; }

static uint8_t **fpga_test_get_flash(void *board)
{ return &((struct fpga_test *)board)->soc.flash; }

static uint8_t **fpga_test_get_ram(void *board)
{ return &((struct fpga_test *)board)->soc.ram; }

static uint32_t fpga_test_get_sysclk(void *board)
{ return FPGA_TEST_SYSCLK_HZ; }

const struct machine_desc fpga_test_machine = {
    .name        = "fpga-test",
    .description = "STM32F411 + iCE40UP5K (generic test wiring)",
    .board_size  = sizeof(struct fpga_test),
    .init        = fpga_test_init_wrap,
    .tick        = fpga_test_tick_wrap,
    .get_cpu     = fpga_test_get_cpu,
    .get_bus     = fpga_test_get_bus,
    .get_flash   = fpga_test_get_flash,
    .get_ram     = fpga_test_get_ram,
    .get_sysclk  = fpga_test_get_sysclk,
    .load_device = fpga_test_load_device,
};
