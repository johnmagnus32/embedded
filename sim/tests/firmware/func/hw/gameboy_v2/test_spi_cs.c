#include "gameboy_v2_test.h"
void test_main(void) {
    SPI1_CR1 = (1 << 6) | (2 << 3);
    TEST("spi_cs_transitions");
    ppu_send_sprite_frame(0, 0, 0, 0, 0, 0);
    delay(50000);
    ppu_send_sprite_frame(0, 0, 0, 0, 0, 0);
    delay(50000);
    TEST_DONE("test_spi_cs");
}
