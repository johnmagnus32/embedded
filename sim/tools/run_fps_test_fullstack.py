#!/usr/bin/env python3
"""Run Point 1+3 test: sim-web.py manages sim-core, measure via WebSocket."""
import subprocess, socket, time, sys, os

SIM_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DURATION = 20

subprocess.run(['pkill', '-9', '-f', 'sim-core'], capture_output=True)
subprocess.run(['pkill', '-9', '-f', 'sim-web'], capture_output=True)
time.sleep(1)

fw = os.path.join(SIM_DIR, '..', 'projects', 'gameboy', 'build', 'gameboy.elf')

# Start sim-web.py which starts sim-core internally
print("Starting sim-web.py (manages sim-core)...")
web = subprocess.Popen([
    sys.executable,
    os.path.join(SIM_DIR, 'src', 'sim-web', 'sim-web.py'),
    '--machine', 'gameboy', '--firmware', fw,
], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
time.sleep(4)

# Send continue via debug port (sim-web proxies to sim-core)
print("Sending continue...")
try:
    d = socket.socket(); d.settimeout(3); d.connect(('127.0.0.1', 3000))
    d.sendall(b'POST /cmd HTTP/1.1\r\nHost: localhost\r\nContent-Length: 18\r\n\r\n{"cmd":"continue"}')
    resp = d.recv(4096)
    d.close()
    print(f"  Response: {resp[:80]}")
except Exception as e:
    print(f"  Failed: {e}")
    web.kill(); web.wait()
    sys.exit(1)

time.sleep(1)

# Run Point 3 measurement
print(f"Measuring Point 3 for {DURATION}s...")
p3 = subprocess.Popen([
    sys.executable, os.path.join(SIM_DIR, 'tools', 'measure_display_fps_ws.py'),
    '--port', '3000', '--duration', str(DURATION),
], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

p3_out, _ = p3.communicate(timeout=DURATION + 10)

# Collect stderr (has Point 1 [display] lines from sim-core via sim-web)
web.terminate()
try: web.wait(timeout=3)
except: web.kill(); web.wait()
web_err = web.stderr.read().decode(errors='replace')
p1_lines = [l for l in web_err.split('\n') if '[display]' in l]

print("\n=== Point 1: ili9341_flush ===")
for l in p1_lines: print(l)
if not p1_lines: print("(no display stats)")

print("\n" + p3_out.decode())
