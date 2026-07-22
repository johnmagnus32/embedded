#include "gameboy_v2_test.h"
void test_main(void) {
    SPI1_CR1 = (1 << 6) | (2 << 3);
    TEST("lcd_bg_color");
    ppu_set_bg_color(0xF800);
    delay(500000);
    TEST_DONE("test_lcd_bg_color");
}
