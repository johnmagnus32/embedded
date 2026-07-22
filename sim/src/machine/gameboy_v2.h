#ifndef GAMEBOY_V2_H
#define GAMEBOY_V2_H

#include "stm32f411.h"
#include "ili9341.h"
#include "trace_dev.h"
#include "chardev.h"
#include "max98357a.h"
#include "w25q128.h"
#include "ice40up5k.h"

struct signal_trace;

#define BOARD_V2_SYSCLK_HZ  16000000
#define BOARD_V2_MCU_HZ     100000000
#define BOARD_V2_FPGA_HZ     24000000

struct gameboy_v2 {
    struct stm32f411  soc;
    struct ili9341    display;
    struct trace_dev  trace;
    struct max98357a  audio;
    struct w25q128    flash;
    struct ice40up5k  fpga;
    struct chardev   *io_chardev;
    struct chardev_table *chardevs;

    /* FPGA clock ratio accumulator */
    uint64_t fpga_accum;
    int fpga_ticked_this_cycle;
    int fpga_active;            /* starts 0, set to 1 on first SPI1 byte */

    /* SPI bridge state */
    int fpga_spi_clk_pin;
    int fpga_spi_mosi_pin;
    int fpga_spi_cs_pin;

    /* SPI bit-bang shift register (clocked by gameboy_v2_tick) */
    uint8_t spi_shift_reg;
    int spi_bits_remaining;
    int spi_phase;          /* 0=setup MOSI+CLK low, 1=CLK high */
    int spi_tick_counter;   /* counts FPGA ticks per SPI phase */
    int spi_cs_deassert_pending;
#define SPI_FIFO_SIZE 16
    uint8_t spi_fifo[SPI_FIFO_SIZE];
    int spi_fifo_count;

    /* LCD tap state */
    int fpga_lcd_wr_pin;
    int fpga_lcd_dc_pin;
    int fpga_lcd_d_pins[8];
    uint8_t lcd_dc;

    /* Signal traces */
    struct signal_trace *trace_spi1;
    struct signal_trace *trace_lcd_wr;
    struct signal_trace *trace_display;
};

void gameboy_v2_init(struct gameboy_v2 *b, struct chardev_table *chardevs);
int  gameboy_v2_tick(struct gameboy_v2 *b);

struct machine_desc;
extern const struct machine_desc gameboy_v2_machine;

#endif
