/*
 * sound.c — Audio synthesis via audio driver API
 *
 * No register addresses. Uses audio_start() with a fill callback.
 */

#include "app.h"
#include "drivers/audio.h"
#include "drivers/adc.h"
#include "trace.h"

#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_REST 0

#define SAMPLE_RATE 22050
#define BUF_SAMPLES 256

static const struct { uint16_t freq; uint16_t dur_ms; } melody[] = {
    {NOTE_C4, 200}, {NOTE_E4, 200}, {NOTE_G4, 200}, {NOTE_C5, 400},
    {NOTE_G4, 200}, {NOTE_E4, 200}, {NOTE_C4, 400},
    {NOTE_REST, 200},
    {NOTE_D4, 200}, {NOTE_F4, 200}, {NOTE_A4, 200}, {NOTE_B4, 400},
    {NOTE_A4, 200}, {NOTE_F4, 200}, {NOTE_D4, 400},
    {NOTE_REST, 200},
};
#define MELODY_LEN (sizeof(melody)/sizeof(melody[0]))

/* SFX state */
static volatile int sfx_active;
static volatile uint16_t sfx_freq;
static volatile uint16_t sfx_dur_ms;

void sfx_jump(void) { sfx_freq = 880; sfx_dur_ms = 80; sfx_active = 1; }
void sfx_beep(void) { sfx_freq = 1200; sfx_dur_ms = 50; sfx_active = 1; }

/* Synthesis state */
static uint32_t phase;
static int note_idx;
static uint32_t volume = 2048;
static uint32_t sample_count;

static int16_t square_sample(uint32_t p, uint16_t freq)
{
    if (freq == 0) return 0;
    uint32_t half_period = SAMPLE_RATE / (2 * freq);
    if (half_period == 0) half_period = 1;
    return (p % (half_period * 2)) < half_period ? 8000 : -8000;
}

/* Called from DMA ISR context via I2S driver */
static void fill_audio(int16_t *buf, int count, void *user_data)
{
    trace_begin("audio_fill");
    (void)user_data;

    for (int i = 0; i < count; i++) {
        if (++sample_count >= 256) {
            volume = adc_read(adc_dev);
            sample_count = 0;
        }

        uint16_t freq;
        uint32_t p;
        if (sfx_active) {
            freq = sfx_freq;
            p = i;
        } else {
            freq = melody[note_idx].freq;
            p = phase + i;
        }

        int16_t s = square_sample(p, freq);
        s = (int16_t)((int32_t)s * (int32_t)volume / 4095);
        buf[i] = s;
    }

    /* Advance melody state */
    if (!sfx_active) {
        uint32_t note_samples = (uint32_t)melody[note_idx].dur_ms * SAMPLE_RATE / 1000;
        phase += count;
        if (phase >= note_samples) {
            phase = 0;
            note_idx = (note_idx + 1) % MELODY_LEN;
        }
    } else {
        uint32_t sfx_samples = (uint32_t)sfx_dur_ms * SAMPLE_RATE / 1000;
        if ((uint32_t)count >= sfx_samples)
            sfx_active = 0;
    }
    trace_end();
}

void start_audio(void)
{
    audio_start(audio_dev, fill_audio, 0);
}
