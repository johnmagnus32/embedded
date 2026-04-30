#!/usr/bin/env python3
"""
fps_test.py — Automated FPS measurement for sim-core.

Starts sim-core with chardevs, connects to debug port, sends continue,
connects to display chardev (so ili9341_flush sends frames and prints stats),
runs for a specified duration, then kills and collects stderr.

Usage: python3 fps_test.py <duration_secs> <label> [extra sim-core args...]
Output: fps_<label>.log (stderr from sim-core)
"""
import subprocess, socket, time, sys, os, signal, threading

duration = int(sys.argv[1]) if len(sys.argv) > 1 else 30
label = sys.argv[2] if len(sys.argv) > 2 else "test"

sim_dir = os.path.dirname(os.path.abspath(__file__))
fw = os.path.join(sim_dir, '..', 'projects', 'gameboy', 'build', 'gameboy.elf')
logfile = os.path.join(sim_dir, f'fps_{label}.log')

# Kill old
subprocess.run(['pkill', '-9', '-f', 'sim-core'], capture_output=True)
time.sleep(1)

# Start sim-core with chardevs
proc = subprocess.Popen([
    os.path.join(sim_dir, 'build', 'sim-core'),
    '--machine', 'gameboy',
    '--firmware', fw,
    '--debug', '9001',
    '--chardev', 'usart2=9002',
    '--chardev', 'display=9004',
    '--chardev', 'audio=9005',
], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

time.sleep(2)

# Connect to display chardev so ili9341_flush has a consumer
disp = socket.socket()
disp.settimeout(3)
try:
    disp.connect(('127.0.0.1', 9004))
except:
    print("Failed to connect to display chardev")
    proc.kill()
    sys.exit(1)

# Drain display data in background (otherwise TCP buffer fills and blocks)
def drain_display():
    while True:
        try:
            d = disp.recv(65536)
            if not d: break
        except: break
threading.Thread(target=drain_display, daemon=True).start()

# Connect to audio chardev too
audio = socket.socket()
try:
    audio.settimeout(3)
    audio.connect(('127.0.0.1', 9005))
    def drain_audio():
        while True:
            try:
                d = audio.recv(65536)
                if not d: break
            except: break
    threading.Thread(target=drain_audio, daemon=True).start()
except:
    pass

# Connect to debug port and continue
dbg = socket.socket()
dbg.settimeout(5)
dbg.connect(('127.0.0.1', 9001))
dbg.sendall(b'{"cmd":"continue"}\n')

print(f"Running for {duration}s...")
time.sleep(duration)

# Kill
proc.terminate()
try:
    proc.wait(timeout=5)
except:
    proc.kill()
    proc.wait()

disp.close()
dbg.close()

# Collect stderr
err = proc.stderr.read().decode(errors='replace')
with open(logfile, 'w') as f:
    f.write(err)

# Extract display stats
display_lines = [l for l in err.split('\n') if '[display]' in l]
print(f"\nWrote {logfile} ({len(err)} bytes)")
print(f"Display stat lines: {len(display_lines)}")
for l in display_lines[-5:]:
    print(l)
