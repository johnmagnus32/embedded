/*
 * sound.c — DMA double-buffered audio synthesis
 *
 * Uses DMA1 Stream 4 to feed SPI2/I2S with audio samples.
 * Double buffer: fill one half while DMA plays the other.
 */

#include "app.h"
#include "drivers/audio.h"
#include "drivers/adc.h"
#include "sched.h"

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

/* DMA1 registers */
#define DMA1_BASE       0x40026000
#define DMA1_LIFCR      (*(volatile uint32_t *)(DMA1_BASE + 0x08))
#define DMA1_S4CR       (*(volatile uint32_t *)(DMA1_BASE + 0x70))
#define DMA1_S4NDTR     (*(volatile uint32_t *)(DMA1_BASE + 0x74))
#define DMA1_S4PAR      (*(volatile uint32_t *)(DMA1_BASE + 0x78))
#define DMA1_S4M0AR     (*(volatile uint32_t *)(DMA1_BASE + 0x7C))

/* SPI2/I2S registers */
#define SPI2_BASE       0x40003800
#define SPI2_CR2        (*(volatile uint32_t *)(SPI2_BASE + 0x04))
#define SPI2_DR         (*(volatile uint32_t *)(SPI2_BASE + 0x0C))

/* NVIC */
#define NVIC_ISER0      (*(volatile uint32_t *)0xE000E100)

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

/* Double buffer: each sample is 16-bit, L and R interleaved = 2 words per stereo sample */
static int16_t audio_buf[2][BUF_SAMPLES];
static volatile int dma_buf_done;  /* which buffer just finished */
static struct wait_queue dma_wq = WAIT_QUEUE_INIT;

void dma1_stream4_handler(void)
{
    /* Clear transfer-complete flag for stream 4 (bit 5 of LISR) */
    DMA1_LIFCR = (1 << 5);
    dma_buf_done = 1;
    sched_wake(&dma_wq);
}

static int16_t square_sample(uint32_t phase, uint16_t freq)
{
    if (freq == 0) return 0;
    uint32_t half_period = SAMPLE_RATE / (2 * freq);
    if (half_period == 0) half_period = 1;
    return (phase % (half_period * 2)) < half_period ? 8000 : -8000;
}

static void i2s_dma_init(void)
{
    /* Enable TXDMAEN in SPI2_CR2 */
    SPI2_CR2 = (1 << 1);

    /* Configure DMA1 Stream 4:
     * - Channel 0 (CHSEL=0)
     * - Mem→Periph (DIR=01)
     * - 16-bit periph & mem size (PSIZE=01, MSIZE=01)
     * - Memory increment (MINC=1)
     * - Circular mode (CIRC=1)
     * - Transfer complete interrupt (TCIE=1)
     */
    DMA1_S4CR = 0; /* disable first */
    DMA1_S4PAR = (uint32_t)&SPI2_DR;
    DMA1_S4M0AR = (uint32_t)audio_buf[0];
    DMA1_S4NDTR = BUF_SAMPLES * 2; /* total samples across both buffers */

    uint32_t cr = (1 << 4)   /* TCIE */
                | (1 << 6)   /* DIR=01: mem→periph */
                | (1 << 8)   /* CIRC */
                | (1 << 10)  /* MINC */
                | (1 << 11)  /* PSIZE=01 (16-bit) */
                | (1 << 13)  /* MSIZE=01 (16-bit) */
                | (1 << 3);  /* HTIE — half transfer interrupt */
    DMA1_S4CR = cr | 1;      /* EN=1 */

    /* Enable DMA1_Stream4 IRQ (IRQ 15) in NVIC */
    NVIC_ISER0 = (1 << 15);
}

void task_audio(void)
{
    uart_print("audio: DMA init\n");

    /* Pre-fill both buffer halves */
    for (int i = 0; i < BUF_SAMPLES * 2; i++)
        ((int16_t *)audio_buf)[i] = 0;

    i2s_dma_init();

    uint32_t phase = 0;
    int note_idx = 0;
    uint32_t volume = 2048;
    uint32_t sample_count = 0;
    int buf_idx = 0;  /* which half to fill next */

    while (1) {
        /* Wait for DMA to finish a buffer */
        while (!dma_buf_done)
            sched_block(&dma_wq);
        dma_buf_done = 0;

        int16_t *buf = audio_buf[buf_idx];
        buf_idx ^= 1;

        /* Fill the buffer */
        for (int i = 0; i < BUF_SAMPLES; i++) {
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
            buf[i] = s;  /* mono: L=R in interleaved DMA */
        }

        /* Advance melody state */
        if (!sfx_active) {
            uint32_t note_samples = (uint32_t)melody[note_idx].dur_ms * SAMPLE_RATE / 1000;
            phase += BUF_SAMPLES;
            if (phase >= note_samples) {
                phase = 0;
                note_idx = (note_idx + 1) % MELODY_LEN;
            }
        } else {
            uint32_t sfx_samples = (uint32_t)sfx_dur_ms * SAMPLE_RATE / 1000;
            if (BUF_SAMPLES >= sfx_samples)
                sfx_active = 0;
        }
    }
}
