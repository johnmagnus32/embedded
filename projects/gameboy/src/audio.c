/*
 * audio.c — PCM audio playback from external W25Q128 flash
 *
 * Uses the buffer submission API:
 *   audio_get_buffer() — blocks until DMA frees a half-buffer
 *   fill the buffer (SPI flash reads are safe — we're in task context)
 *   audio_put_buffer() — hand back to DMA
 */

#include "board.h"
#include "audio.h"
#include "drivers/audio.h"
#include "drivers/flash.h"

/* Flash streaming state */
#define STREAM_BUF_SIZE 512
static uint8_t stream_buf[STREAM_BUF_SIZE];
static uint32_t stream_pos;
static uint32_t stream_len;
static uint32_t flash_offset;
static uint32_t flash_total;

/* SFX state */
static volatile uint32_t sfx_pos = 0;
static volatile uint32_t sfx_len = 0;
static volatile int sfx_active = 0;

DEVICE_DT_DECLARE(w25q128);

void sfx_jump(void)
{
    sfx_len = 4410;
    sfx_pos = 0;
    sfx_active = 1;
}

/* Convert unsigned 8-bit PCM (0-255) to signed 16-bit */
static inline int16_t pcm_u8_to_s16(uint8_t sample)
{
    return ((int16_t)sample - 128) * 255;
}

/* Read next 512 bytes of music from flash, looping at the end */
static void flash_read_next_chunk(void)
{
    if (stream_pos < stream_len) return;
    if (flash_total == 0) return;

    uint32_t filled = 0;
    while (filled < STREAM_BUF_SIZE) {
        uint32_t remaining = (flash_total + 4) - flash_offset;
        if (remaining == 0) {
            flash_offset = 4;
            remaining = flash_total;
        }
        uint32_t chunk = STREAM_BUF_SIZE - filled;
        if (chunk > remaining) chunk = remaining;
        flash_read(DEVICE_DT_GET(w25q128), flash_offset, stream_buf + filled, chunk);
        flash_offset += chunk;
        filled += chunk;
    }
    stream_pos = 0;
    stream_len = filled;
}

/*
 * Audio task — get buffer, fill it, put it back. Repeat forever.
 */
void audio_task(void)
{
    flash_read(DEVICE_DT_GET(w25q128), 0, &flash_total, 4);

    if (flash_total == 0 || flash_total > 0x01000000) {
        flash_total = 0;
        uart_print("audio: no music in flash\n");
    } else {
        uart_print("audio: ");
        print_int(flash_total);
        uart_print(" bytes in flash\n");
    }

    flash_offset = 4;
    stream_pos = 0;
    stream_len = 0;

    audio_start(audio_dev);

    for (;;) {
        int16_t *buf = audio_get_buffer(audio_dev);
        if (!buf) break;

        flash_read_next_chunk();

        for (int i = 0; i < AUDIO_BUF_SAMPLES; i++) {
            int16_t music = pcm_u8_to_s16(stream_buf[stream_pos++]);

            int16_t sfx = 0;
            if (sfx_active && sfx_pos < sfx_len) {
                uint32_t p = sfx_pos;
                uint32_t half = sfx_len / 2;
                uint32_t freq = (p < half) ? (200 + p * 800 / half) : (1000 - (p - half) * 800 / half);
                uint32_t period = 22050 / freq;
                sfx = ((p % period) < period / 2) ? 12000 : -12000;
                sfx = (int16_t)((int32_t)sfx * (int32_t)(sfx_len - p) / (int32_t)sfx_len);
                sfx_pos++;
                if (sfx_pos >= sfx_len) sfx_active = 0;
            }

            int16_t out = music + sfx;
            buf[i * 2]     = out;  // left
            buf[i * 2 + 1] = out;  // right
        }

        audio_put_buffer(audio_dev, buf);
    }
}
