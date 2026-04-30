Diagnose why the display animation is not smooth in the simulator at `/home/johmagnu/learning/simple-stm32/sim`. The game animation was smooth before audio was added and is now noticeably choppy. The goal is to identify exactly what is causing the slowdown and quantify it. Read `src/main.c`, `src/machine/gameboy.c`, `src/devices/max98357a.c`, `src/devices/ili9341.c`, and `src/hw/stm32/stm32_dma.c` before making changes. Build with `make` from `sim/`.

## Problem

The game display animation is choppy after adding audio (DMA + MAX98357A). Before audio was added, the animation was smooth. Something in the audio path is stealing enough time from the main tick loop to cause visible frame drops or inconsistent frame timing.

## Part 1: Add display frame timing instrumentation

Add a `display_perf` struct to `ili9341` (or alongside it) that tracks frame output timing. Update `ili9341_flush` to record wall-clock time each time a frame is sent to the display chardev:

```c
struct display_perf {
    uint64_t frame_count;
    struct timespec last_frame_time;
    double intervals_ms[256];    /* rolling window of frame intervals */
    int idx;
    int count;
    double min_ms, max_ms, sum_ms;
    int glitch_count;            /* intervals > 2× average */
};
```

In `ili9341_flush`, after sending the frame:
- Compute interval since last frame in milliseconds
- Update min, max, rolling sum
- If interval > 2× rolling average, increment glitch count
- Every 100 frames, print a summary to stderr:

```
[display] 28.3 FPS | min=31.2ms avg=35.3ms max=82.1ms | glitches: 3/100
```

## Part 2: Add per-subsystem wall-clock breakdown

Instrument `gameboy_tick` to measure where host wall-clock time is spent. Accumulate into static counters, print every 1M ticks:

```
[perf] 8.7 MIPS | cpu=78% audio=15% io=1% other=6%
```

Measure these separately:
- `stm32f411_tick` (all CPU + peripherals)
- `max98357a_tick` (audio pull + chardev write)
- `gameboy_poll_io` (button input)

## Part 3: Add benchmark mode

Add `--bench <cycles>` flag. When set, skip the debug server. Load firmware, reset CPU, run N ticks, print results, exit. Also add `--no-chardev` flag to skip all chardev setup.

## Part 4: Run isolation tests and write results

Run these tests to isolate the cause. Write all results to `sim/PERF.md`.

**Test A: Baseline without audio**
Temporarily comment out the `max98357a_tick` call in `gameboy_tick`. Rebuild. Run:
```bash
./build/sim-core --machine gameboy --firmware ../../projects/gameboy/build/gameboy.elf --bench 10000000 --no-chardev
```
Record MIPS and display stats.

**Test B: With audio tick, no chardev consumers**
Restore `max98357a_tick`. Rebuild. Run same bench command (no sim-web.py, chardevs have no clients):
```bash
./build/sim-core --machine gameboy --firmware ../../projects/gameboy/build/gameboy.elf --bench 10000000 --no-chardev
```
Record MIPS and display stats. Compare to Test A — the difference is the overhead of `max98357a_tick` itself (membus reads, DMA counter management, `clock_gettime` calls).

**Test C: With audio tick + audio chardev, no consumer**
```bash
./build/sim-core --machine gameboy --firmware ../../projects/gameboy/build/gameboy.elf --bench 10000000 --chardev audio=9005
```
No sim-web.py connecting. Chardev listens but has no client, so `chardev_write_buf` is a no-op. Compare to Test B — if MIPS is the same, TCP writes aren't the issue.

**Test D: With audio tick + audio chardev + consumer**
Start sim-web.py, let it connect to the audio chardev. Run bench. Compare to Test C — if MIPS drops, the blocking TCP write to sim-web.py is the bottleneck.

**Test E: Full stack display smoothness**
Run the full simulator with sim-web.py and the web UI. Let the game run for 30 seconds. Record the display perf output (FPS, min/avg/max interval, glitch count). Then comment out `max98357a_tick`, rebuild, run again for 30 seconds. Compare the two — this directly measures whether audio causes display glitches.

## Part 5: Write PERF.md

```markdown
# Display Smoothness Investigation

Date: <today>
Host: <uname -a>

## Problem
Game animation is choppy after adding audio support.

## Isolation Tests

### Test A: No audio tick
MIPS: <>
Display: <FPS> FPS, avg interval <>ms, glitches <N>

### Test B: Audio tick, no chardevs
MIPS: <>
Display: <FPS> FPS, avg interval <>ms, glitches <N>
**Overhead vs Test A: <>% MIPS drop**

### Test C: Audio tick + audio chardev, no consumer
MIPS: <>
**Overhead vs Test B: <>% MIPS drop**

### Test D: Audio tick + audio chardev + consumer
MIPS: <>
**Overhead vs Test C: <>% MIPS drop**

### Test E: Full stack display smoothness (30 seconds)
With audio:
  FPS: <>, interval: min=<>ms avg=<>ms max=<>ms, glitches: <N>/<total>

Without audio:
  FPS: <>, interval: min=<>ms avg=<>ms max=<>ms, glitches: <N>/<total>

## Subsystem Breakdown (Test B, 10M ticks)
| Subsystem | % wall time |
|-----------|-------------|
| cpu_step  |             |
| audio     |             |
| io_poll   |             |
| other     |             |

## Conclusion
The bottleneck is: <membus reads in max98357a_tick / TCP chardev writes / clock_gettime syscalls / other>
Recommended fix: <>
```
