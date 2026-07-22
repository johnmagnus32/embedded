#include "gameboy_v2_test.h"
void test_main(void) {
    SPI1_CR1 = (1 << 6) | (2 << 3);
    TEST("fpga_clock_ratio");
    delay(160000);
    TEST_DONE("test_fpga_ticking");
}
