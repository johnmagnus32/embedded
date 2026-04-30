#!/bin/bash
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
RESULTS_DIR="$DIR/../results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULT_FILE="$RESULTS_DIR/perf_$TIMESTAMP.txt"
SIM="$DIR/../../sim/build/sim-core"
SIM_WEB="$DIR/../../sim/src/sim-web/sim-web.py"
FW="$DIR/../../projects/gameboy/build/gameboy.elf"

mkdir -p "$RESULTS_DIR"
pkill -9 -f sim-core 2>/dev/null; pkill -9 -f sim-web 2>/dev/null; sleep 1

send_cmd() {
  python3 -c "import socket; s=socket.socket(); s.settimeout(3); s.connect(('127.0.0.1',$1)); s.sendall(b'$2\n'); s.close()" 2>/dev/null
}

press_a() {
  python3 -c "import socket; s=socket.socket(); s.settimeout(2); s.connect(('127.0.0.1',$1)); s.sendall(b'gpio:1:0:1\n'); __import__('time').sleep(0.1); s.sendall(b'gpio:1:0:0\n'); s.close()" 2>/dev/null
}

header() { echo "" | tee -a "$RESULT_FILE"; echo "=== $1 ===" | tee -a "$RESULT_FILE"; }

echo "=== Performance Test Suite ===" | tee "$RESULT_FILE"
echo "Date: $(date)" | tee -a "$RESULT_FILE"
echo "Host: $(uname -n) $(uname -m)" | tee -a "$RESULT_FILE"

# --- Test 1: MIPS benchmark ---
header "Test 1: MIPS Benchmark (10M ticks)"
"$SIM" --machine gameboy --firmware "$FW" --bench 10000000 --no-chardev 2>&1 | tee -a "$RESULT_FILE"

# --- Test 2: Display FPS via chardev (30s) ---
header "Test 2: Display FPS (chardev direct, 30s)"
"$SIM" --machine gameboy --firmware "$FW" --debug 9001 \
  --chardev display=9004 --chardev usart2=9002 --chardev audio=9005 --chardev io=9006 2>/dev/null &
SIM_PID=$!; sleep 2

python3 "$DIR/display_fps.py" --port 9004 --duration 30 2>&1 | tee -a "$RESULT_FILE" &
FPS_PID=$!; sleep 1

send_cmd 9001 '{"cmd":"continue"}'

# Periodic A button press to restart game
for i in $(seq 1 8); do sleep 3; press_a 9006; done &
BTN_PID=$!

wait $FPS_PID 2>/dev/null
kill $BTN_PID $SIM_PID 2>/dev/null; wait $SIM_PID 2>/dev/null; wait $BTN_PID 2>/dev/null

# --- Test 3: Display FPS via WebSocket (30s) ---
header "Test 3: Display FPS (WebSocket, 30s)"
pkill -9 -f sim-core 2>/dev/null; pkill -9 -f sim-web 2>/dev/null; sleep 1
python3 "$SIM_WEB" --machine gameboy --firmware "$FW" 2>/dev/null &
WEB_PID=$!; sleep 3

python3 "$DIR/display_fps_ws.py" --port 3000 --duration 30 2>&1 | tee -a "$RESULT_FILE" &
FPS_PID=$!; sleep 1

# Continue via HTTP
python3 -c "import urllib.request; urllib.request.urlopen(urllib.request.Request('http://localhost:3000/cmd', data=b'{\"cmd\":\"continue\"}', method='POST'))" 2>/dev/null

# Periodic A button via HTTP
for i in $(seq 1 8); do
  sleep 3
  python3 -c "import urllib.request; urllib.request.urlopen(urllib.request.Request('http://localhost:3000/io', data=b'gpio:1:0:1', method='POST'))" 2>/dev/null
  sleep 0.1
  python3 -c "import urllib.request; urllib.request.urlopen(urllib.request.Request('http://localhost:3000/io', data=b'gpio:1:0:0', method='POST'))" 2>/dev/null
done &
BTN_PID=$!

wait $FPS_PID 2>/dev/null
kill $BTN_PID $WEB_PID 2>/dev/null; wait 2>/dev/null
pkill -9 -f sim-core 2>/dev/null

echo "" | tee -a "$RESULT_FILE"
echo "=== Results: $RESULT_FILE ===" | tee -a "$RESULT_FILE"
echo ""
echo "PERF_SUMMARY:"
grep -E "MIPS|Bench|FPS|frames|Total" "$RESULT_FILE"
