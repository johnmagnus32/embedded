#!/usr/bin/env python3
"""Run Point 1+3 test: full stack with sim-web.py, WebSocket measurement."""
import subprocess, socket, time, sys, os

SIM_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DURATION = 20

subprocess.run(['pkill', '-9', '-f', 'sim-core'], capture_output=True)
subprocess.run(['pkill', '-9', '-f', 'sim-web'], capture_output=True)
time.sleep(1)

# Start full stack via ./sim wrapper
print("Starting full stack...")
sim = subprocess.Popen([
    os.path.join(SIM_DIR, 'sim'),
    os.path.join(SIM_DIR, '..', 'projects', 'gameboy', 'build', 'gameboy.elf'),
], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
time.sleep(3)

# Send continue via debug port
print("Sending continue...")
try:
    d = socket.socket()
    d.settimeout(3)
    d.connect(('127.0.0.1', 9001))
    d.sendall(b'{"cmd":"continue"}\n')
    d.close()
except Exception as e:
    print(f"Failed to connect to debug port: {e}")
    sim.kill()
    sys.exit(1)

time.sleep(1)

# Start Point 3 measurement
print(f"Starting Point 3 measurement ({DURATION}s)...")
p3 = subprocess.Popen([
    sys.executable, os.path.join(SIM_DIR, 'tools', 'measure_display_fps_ws.py'),
    '--port', '3000', '--duration', str(DURATION),
], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

p3_out, _ = p3.communicate(timeout=DURATION + 10)

# Kill
sim.terminate()
try: sim.wait(timeout=3)
except: sim.kill(); sim.wait()

print("\n" + p3_out.decode())
