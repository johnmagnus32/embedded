/*
 * sound.c — Audio synthesis task
 *
 * Plays chiptune melody with ADC volume control.
 * No register addresses — uses audio and ADC driver APIs.
 */

#include "app.h"
#include "drivers/audio.h"
#include "drivers/adc.h"

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

static const struct { uint16_t freq; uint16_t dur_ms; } melody[] = {
    {NOTE_C4, 200}, {NOTE_E4, 200}, {NOTE_G4, 200}, {NOTE_C5, 400},
    {NOTE_G4, 200}, {NOTE_E4, 200}, {NOTE_C4, 400},
    {NOTE_REST, 200},
    {NOTE_D4, 200}, {NOTE_F4, 200}, {NOTE_A4, 200}, {NOTE_B4, 400},
    {NOTE_A4, 200}, {NOTE_F4, 200}, {NOTE_D4, 400},
    {NOTE_REST, 200},
};
#define MELODY_LEN (sizeof(melody)/sizeof(melody[0]))

/* SFX state — set from ISR, consumed by audio task */
static volatile int sfx_active;
static volatile uint16_t sfx_freq;
static volatile uint16_t sfx_dur_ms;

void sfx_jump(void) { sfx_freq = 880; sfx_dur_ms = 80; sfx_active = 1; }
void sfx_beep(void) { sfx_freq = 1200; sfx_dur_ms = 50; sfx_active = 1; }

static int16_t square_sample(uint32_t phase, uint16_t freq)
{
    if (freq == 0) return 0;
    uint32_t half_period = SAMPLE_RATE / (2 * freq);
    if (half_period == 0) half_period = 1;
    return (phase % (half_period * 2)) < half_period ? 8000 : -8000;
}

void task_audio(void)
{
    uart_print("audio: init\n");
    uint32_t phase = 0;
    int note_idx = 0;
    uint32_t volume = 2048;
    uint32_t sample_count = 0;

    while (1) {
        /* Check for SFX trigger */
        if (sfx_active) {
            uint16_t f = sfx_freq;
            uint32_t samples = (uint32_t)sfx_dur_ms * SAMPLE_RATE / 1000;
            sfx_active = 0;
            for (uint32_t i = 0; i < samples; i++) {
                if (++sample_count >= 256) {
                    volume = adc_read(adc_dev);
                    sample_count = 0;
                }
                int16_t s = square_sample(i, f);
                s = (int16_t)((int32_t)s * (int32_t)volume / 4095);
                audio_write_sample(audio_dev, s, s);
            }
        }

        /* Play melody */
        uint16_t freq = melody[note_idx].freq;
        uint32_t samples = (uint32_t)melody[note_idx].dur_ms * SAMPLE_RATE / 1000;
        for (uint32_t i = 0; i < samples; i++) {
            if (++sample_count >= 256) {
                volume = adc_read(adc_dev);
                sample_count = 0;
            }
            int16_t s = square_sample(phase + i, freq);
            s = (int16_t)((int32_t)s * (int32_t)volume / 4095);
            audio_write_sample(audio_dev, s, s);
        }
        phase += samples;
        note_idx = (note_idx + 1) % MELODY_LEN;
    }
}
