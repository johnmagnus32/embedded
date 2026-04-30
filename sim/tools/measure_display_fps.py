#!/usr/bin/env python3
"""
Measure display FPS by connecting directly to the display chardev TCP port.

Usage:
  1. Start sim-core with --chardev display=9004
  2. python3 tools/measure_display_fps.py --port 9004 --duration 30
  3. Send "continue" to debug port to start the game
"""
import socket, time, struct, argparse, math, signal, sys

ap = argparse.ArgumentParser()
ap.add_argument('--port', type=int, default=9004)
ap.add_argument('--duration', type=int, default=30)
args = ap.parse_args()

HEADER = 4  # uint16_le width, uint16_le height

s = socket.socket()
s.connect(('127.0.0.1', args.port))
s.settimeout(1.0)

intervals = []
timestamps = []
buf = b''
frame_count = 0
frame_size = None
t_start = time.monotonic()
t_last_frame = None
t_last_print = t_start
running = True

def finish(*_):
    global running
    running = False
signal.signal(signal.SIGINT, finish)

while running and (time.monotonic() - t_start) < args.duration:
    try:
        data = s.recv(262144)
        if not data: break
        buf += data
    except socket.timeout:
        continue
    except (ConnectionResetError, BrokenPipeError, OSError):
        break

    while len(buf) >= HEADER:
        ew = buf[0] | (buf[1] << 8)
        eh = buf[2] | (buf[3] << 8)
        fsz = ew * eh * 2
        if len(buf) < HEADER + fsz:
            break
        buf = buf[HEADER + fsz:]
        now = time.monotonic()
        frame_count += 1
        if t_last_frame is not None:
            ms = (now - t_last_frame) * 1000
            intervals.append(ms)
            timestamps.append(now - t_start)
        t_last_frame = now

    now = time.monotonic()
    if now - t_last_print >= 1.0 and intervals:
        elapsed = now - t_start
        recent = intervals[-30:] if len(intervals) >= 30 else intervals
        avg = sum(recent) / len(recent)
        fps = 1000.0 / avg if avg > 0 else 0
        glitches = sum(1 for x in recent if x > avg * 2)
        print(f"[{elapsed:.1f}s] {frame_count} frames | {fps:.1f} FPS | "
              f"min={min(recent):.1f}ms avg={avg:.1f}ms max={max(recent):.1f}ms | "
              f"glitches: {glitches}")
        t_last_print = now

s.close()
elapsed = time.monotonic() - t_start

if not intervals:
    print("No frames received")
    sys.exit(1)

avg = sum(intervals) / len(intervals)
stddev = math.sqrt(sum((x - avg) ** 2 for x in intervals) / len(intervals))
glitches = sum(1 for x in intervals if x > avg * 2)
longest_idx = intervals.index(max(intervals))
longest_time = timestamps[longest_idx]

print(f"\n=== Point 2: Chardev Direct ({elapsed:.1f}s) ===")
print(f"Total frames: {frame_count}")
print(f"Average FPS: {1000.0 / avg:.1f}")
print(f"Interval: min={min(intervals):.1f}ms avg={avg:.1f}ms max={max(intervals):.1f}ms stddev={stddev:.1f}ms")
print(f"Glitches (>2x avg): {glitches}/{len(intervals)} ({glitches/len(intervals)*100:.1f}%)")
print(f"Longest gap: {max(intervals):.1f}ms at t={longest_time:.1f}s (frame {longest_idx+1})")
