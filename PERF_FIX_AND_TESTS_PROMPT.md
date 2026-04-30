Fix the clock_gettime bottleneck, clean up invalid test results, and create a permanent performance test suite. Work across `/home/johmagnu/learning/simple-stm32/sim`, `/home/johmagnu/learning/simple-stm32/projects/gameboy`, and `/home/johmagnu/learning/simple-stm32/os`. Read all Makefiles, `sim/PERF.md`, `sim/src/machine/gameboy.c`, and `sim/src/main.c` before making changes.

## Part 1: Fix the clock_gettime bottleneck

The per-tick `clock_gettime` instrumentation in `gameboy_tick` consumes 62% of host CPU. Remove it entirely. The per-subsystem timing breakdown (cpu=X% audio=Y% io=Z%) was useful for diagnosis but is too expensive to leave in.

Replace it with a lightweight MIPS counter that only samples every 1M ticks:

```c
// In the main run loop (during continue/run), not in gameboy_tick:
static uint64_t last_cycles = 0;
static struct timespec last_ts = {0};
if (cpu->cycle_count - last_cycles >= 1000000) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (last_ts.tv_sec) {
        double elapsed = (now.tv_sec - last_ts.tv_sec)
                       + (now.tv_nsec - last_ts.tv_nsec) / 1e9;
        double mips = (cpu->cycle_count - last_cycles) / elapsed / 1e6;
        fprintf(stderr, "\r[perf] %.1f MIPS  ", mips);
    }
    last_cycles = cpu->cycle_count;
    last_ts = now;
}
```

This calls `clock_gettime` ~13 times/sec instead of 26M times/sec. Negligible overhead.

Remove any other per-tick `clock_gettime` calls in `gameboy_tick` or `stm32f411_tick`.

## Part 2: Clean up PERF.md

Delete `sim/PERF.md` entirely. The results in it were collected with the `clock_gettime` instrumentation active, which was consuming 62% of CPU. All MIPS numbers, FPS numbers, and subsystem breakdowns are invalid — they measured a crippled emulator.

Also delete `sim/PERF_PROMPT.md`, `sim/PERF_PROFILE_PROMPT.md`, and `sim/FPS_CHARDEV_TEST_PROMPT.md` — these were one-time diagnostic prompts, not permanent documentation.

## Part 3: Create the performance test suite

Create `tests/` at the repo root with automated performance tests that run against the built simulator and firmware.

### Directory structure

```
simple-stm32/
├── tests/
│   ├── Makefile                  ← orchestrates all tests
│   ├── perf/
│   │   ├── bench.sh              ← MIPS benchmark (sim-core --bench)
│   │   ├── display_fps.py        ← chardev FPS measurement (Point 2)
│   │   ├── display_fps_http.py   ← HTTP FPS measurement (Point 3)
│   │   └── run_all.sh            ← runs all perf tests, outputs results
│   └── results/
│       └── .gitkeep              ← results written here, gitignored
```

### tests/perf/bench.sh

Runs sim-core in bench mode and captures MIPS:

```bash
#!/bin/bash
# Usage: ./bench.sh [cycles]
CYCLES=${1:-10000000}
SIM=../../sim/build/sim-core
FW=../../projects/gameboy/build/gameboy.elf

echo "=== MIPS Benchmark ($CYCLES ticks) ==="
$SIM --machine gameboy --firmware $FW --bench $CYCLES --no-chardev 2>&1
```

### tests/perf/display_fps.py

The chardev FPS measurement script (from the earlier prompt). Connects to display chardev TCP port, measures frame arrival rate for a specified duration. Outputs JSON for easy parsing:

```json
{"point": "chardev", "duration_s": 30, "frames": 847, "avg_fps": 28.2,
 "min_ms": 29.1, "avg_ms": 35.4, "max_ms": 112.3, "stddev_ms": 5.2,
 "glitches": 12, "glitch_pct": 1.4}
```

### tests/perf/display_fps_http.py

The HTTP FPS measurement script. Polls sim-web.py's `/display` endpoint. Same JSON output format with `"point": "http"`.

### tests/perf/run_all.sh

Orchestrates all perf tests and writes a summary:

