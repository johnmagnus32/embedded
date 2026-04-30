#!/usr/bin/env python3
"""
Measure display FPS via sim-web.py's WebSocket /ws-display endpoint.
Simulates what the browser sees.

Usage:
  1. Start full stack: ./sim <firmware.elf>
  2. python3 tools/measure_display_fps_ws.py --url ws://localhost:3000/ws-display --duration 30
  3. Send "continue" via web UI or debug port
"""
import socket, time, hashlib, base64, struct, argparse, math, signal, sys, os

ap = argparse.ArgumentParser()
ap.add_argument('--host', default='localhost')
ap.add_argument('--port', type=int, default=3000)
ap.add_argument('--duration', type=int, default=30)
args = ap.parse_args()

# Manual WebSocket handshake (no external deps)
WS_GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11'
key = base64.b64encode(os.urandom(16)).decode()

s = socket.socket()
s.connect((args.host, args.port))
s.sendall(f'GET /ws-display HTTP/1.1\r\nHost: {args.host}:{args.port}\r\n'
          f'Upgrade: websocket\r\nConnection: Upgrade\r\n'
          f'Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n'.encode())

# Read handshake response
resp = b''
while b'\r\n\r\n' not in resp:
    resp += s.recv(4096)
if b'101' not in resp.split(b'\r\n')[0]:
    print(f"WebSocket handshake failed: {resp[:100]}")
    sys.exit(1)

# Leftover data after headers
extra = resp.split(b'\r\n\r\n', 1)[1]

def ws_read_frame(sock, buf):
    """Read one WebSocket frame, return (opcode, payload, remaining_buf)."""
    while len(buf) < 2:
        buf += sock.recv(65536)
    op = buf[0] & 0x0F
    length = buf[1] & 0x7F
    offset = 2
    if length == 126:
        while len(buf) < 4: buf += sock.recv(65536)
        length = struct.unpack('>H', buf[2:4])[0]
        offset = 4
    elif length == 127:
        while len(buf) < 10: buf += sock.recv(65536)
        length = struct.unpack('>Q', buf[2:10])[0]
        offset = 10
    while len(buf) < offset + length:
        buf += sock.recv(65536)
    payload = buf[offset:offset + length]
    return op, payload, buf[offset + length:]

intervals = []
timestamps = []
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
        op, payload, wsbuf = ws_read_frame(s, wsbuf)
    except socket.timeout:
        continue
    except Exception:
        break

    if op == 0x02:  # binary frame = display data
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

print(f"\n=== Point 3: WebSocket /ws-display ({elapsed:.1f}s) ===")
print(f"Total frames: {frame_count}")
print(f"Average FPS: {1000.0 / avg:.1f}")
print(f"Interval: min={min(intervals):.1f}ms avg={avg:.1f}ms max={max(intervals):.1f}ms stddev={stddev:.1f}ms")
print(f"Glitches (>2x avg): {glitches}/{len(intervals)} ({glitches/len(intervals)*100:.1f}%)")
print(f"Longest gap: {max(intervals):.1f}ms at t={longest_time:.1f}s (frame {longest_idx+1})")
