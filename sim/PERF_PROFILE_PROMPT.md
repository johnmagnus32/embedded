Profile the simulator binary using Linux `perf` to identify where host CPU time is spent during emulation. The goal is to understand why the display animation isn't as smooth as it was before audio was added, and to find optimization opportunities. Work in `/home/johmagnu/learning/simple-stm32/sim`.

## Problem

The per-subsystem instrumentation showed cpu=100% in `stm32f411_tick`, but we don't know what's expensive *inside* that function. We need a host-level profile to see: is it instruction decode? membus lookups? `find_region`? `clock_gettime` overhead from our own instrumentation? Something else?

## Step 1: Build with debug symbols

The emulator is already built with `-g` (check the Makefile CFLAGS). Verify with:
```bash
file build/sim-core
# Should show "with debug_info" or "not stripped"
```

If not, ensure `-g` is in CFLAGS and rebuild.

## Step 2: Run under perf (with audio)

Run the emulator in bench mode under `perf record`:

```bash
cd /home/johmagnu/learning/simple-stm32/sim
perf record -g --call-graph dwarf -F 999 -- ./build/sim-core \
    --machine gameboy \
    --firmware ../projects/gameboy/build/gameboy.elf \
    --bench 10000000 \
    --no-chardev
```

Flags:
- `-g --call-graph dwarf` — record call stacks using DWARF unwind info
- `-F 999` — sample at 999 Hz (avoids aliasing with timer frequencies)
- `--bench 10000000` — run 10M ticks then exit
- `--no-chardev` — no TCP overhead, pure emulation

## Step 3: Run under perf (without audio)

Temporarily comment out the `max98357a_tick` call in `gameboy_tick`, rebuild, and run the same perf command. This gives a baseline to compare against.

## Step 4: Generate reports

For each run, generate a text report:

```bash
# Flat profile — which functions use the most CPU
perf report --stdio --no-children --sort=overhead,symbol > perf_with_audio.txt

# With call graph — where are the expensive functions called from
perf report --stdio --sort=overhead,symbol > perf_with_audio_callgraph.txt
```

Do the same for the without-audio run.

## Step 5: Extract key data

From each `perf report`, extract the top 20 functions by overhead. Look for:

1. **`armv7m_cpu_step`** — expected to be the biggest. What percentage?
2. **`membus_read32` / `membus_read16` / `membus_write32`** — how much time in memory dispatch?
3. **`find_region`** — is the linear scan of the region table expensive?
4. **`max98357a_tick`** — how much host time does the audio pull consume?
5. **`clock_gettime`** — is our perf instrumentation itself a bottleneck?
6. **`chardev_write_buf` / `write`** — TCP overhead (should be zero with `--no-chardev`)
7. **`stm32_dma_tick`** — DMA per-tick overhead
8. **`armv7m_nvic_update`** — interrupt checking overhead
9. **`spi_bus_transfer`** — SPI dispatch for display writes
10. **`ili9341_transfer`** — display pixel processing

Also look at the call graph to see: when `membus_read16` is called, is it mostly from `armv7m_cpu_step` (instruction fetch) or from `max98357a_tick` (audio DMA pull)?

## Step 6: Write results to PERF.md

Append to the existing `sim/PERF.md` (or create if it doesn't exist):

```markdown
## Host CPU Profile (perf)

### With audio (10M ticks, no chardevs)

| Function | % CPU | Called from |
|----------|-------|-------------|
| armv7m_cpu_step | | |
| membus_read16 | | cpu_step / max98357a_tick / other |
| membus_read32 | | |
| find_region | | |
| max98357a_tick | | |
| clock_gettime | | |
| stm32_dma_tick | | |
| armv7m_nvic_update | | |
| ili9341_transfer | | |
| spi_bus_transfer | | |
| (other) | | |

### Without audio (10M ticks, no chardevs)

| Function | % CPU | Called from |
|----------|-------|-------------|
| armv7m_cpu_step | | |
| membus_read16 | | |
| membus_read32 | | |
| find_region | | |
| (other) | | |

### Comparison

| Function | With audio | Without audio | Delta |
|----------|-----------|---------------|-------|
| armv7m_cpu_step | | | |
| membus_read* | | | |
| max98357a_tick | | | |
| find_region | | | |
| clock_gettime | | | |

### Analysis

Where is the time going?
- Instruction decode/execute: <>%
- Memory bus dispatch: <>%
- Audio DMA pull: <>%
- Perf instrumentation overhead: <>%

Is `find_region` a bottleneck? (If >5%, consider switching from linear scan to a hash table or sorted binary search)

Is `clock_gettime` a bottleneck? (If >2%, reduce how often we call it — sample every 100K ticks instead of every tick)

Is `max98357a_tick` doing too many membus_read16 calls? (If the call graph shows membus_read16 called heavily from max98357a_tick, consider reading guest RAM directly instead of going through the membus)

### Recommended optimizations
<based on the data>
```

## Important

- Do NOT make any code optimizations yet. This is a measurement-only task.
- The only code changes allowed are: temporarily commenting out `max98357a_tick` for the baseline run, and restoring it after.
- If `perf` is not available, try `perf stat` first to check, or install with `sudo yum install perf` or `sudo apt install linux-tools-common`.
- If `perf record` fails with permission errors, try `perf record --no-inherit` or check `/proc/sys/kernel/perf_event_paranoid`.
