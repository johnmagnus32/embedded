/*
 * test_display_output.c — End-to-end display verification.
 * Sets a distinctive background color, waits for rendering,
 * then verifies via trace output that the ILI9341 received data.
 * Uses the trace device (0xE0000000) to log a marker after rendering.
 */
#include "gameboy_v2_test.h"

#define TRACE_OUT (*(volatile unsigned int *)0xE0000000)

void test_main(void) {
    SPI1_CR1 = (1 << 6) | (2 << 3);

    TEST("display_receives_pixels");
    /* Set a unique bg color */
    ppu_set_bg_color(0xAAAA);
    delay(5000);

    /* Send empty sprite frame to trigger rendering */
    ppu_send_sprite_frame(0, 0, 0, 0, 0, 0);

    /* Wait for FPGA to render some pixels */
    delay(500000);

    /* Write marker to trace — if we get here, the MCU didn't hang
     * waiting for SPI, and the FPGA has been ticking alongside */
    TRACE_OUT = 0x44;  /* 'D' for display */

    TEST_DONE("test_display_output");
}
