#include "gameboy_v2_test.h"
void test_main(void) {
    SPI1_CR1 = (1 << 6) | (2 << 3);
    TEST("lcd_wr_active");
    delay(200000);
    TEST_DONE("test_lcd_wr_strobes");
}
