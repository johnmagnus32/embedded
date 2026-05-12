Refactor the audio driver API from a callback model (driver calls into app) to a buffer submission model (app drives the loop). This matches Zephyr's I2S API and moves DMA/semaphore concerns out of application code. Work in `/home/johmagnu/learning/simple-stm32/rtos/drivers/i2s` and `rtos/include/drivers/audio.h`. Test with `make -C projects/gameboy`.

## Problem

The current API requires the application to provide a callback that runs in ISR context:

```c
// Current: driver calls app from ISR
audio_start(dev, fill_audio_isr, user_data);

// App must provide:
void fill_audio_isr(int16_t *buf, int count, void *user_data) {
    // Can't do SPI reads here (ISR context!)
    // Must signal a semaphore and defer to a task
    pending_buf_ptr = buf;
    sem_give(&audio_sem);
}
```

This forces the app to manage its own semaphore, track which buffer half is pending, and split logic between ISR callback and task. The ISR-to-task handoff is boilerplate that every audio user would need to repeat.

## Goal

Replace with a blocking buffer API where the app just loops:

```c
// New: app drives the loop, no callbacks
audio_start(dev);
for (;;) {
    int16_t *buf = audio_get_buffer(dev);  // blocks until DMA frees a half-buffer
    fill_buffer(buf, AUDIO_BUF_SAMPLES);   // app fills at leisure (task context, SPI ok)
    audio_release_buffer(dev, buf);          // done filling, DMA can have it back
}
```

No callbacks, no semaphore in app code, no ISR concerns leaking into the application. The driver owns the double-buffering and synchronization internally.

## New API (`rtos/include/drivers/audio.h`)

```c
#ifndef DRIVERS_AUDIO_H
#define DRIVERS_AUDIO_H

#include "device.h"
#include <stdint.h>

#define AUDIO_BUF_SAMPLES 128  /* samples per half-buffer (stereo pairs) */

struct audio_driver_api {
    int      (*start)(const struct device *dev);
    int      (*stop)(const struct device *dev);
    int16_t *(*get_buffer)(const struct device *dev);
    void     (*release_buffer)(const struct device *dev, int16_t *buf);
};

/* Start DMA playback (initially silent until first release_buffer) */
static inline int audio_start(const struct device *dev)
{
    const struct audio_driver_api *api = dev->api;
    return api->start(dev);
}

/* Stop DMA playback. Unblocks any thread waiting in get_buffer. */
static inline int audio_stop(const struct device *dev)
{
    const struct audio_driver_api *api = dev->api;
    return api->stop(dev);
}

/*
 * Get a free buffer half to fill. Blocks until DMA releases one.
 * Returns NULL if audio_stop() was called while waiting.
 */
static inline int16_t *audio_get_buffer(const struct device *dev)
{
    const struct audio_driver_api *api = dev->api;
    return api->get_buffer(dev);
}

/* Release a filled buffer back to the driver for DMA playback. */
static inline void audio_release_buffer(const struct device *dev, int16_t *buf)
{
    const struct audio_driver_api *api = dev->api;
    api->release_buffer(dev, buf);
}

#endif
```

## Driver internals (`rtos/drivers/i2s/i2s_stm32.c`)

The driver owns the double-buffer, DMA, and semaphore:

```c
struct i2s_stm32_data {
    int16_t buf[2][AUDIO_BUF_SAMPLES * 2];  /* two half-buffers, stereo */
    struct semaphore buf_free;               /* signaled when a half is consumed by DMA */
    int write_idx;                           /* which half the app is filling (0 or 1) */
    int playing;
};

/* DMA half/complete ISR — internal, not exposed to app */
static void i2s_dma_isr(void *arg)
{
    struct i2s_stm32_data *data = arg;
    sem_give(&data->buf_free);  /* wake up whoever is blocked in get_buffer */
}

static int i2s_start(const struct device *dev)
{
    struct i2s_stm32_data *data = dev->data;
    /* Zero both buffers (silence) */
    memset(data->buf, 0, sizeof(data->buf));
    data->write_idx = 0;
    data->playing = 1;
    /* Start DMA circular on buf[0]+buf[1] with half-transfer interrupt */
    dma_start_circular(data->buf, sizeof(data->buf), i2s_dma_isr, data);
    return 0;
}

static int i2s_stop(const struct device *dev)
{
    struct i2s_stm32_data *data = dev->data;
    data->playing = 0;
    dma_stop(...);
    sem_give(&data->buf_free);  /* unblock any thread waiting in get_buffer */
    return 0;
}

static int16_t *i2s_get_buffer(const struct device *dev)
{
    struct i2s_stm32_data *data = dev->data;
    sem_take(&data->buf_free);  /* block until DMA finishes a half */
    if (!data->playing) return NULL;  /* stopped — caller should exit loop */
    return data->buf[data->write_idx];
}

static void i2s_release_buffer(const struct device *dev, int16_t *buf)
{
    struct i2s_stm32_data *data = dev->data;
    (void)buf;  /* buffer is in-place in DMA ring — no copy needed */
    data->write_idx ^= 1;  /* swap to other half for next get_buffer */
}
```

