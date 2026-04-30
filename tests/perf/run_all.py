#!/usr/bin/env python3
"""Run all perf tests and write baseline results."""
import subprocess, socket, time, threading, sys, os

DIR = os.path.dirname(os.path.abspath(__file__))
SIM = os.path.join(DIR, '..', '..', 'sim', 'build', 'sim-core')
FW = os.path.join(DIR, '..', '..', 'projects', 'gameboy', 'build', 'gameboy.elf')
RESULTS = os.path.join(DIR, '..', 'results')
os.makedirs(RESULTS, exist_ok=True)

def kill_all():
    subprocess.run(['pkill', '-9', '-f', 'sim-core'], capture_output=True)
    subprocess.run(['pkill', '-9', '-f', 'sim-web'], capture_output=True)
    time.sleep(1)

def send_tcp(port, msg):
    try:
        s = socket.socket(); s.settimeout(3)
        s.connect(('127.0.0.1', port)); s.sendall(msg.encode()); s.close()
    except: pass

def press_a(port, count=8, interval=3):
    """Press A button periodically to restart game."""
    for _ in range(count):
        time.sleep(interval)
        try:
            s = socket.socket(); s.settimeout(2)
            s.connect(('127.0.0.1', port))
            s.sendall(b'gpio:1:0:1\n'); time.sleep(0.1)
            s.sendall(b'gpio:1:0:0\n'); s.close()
        except: pass

out = []
def log(msg):
    print(msg); out.append(msg)

kill_all()

# --- Test 1: MIPS ---
log("=== Test 1: MIPS Benchmark (10M ticks) ===")
r = subprocess.run([SIM, '--machine', 'gameboy', '--firmware', FW,
    '--bench', '600000000', '--no-chardev'], capture_output=True, text=True)
for line in r.stderr.strip().split('\n'):
    if 'Bench' in line: log(line)

# --- Test 2: Display FPS chardev (15s to fit in timeout) ---
log("\n=== Test 2: Display FPS (chardev direct, 15s) ===")
kill_all()
core = subprocess.Popen([SIM, '--machine', 'gameboy', '--firmware', FW,
    '--debug', '9001', '--chardev', 'display=9004', '--chardev', 'usart2=9002',
    '--chardev', 'audio=9005', '--chardev', 'io=9006'],
    stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
time.sleep(2)

fps = subprocess.Popen([sys.executable, os.path.join(DIR, 'display_fps.py'),
    '--port', '9004', '--duration', '15'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
time.sleep(1)
send_tcp(9001, '{"cmd":"continue"}\n')
btn = threading.Thread(target=press_a, args=(9006, 4, 3), daemon=True)
btn.start()
fps_out, _ = fps.communicate(timeout=25)
core.terminate()
try: core.wait(timeout=3)
except: core.kill(); core.wait()
core_err = core.stderr.read().decode(errors='replace')

log("  Point 1 (ili9341_flush, core only):")
p1_lines = [l for l in core_err.split('\n') if '[display]' in l]
for line in p1_lines: log(f"  {line}")
if not p1_lines: log("  (no display stats)")

log("  Point 2 (chardev direct):")
for line in fps_out.decode().strip().split('\n'):
    log(f"  {line}")

# --- Test 3: Display FPS WebSocket (15s) ---
log("\n=== Test 3: Display FPS (WebSocket, 15s) ===")
kill_all()
SIM_WEB = os.path.join(DIR, '..', '..', 'sim', 'src', 'sim-web', 'sim-web.py')
web = subprocess.Popen([sys.executable, SIM_WEB, '--machine', 'gameboy', '--firmware', FW],
    stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
time.sleep(3)

fps = subprocess.Popen([sys.executable, os.path.join(DIR, 'display_fps_ws.py'),
    '--port', '3000', '--duration', '15'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
time.sleep(1)

# Continue via HTTP
try:
    import urllib.request
    urllib.request.urlopen(urllib.request.Request(
        'http://localhost:3000/cmd', data=b'{"cmd":"continue"}', method='POST'))
except: pass

# Press A via HTTP
def press_a_http(count=4, interval=3):
    for _ in range(count):
        time.sleep(interval)
        try:
            urllib.request.urlopen(urllib.request.Request(
                'http://localhost:3000/io', data=b'gpio:1:0:1', method='POST'))
            time.sleep(0.1)
            urllib.request.urlopen(urllib.request.Request(
                'http://localhost:3000/io', data=b'gpio:1:0:0', method='POST'))
        except: pass
btn = threading.Thread(target=press_a_http, args=(4, 3), daemon=True)
btn.start()

fps_out, _ = fps.communicate(timeout=25)
web.terminate()
try: web.wait(timeout=3)
except: web.kill(); web.wait()
web_err = web.stderr.read().decode(errors='replace')
kill_all()

log("  Point 1 (ili9341_flush, during WebSocket test):")
p1_lines = [l for l in web_err.split('\n') if '[display]' in l]
for line in p1_lines: log(f"  {line}")
if not p1_lines: log("  (no display stats)")

log("  Point 3 (WebSocket):")
for line in fps_out.decode().strip().split('\n'):
    log(f"  {line}")

# Write results
result_file = os.path.join(RESULTS, 'baseline.txt')
with open(result_file, 'w') as f:
    f.write('\n'.join(out) + '\n')
log(f"\n=== Results written to {result_file} ===")