```bash
#!/bin/bash
set -e
RESULTS_DIR="$(dirname $0)/../results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULT_FILE="$RESULTS_DIR/perf_$TIMESTAMP.txt"

echo "=== Performance Test Suite ===" | tee $RESULT_FILE
echo "Date: $(date)" | tee -a $RESULT_FILE
echo "Host: $(uname -a)" | tee -a $RESULT_FILE
echo "" | tee -a $RESULT_FILE

# Test 1: MIPS benchmark
echo "--- Test 1: MIPS Benchmark ---" | tee -a $RESULT_FILE
./bench.sh 10000000 2>&1 | tee -a $RESULT_FILE
echo "" | tee -a $RESULT_FILE

# Test 2: Display FPS via chardev (30 seconds)
echo "--- Test 2: Display FPS (chardev direct) ---" | tee -a $RESULT_FILE
# Start sim-core in background with debug port and display chardev
SIM=../../sim/build/sim-core
FW=../../projects/gameboy/build/gameboy.elf
$SIM --machine gameboy --firmware $FW --debug 9001 --chardev display=9004 &
SIM_PID=$!
sleep 1

# Start FPS measurement
python3 display_fps.py --port 9004 --duration 30 --json >> $RESULT_FILE &
FPS_PID=$!
sleep 0.5

# Send continue
echo '{"cmd":"continue"}' | nc -q0 localhost 9001
wait $FPS_PID
kill $SIM_PID 2>/dev/null
echo "" | tee -a $RESULT_FILE

# Test 3: Display FPS via HTTP (30 seconds)
echo "--- Test 3: Display FPS (HTTP /display) ---" | tee -a $RESULT_FILE
# Start full stack
cd ../../sim
./sim ../projects/gameboy/build/gameboy.elf &
SIM_PID=$!
sleep 2
cd ../tests/perf

python3 display_fps_http.py --url http://localhost:3000 --duration 30 --json >> $RESULT_FILE &
FPS_PID=$!
sleep 0.5

curl -s -X POST http://localhost:3000/cmd -d '{"cmd":"continue"}' > /dev/null
wait $FPS_PID
kill $SIM_PID 2>/dev/null
echo "" | tee -a $RESULT_FILE

echo "=== Results written to $RESULT_FILE ==="

# Print summary for commit message
echo ""
echo "PERF_SUMMARY:"
grep -E "MIPS|avg_fps|Total frames" $RESULT_FILE
```

### tests/Makefile

```makefile
.PHONY: perf perf-bench perf-fps clean

# Run all performance tests
perf: perf-bench perf-fps

# Quick benchmark only (no chardev, no sim-web)
perf-bench:
	cd perf && bash bench.sh

# Full FPS tests (requires 60+ seconds)
perf-fps:
	cd perf && bash run_all.sh

clean:
	rm -f results/*.txt
```

### tests/results/.gitignore

```
*
!.gitkeep
!.gitignore
```

Results are local — don't commit the raw output files. The summary goes in commit messages.

## Part 4: Integrate into build Makefiles

### sim/Makefile — add test target

```makefile
.PHONY: test perf

test: all
	cd ../tests && make perf-bench

perf: all
	cd ../tests && make perf
```

### projects/gameboy/Makefile — add test target

```makefile
.PHONY: test

test: all
	cd ../../tests && make perf-bench
```

### os/ — no test target needed (OS changes are tested via the firmware build)

## Part 5: Commit message convention

After running tests, include the MIPS number in the commit message footer:

```
feat(audio): Add DMA-based I2S audio output

Replace polling audio with DMA double-buffered I2S streaming.
MAX98357A model uses wall-clock pacing for real-time output.

perf: 33.2 MIPS (bench 10M ticks, no chardev)
perf: 28.1 FPS avg (chardev direct, 30s)
```

The `run_all.sh` script prints a `PERF_SUMMARY:` block at the end that can be copy-pasted into the commit message.

## Part 6: Run the tests and verify

After all changes:

1. Build the simulator: `cd sim && make`
2. Build the firmware: `cd projects/gameboy && make`
3. Run the quick benchmark: `cd tests && make perf-bench`
4. Verify MIPS is significantly higher than the old 13.4 (expect ~30+ MIPS now that clock_gettime overhead is removed)
5. Run the full FPS tests: `cd tests && make perf-fps`
6. Check that display FPS is smooth (>25 FPS avg, <2% glitches)

Write the results of the first clean run to `tests/results/baseline.txt` and commit it as the baseline. This one file IS committed — it's the reference point for future comparisons.
