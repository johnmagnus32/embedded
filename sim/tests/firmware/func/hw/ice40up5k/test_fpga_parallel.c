/*
 * test_fpga_parallel.c - 8-bit parallel output register
 * Netlist: parallel_out.json (clk, din[7:0], load -> dout[7:0])
 * Wiring: GPIOA[0]=clk, [1..8]=din[0..7], [9]=load
 *         GPIOB[0..7]=dout[0..7]
 */
#include "test.h"
#include "fpga_test.h"

#define GPIOA_ODR (*(volatile unsigned int *)0x40020014)
#define GPIOB_IDR (*(volatile unsigned int *)0x40020410)

static void load_byte(unsigned int data)
{
    /* Set din + load=1 */
    GPIOA_ODR = ((data & 0xFF) << 1) | (1u << 9);
    delay(100);  /* clock edge captures */
    GPIOA_ODR &= ~(1u << 9);  /* clear load */
}

static unsigned int read_dout(void) { return GPIOB_IDR & 0xFF; }

void test_main(void)
{
    TEST("parallel_load_a5");
    load_byte(0xA5);
    CHECK(read_dout() == 0xA5);

    TEST("parallel_load_3c");
    load_byte(0x3C);
    CHECK(read_dout() == 0x3C);

    TEST("parallel_hold");
    /* Don't load, just tick — output should hold */
    GPIOA_ODR = (0xFF << 1);  /* din=0xFF but load=0 */
    delay(100);
    CHECK(read_dout() == 0x3C);  /* still previous value */

    TEST("parallel_bit0");
    load_byte(0x01);
    CHECK(read_dout() == 0x01);

    TEST("parallel_bit7");
    load_byte(0x80);
    CHECK(read_dout() == 0x80);

    TEST_DONE("fpga_parallel");
}
