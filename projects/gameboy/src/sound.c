/*
 * sound.c — PCM audio playback with sound effects
 *
 * Background music: 10-second loop from flash (loop_audio.h)
 * Jump SFX: 0.5-second sample from flash (sfx_jump_audio.h)
 * Both mixed together, volume controlled by ADC potentiometer.
 */

#include "app.h"
#include "drivers/audio.h"
#include "drivers/adc.h"
#include "trace.h"
#include "loop_audio.h"
#include "sfx_jump_audio.h"

static uint32_t audio_pos = 0;

/* SFX state — set by sfx_jump/sfx_beep, consumed by fill_audio */
static volatile uint32_t sfx_pos = 0;
static volatile uint32_t sfx_len = 0;
static volatile const unsigned char *sfx_data = NULL;

void sfx_jump(void)
{
    sfx_data = assets_sfx_jump_raw;
    sfx_len = assets_sfx_jump_raw_len;
    sfx_pos = 0;
}

void sfx_beep(void)
{
    sfx_jump();  /* reuse jump sound */
}

static uint32_t volume = 2048;
static uint32_t sample_count = 0;

/* Called from DMA ISR context via I2S driver.
 * Buffer is sent to I2S as sequential 16-bit words.
 * I2S alternates left/right, so we must interleave: L,R,L,R,...
 * For mono playback, write each sample twice. */
static void fill_audio(int16_t *buf, int count, void *user_data)
{
    trace_begin("audio_fill");
    (void)user_data;

    /* count is the number of 16-bit words in the buffer.
     * Since I2S is stereo, we get count/2 sample slots. */
    int nsamples = count / 2;

    for (int i = 0; i < nsamples; i++) {
        if (++sample_count >= 256) {
            volume = adc_read(adc_dev);
            sample_count = 0;
        }

        /* Background music from PCM loop */
        int16_t music = ((int16_t)assets_loop_raw[audio_pos] - 128) * 200;
        audio_pos = (audio_pos + 1) % assets_loop_raw_len;

        /* Sound effect from PCM sample */
        int16_t sfx = 0;
        if (sfx_pos < sfx_len && sfx_data) {
            sfx = ((int16_t)sfx_data[sfx_pos] - 128) * 256;
            sfx_pos++;
        }

        /* Mix and apply volume */
        int16_t mix = music + sfx;
        mix = (int16_t)((int32_t)mix * (int32_t)volume / 4095);

        /* Stereo interleave: same sample for left and right */
        buf[i * 2]     = mix;  /* left */
        buf[i * 2 + 1] = mix;  /* right */
    }
    trace_end();
}

void start_audio(void)
{
    audio_start(audio_dev, fill_audio, 0);
}
