Measure display FPS and frame timing in the full simulator stack at `/home/johmagnu/learning/simple-stm32/sim`. The goal is to get actual FPS numbers and frame consistency data with and without audio, using the display instrumentation already in `ili9341_flush`. Read `src/devices/ili9341.c` to understand the existing `[display]` logging.

## Problem

All previous benchmarks used `--bench --no-chardev`, which meant no display chardev was connected and `ili9341_flush` returned early without printing frame stats. We have FPS instrumentation but have never actually captured its output.

## Setup

You need two terminals. The simulator must run with sim-web.py so the display chardev has a consumer and `ili9341_flush` actually sends frames and prints stats.

## Test 1: Full stack WITH audio (current state)

Terminal 1:
```bash
cd /home/johmagnu/learning/simple-stm32/sim
./sim ../projects/gameboy/build/gameboy.elf 2>fps_with_audio.log
```

Terminal 2 (or browser):
- Open http://localhost:3000
- Click "continue" to start the game running
- Let it run for 60 seconds
- Press Ctrl+C in terminal 1 to stop

The `[display]` lines from `ili9341_flush` and `[perf]` lines will be captured in `fps_with_audio.log`.

Extract the display stats:
```bash
grep '\[display\]' fps_with_audio.log > fps_with_audio_display.txt
grep '\[perf\]' fps_with_audio.log > fps_with_audio_perf.txt
```

## Test 2: Full stack WITHOUT audio

Temporarily comment out the `max98357a_tick(&b->audio)` call in `gameboy_tick` (in `src/machine/gameboy.c`). Also comment out `audio_start` in the firmware's `main()` (in `projects/gameboy/src/main.c`) so the firmware doesn't try to configure DMA for audio. Rebuild both:

```bash
cd /home/johmagnu/learning/simple-stm32/sim && make
cd /home/johmagnu/learning/simple-stm32/projects/gameboy && make
```

Run the same test:
```bash
cd /home/johmagnu/learning/simple-stm32/sim
./sim ../projects/gameboy/build/gameboy.elf 2>fps_without_audio.log
```

Let it run for 60 seconds with the game running, Ctrl+C, extract:
```bash
grep '\[display\]' fps_without_audio.log > fps_without_audio_display.txt
grep '\[perf\]' fps_without_audio.log > fps_without_audio_perf.txt
```

**Restore the commented-out code after this test.** Rebuild both.

## Test 3: Full stack with audio but clock_gettime instrumentation removed

The perf profile showed `clock_gettime` consuming 62% of host CPU. Remove or reduce the per-tick `clock_gettime` calls in `gameboy_tick` (the subsystem timing instrumentation). Either:
- Comment out the timing code entirely, or
- Change it to only sample every 100,000 ticks instead of every tick

Rebuild and run:
```bash
cd /home/johmagnu/learning/simple-stm32/sim && make
./sim ../projects/gameboy/build/gameboy.elf 2>fps_no_clockgettime.log
```

60 seconds, extract display and perf lines.

## Capture results

After all three tests, write the results to `sim/PERF.md` (append to existing content):

```markdown
## Display FPS Measurements (Full Stack, 60 seconds)

### Test 1: With audio + perf instrumentation
```
<paste last 10 [display] lines from fps_with_audio_display.txt>
```
Average FPS: <>
Min interval: <>ms
Max interval: <>ms
Avg interval: <>ms
Glitches: <> out of <> frames (<>%)
MIPS (from [perf] lines): <>

### Test 2: Without audio
```
<paste last 10 [display] lines from fps_without_audio_display.txt>
```
Average FPS: <>
Min interval: <>ms
Max interval: <>ms
Avg interval: <>ms
Glitches: <> out of <> frames (<>%)
MIPS: <>

### Test 3: With audio, clock_gettime instrumentation removed
```
<paste last 10 [display] lines from fps_no_clockgettime_display.txt>
```
Average FPS: <>
Min interval: <>ms
Max interval: <>ms
Avg interval: <>ms
Glitches: <> out of <> frames (<>%)
MIPS: <>

### Summary

| Metric | With audio | Without audio | Audio + no clock_gettime |
|--------|-----------|---------------|--------------------------|
| FPS | | | |
| Avg interval (ms) | | | |
| Max interval (ms) | | | |
| Glitch % | | | |
| MIPS | | | |

### Conclusion
<Which change had the biggest impact on FPS and smoothness?>
<Is the remaining choppiness from firmware audio ISR, emulator overhead, or something else?>
<What should be optimized next?>
```

## Important

- Restore all temporary code changes after testing
- The `./sim` script handles starting sim-core and sim-web.py together
- Make sure to click "continue" in the web UI so the game is actually running during the test
- 60 seconds gives enough data for stable averages (should be ~1800 frames at 30 FPS)
