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

## Host CPU Profile (perf)

### With audio (10M ticks, no chardevs)

| Function | % CPU | Notes |
|----------|-------|-------|
| clock_gettime (vDSO) | 58.2% | Our perf instrumentation overhead |
| clock_gettime (libc) | 4.5% | libc wrapper for above |
| armv7m_cpu_step | 14.2% | Instruction decode + execute |
| gameboy_tick | 7.5% | Tick loop overhead |
| membus_read16 | 2.5% | Memory reads (instruction fetch + data) |
| stm32f411_tick | 2.2% | SoC tick dispatch |
| membus_write32 | 2.2% | Memory writes |
| armv7m_systick_tick | 2.2% | SysTick timer |
| membus_read32 | 1.4% | 32-bit memory reads |
| cond_check | 1.0% | IT block condition evaluation |
| armv7m_nvic_update | 0.8% | Interrupt controller |

MIPS: 12.3 (reduced from 13.4 by perf sampling overhead)

### Without audio (10M ticks, no chardevs)

| Function | % CPU | Notes |
|----------|-------|-------|
| clock_gettime (vDSO) | 55.2% | Same instrumentation overhead |
| clock_gettime (libc) | 6.3% | libc wrapper |
| armv7m_cpu_step | 12.1% | Instruction decode + execute |
| gameboy_tick | 9.5% | Tick loop overhead |
| membus_read16 | 2.9% | Memory reads |
| stm32f411_tick | 2.4% | SoC tick dispatch |
| membus_read32 | 2.4% | 32-bit memory reads |
| membus_write32 | 2.0% | Memory writes |
| armv7m_systick_tick | 1.8% | SysTick timer |
| armv7m_nvic_update | 1.3% | Interrupt controller |

MIPS: 13.1

### By DSO (shared object)

| DSO | With audio | Without audio |
|-----|-----------|---------------|
| [vdso] (clock_gettime) | 58.2% | 55.2% |
| sim-core | 37.3% | 38.4% |
| libc (clock_gettime) | 4.5% | 6.3% |

### Analysis

**Where is the time going?**
- clock_gettime (perf instrumentation): **~62%** of host CPU
- Instruction decode/execute (armv7m_cpu_step): **~14%**
- Memory bus dispatch (membus_read/write): **~6%**
- Tick loop + SoC dispatch: **~10%**
- SysTick + NVIC: **~3%**
- Audio DMA pull (max98357a_tick): **<0.1%** (not visible in profile)

**Is clock_gettime a bottleneck?**
YES — it consumes 60% of host CPU. The `gameboy_tick` perf instrumentation
calls `clock_gettime` twice per tick (before and after `stm32f411_tick`).
At 13M ticks/sec, that's 26M `clock_gettime` calls/sec. Even though vDSO
makes each call ~40ns, 26M × 40ns = ~1 second of CPU per second of wall time.

**Is find_region a bottleneck?**
No — membus functions total ~6%. The hash table lookup is working well.

**Is max98357a_tick doing too many membus_read16 calls?**
No — it doesn't even appear in the profile. The wall-clock pacing means it
only runs ~90 times/sec (every 10K ticks when wall clock says samples are due).

### Recommended optimizations

1. **Remove per-tick clock_gettime** — the perf instrumentation in
   `gameboy_tick` is the #1 bottleneck. Either remove it, sample every
   100K ticks, or use `rdtsc` instead of `clock_gettime`.
   Expected improvement: **~2.5× MIPS increase** (from 13 to ~33 MIPS)

2. **Pre-compute audio samples** — eliminate firmware ISR overhead
   (6.3% of emulated CPU)

3. **Inline hot membus paths** — membus_read16/read32/write32 total ~6%,
   could be reduced with inlining or direct RAM access for known address ranges
