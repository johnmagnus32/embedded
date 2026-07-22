/*
 * test_fpga_spi.c - SPI slave protocol test
 * Netlist: spi_slave.json (clk, spi_clk, spi_mosi, spi_cs -> reg_out[7:0])
 * Wiring: GPIOA[0]=clk, [1]=spi_clk, [2]=spi_mosi, [3]=spi_cs
 *         GPIOB[0..7]=reg_out[0..7]
 *
 * The SPI slave uses clock domain crossing (3-stage synchronizer on spi_clk).
 * We need to tick the FPGA clock multiple times per SPI clock edge to let
 * the synchronizer propagate.
 */
#include "test.h"
#include "fpga_test.h"

#define GPIOA_ODR (*(volatile unsigned int *)0x40020014)
#define GPIOB_IDR (*(volatile unsigned int *)0x40020410)

#define PIN_SCLK (1u << 1)
#define PIN_MOSI (1u << 2)
#define PIN_CS   (1u << 3)

static unsigned int base_odr;  /* tracks current pin state */

static void spi_send_byte(unsigned char byte)
{
    /* Assert CS (active low: cs=0) */
    base_odr &= ~PIN_CS;
    GPIOA_ODR = base_odr;
    delay(100);

    for (int bit = 7; bit >= 0; bit--) {
        /* Set MOSI */
        if ((byte >> bit) & 1)
            base_odr |= PIN_MOSI;
        else
            base_odr &= ~PIN_MOSI;

        /* SCLK low (setup) */
        base_odr &= ~PIN_SCLK;
        GPIOA_ODR = base_odr;
        delay(100);

        /* SCLK high (capture) */
        base_odr |= PIN_SCLK;
        GPIOA_ODR = base_odr;
        delay(100);
    }

    /* SCLK low */
    base_odr &= ~PIN_SCLK;
    GPIOA_ODR = base_odr;
    delay(100);

    /* Deassert CS */
    base_odr |= PIN_CS;
    GPIOA_ODR = base_odr;
    delay(100);
}

static unsigned int read_reg(void) { return GPIOB_IDR & 0xFF; }

void test_main(void)
{
    /* Init: CS high (deasserted), SCLK low */
    base_odr = PIN_CS;
    GPIOA_ODR = base_odr;
    delay(100);

    TEST("spi_send_a5");
    spi_send_byte(0xA5);
    CHECK(read_reg() == 0xA5);

    TEST("spi_send_3c");
    spi_send_byte(0x3C);
    CHECK(read_reg() == 0x3C);

    TEST("spi_send_00");
    spi_send_byte(0x00);
    CHECK(read_reg() == 0x00);

    TEST("spi_send_ff");
    spi_send_byte(0xFF);
    CHECK(read_reg() == 0xFF);

    TEST_DONE("fpga_spi");
}
