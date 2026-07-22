/*
 * test_fpga_spram.c - SB_SPRAM256KA test
 * Netlist: spram.json (clk, addr[3:0], wdata[7:0], we -> rdata[7:0])
 * Wiring (by port discovery order):
 *   GPIOA[0]=clk, [1..4]=addr[0..3], [5..12]=wdata[0..7], [13]=we
 *   GPIOB[0..7]=rdata[0..7]
 */
#include "test.h"
#include "fpga_test.h"

#define GPIOA_ODR (*(volatile unsigned int *)0x40020014)
#define GPIOB_IDR (*(volatile unsigned int *)0x40020410)

static void spram_write(unsigned int addr, unsigned int data)
{
    /* addr on bits[1..4], wdata on bits[5..12], we on bit 13 */
    GPIOA_ODR = ((addr & 0xF) << 1) | ((data & 0xFF) << 5) | (1u << 13);
    delay(100);
    /* Clear WE, keep addr for read-back */
    GPIOA_ODR = ((addr & 0xF) << 1) | ((data & 0xFF) << 5);
    delay(100);
}

static unsigned int spram_read(unsigned int addr)
{
    /* Set addr, we=0 */
    GPIOA_ODR = ((addr & 0xF) << 1);
    delay(100);  /* need 2 ticks: latch addr + output */
    return GPIOB_IDR & 0xFF;
}

void test_main(void)
{
    TEST("spram_write_read");
    spram_write(0x0, 0xAB);
    CHECK(spram_read(0x0) == 0xAB);

    TEST("spram_different_addr");
    spram_write(0x5, 0x55);
    CHECK(spram_read(0x5) == 0x55);
    CHECK(spram_read(0x0) == 0xAB);

    TEST("spram_overwrite");
    spram_write(0x0, 0x12);
    CHECK(spram_read(0x0) == 0x12);

    TEST_DONE("fpga_spram");
}
