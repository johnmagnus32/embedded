Add looping background music and a jump sound effect to the game. The source WAV files are already in `projects/gameboy/assets/`. Work in `/home/johmagnu/learning/simple-stm32/projects/gameboy`. Build with `make`.

## Audio assets

Source files (downloaded from YouTube, already in `assets/`):
- `assets/music.wav` — full background music track
- `assets/jump.wav` — Mario jump sound effect

## Step 1: Convert to raw PCM

Requires `ffmpeg` (install static binary if needed: `curl -L https://johnvansickle.com/ffmpeg/releases/ffmpeg-release-amd64-static.tar.xz | tar xJ -C /tmp && cp /tmp/ffmpeg-*-static/ffmpeg ~/.local/bin/`).

```bash
cd /home/johmagnu/learning/simple-stm32/projects/gameboy

# Music: first 10 seconds, 22050 Hz, 8-bit unsigned, mono (~220KB)
ffmpeg -i assets/music.wav -ss 0 -t 10 -ar 22050 -ac 1 -f u8 -acodec pcm_u8 assets/loop.raw

# Jump SFX: first 0.5 seconds (~11KB)
ffmpeg -i assets/jump.wav -ss 0 -t 0.5 -ar 22050 -ac 1 -f u8 -acodec pcm_u8 assets/sfx_jump.raw
```

Adjust `-ss` (start time) and `-t` (duration) to taste. Preview on Mac with:
```bash
ffmpeg -f u8 -ar 22050 -ac 1 -i assets/loop.raw /tmp/preview.wav && afplay /tmp/preview.wav
```

## Step 2: Generate C headers

```bash
xxd -i assets/loop.raw > src/loop_audio.h
xxd -i assets/sfx_jump.raw > src/sfx_jump_audio.h

# Add 'const' so arrays go in flash (not RAM)
sed -i 's/^unsigned char/const unsigned char/' src/loop_audio.h src/sfx_jump_audio.h
sed -i 's/^unsigned int/const unsigned int/' src/loop_audio.h src/sfx_jump_audio.h
```

## Step 3: Update src/sound.c

Replace the chiptune synthesizer with PCM playback. Background music loops from flash, jump SFX plays on top when triggered:

```c
#include "loop_audio.h"
#include "sfx_jump_audio.h"

static uint32_t audio_pos = 0;

/* SFX state — set by sfx_jump(), consumed by fill_audio */
static volatile uint32_t sfx_pos = 0;
static volatile uint32_t sfx_len = 0;
static volatile const unsigned char *sfx_data = NULL;

void sfx_jump(void)
{
    sfx_data = assets_sfx_jump_raw;
    sfx_len = assets_sfx_jump_raw_len;
    sfx_pos = 0;
}

void sfx_beep(void) { sfx_jump(); }

static void fill_audio(int16_t *buf, int count, void *user_data)
{
    uint32_t volume = adc_read(adc_dev);

    for (int i = 0; i < count; i++) {
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
        buf[i] = mix;
    }
}
```

## Step 4: Build and verify

```bash
make
# text should be ~239KB (code + audio), well under 512KB flash
```

## Step 5: Test

```bash
cd /home/johmagnu/learning/simple-stm32/sim
./sim ../projects/gameboy/build/gameboy.elf
```

Open http://localhost:3000. Click or press any key to enable audio (browser requirement). You should hear the background music looping and the jump sound when pressing A (z key / spacebar).

## Flash budget

| Asset | Size |
|-------|------|
| Music loop (10s, 22kHz, 8-bit) | 220 KB |
| Jump SFX (0.5s, 22kHz, 8-bit) | 11 KB |
| Firmware code + data | ~8 KB |
| **Total** | **~239 KB / 512 KB** |

## .gitignore

```
assets/music.wav
assets/jump.wav
assets/loop.raw
assets/sfx_jump.raw
```

Commit `src/loop_audio.h` and `src/sfx_jump_audio.h` (the C arrays). The WAV and raw files are derived/downloaded and can be regenerated.
