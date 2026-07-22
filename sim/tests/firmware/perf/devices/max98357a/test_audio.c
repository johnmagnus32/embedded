/*
 * test_perf_chardev_audio.c — Audio chardev throughput.
 *
 * Continuously writes stereo samples to I2S via CPU polling.
 * Test runner measures bytes arriving on the audio chardev.
 *
 * At 22050Hz, expect ~22050 samples/sec * 4 bytes = ~88KB/sec.
 * In 2 seconds: ~176KB = ~44100 samples.
 */
#include "test.h"

#define SPI2_SR      (*(volatile unsigned int *)0x40003808)
#define SPI2_DR      (*(volatile unsigned int *)0x4000380C)
#define SPI2_I2SCFGR (*(volatile unsigned int *)0x4000381C)
#define SPI2_I2SPR   (*(volatile unsigned int *)0x40003820)

void test_main(void)
{
    SPI2_I2SCFGR = (1 << 10) | (1 << 11) | (2 << 8);
    SPI2_I2SPR = 0;

    int sample = 0;
    while (1) {
        while (!(SPI2_SR & (1 << 1))) {}
        SPI2_DR = (unsigned int)(sample & 0x7FFF);  /* left */
        while (!(SPI2_SR & (1 << 1))) {}
        SPI2_DR = (unsigned int)(sample & 0x7FFF);  /* right */
        sample++;
    }
}
