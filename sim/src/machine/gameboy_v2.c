/*
 * gameboy_v2.c — STM32F411 + iCE40UP5K PPU + ILI9341
 *
 * SPI bridge: MCU SPI1 bytes → FPGA input pins (bit-banged)
 * LCD tap: FPGA LCD output pins → ILI9341 model (via gpio_line callbacks)
 * Clock: FPGA ticks at 24MHz relative to MCU's 100MHz
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gameboy_v2.h"
#include "machine.h"
#include "stm32_gpio.h"
#include "event_queue.h"
#include "signal_trace.h"
#include "netlist.h"

#define FPGA_CYCLES_PER_SPI_PHASE 6

/* --- SPI bridge: MCU SPI byte → FPGA input pins --- */

static uint8_t spi_to_fpga_transfer(void *opaque, uint8_t byte)
{
    struct gameboy_v2 *b = (struct gameboy_v2 *)opaque;
    signal_trace_byte(b->trace_spi1, byte);
    b->fpga_active = 1;
    b->spi_shift_reg = byte;
    b->spi_bits_remaining = 8;
    b->spi_phase = 0;
    b->spi_tick_counter = 0;
    return 0;
}

static void spi_to_fpga_cs(void *opaque, int level)
{
    struct gameboy_v2 *b = (struct gameboy_v2 *)opaque;
    signal_trace_event(b->trace_spi1, level ? "CS_HIGH" : "CS_LOW");
    b->soc.spis[0].bus.slaves[0].cs_active = !level;
    if (level == 0) {
        b->spi_cs_deassert_pending = 0;
        ice40up5k_set_pin(&b->fpga, b->fpga_spi_cs_pin, 0);
    } else {
        /* Defer CS deassert until the last byte finishes bit-banging.
         * On real hardware, CS is a separate GPIO — firmware can raise it
         * while SPI is still clocking the last byte. But the FPGA samples
         * CS after the final clock edge, so data arrives before CS rises.
         * In the sim, the SPI model delivers bytes instantly (for TXE/RXNE)
         * while the bit-bang runs slower. Deferring CS matches real timing. */
        b->spi_cs_deassert_pending = 1;
    }
}

/* Advance SPI bit-bang by one FPGA cycle (called before each FPGA tick) */
static void spi_bridge_step(struct gameboy_v2 *b)
{
    if (b->spi_bits_remaining == 0) {
        if (b->spi_cs_deassert_pending) {
            b->spi_cs_deassert_pending = 0;
            ice40up5k_set_pin(&b->fpga, b->fpga_spi_cs_pin, 1);
        }
        return;
    }
    b->spi_tick_counter++;
    if (b->spi_tick_counter < FPGA_CYCLES_PER_SPI_PHASE) return;
    b->spi_tick_counter = 0;

    if (b->spi_phase == 0) {
        int bit = (b->spi_shift_reg >> (b->spi_bits_remaining - 1)) & 1;
        ice40up5k_set_pin(&b->fpga, b->fpga_spi_mosi_pin, bit);
        ice40up5k_set_pin(&b->fpga, b->fpga_spi_clk_pin, 0);
        b->spi_phase = 1;
    } else {
        ice40up5k_set_pin(&b->fpga, b->fpga_spi_clk_pin, 1);
        b->spi_phase = 0;
        b->spi_bits_remaining--;
        if (b->spi_bits_remaining == 0)
            ice40up5k_set_pin(&b->fpga, b->fpga_spi_clk_pin, 0);
    }
}

static void lcd_wr_handler(void *opaque, int level)
{
    struct gameboy_v2 *b = (struct gameboy_v2 *)opaque;
    if (level == 1) {
        uint8_t byte = 0;
        for (int i = 0; i < 8; i++)
            byte |= (b->fpga.fpga->nets[b->fpga.pins[b->fpga_lcd_d_pins[i]].net_id] << i);
        signal_trace_byte(b->trace_lcd_wr, byte);
        int dc = b->fpga.fpga->nets[b->fpga.pins[b->fpga_lcd_dc_pin].net_id];
        ili9341_set_dc(&b->display, dc);
        ili9341_transfer(&b->display, byte);
    }
}

static void lcd_cs_handler(void *opaque, int level)
{
    struct gameboy_v2 *b = (struct gameboy_v2 *)opaque;
    if (level == 1) {
        ili9341_flush(&b->display);
    }
}

static void lcd_dc_handler(void *opaque, int level)
{
    struct gameboy_v2 *b = (struct gameboy_v2 *)opaque;
    b->lcd_dc = level;
}

/* --- Periodic callbacks --- */

static void ili9341_refresh_cb(void *opaque);
static void chardev_flush_cb(void *opaque);

/* FPGA config SS (PB6): when deasserted (high), set CDONE (PB2) high */
static void fpga_ss_handler(void *opaque, int level)
{
    struct gameboy_v2 *b = (struct gameboy_v2 *)opaque;
    if (level == 1)
        stm32_gpio_set_input(&b->soc.gpio[1], 2, 1);
}

/* --- Init --- */

