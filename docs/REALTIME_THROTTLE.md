Add a `--realtime` flag to sim-core that throttles the emulator to match the real STM32's clock speed. Work in `/home/johmagnu/learning/simple-stm32/sim/mcu`. Read `src/main.c` before making changes. Build with `make` from `sim/mcu/`.

## Problem

The emulator runs at ~6× real-time (e.g., 96 MIPS vs 16 MHz target). This causes:
- Game animation at 6× speed
- Audio samples produced 6× too fast
- `sched_sleep_ms(33)` completes in ~5.5ms wall time instead of 33ms
- Everything looks and sounds wrong compared to real hardware

## Solution

Add `--realtime` flag. When set, the tick loop periodically checks if the emulator is running ahead of wall-clock time and sleeps to catch up. The emulator runs at exactly the target clock speed (16 MHz).

## Implementation

In `main.c`, add a `--realtime` flag:

```c
int realtime = 0;
// in arg parsing:
else if (strcmp(argv[i], "--realtime") == 0)
    realtime = 1;
```

Add a throttle function:

```c
#include <time.h>

static struct timespec rt_start_wall;
static uint64_t rt_start_cycles;
static int rt_init;
static uint64_t target_hz;
static uint64_t check_interval;

static void realtime_throttle(uint64_t cycle_count)
{
    if (!rt_init) {
        clock_gettime(CLOCK_MONOTONIC, &rt_start_wall);
        rt_start_cycles = cycle_count;
        rt_init = 1;
        return;
    }

    uint64_t sim_ticks = cycle_count - rt_start_cycles;
    if (sim_ticks % check_interval != 0) return;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double wall_elapsed = (now.tv_sec - rt_start_wall.tv_sec)
                        + (now.tv_nsec - rt_start_wall.tv_nsec) / 1e9;
    double sim_elapsed = (double)sim_ticks / target_hz;

    if (sim_elapsed > wall_elapsed) {
        double sleep_sec = sim_elapsed - wall_elapsed;
        if (sleep_sec > 0.001) {  /* only sleep if >1ms ahead */
            struct timespec ts = {
                .tv_sec = (time_t)sleep_sec,
                .tv_nsec = (long)((sleep_sec - (time_t)sleep_sec) * 1e9)
            };
            nanosleep(&ts, NULL);
        }
    }
}
```

Call it in the main tick loop:

```c
while (1) {
    int r = mach->tick(board);
    if (realtime)
        realtime_throttle(cpu->cycle_count);
    if (r & CPU_SEMIHOST_EXIT) { ... }
}
```

Also call it in the GDB stub's continue loop if present.

## Update sim launch script

Update `sim/mcu/sim` to pass `--realtime` by default:

```bash
exec python3 "$DIR/src/sim-web/sim-web.py" --machine gameboy --firmware "$ELF" --extra --realtime
```

Or update `sim-web.py` to add `--realtime` to the sim-core command it spawns.

## What this fixes

- Game runs at correct 30 FPS (33ms per frame in both emulated and wall time)
- Audio samples produced at 22,050 Hz (no wall-clock pacing hack needed in MAX98357A)
- `sched_sleep_ms` durations match wall time
- Display refresh at real 60 Hz
- Everything looks and sounds like real hardware

## What stays fast

- `--bench` mode: no throttle, runs at full speed
- Tests: no throttle
- `--gdb` mode without `--realtime`: full speed (debugger controls execution)
- Only the `--realtime` flag enables throttling

## Performance note

The `clock_gettime` call happens every 16,000 ticks = every 1ms of simulated time. At 16 MHz that's 1000 calls/sec — negligible overhead (we measured earlier that even 26M calls/sec only cost ~1 second of CPU). The `nanosleep` call only happens when the emulator is ahead, which is most of the time since it runs 6× faster than needed.

## Clock speed from machine descriptor

Don't hardcode 16 MHz. Add a `get_sysclk` accessor to the machine descriptor:

```c
struct machine_desc {
    /* ... existing ... */
    uint32_t (*get_sysclk)(void *board);
};

// In gameboy.c:
static uint32_t gameboy_get_sysclk(void *board) {
    return ((struct gameboy *)board)->soc.sysclk_hz;
}
```

In `main.c`, after machine init:
```c
if (realtime) {
    target_hz = mach->get_sysclk(board);
    check_interval = target_hz / 1000;  // check every ~1ms of sim time
}
```

If the firmware configures the PLL to 100 MHz and the SoC model updates `sysclk_hz`, the throttle automatically adjusts.

## Simplify audio (part of this change)

With `--realtime`, the emulator produces audio samples at exactly 22,050 Hz wall-clock time. The wall-clock pacing hacks in the audio pipeline are no longer needed and should be removed:

1. **MAX98357A model**: revert to the simple push model. Remove the wall-clock drain (`max98357a_drain`), the ring buffer, and the `clock_gettime` calls. Just buffer samples and flush to chardev when the buffer is full — the samples arrive at the correct rate because the emulator is throttled.

2. **Browser audio**: set `AUDIO_SAMPLE_RATE = 22050` (if not already). Remove any sample rate compensation. The `_audioNextTime` reset logic can stay as-is — it handles minor jitter gracefully.

3. **sim-web.py audio buffer**: the 17640-byte cap can be removed or increased. At 1× speed, audio arrives at ~44KB/sec (22050 samples × 2 bytes), which the WebSocket easily handles.

## Testing

Run with `--realtime`:
```bash
./build/sim-core --machine gameboy --firmware ../projects/gameboy/build/gameboy.elf --realtime --chardev display=9004 ...
```

The game should run at the same speed as real hardware. Audio should sound correct at 22,050 Hz without any browser-side sample rate compensation. The UART task_a/task_b counters should increment once per second of wall time.

Run without `--realtime`:
```bash
./build/sim-core --machine gameboy --firmware ../projects/gameboy/build/gameboy.elf --bench 10000000 --no-chardev
```

Should still show ~40+ MIPS (full speed, no throttle).
