/*
 * test_ice40_spi_integrity.c — Reproduce gate-level evaluator SPI bit error
 *
 * Drives the production PPU netlist through the ice40up5k evaluator using
 * the same SPI sequence as the firmware (bg_color command), and checks that
 * the received value matches what was sent.
 *
 * EXPECTED: bg_color = 0xF800 after sending CMD_BG_COLOR(0x03) + 0xF8 + 0x00
 * ACTUAL BUG: bg_color = 0x7800 (MSB of second byte lost — 1-bit shift error)
 *
 * This proves the bug is in eval.c/netlist.c, not in the Verilog or SPI bridge.
 * Verilator produces the correct result (0xF800) for the same Verilog.
 *
 * Build: make -C .. test_ice40_spi_integrity
 * Run:   ./test_ice40_spi_integrity <path-to-ppu_top.json>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ice40up5k.h"
#include "netlist.h"

#define CYCLES_PER_SPI_PHASE 6

static struct ice40up5k fpga;
static int pin_clk, pin_mosi, pin_cs;

static void send_byte(uint8_t byte)
{
    for (int bit = 7; bit >= 0; bit--) {
        ice40up5k_set_pin(&fpga, pin_mosi, (byte >> bit) & 1);
        ice40up5k_set_pin(&fpga, pin_clk, 0);
        for (int i = 0; i < CYCLES_PER_SPI_PHASE; i++)
            ice40up5k_tick(&fpga);
        ice40up5k_set_pin(&fpga, pin_clk, 1);
        for (int i = 0; i < CYCLES_PER_SPI_PHASE; i++)
            ice40up5k_tick(&fpga);
    }
    ice40up5k_set_pin(&fpga, pin_clk, 0);
    for (int i = 0; i < CYCLES_PER_SPI_PHASE; i++)
        ice40up5k_tick(&fpga);
}

/*
 * Read bg_color from the FPGA netlist.
 * Yosys optimizes away bits whose value is fixed by the initial state.
 * We read via the lcd_data DFF outputs by monitoring LCD_WR strobes,
 * which captures the ACTUAL rendered pixel values sent to the display.
 */
static uint16_t capture_first_pixel(void)
{
    int wr_pin = ice40up5k_find_pin(&fpga, "LCD_WR");
    int dc_pin = ice40up5k_find_pin(&fpga, "LCD_DC");
    int d_pins[8];
    for (int i = 0; i < 8; i++) {
        char name[16];
        snprintf(name, sizeof(name), "LCD_D[%d]", i);
        d_pins[i] = ice40up5k_find_pin(&fpga, name);
    }

    /* Run until we see the 0x2C command (memory write start) then capture first pixel */
    int saw_2c = 0;
    int pixel_bytes = 0;
    uint8_t hi_byte = 0;
    uint8_t prev_wr = 0;

    for (int t = 0; t < 2000000; t++) {
        prev_wr = fpga.pins[wr_pin].level;
        ice40up5k_tick(&fpga);

        uint8_t cur_wr = fpga.fpga->nets[fpga.pins[wr_pin].net_id];
        if (cur_wr && !prev_wr) {
            /* WR rising edge */
            uint8_t byte = 0;
            for (int i = 0; i < 8; i++)
                byte |= (fpga.fpga->nets[fpga.pins[d_pins[i]].net_id] << i);
            uint8_t dc = fpga.fpga->nets[fpga.pins[dc_pin].net_id];

            if (!dc) {
                if (byte == 0x2C) saw_2c = 1;
                else saw_2c = 0;
            } else if (saw_2c) {
                if (pixel_bytes == 0) { hi_byte = byte; pixel_bytes++; }
                else return (uint16_t)(hi_byte << 8) | byte;
            }
        }
    }
    return 0xDEAD; /* timeout */
}

int main(int argc, char **argv)
{
    const char *netlist = (argc > 1) ? argv[1] : "../projects/gameboy-v2/build/ppu_top.json";

    ice40up5k_init(&fpga, netlist);

    pin_clk  = ice40up5k_find_pin(&fpga, "SPI_CLK");
    pin_mosi = ice40up5k_find_pin(&fpga, "SPI_MOSI");
    pin_cs   = ice40up5k_find_pin(&fpga, "SPI_CS");

    if (pin_clk < 0 || pin_mosi < 0 || pin_cs < 0) {
        fprintf(stderr, "FAIL: could not find SPI pins\n");
        return 1;
    }

    /* --- Test: bg_color via SPI command 0x03 → verify via LCD output --- */

    /* Start idle: CS high, CLK low */
    ice40up5k_set_pin(&fpga, pin_cs, 1);
    ice40up5k_set_pin(&fpga, pin_clk, 0);
    ice40up5k_set_pin(&fpga, pin_mosi, 0);
    for (int i = 0; i < 20; i++) ice40up5k_tick(&fpga);

    /* Assert CS */
    ice40up5k_set_pin(&fpga, pin_cs, 0);
    for (int i = 0; i < 20; i++) ice40up5k_tick(&fpga);

    /* Send: CMD_BG_COLOR=0x03, high=0xF8, low=0x00 → bg_color should be 0xF800 */
    send_byte(0x03);
    send_byte(0xF8);
    send_byte(0x00);

    /* Settle and deassert CS */
    for (int i = 0; i < 20; i++) ice40up5k_tick(&fpga);
    ice40up5k_set_pin(&fpga, pin_cs, 1);
    for (int i = 0; i < 20; i++) ice40up5k_tick(&fpga);

    /* Send a frame_valid (sprite update with 0 sprites) to trigger rendering */
    ice40up5k_set_pin(&fpga, pin_cs, 0);
    for (int i = 0; i < 20; i++) ice40up5k_tick(&fpga);
    send_byte(0x01);  /* CMD_SPRITE_UPDATE */
    send_byte(0x00);  /* 0 sprites */
    for (int i = 0; i < 20; i++) ice40up5k_tick(&fpga);
    ice40up5k_set_pin(&fpga, pin_cs, 1);
    for (int i = 0; i < 20; i++) ice40up5k_tick(&fpga);

    /* Now capture the first pixel from the LCD output.
     * The LCD driver renders continuously; after setting bg_color and triggering
     * frame_valid, the next frame should output pixels with the new bg_color. */
    uint16_t pixel = capture_first_pixel();
    printf("test_bg_color_f800: got=0x%04X expected=0xF800 ... %s\n",
           pixel, pixel == 0xF800 ? "PASS" : "FAIL");
    int failed = (pixel != 0xF800);

    /* --- Summary --- */
    if (failed)
        printf("\nFAILED — gate-level evaluator corrupts SPI→LCD pixel data\n");
    else
        printf("\nPASSED — SPI data integrity OK\n");

    ice40up5k_free(&fpga);
    return failed ? 1 : 0;
}