void gameboy_v2_init(struct gameboy_v2 *b, struct chardev_table *chardevs)
{
    stm32f411_init(&b->soc, BOARD_V2_SYSCLK_HZ);

    /* UART */
    struct chardev *cd = chardevs ? chardev_find(chardevs, "usart1") : NULL;
    if (cd) stm32_uart_init(&b->soc.usarts[0], cd);

    /* Trace port */
    struct chardev *trace_cd = chardevs ? chardev_find(chardevs, "trace") : NULL;
    trace_dev_init(&b->trace, trace_cd, &b->soc.cpu.cycle_count);
    membus_register(&b->soc.bus, 0xE0000000, 0x04, trace_dev_read, trace_dev_write, &b->trace);

    /* ILI9341 display (driven by FPGA, not MCU) */
    ili9341_init(&b->display);
    b->display.chardev = chardevs ? chardev_find(chardevs, "display") : NULL;
    b->display.cycle_count_ptr = &b->soc.cpu.cycle_count;

    /* MAX98357A audio on I2S2 */
    struct chardev *audio_cd = chardevs ? chardev_find(chardevs, "audio") : NULL;
    max98357a_init(&b->audio);
    b->audio.cd = audio_cd;
    b->soc.spis[1].i2s_sink = &b->audio.sink;

    /* W25Q128 flash on SPI3 */
    w25q128_init(&b->flash);
    b->flash.cycle_count_ptr = &b->soc.cpu.cycle_count;
    int flash_idx = spi_bus_attach(&b->soc.spis[2].bus, &b->flash, w25q128_transfer);
    b->flash.spi_slave = &b->soc.spis[2].bus.slaves[flash_idx];
    b->soc.gpio[1].out[0].handler = w25q128_cs_handler;
    b->soc.gpio[1].out[0].opaque = &b->flash;

    /* SPI3 config bus — null sink for bitstream loading (FPGA already loaded via --device) */
    /* CDONE starts low. Goes high when firmware deasserts SS (PB6) after bitstream. */
    stm32_gpio_set_input(&b->soc.gpio[1], 2, 0);
    b->soc.gpio[1].out[6].handler = fpga_ss_handler;
    b->soc.gpio[1].out[6].opaque = b;

    /* iCE40UP5K on SPI1 — FPGA loaded later via load_device */
    b->fpga_accum = 0;

    /* Board I/O */
    b->io_chardev = chardevs ? chardev_find(chardevs, "io") : NULL;
    b->chardevs = chardevs;

    /* Display flush is driven by LCD_CS rising edge (frame complete) — no timer needed */
    if (chardevs)
        event_schedule(&b->soc.eq, 6, 10000, chardev_flush_cb, b);
}

static void wire_fpga(struct gameboy_v2 *b)
{
    /* Wire MCU SPI1 → FPGA SPI input pins */
    b->fpga_spi_clk_pin  = ice40up5k_find_pin(&b->fpga, "SPI_CLK");
    b->fpga_spi_mosi_pin = ice40up5k_find_pin(&b->fpga, "SPI_MOSI");
    b->fpga_spi_cs_pin   = ice40up5k_find_pin(&b->fpga, "SPI_CS");

    int idx = spi_bus_attach(&b->soc.spis[0].bus, b, spi_to_fpga_transfer);
    b->soc.spis[0].bus.slaves[idx].cs_active = 0;

    /* Pace SPI1 TXE to match FPGA bit-bang speed:
     * Each byte needs 8 bits × 2 phases × FPGA_CYCLES_PER_SPI_PHASE FPGA ticks
     * = 96 FPGA ticks. At 24/16 ratio = 64 MCU ticks per byte. */
    b->soc.spis[0].spi_cycles_per_byte = 8 * 2 * FPGA_CYCLES_PER_SPI_PHASE * BOARD_V2_SYSCLK_HZ / BOARD_V2_FPGA_HZ;
    b->soc.spis[0].spi_cycles_override = 1;

    /* CS driven by GPIO PA4 */
    b->soc.gpio[0].out[4].handler = spi_to_fpga_cs;
    b->soc.gpio[0].out[4].opaque = b;

    /* Wire FPGA LCD output pins → ILI9341 */
    for (int i = 0; i < 8; i++) {
        char name[16];
        snprintf(name, sizeof(name), "LCD_D[%d]", i);
        b->fpga_lcd_d_pins[i] = ice40up5k_find_pin(&b->fpga, name);
    }
    b->fpga_lcd_wr_pin = ice40up5k_find_pin(&b->fpga, "LCD_WR");
    b->fpga_lcd_dc_pin = ice40up5k_find_pin(&b->fpga, "LCD_DC");

    fprintf(stderr, "[gameboy-v2] FPGA wired: SPI(%d,%d,%d) LCD_WR=%d LCD_DC=%d\n",
            b->fpga_spi_clk_pin, b->fpga_spi_mosi_pin, b->fpga_spi_cs_pin,
            b->fpga_lcd_wr_pin, b->fpga_lcd_dc_pin);

    /* Wire LCD callbacks immediately so ILI9341 receives init commands */
    if (b->fpga_lcd_wr_pin >= 0) {
        b->fpga.pins[b->fpga_lcd_wr_pin].out.handler = lcd_wr_handler;
        b->fpga.pins[b->fpga_lcd_wr_pin].out.opaque = b;
    }
    if (b->fpga_lcd_dc_pin >= 0) {
        b->fpga.pins[b->fpga_lcd_dc_pin].out.handler = lcd_dc_handler;
        b->fpga.pins[b->fpga_lcd_dc_pin].out.opaque = b;
    }
    int lcd_cs_pin = ice40up5k_find_pin(&b->fpga, "LCD_CS");
    if (lcd_cs_pin >= 0) {
        b->fpga.pins[lcd_cs_pin].out.handler = lcd_cs_handler;
        b->fpga.pins[lcd_cs_pin].out.opaque = b;
    }
}

