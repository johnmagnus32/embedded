Create display FPS measurement tools and run tests at three points in the data path. Work in `/home/johmagnu/learning/simple-stm32/sim`. Read `src/devices/ili9341.c`, `src/sim-web/sim-web.py`, and `src/sim-web/index.html` before making changes. Build with `make` from `sim/`.

## Goal

Measure frame rate at three points to identify where frames are lost or delayed:

```
Point 1: Inside sim-core (ili9341_flush)     — already instrumented with [display] log
Point 2: Chardev TCP consumer                — new script, connects to display port directly
Point 3: HTTP consumer (simulates browser)   — new script, polls sim-web.py's /display endpoint
```

If Point 1 shows 30 FPS but Point 2 shows 20 FPS, frames are being lost in the TCP chardev layer.
If Point 2 shows 30 FPS but Point 3 shows 15 FPS, frames are being lost in sim-web.py or the HTTP layer.

## Part 1: Create chardev FPS measurement script (Point 2)

Create `sim/tools/measure_display_fps.py`:

```python
#!/usr/bin/env python3
"""
Measure display FPS by connecting directly to the display chardev TCP port.
No browser, no sim-web.py in the path.

Usage:
  1. Start sim-core with --chardev display=9004
  2. Run: python3 tools/measure_display_fps.py --port 9004 --duration 30
  3. Send "continue" to the debug port to start the game
"""
```

The display chardev protocol (from `ili9341_flush`): each frame is a 4-byte header (uint16_le width, uint16_le height) followed by `width * height * 2` bytes of RGB565 pixel data.

The script should:
1. Connect to `localhost:PORT`
2. Read data, parse 4-byte headers to detect frame boundaries and know the frame size
3. For each complete frame: record timestamp, compute interval since previous frame
4. Track: frame count, min/max/avg interval, stddev, glitch count (interval > 2× rolling avg)
5. Print live summary every second:
   ```
   [1.0s] 28 frames | 28.0 FPS | min=31.2ms avg=35.7ms max=48.3ms | glitches: 0
   ```
6. On Ctrl+C or `--duration` expiry, print final summary:
   ```
   === Point 2: Chardev Direct (30.0s) ===
   Total frames: 847
   Average FPS: 28.2
   Interval: min=29.1ms avg=35.4ms max=112.3ms stddev=5.2ms
   Glitches (>2x avg): 12/847 (1.4%)
   Longest gap: 112.3ms at frame 423
   ```

## Part 2: Create HTTP FPS measurement script (Point 3)

Create `sim/tools/measure_display_fps_http.py`:

```python
#!/usr/bin/env python3
"""
Measure display FPS by polling sim-web.py's /display HTTP endpoint.
Simulates what the browser does.

Usage:
  1. Start the full sim stack: ./sim <firmware.elf>
  2. Run: python3 tools/measure_display_fps_http.py --url http://localhost:3000 --duration 30
  3. Send "continue" via the web UI or debug port
"""
```

This script:
1. Polls `GET /display` in a tight loop (or as fast as the server responds)
2. For each response with data (Content-Length > 0): record timestamp, compute interval
3. Track same stats as Point 2
4. Print live summary every second
5. Final summary:
   ```
   === Point 3: HTTP /display (30.0s) ===
   Total frames: 612
   Average FPS: 20.4
   Interval: min=38.2ms avg=49.0ms max=203.1ms stddev=12.3ms
   Glitches (>2x avg): 28/612 (4.6%)
   Longest gap: 203.1ms at frame 301
   Request latency: min=2.1ms avg=8.3ms max=45.2ms
   ```
   Note: also track HTTP request latency (time from sending request to receiving response) to separate server processing time from frame production rate.

## Part 3: Run the tests

Run all three measurement points simultaneously for the current state of the code (with audio + perf instrumentation). This gives a direct comparison of the same run measured at all three points.

**Start sim-core with debug port and display chardev (no sim-web.py yet):**
```bash
./build/sim-core --machine gameboy --firmware ../projects/gameboy/build/gameboy.elf \
    --debug 9001 --chardev display=9004 --chardev usart2=9002 --chardev audio=9005 --chardev io=9006 \
    2>fps_point1.log
```

**Start Point 2 measurement (chardev direct):**
```bash
python3 tools/measure_display_fps.py --port 9004 --duration 30 > fps_point2.txt
```

**Send continue to start the game:**
```bash
echo '{"cmd":"continue"}' | nc localhost 9001
```

Wait 30 seconds. Point 2 script exits. Ctrl+C sim-core. Extract Point 1 data:
```bash
grep '\[display\]' fps_point1.log > fps_point1.txt
```

Now run again with the full stack for Point 3:

**Start full stack:**
```bash
./sim ../projects/gameboy/build/gameboy.elf 2>fps_point1_fullstack.log
```

**Start Point 3 measurement (HTTP):**
```bash
python3 tools/measure_display_fps_http.py --url http://localhost:3000 --duration 30 > fps_point3.txt
```

**Start the game via web UI or:**
```bash
curl -X POST http://localhost:3000/cmd -d '{"cmd":"continue"}'
```

Wait 30 seconds. Extract Point 1:
```bash
grep '\[display\]' fps_point1_fullstack.log > fps_point1_fullstack.txt
```

## Part 4: Append results to PERF.md

```markdown
## Display FPS — Three-Point Measurement

Measured at three points in the display data path to identify where frames are lost.

```
sim-core ili9341_flush → TCP chardev → sim-web.py → HTTP /display → browser
         Point 1          Point 2                     Point 3
```

### Run 1: sim-core only (no sim-web.py)

| Metric | Point 1 (ili9341_flush) | Point 2 (chardev TCP) |
|--------|------------------------|----------------------|
| Avg FPS | | |
| Min interval (ms) | | |
| Avg interval (ms) | | |
| Max interval (ms) | | |
| Stddev (ms) | | |
| Glitch % | | |

Frame loss between Point 1 and 2: <N> frames (<percent>%)

### Run 2: Full stack (with sim-web.py)

| Metric | Point 1 (ili9341_flush) | Point 3 (HTTP /display) |
|--------|------------------------|------------------------|
| Avg FPS | | |
| Min interval (ms) | | |
| Avg interval (ms) | | |
| Max interval (ms) | | |
| Stddev (ms) | | |
| Glitch % | | |
| HTTP request latency avg | N/A | |

Frame loss between Point 1 and 3: <N> frames (<percent>%)

### Analysis

Where are frames being lost?
- sim-core → chardev TCP: <>% loss
- chardev TCP → HTTP /display: <>% loss (sim-web.py overhead)
- Total pipeline loss: <>%

What causes the largest frame interval spikes?
- Point 1 max interval: <>ms (emulator-side stall)
- Point 2 max interval: <>ms (TCP buffering?)
- Point 3 max interval: <>ms (HTTP polling latency?)

### Conclusion
<Where is the bottleneck in the display pipeline?>
<Is the problem frame production (sim-core) or frame delivery (TCP/HTTP)?>
<Recommended fix?>
```
