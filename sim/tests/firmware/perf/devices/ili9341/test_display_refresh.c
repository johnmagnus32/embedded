/*
 * test_perf_display_refresh.c — Verify ILI9341 60Hz refresh on chardev.
 *
 * Firmware does nothing but idle. The ILI9341's internal refresh timer
 * pushes frames to the display chardev at 60Hz. The test runner connects
 * to the chardev and counts frames received over 2 seconds.
 *
 * Each frame is: 4-byte header (width_lo, width_hi, height_lo, height_hi)
 *                + width*height*2 bytes of RGB565 pixel data.
 * For 240x320: 4 + 153,600 = 153,604 bytes per frame.
 *
 * Expected: ~120 frames in 2 seconds (60 FPS).
 * Asserts: at least 50 FPS (allows for slow CI hosts).
 */
#include "test.h"

void test_main(void)
{
    /* Just idle — the ILI9341 refresh timer does the work */
    for (volatile int i = 0; i < 100000000; i++) {}
    semi_exit(0);
}