/* --- Tick --- */

int gameboy_v2_tick(struct gameboy_v2 *b)
{
    int r = stm32f411_tick(&b->soc);

    /* Tick FPGA at correct clock ratio.
     * Skip until first SPI1 byte arrives — avoids wasting cycles during
     * the 104KB bitstream upload on SPI3 (which doesn't involve the FPGA). */
    if (b->fpga.fpga && b->fpga_active) {
        b->fpga_accum += BOARD_V2_FPGA_HZ;
        while (b->fpga_accum >= BOARD_V2_SYSCLK_HZ) {
            spi_bridge_step(b);
            ice40up5k_tick(&b->fpga);
            b->fpga_accum -= BOARD_V2_SYSCLK_HZ;
        }
    }

    return r;
}

/* --- Callbacks --- */

/* ili9341_refresh_cb removed — flush is triggered by LCD_CS rising edge */

static void chardev_flush_cb(void *opaque)
{
    struct gameboy_v2 *b = (struct gameboy_v2 *)opaque;
    ili9341_send(&b->display);
    chardev_flush_all(b->chardevs);
    event_schedule(&b->soc.eq, 6,
                   b->soc.cpu.cycle_count + 10000,
                   chardev_flush_cb, b);
}

/* --- Load device --- */

static int gameboy_v2_load_device(void *board, const char *name, const char *path)
{
    struct gameboy_v2 *b = (struct gameboy_v2 *)board;
    if (strcmp(name, "fpga0") == 0) {
        ice40up5k_init(&b->fpga, path);
        wire_fpga(b);
        return 0;
    }
    if (strcmp(name, "flash0") == 0) {
        int n = w25q128_load(&b->flash, path);
        if (n < 0) return -1;
        fprintf(stderr, "[gameboy-v2] Loaded %d bytes into W25Q128\n", n);
        return 0;
    }
    fprintf(stderr, "[gameboy-v2] Unknown device: %s\n", name);
    return -1;
}

static void gameboy_v2_set_traces(void *board, struct trace_table *traces)
{
    struct gameboy_v2 *b = (struct gameboy_v2 *)board;
    uint64_t *cyc = &b->soc.cpu.cycle_count;
    b->trace_spi1 = signal_trace_create(trace_find(traces, "spi1"), cyc);
    b->trace_lcd_wr = signal_trace_create(trace_find(traces, "lcd_wr"), cyc);
    b->trace_display = signal_trace_create(trace_find(traces, "display"), cyc);
}

/* --- Machine descriptor --- */

static void gameboy_v2_init_wrap(void *board, struct chardev_table *cd)
{ gameboy_v2_init((struct gameboy_v2 *)board, cd); }

static int gameboy_v2_tick_wrap(void *board)
{ return gameboy_v2_tick((struct gameboy_v2 *)board); }

static struct armv7m_cpu *gameboy_v2_get_cpu(void *board)
{ return &((struct gameboy_v2 *)board)->soc.cpu; }

static struct membus *gameboy_v2_get_bus(void *board)
{ return &((struct gameboy_v2 *)board)->soc.bus; }

static uint8_t **gameboy_v2_get_flash(void *board)
{ return &((struct gameboy_v2 *)board)->soc.flash; }

static uint8_t **gameboy_v2_get_ram(void *board)
{ return &((struct gameboy_v2 *)board)->soc.ram; }

static uint32_t gameboy_v2_get_sysclk(void *board)
{ return ((struct gameboy_v2 *)board)->soc.sysclk_hz; }

const struct machine_desc gameboy_v2_machine = {
    .name        = "gameboy-v2",
    .description = "STM32F411 + iCE40UP5K PPU + ILI9341",
    .board_size  = sizeof(struct gameboy_v2),
    .init        = gameboy_v2_init_wrap,
    .tick        = gameboy_v2_tick_wrap,
    .get_cpu     = gameboy_v2_get_cpu,
    .get_bus     = gameboy_v2_get_bus,
    .get_flash   = gameboy_v2_get_flash,
    .get_ram     = gameboy_v2_get_ram,
    .get_sysclk  = gameboy_v2_get_sysclk,
    .load_device = gameboy_v2_load_device,
    .set_traces  = gameboy_v2_set_traces,
};
