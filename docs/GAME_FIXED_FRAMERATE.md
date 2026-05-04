Fix the game loop to maintain a consistent 30 FPS regardless of render time. Work in `projects/gameboy/src/game.c`. Build with `make` from `projects/gameboy/`.

## Problem

The game loop sleeps a fixed 33ms after rendering:

```c
// render frame...
sched_sleep_ms(33);
```

Total frame time = render_time + 33ms. If rendering takes 20ms, the frame is 53ms = 19 FPS. If rendering takes 1ms, the frame is 34ms = 29 FPS. The frame rate varies with scene complexity.

## Fix

Measure how long rendering took and sleep only the remaining time to hit the 33ms target:

```c
#define FRAME_MS 33  /* ~30 FPS */

uint32_t frame_start = systick_get_ticks();

// ... all rendering and game logic ...

uint32_t elapsed = systick_get_ticks() - frame_start;
if (elapsed < FRAME_MS)
    sched_sleep_ms(FRAME_MS - elapsed);
```

`systick_get_ticks()` returns milliseconds (SysTick is configured at 1000 Hz). The frame rate is now a consistent 30 FPS whether rendering takes 1ms or 30ms. If rendering ever exceeds 33ms, the sleep is skipped and the next frame starts immediately (graceful degradation instead of compounding delay).

## Testing

The game should run at the same visual speed regardless of how many obstacles are on screen or whether the player is jumping. On real hardware, this gives a locked 30 FPS. On the emulator, it still runs at ~6× speed (because the emulator's tick rate is faster than real-time), but the frame pacing is consistent.
