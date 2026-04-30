#!/usr/bin/env python3
"""Run Point 1+2 test: sim-core only, chardev direct measurement."""
import subprocess, socket, time, threading, sys, os

SIM_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DURATION = 20

subprocess.run(['pkill', '-9', '-f', 'sim-core'], capture_output=True)
time.sleep(1)

# Start sim-core
print("Starting sim-core...")
core = subprocess.Popen([
    os.path.join(SIM_DIR, 'build', 'sim-core'),
    '--machine', 'gameboy',
    '--firmware', os.path.join(SIM_DIR, '..', 'projects', 'gameboy', 'build', 'gameboy.elf'),
    '--debug', '9001',
    '--chardev', 'display=9004',
    '--chardev', 'usart2=9002',
    '--chardev', 'audio=9005',
], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
time.sleep(2)

# Start Point 2 measurement in background
print("Starting Point 2 measurement...")
p2 = subprocess.Popen([
    sys.executable, os.path.join(SIM_DIR, 'tools', 'measure_display_fps.py'),
    '--port', '9004', '--duration', str(DURATION),
], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
time.sleep(1)

# Send continue
print("Sending continue...")
d = socket.socket()
d.settimeout(3)
d.connect(('127.0.0.1', 9001))
d.sendall(b'{"cmd":"continue"}\n')
d.close()

# Wait for Point 2 to finish
print(f"Running for {DURATION}s...")
p2_out, _ = p2.communicate(timeout=DURATION + 10)

# Kill sim-core
core.terminate()
try: core.wait(timeout=3)
except: core.kill(); core.wait()
core_err = core.stderr.read().decode(errors='replace')

# Extract Point 1
p1_lines = [l for l in core_err.split('\n') if '[display]' in l]

print("\n=== Point 1: ili9341_flush (sim-core stderr) ===")
for l in p1_lines: print(l)
if not p1_lines: print("(no display stats — check frame count threshold)")

print("\n" + p2_out.decode())
