#!/usr/bin/env python3
"""Measure display FPS via WebSocket /ws-display (Point 3 in display pipeline)."""
import socket, time, math, argparse, signal, sys, json, os, base64, struct

ap = argparse.ArgumentParser()
ap.add_argument('--host', default='localhost')
ap.add_argument('--port', type=int, default=3000)
ap.add_argument('--duration', type=int, default=30)
ap.add_argument('--json', action='store_true')
args = ap.parse_args()

key = base64.b64encode(os.urandom(16)).decode()
s = socket.socket()
s.connect((args.host, args.port))
s.sendall(f'GET /ws-display HTTP/1.1\r\nHost: {args.host}:{args.port}\r\n'
          f'Upgrade: websocket\r\nConnection: Upgrade\r\n'
          f'Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n'.encode())
resp = b''
while b'\r\n\r\n' not in resp: resp += s.recv(4096)
if b'101' not in resp.split(b'\r\n')[0]:
    print(f"WebSocket handshake failed"); sys.exit(1)
extra = resp.split(b'\r\n\r\n', 1)[1]

def ws_read(sock, buf):
    while len(buf) < 2: buf += sock.recv(65536)
    length = buf[1] & 0x7F; off = 2
    if length == 126:
        while len(buf) < 4: buf += sock.recv(65536)
        length = struct.unpack('>H', buf[2:4])[0]; off = 4
    elif length == 127:
        while len(buf) < 10: buf += sock.recv(65536)
        length = struct.unpack('>Q', buf[2:10])[0]; off = 10
    while len(buf) < off + length: buf += sock.recv(65536)
    return buf[0] & 0x0F, buf[off:off+length], buf[off+length:]

intervals, timestamps = [], []
frame_count = 0
t_start = time.monotonic()
t_last_frame = None
t_last_print = t_start
running = True
wsbuf = extra

def finish(*_):
    global running
    running = False
signal.signal(signal.SIGINT, finish)
s.settimeout(1.0)

while running and (time.monotonic() - t_start) < args.duration:
    try:
        op, payload, wsbuf = ws_read(s, wsbuf)
    except (socket.timeout, Exception):
        continue
    if op == 0x02:
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
        print(f"[{now-t_start:.1f}s] {frame_count} frames | {1000/avg:.1f} FPS | "
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
    print(json.dumps({"point": "websocket", "duration_s": round(elapsed, 1),
        "frames": frame_count, "avg_fps": round(1000/avg, 1),
        "min_ms": round(min(intervals), 1), "avg_ms": round(avg, 1),
        "max_ms": round(max(intervals), 1), "stddev_ms": round(stddev, 1),
        "glitches": glitches, "glitch_pct": round(glitches/len(intervals)*100, 1)}))
else:
    print(f"\n=== Point 3: WebSocket ({elapsed:.1f}s) ===")
    print(f"Total frames: {frame_count}")
    print(f"Average FPS: {1000/avg:.1f}")
    print(f"Interval: min={min(intervals):.1f}ms avg={avg:.1f}ms max={max(intervals):.1f}ms stddev={stddev:.1f}ms")
    print(f"Glitches (>2x avg): {glitches}/{len(intervals)} ({glitches/len(intervals)*100:.1f}%)")
