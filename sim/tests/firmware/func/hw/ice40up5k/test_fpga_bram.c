/*
 * test_fpga_bram.c - SB_RAM40_4K block RAM test
 * Netlist: bram.json (clk, we, addr[7:0], wdata[7:0] -> rdata[7:0])
 *
 * Wiring: GPIOA[0]=clk, [1]=we, [2..9]=addr[0..7], [10..15]=wdata[0..5]
 *         GPIOB[0..7]=rdata[0..7]
 */
#include "test.h"
#include "fpga_test.h"

#define GPIOA_ODR (*(volatile unsigned int *)0x40020014)
#define GPIOB_IDR (*(volatile unsigned int *)0x40020410)

static void bram_write(unsigned int addr, unsigned int data)
{
    GPIOA_ODR = (1u << 1) | ((addr & 0xFF) << 2) | ((data & 0x3F) << 10);
    delay(100);
    GPIOA_ODR = ((addr & 0xFF) << 2) | ((data & 0x3F) << 10);
    delay(100);
}

static unsigned int bram_read(unsigned int addr)
{
    GPIOA_ODR = ((addr & 0xFF) << 2);
    delay(100);
    return GPIOB_IDR & 0xFF;
}

void test_main(void)
{
    TEST("bram_write_read");
    bram_write(0x00, 0x3F);
    CHECK(bram_read(0x00) == 0x3F);

    TEST("bram_overwrite");
    bram_write(0x00, 0x15);
    CHECK(bram_read(0x00) == 0x15);

    TEST("bram_addr2");
    bram_write(0x02, 0x2A);
    CHECK(bram_read(0x02) == 0x2A);

    TEST("bram_addr2_no_clobber");
    CHECK(bram_read(0x00) == 0x15);

    TEST_DONE("fpga_bram");
}
