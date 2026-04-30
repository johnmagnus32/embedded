Create a standalone display FPS measurement tool and add it to the test suite. Work in `/home/johmagnu/learning/simple-stm32/sim`.

## Goal

Create a simple Python script that connects to the display chardev TCP port, receives framebuffer data, and measures frame arrival rate and consistency. This gives a clean measurement of how fast sim-core produces frames without any browser, HTTP, or sim-web.py overhead in the path.

## Part 1: Create the measurement script

Create `sim/tools/measure_display_fps.py`:

```python
#!/usr/bin/env python3
"""
Measure display FPS by connecting directly to the display chardev.

Usage:
  1. Start sim-core with --chardev display=9004
  2. Run this script: python3 tools/measure_display_fps.py [--port 9004] [--duration 30]
  3. Start the game (connect debugger, send "continue")

The script connects to the display chardev TCP port, receives raw
framebuffer data, detects frame boundaries from the 4-byte header
(width, height), and measures timing between complete frames.
"""
```

The display chardev protocol (from `ili9341_flush`): each frame is a 4-byte header (uint16 width, uint16 height) followed by `width * height * 2` bytes of RGB565 pixel data.

The script should:

1. Connect to `localhost:PORT`
2. Read data, detect frame boundaries by parsing the 4-byte header to know the frame size
3. For each complete frame received:
   - Record wall-clock timestamp
   - Compute interval since previous frame
   - Track: frame count, min/max/avg interval, stddev, glitch count (interval > 2× avg)
4. Print a live summary every second:
   ```
   [1.0s] 28 frames | 28.0 FPS | interval: min=31.2ms avg=35.7ms max=48.3ms | glitches: 0
   [2.0s] 57 frames | 29.0 FPS | interval: min=30.8ms avg=34.5ms max=52.1ms | glitches: 1
   ```
5. On Ctrl+C or after `--duration` seconds, print a final summary:
   ```
   === Final Results (30.0 seconds) ===
   Total frames: 847
   Average FPS: 28.2
   Interval: min=29.1ms avg=35.4ms max=112.3ms stddev=5.2ms
   Glitches (>2x avg): 12/847 (1.4%)
   Longest gap: 112.3ms at frame 423
   ```
6. Exit with code 0 if avg FPS > 25, exit with code 1 if below (for CI use)

## Part 2: Run the tests

Run three tests using this script. For each test, start sim-core in one terminal and the measurement script in another. Use the debug protocol to send "continue" so the game runs.

**Test 1: With audio + perf instrumentation (current state)**
```bash
# Terminal 1:
./build/sim-core --machine gameboy --firmware ../projects/gameboy/build/gameboy.elf \
    --debug 9001 --chardev display=9004 --chardev audio=9005 --chardev io=9006

# Terminal 2:
python3 tools/measure_display_fps.py --port 9004 --duration 30 > fps_test1.txt &

# Terminal 3 (send continue):
echo '{"cmd":"continue"}' | nc localhost 9001

# Wait 30 seconds, script exits automatically
```

**Test 2: Without audio**
Comment out `max98357a_tick` in `gameboy_tick` and `audio_start` in firmware `main()`. Rebuild both. Run same test.

**Test 3: With audio, clock_gettime instrumentation removed**
Restore audio. Remove/reduce the per-tick `clock_gettime` calls in `gameboy_tick`. Rebuild. Run same test.

**Restore all code changes after testing.**

## Part 3: Append results to PERF.md

```markdown
## Display FPS (Chardev Direct Measurement)

Measured by connecting directly to display chardev TCP port.
No browser, no sim-web.py, no HTTP in the measurement path.

### Test 1: Current state (audio + perf instrumentation)
```
<paste final summary from fps_test1.txt>
```

### Test 2: Without audio
```
<paste final summary>
```

### Test 3: Audio enabled, clock_gettime reduced
```
<paste final summary>
```

### Comparison

| Metric | With audio | No audio | Audio + no clock_gettime |
|--------|-----------|----------|--------------------------|
| Avg FPS | | | |
| Min interval (ms) | | | |
| Avg interval (ms) | | | |
| Max interval (ms) | | | |
| Stddev (ms) | | | |
| Glitch % | | | |

### Conclusion
<which configuration gives smooth animation?>
<what is the primary cause of frame drops?>
```
