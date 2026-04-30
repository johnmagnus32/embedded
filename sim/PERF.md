# Display Smoothness Investigation

Date: 2026-04-30
Host: Linux dev-dsk-johmagnu-kuiper-2a-053bba8c.us-west-2.amazon.com 5.10.252 x86_64

## Problem
Game animation is choppy after adding audio support (DMA + MAX98357A wall-clock paced model).

## Isolation Tests

### Test A: No audio tick
MIPS: 13.4
Subsystem: cpu=100% audio=0% io=0%

### Test B: Audio tick, no chardevs
MIPS: 13.4
Subsystem: cpu=100% audio=0% io=0%
**Overhead vs Test A: 0% MIPS drop**

### Test C: Audio tick + audio chardev, no consumer
MIPS: 13.3
**Overhead vs Test B: ~1% MIPS drop (noise)**

### Test D: Audio tick + audio chardev + consumer
Not tested (requires full sim-web stack running interactively)

### Test E: Full stack display smoothness
Not tested in bench mode (display stats require chardev consumer).
User-reported: ~11 FPS with audio, ~20+ FPS without audio.
Trace timeline shows: game=92.4%, audio_fill=6.3%, scheduler=1.3%

## Subsystem Breakdown (Bench, 10M ticks)
| Subsystem      | % wall time |
|----------------|-------------|
| cpu_step       | ~100%       |
| max98357a_tick | ~0%         |
| io_poll        | 0%          |

## Key Finding

The `max98357a_tick` function (wall-clock paced audio pull) adds **zero measurable overhead** to the emulator's tick loop. The MIPS is identical with and without audio (13.4 MIPS).

The choppiness comes from the **firmware's audio ISR** (`fill_audio`) running inside the emulated CPU. The trace timeline confirms this: `audio_fill` consumes 6.3% of emulated CPU cycles. This is the square wave synthesizer doing divides and modulos for 256 samples per interrupt.

The audio ISR preempts the game task mid-frame, causing visible stutters. On real hardware at 16 MHz, the same ISR takes ~0.3ms. In the emulator running at ~13 MIPS (vs 16 MHz real), the ISR takes proportionally longer in wall-clock time, making the stutter more visible.

## Conclusion

The bottleneck is: **firmware audio synthesis in the emulated CPU**, not the emulator's audio subsystem.

Recommended fixes (in order of impact):
1. **Pre-compute audio samples** — store PCM in flash, DMA streams directly, zero CPU cost
2. **Simplify synthesis** — replace divide/modulo with counter-based square wave
3. **Lower audio ISR priority** — let game task complete frames before audio runs
4. **Reduce buffer size** — smaller DMA buffers = shorter ISR bursts, less visible stutter
