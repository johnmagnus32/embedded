Add a 5-second looping audio clip as background music for the game. Work in `/home/johmagnu/learning/simple-stm32/projects/gameboy`. Build with `make` from `projects/gameboy/`.

## Prerequisites

`xxd` is required (usually pre-installed). For downloading from YouTube, `yt-dlp` and `ffmpeg` are needed but YouTube may block downloads from cloud desktops. As a fallback, audio can be generated programmatically with Python.

Install tools (if downloading from YouTube):
```bash
# yt-dlp standalone binary
curl -L https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp -o ~/.local/bin/yt-dlp
chmod +x ~/.local/bin/yt-dlp

# ffmpeg static binary
curl -L https://johnvansickle.com/ffmpeg/releases/ffmpeg-release-amd64-static.tar.xz -o /tmp/ffmpeg.tar.xz
cd /tmp && tar xf ffmpeg.tar.xz
cp /tmp/ffmpeg-*-amd64-static/ffmpeg ~/.local/bin/ffmpeg
```

## Step 1: Create audio assets

### Option A: Download from YouTube (may require browser cookies)
```bash
cd /home/johmagnu/learning/simple-stm32/projects/gameboy
mkdir -p assets
yt-dlp -x --audio-format wav "https://www.youtube.com/watch?v=CGsLkosT6HI" -o assets/music_full.wav
yt-dlp -x --audio-format wav "https://www.youtube.com/watch?v=JjzesX_uMlQ" -o assets/sfx_full.wav
ffmpeg -i assets/music_full.wav -ss 0 -t 10 -ar 22050 -ac 1 -f u8 -acodec pcm_u8 assets/loop.raw
ffmpeg -i assets/sfx_full.wav -ss 0 -t 1 -ar 22050 -ac 1 -f u8 -acodec pcm_u8 assets/sfx_jump.raw
```

### Option B: Generate programmatically (no download needed — works on any machine)
```bash
cd /home/johmagnu/learning/simple-stm32/projects/gameboy
mkdir -p assets

# 10-second chiptune melody loop
python3 -c "
RATE = 22050; DUR = 10
notes = [262, 330, 392, 523, 392, 330, 262, 196]
note_dur = RATE // 4
samples = []
for i in range(RATE * DUR):
    freq = notes[(i // note_dur) % len(notes)]
    phase = (i * freq / RATE) % 1.0
    samples.append(180 if phase < 0.5 else 76)
open('assets/loop.raw','wb').write(bytes(samples))
print(f'{len(samples)} samples ({len(samples)/RATE:.1f}s)')
"

# 0.5-second rising-pitch jump sound
python3 -c "
RATE = 22050; DUR = 0.5
samples = []
for i in range(int(RATE * DUR)):
    t = i / RATE
    freq = 200 + 600 * (t / DUR)
    phase = (i * freq / RATE) % 1.0
    env = 1.0 - (t / DUR)
    samples.append(max(0, min(255, int(128 + 80 * env * (1 if phase < 0.5 else -1)))))
open('assets/sfx_jump.raw','wb').write(bytes(samples))
print(f'{len(samples)} samples ({len(samples)/RATE:.1f}s)')
"
```

## Step 2: Update the audio fill callback

In `src/sound.c`, replace the chiptune synthesizer with PCM playback. Background music loops from the stored clip, jump sound effect plays from the stored sample:

```c
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
    sfx_jump();  /* reuse jump sound for now */
}

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

## Step 3: Verify flash usage

After building, check that the audio data fits:
```bash
make
# Check the size output:
#    text    data     bss     dec     hex filename
#  118170       8   35096  153274   25xxx build/gameboy.elf
#
# text section includes code + const data (the audio loop)
# Should be well under 512KB (524288 bytes)
```

If the audio is too large, reduce the clip length:
```bash
# 3 seconds instead of 5:
ffmpeg -i audio_src.wav -ss 0 -t 3 -ar 22050 -ac 1 -f u8 -acodec pcm_u8 assets/loop.raw
xxd -i assets/loop.raw > src/loop_audio.h
# Edit to add const
```

Or reduce sample rate to 11025 Hz (lower quality but half the size):
```bash
ffmpeg -i audio_src.wav -ss 0 -t 5 -ar 11025 -ac 1 -f u8 -acodec pcm_u8 assets/loop.raw
```
If using 11025 Hz, update the I2S sample rate configuration to match.

## Step 4: Test

```bash
cd /home/johmagnu/learning/simple-stm32/sim
./sim ../projects/gameboy/build/gameboy.elf
```

Open http://localhost:3000. You should hear the 5-second clip looping as background music. Button presses should trigger sound effects mixed on top. The volume slider should control overall volume.

## Files changed/added

- `assets/loop.raw` — raw PCM background music (gitignore this)
- `assets/sfx_jump.raw` — raw PCM jump sound effect (gitignore this)
- `src/loop_audio.h` — C array generated from loop.raw (commit this)
- `src/sfx_jump_audio.h` — C array generated from sfx_jump.raw (commit this)
- `src/sound.c` — updated fill_audio callback with PCM playback + mixing

## .gitignore additions

```
assets/loop.raw
assets/sfx_jump.raw
audio_src.wav
sfx_jump_src.wav
```
