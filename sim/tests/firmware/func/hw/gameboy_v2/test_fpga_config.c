#include "gameboy_v2_test.h"
void test_main(void) {
    TEST("fpga_cdone_after_ss");
    /* Simulate bitstream load: assert SS low, then high */
    /* PB6 = SS pin */
    *(volatile unsigned int *)0x40020418 = (1 << (6 + 16));  /* BSRR: reset PB6 */
    delay(1000);
    *(volatile unsigned int *)0x40020418 = (1 << 6);  /* BSRR: set PB6 (deassert) */
    delay(1000);
    /* Now CDONE (PB2) should be high */
    int cdone = (GPIOB_IDR >> 2) & 1;
    CHECK(cdone == 1);
    TEST_DONE("test_fpga_config");
}