## Application code (simplified `audio.c`)

```c
void audio_task(void)
{
    /* Read PCM length from flash header */
    flash_read(DEVICE_DT_GET(w25q128), 0, &flash_total, 4);
    flash_offset = 4;
    stream_pos = stream_len = 0;

    audio_start(audio_dev);

    for (;;) {
        int16_t *buf = audio_get_buffer(audio_dev);
        if (!buf) break;  /* audio_stop() was called — exit cleanly */
        stream_refill();
        for (int i = 0; i < AUDIO_BUF_SAMPLES; i++) {
            int16_t sample = ((int16_t)stream_buf[stream_pos++] - 128) * 50;
            if (stream_pos >= stream_len) stream_refill();
            buf[i * 2] = buf[i * 2 + 1] = sample;
        }
        audio_release_buffer(audio_dev, buf);
    }
}
```

No semaphore, no callback, no ISR awareness. Just: get buffer → fill → release → repeat.
Returns cleanly if `audio_stop()` is called from another task (e.g., upload mode).

## What moves where

| Concern | Before (in app) | After (in driver) |
|---------|-----------------|-------------------|
| Double-buffer memory | App allocated | Driver owns |
| Semaphore | App creates + manages | Driver internal |
| DMA ISR callback | App provides `fill_audio_isr` | Driver's internal `i2s_dma_isr` |
| Buffer index tracking | App tracks `pending_buf_idx` | Driver tracks `write_idx` |
| "Which buffer to fill" | App decides from ISR signal | Driver returns correct pointer from `get_buffer` |
| Stop unblocking | Not handled (hang forever) | `stop()` gives semaphore, `get_buffer` returns NULL |

## Changes summary

| File | Change |
|------|--------|
| `rtos/include/drivers/audio.h` | REWRITE: remove `audio_fill_fn` callback, add `get_buffer`/`release_buffer`, document NULL return |
| `rtos/drivers/i2s/i2s_stm32.c` | REWRITE: own the buffers + semaphore, expose blocking get/release, stop unblocks waiters |
| `projects/gameboy/src/audio.c` | SIMPLIFY: remove semaphore, remove ISR callback, loop on get/release, handle NULL |

## Testing

| Test | What it verifies |
|------|-----------------|
| Build | `make -C projects/gameboy` succeeds |
| Sim audio | Run in MCU sim, verify audio chardev still outputs PCM samples |
| Blocking behavior | `audio_get_buffer` blocks until DMA half-complete (verify with trace timestamps) |
| No ISR work in app | Confirm no app code runs in interrupt context (only driver ISR touches semaphore) |
| Looping | Music loops seamlessly when flash stream wraps |

## Verification checklist

- [ ] `audio_fill_fn` typedef removed from API
- [ ] No semaphore in application audio code
- [ ] No ISR callback registered by application
- [ ] Driver owns buffer memory and synchronization
- [ ] `audio_get_buffer` blocks correctly (doesn't busy-wait)
- [ ] `audio_get_buffer` returns NULL after `audio_stop()` is called
- [ ] `audio_stop` unblocks any thread waiting in `get_buffer`
- [ ] `audio_release_buffer` advances write index (no copy)
- [ ] MCU sim audio output unchanged (same PCM data reaches chardev)
- [ ] SFX mixing still works (app mixes into the buffer between get/release)
- [ ] Upload mode can stop audio cleanly without hanging the audio task
