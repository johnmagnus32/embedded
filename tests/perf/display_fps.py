#!/usr/bin/env python3
"""Measure display FPS via chardev TCP (Point 2 in display pipeline)."""
import socket, time, math, argparse, signal, sys, json

ap = argparse.ArgumentParser()
ap.add_argument('--port', type=int, default=9004)
ap.add_argument('--duration', type=int, default=30)
ap.add_argument('--json', action='store_true')
args = ap.parse_args()

HEADER = 4
s = socket.socket()
s.connect(('127.0.0.1', args.port))
s.settimeout(1.0)

intervals, timestamps = [], []
buf = b''
frame_count = 0
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
        if len(buf) < HEADER + fsz: break
        buf = buf[HEADER + fsz:]
        now = time.monotonic()
        frame_count += 1
        if t_last_frame is not None:
            ms = (now - t_last_frame) * 1000
            intervals.append(ms)
            timestamps.append(now - t_start)
        t_last_frame = now

    now = time.monotonic()
    if not args.json and now - t_last_print >= 1.0 and intervals:
        recent = intervals[-30:]
        avg = sum(recent) / len(recent)
        print(f"[{now - t_start:.1f}s] {frame_count} frames | {1000/avg:.1f} FPS | "
              f"min={min(recent):.1f}ms avg={avg:.1f}ms max={max(recent):.1f}ms")
        t_last_print = now

s.close()
elapsed = time.monotonic() - t_start

if not intervals:
    print("No frames received"); sys.exit(1)

avg = sum(intervals) / len(intervals)
stddev = math.sqrt(sum((x - avg)**2 for x in intervals) / len(intervals))
glitches = sum(1 for x in intervals if x > avg * 2)

if args.json:
    print(json.dumps({"point": "chardev", "duration_s": round(elapsed, 1),
        "frames": frame_count, "avg_fps": round(1000/avg, 1),
        "min_ms": round(min(intervals), 1), "avg_ms": round(avg, 1),
        "max_ms": round(max(intervals), 1), "stddev_ms": round(stddev, 1),
        "glitches": glitches, "glitch_pct": round(glitches/len(intervals)*100, 1)}))
else:
    print(f"\n=== Point 2: Chardev Direct ({elapsed:.1f}s) ===")
    print(f"Total frames: {frame_count}")
    print(f"Average FPS: {1000/avg:.1f}")
    print(f"Interval: min={min(intervals):.1f}ms avg={avg:.1f}ms max={max(intervals):.1f}ms stddev={stddev:.1f}ms")
    print(f"Glitches (>2x avg): {glitches}/{len(intervals)} ({glitches/len(intervals)*100:.1f}%)")
