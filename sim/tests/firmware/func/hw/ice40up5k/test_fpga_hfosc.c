/*
 * test_fpga_hfosc.c - SB_HFOSC internal oscillator test
 * Netlist: hfosc.json (no inputs, count[3:0] output)
 * Wiring: GPIOB[0..3]=count[0..3]
 * The FPGA clocks itself via SB_HFOSC (free-running).
 */
#include "test.h"
#include "fpga_test.h"

#define GPIOB_IDR (*(volatile unsigned int *)0x40020410)

static unsigned int read_count(void) { return GPIOB_IDR & 0x0F; }

void test_main(void)
{
    TEST("hfosc_advances");
    unsigned int v1 = read_count();
    delay(200);
    unsigned int v2 = read_count();
    CHECK(v2 != v1);

    TEST("hfosc_keeps_counting");
    delay(200);
    unsigned int v3 = read_count();
    CHECK(v3 != v2);

    TEST_DONE("fpga_hfosc");
}
