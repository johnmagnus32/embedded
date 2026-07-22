# Signal Capture & Automated Testing

Generic signal capture (like a logic analyzer) for any bus in the sim, plus deterministic test programs that assert captured data against known-good references. Start with gameboy (v1) since it's simpler and fully working. Work in `/home/johmagnu/learning/embedded/sim` (capture infrastructure) and `/home/johmagnu/learning/embedded/projects/gameboy` (test program).

## Goal

1. Trace any signal in the sim (SPI, I2S, parallel, GPIO) with zero overhead when not enabled
2. Run a deterministic test program on the real `--machine gameboy` (no special test machine)
3. Capture bus traffic + display frames, assert against expected values
4. Same infrastructure works for gameboy-v2 (FPGA parallel output) with no code changes — just different trace points on the command line

## Architecture

```
sim-core --machine gameboy \
         --firmware test_program.elf \
         --device flash0=test_songs.csv \
         --trace spi1=build/spi1.csv \
         --trace spi3=build/spi3.csv \
         --trace i2s2=build/i2s.csv \
         --trace display=build/frames.csv

┌─────────────────────────────────────────────────────────────────┐
│  sim-core                                                        │
│                                                                  │
│  ┌──────────────┐                                                │
│  │ MCU firmware  │                                                │
│  │ (test program)│                                                │
│  └──────┬───────┘                                                │
│         │                                                        │
│    ┌────┼──────────────────────────────────────────────────┐     │
│    │    ▼           ▼           ▼           ▼              │     │
│    │  SPI1        SPI3        I2S2       Display           │     │
│    │  (ILI9341)   (W25Q128)   (audio)    (framebuffer)     │     │
│    │    │           │           │           │              │     │
│    │    ▼           ▼           ▼           ▼              │     │
│    │  [tap]       [tap]       [tap]       [tap]            │     │
│    │    │           │           │           │              │     │
│    │    ▼           ▼           ▼           ▼              │     │
│    │  spi1.csv    spi3.csv    i2s.csv    frames.csv         │     │
│    └───────────────────────────────────────────────────────┘     │
│                                                                  │
│  Taps are NO-OP when not specified on command line               │
└─────────────────────────────────────────────────────────────────┘
```

## Signal trace design

### Trace point interface

A trace is a passive observer attached to an existing bus. It records every transaction with a timestamp. Zero cost when not attached (just a NULL pointer check).

```c
/* signal_trace.h */
#ifndef SIGNAL_TRACE_H
#define SIGNAL_TRACE_H

#include <stdint.h>
#include <stdio.h>

struct signal_trace {
    FILE *fp;
    uint64_t *cycle_ptr;
};

/* Create a trace (opens CSV file). Returns NULL if path is NULL. */
struct signal_trace *signal_trace_create(const char *path, uint64_t *cycle_ptr);

/* Record a data byte */
static inline void signal_trace_byte(struct signal_trace *t, uint8_t byte)
{
    if (!t) return;  /* zero cost when not tracing */
    fprintf(t->fp, "%llu,data,0x%02X\n", (unsigned long long)*t->cycle_ptr, byte);
}

/* Record a control event */
static inline void signal_trace_event(struct signal_trace *t, const char *event)
{
    if (!t) return;
    fprintf(t->fp, "%llu,event,%s\n", (unsigned long long)*t->cycle_ptr, event);
}

/* Close and flush */
void signal_trace_close(struct signal_trace *t);

#endif
```

### Event names

Events are plain strings in the CSV:
- `CS_LOW` — chip select asserted
- `CS_HIGH` — chip select deasserted
- `WS_LEFT` — I2S word select = left channel
- `WS_RIGHT` — I2S word select = right channel
- `FRAME` — display frame boundary (vsync)

### Trace attachment points

Taps are attached in the machine init, keyed by name:

```c
/* In gameboy.c init: */
b->spi1_tap = signal_trace_create(trace_find(taps, "spi1"), &b->soc.cpu.cycle_count);
b->spi3_tap = signal_trace_create(trace_find(taps, "spi3"), &b->soc.cpu.cycle_count);
b->i2s_tap  = signal_trace_create(trace_find(taps, "i2s2"), &b->soc.cpu.cycle_count);
b->display_tap = signal_trace_create(trace_find(taps, "display"), &b->soc.cpu.cycle_count);
```

Then in the SPI transfer callback:

```c
static uint8_t ili9341_spi_transfer(void *opaque, uint8_t byte)
{
    signal_trace_byte(b->spi1_tap, byte);  /* no-op if trace is NULL */
    return ili9341_transfer(&display, byte);
}
```

In the CS handler:

```c
static void ili9341_cs_handler(void *opaque, int level)
{
    signal_trace_event(b->spi1_tap, level ? TAP_EVT_CS_HIGH : TAP_EVT_CS_LOW);
    /* ... existing CS logic ... */
}
```

In the display flush:

```c
void ili9341_flush(struct ili9341 *d)
{
    /* ... existing flush logic ... */
    if (d->tap) {
        signal_trace_event(d->tap, TAP_EVT_FRAME);
        /* Write framebuffer as hex line */
        fprintf(d->trace->fp, "%llu,frame,", (unsigned long long)*d->cycle_count_ptr);
        for (int i = 0; i < ew*eh; i++) fprintf(d->trace->fp, "%04X", d->fb[i]);
        fprintf(d->trace->fp, "\n");
    }
}
```

### Zero overhead when disabled

Every trace point is a single pointer check:
```c
if (!tap) return;
```

Branch prediction makes this essentially free on modern CPUs. No allocation, no file I/O, no timestamp computation when taps aren't specified.

## Command line

```bash
# Normal run (no taps, full speed):
./build/sim-core --machine gameboy --firmware game.elf

# With signal capture:
./build/sim-core --machine gameboy \
    --firmware test.elf \
    --device flash0=test_songs.csv \
    --trace spi1=build/spi1.csv \
    --trace spi3=build/spi3.csv \
    --trace i2s2=build/i2s.csv \
    --trace display=build/frames.csv
```

The `--trace` argument is parsed generically — the machine maps names to attachment points. Adding a new trace point (e.g., for gameboy-v2's parallel LCD bus) requires only adding one line in the machine init.

## Test program (gameboy v1)

A deterministic firmware that exercises all buses in a predictable sequence:

```c
/* projects/gameboy/test/test_main.c */

void main(void)
{
    /* ... standard init (device_init_all, buttons_init) ... */

    uart_print("TEST START\n");

    /* Phase 1: Display — draw known pattern */
    display_set_rotation(display, 0x28);
    display_fill_rect(display, 0, 0, 320, 240, 0x18E6);  /* sky */
    display_fill_rect(display, 100, 100, 20, 20, 0xF800); /* red square */
    display_vsync(display);

    /* Phase 2: Flash — read first 16 bytes */
    uint8_t flash_data[16];
    flash_read(DEVICE_DT_GET(w25q128), 0, flash_data, 16);
    /* Print to UART for verification */
    uart_print("FLASH:");
    for (int i = 0; i < 16; i++) print_hex8(flash_data[i]);
    uart_print("\n");

    /* Phase 3: Audio — output 256 samples of a known waveform */
    audio_start(audio_dev);
    int16_t *buf = audio_get_buffer(audio_dev);
    for (int i = 0; i < 128; i++) {
        buf[i*2] = buf[i*2+1] = (i < 64) ? 16000 : -16000;  /* square wave */
    }
    audio_put_buffer(audio_dev, buf);

    /* Phase 4: Second display frame — move the square */
    display_fill_rect(display, 100, 100, 20, 20, 0x18E6);  /* erase */
    display_fill_rect(display, 120, 100, 20, 20, 0xF800);  /* redraw */
    display_vsync(display);

    uart_print("TEST DONE\n");
    extern void sys_exit(int);
    sys_exit(0);
}
```

### What this exercises

| Bus | What's captured | What's asserted |
|-----|----------------|-----------------|
| SPI1 (display) | All ILI9341 commands + pixel data | Correct set_window coords, correct pixel colors |
| SPI3 (flash) | READ command + address + response | Correct command (0x03), address (0x00000000), data matches songs.csv header |
| I2S2 (audio) | PCM samples | 128 samples of square wave at known amplitude |
| Display frames | Full framebuffer at each vsync | Frame 1: red square at (100,100). Frame 2: red square at (120,100) |

## Assertion script

```python
#!/usr/bin/env python3
"""tests/test_gameboy_signals.py — Assert captured signals match expected."""

import csv, sys

def parse_trace(path):
    """Parse a signal tap CSV file into (cycle, data) records."""
    records = []
    with open(path) as f:
        reader = csv.reader(f)
        next(reader)  # skip header
        for row in reader:
            if len(row) >= 3:
                records.append((int(row[0]), row[1], row[2]))
    return records

def parse_display_trace(path):
    """Parse display tap into list of frames (each frame = 320*240 uint16 array)."""
    frames = []
    with open(path) as f:
        reader = csv.reader(f)
        next(reader)  # skip header
        for row in reader:
            if len(row) >= 3 and row[1] == 'frame':
                hex_data = row[2]
                pixels = [int(hex_data[i:i+4], 16) for i in range(0, len(hex_data), 4)]
                frames.append(pixels)
    return frames

def test_spi1(path):
    """Assert ILI9341 SPI traffic."""
    records = parse_trace(path)
    # Find first CS_LOW → data sequence (first fill_rect)
    # ... assert 0x2A, 0x2B, 0x2C commands present ...
    print(f"  spi1: {len(records)} records")
    # Assert red pixels (0xF800) appear in the data stream
    red_bytes = sum(1 for _, t, v in records if t == 'data' and v == '0xF8')
    assert red_bytes > 0, "No red pixel high bytes in SPI1 traffic"
    print("  spi1: PASS")

def test_spi3(path):
    """Assert W25Q128 flash read."""
    records = parse_trace(path)
    data_bytes = [v for _, t, v in records if t == 'data']
    # First transaction after CS_LOW should be: 0x03 (READ) + 3 addr bytes
    assert data_bytes[0] == '0x03', f"Expected READ cmd 0x03, got {data_bytes[0]}"
    print("  spi3: PASS")

def test_i2s(path):
    """Assert audio output contains square wave."""
    records = parse_trace(path)
    samples = [v for _, t, v in records if t == 'data']
    assert len(samples) > 0, "No I2S samples captured"
    # Check for high/low transitions (square wave)
    print(f"  i2s: {len(samples)} sample bytes")
    print("  i2s: PASS")

def test_display(path):
    """Assert framebuffer content at each vsync."""
    frames = parse_display_trace(path)
    assert len(frames) >= 2, f"Expected >=2 frames, got {len(frames)}"

    # Frame 1: red square at (100,100), size 20x20
    for y in range(100, 120):
        for x in range(100, 120):
            assert frames[0][y * 320 + x] == 0xF800, \
                f"Frame 1: pixel ({x},{y}) should be RED"

    # Frame 2: red square moved to (120,100)
    # Old position should be sky
    assert frames[1][100 * 320 + 100] == 0x18E6, "Frame 2: old position not erased"
    # New position should be red
    assert frames[1][100 * 320 + 120] == 0xF800, "Frame 2: new position not red"

    print(f"  display: {len(frames)} frames, PASS")

if __name__ == '__main__':
    print("=== Gameboy Signal Tests ===")
    test_spi1('build/spi1.csv')
    test_spi3('build/spi3.csv')
    test_i2s('build/i2s.csv')
    test_display('build/frames.csv')
    print("=== ALL PASS ===")
```

## Running the test

```bash
cd /home/johmagnu/learning/embedded

# Build test firmware
make -C projects/gameboy test-firmware

# Run sim with taps
cd sim
./build/sim-core --machine gameboy \
    --firmware ../projects/gameboy/build/test.elf \
    --device flash0=../projects/gameboy/build/songs.csv \
    --trace spi1=build/spi1.csv \
    --trace spi3=build/spi3.csv \
    --trace i2s2=build/i2s.csv \
    --trace display=build/frames.csv

# Assert
python3 tests/test_gameboy_signals.py
```

Or as a single `make test-signals` target.

## Extending to gameboy-v2

Same infrastructure, different trace points:

```bash
# gameboy-v2: tap the SPI to FPGA + the parallel LCD output
./build/sim-core --machine gameboy-v2 \
    --firmware ../projects/gameboy-v2/build/gameboy-v2.elf \
    --device fpga0=../projects/fpga-ppu/build/ppu_top.json \
    --trace spi1=build/spi1_to_fpga.csv \
    --trace lcd_parallel=build/lcd.csv \
    --trace display=build/frames.csv
```

The `lcd_parallel` trace is added in the gameboy-v2 machine init — one line:

```c
b->lcd_tap = signal_trace_create(trace_find(taps, "lcd_parallel"), &b->soc.cpu.cycle_count);
```

And in the LCD WR handler:

```c
static void lcd_wr_handler(void *opaque, int level) {
    signal_trace_byte(b->lcd_tap, byte);  /* no-op if NULL */
    /* ... existing logic ... */
}
```

No changes to the trace infrastructure, assertion framework, or CSV format.

## Implementation order

### Phase 1: Signal trace infrastructure

1. Create `sim/src/core/signal_trace.h` and `signal_trace.c`
2. Add `--trace name=path` argument parsing to `main.c`
3. Pass trace table to machine init
4. Zero overhead verified: run without `--trace`, confirm no performance regression

### Phase 2: Attach taps to gameboy machine

1. Add trace attachment points in `gameboy.c` for spi1, spi3, i2s2, display
2. Wire `signal_trace_byte` into SPI transfer callbacks
3. Wire `signal_trace_event` into CS handlers
4. Wire display tap into `ili9341_flush`
5. Verify: run with `--trace spi1=...`, confirm file is created with data

### Phase 3: Test firmware

1. Create `projects/gameboy/test/test_main.c`
2. Add `make test-firmware` target (compiles test_main.c instead of game.c)
3. Verify: test firmware runs in sim, exits via semihosting

### Phase 4: Assertion script

1. Create `sim/tests/test_gameboy_signals.py`
2. Implement `parse_trace()` and `parse_display_trace()`
3. Write assertions for each bus
4. Verify: `make test-signals` passes end-to-end

### Phase 5: CI integration

1. Add `make test-signals` to the pre-commit hook
2. Ensure test completes in < 10 seconds (v1 is fast, no FPGA sim)

## Verification checklist

- [ ] `--trace` args parsed, tap files created
- [ ] No performance impact without `--trace` (benchmark before/after)
- [ ] SPI1 tap captures ILI9341 commands + pixel data
- [ ] SPI3 tap captures W25Q128 read commands + responses
- [ ] I2S2 tap captures audio samples
- [ ] Display tap captures full framebuffer at each vsync
- [ ] Test firmware is deterministic (same output every run)
- [ ] Test firmware uses the real `--machine gameboy` (no special test machine)
- [ ] Assertion script validates all four buses
- [ ] Adding a new trace point requires only 1 line in machine init
- [ ] Same trace infrastructure works for gameboy-v2 (different machine, same `--trace` args)
- [ ] Test completes in < 10 seconds wall time

## Performance benchmark

After implementation, measure the overhead of trace instrumentation when tracing is NOT enabled (NULL pointer checks on hot path):

```bash
# Baseline (before trace code is added):
time ./build/sim-core --machine gameboy --firmware game.elf --cycles 16000000

# With trace code but no --trace args (should be ~same):
time ./build/sim-core --machine gameboy --firmware game.elf --cycles 16000000

# With all traces enabled (expected slower due to file I/O):
time ./build/sim-core --machine gameboy --firmware game.elf --cycles 16000000 \
    --trace spi1=build/spi1.csv --trace spi3=build/spi3.csv \
    --trace i2s2=build/i2s.csv --trace display=build/frames.csv
```

Record results here after implementation:

| Configuration | Time (16M cycles) | Overhead |
|--------------|-------------------|----------|
| Baseline (no trace code) | TBD | — |
| Trace code, no --trace args | TBD | TBD% |
| All traces enabled | TBD | TBD% |

If the NULL-check overhead exceeds 5%, switch to a function-pointer approach where the traced wrapper is only installed when `--trace` is specified (zero cost when disabled).

## Hardware-in-the-loop testing

Same test program, same assertions — but running on real hardware with Saleae capturing actual bus signals. No manual interaction required.

### Setup

```
┌─────────────────────────────────────────────────────────────────┐
│  Developer machine (anywhere)                                    │
│                                                                  │
│  make test-hw                                                    │
│    └── SSH to NUC ──────────────────────────────────────────┐    │
│                                                              │    │
└──────────────────────────────────────────────────────────────┼───┘
                                                               │
┌──────────────────────────────────────────────────────────────┼───┐
│  NUC (johmagnu-nuc) — always-on lab machine                  │   │
│                                                              ▼   │
│  ┌──────────┐    SWD     ┌──────────┐    SPI/I2S    ┌─────────┐ │
│  │ ST-Link  │◄──────────►│  STM32   │◄────────────►│ Saleae  │ │
│  │ V2       │            │  F411RE  │              │ Logic   │ │
│  └──────────┘            │  + FPGA  │              │ Pro 16  │ │
│       │                  │  + ILI9341│              └────┬────┘ │
│       │                  └──────────┘                   │      │
│       │                                                  │      │
│  ┌────┴──────────────────────────────────────────────────┴───┐  │
│  │  test_hw.py (orchestrator)                                 │  │
│  │    1. Flash firmware (OpenOCD)                              │  │
│  │    2. Start Saleae capture (automation API)                │  │
│  │    3. Reset board                                          │  │
│  │    4. Wait for UART "TEST DONE"                            │  │
│  │    5. Stop capture, export CSV                             │  │
│  │    6. Run assertions                                       │  │
│  └────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
```

### Shared code between sim and hardware tests

```
tests/
├── assertions/
│   ├── assert_spi_display.py   ← shared: checks ILI9341 commands + pixels
│   ├── assert_spi_flash.py     ← shared: checks W25Q128 read sequence
│   ├── assert_i2s_audio.py     ← shared: checks PCM sample values
│   └── assert_display_frames.py ← shared: checks framebuffer pixels
├── test_sim.py                 ← orchestrator for sim (runs sim-core --trace)
├── test_hw.py                  ← orchestrator for hardware (flashes + Saleae)
└── conftest.py                 ← common: paths, expected values, test firmware config
```

The assertion modules are identical between sim and hardware — they take a CSV path and validate its contents. Only the orchestrator differs (how the CSV is produced).

### Orchestrator: sim (test_sim.py)

```python
def run_sim_test():
    subprocess.run([
        "./build/sim-core", "--machine", "gameboy",
        "--firmware", "../projects/gameboy/build/test.elf",
        "--device", "flash0=../projects/gameboy/build/songs.csv",
        "--trace", "spi1=build/spi1.csv",
        "--trace", "spi3=build/spi3.csv",
        "--trace", "i2s2=build/i2s.csv",
        "--trace", "display=build/frames.csv",
    ], check=True)

    assert_spi_display("build/spi1.csv")
    assert_spi_flash("build/spi3.csv")
    assert_i2s_audio("build/i2s.csv")
    assert_display_frames("build/frames.csv")
```

### Orchestrator: hardware (test_hw.py)

```python
import saleae.automation as sal
import subprocess, serial, time

NUC = "johmagnu-nuc"
SERIAL_PORT = "/dev/ttyUSB0"
BAUD = 115200

# Saleae channel mapping (matches physical wiring)
SPI1_CLK  = 0  # PA5
SPI1_MOSI = 1  # PA7
SPI1_CS   = 2  # PA4
SPI1_DC   = 3  # PB5
SPI3_CLK  = 4  # PC10
SPI3_MOSI = 5  # PC12
SPI3_MISO = 6  # PC11
SPI3_CS   = 7  # PB0
I2S_BCLK  = 8  # PB13
I2S_WS    = 9  # PB12
I2S_SD    = 10 # PB15

def run_hw_test():
    # 1. Flash test firmware
    subprocess.run([
        "ssh", NUC,
        "openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg "
        "-c 'program /tmp/test.elf verify reset exit'"
    ], check=True)

    # 2. Connect to Saleae (running on NUC)
    mgr = sal.Manager.connect(address=f"{NUC}:10430")

    # 3. Configure and start capture
    device_config = sal.LogicDeviceConfiguration(
        digital_channels=list(range(11)),
        digital_sample_rate=25_000_000,
    )
    capture = mgr.start_capture(device_configuration=device_config)

    # 4. Reset board to start test
    subprocess.run([
        "ssh", NUC,
        "openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg "
        "-c 'init; reset run; exit'"
    ], check=True)

    # 5. Wait for "TEST DONE" on UART
    ser = serial.serial_for_url(f"socket://{NUC}:9002", baudrate=BAUD, timeout=10)
    deadline = time.time() + 10
    while time.time() < deadline:
        line = ser.readline().decode(errors='ignore').strip()
        if "TEST DONE" in line:
            break
    ser.close()

    # 6. Stop capture
    capture.stop()

    # 7. Export decoded SPI data as CSV
    spi1_analyzer = capture.add_analyzer("SPI", settings={
        "MOSI": SPI1_MOSI, "Clock": SPI1_CLK,
        "Enable": SPI1_CS, "Bits per Transfer": 8,
    })
    capture.export_data_table(filepath="build/spi1_hw.csv", analyzers=[spi1_analyzer])

    spi3_analyzer = capture.add_analyzer("SPI", settings={
        "MOSI": SPI3_MOSI, "MISO": SPI3_MISO, "Clock": SPI3_CLK,
        "Enable": SPI3_CS, "Bits per Transfer": 8,
    })
    capture.export_data_table(filepath="build/spi3_hw.csv", analyzers=[spi3_analyzer])

    i2s_analyzer = capture.add_analyzer("I2S", settings={
        "Clock": I2S_BCLK, "Frame": I2S_WS, "Data": I2S_SD,
        "Significant Bit": "MSB", "Audio Bit Depth": 16,
    })
    capture.export_data_table(filepath="build/i2s_hw.csv", analyzers=[i2s_analyzer])

    # 8. Run same assertions as sim
    assert_spi_display("build/spi1_hw.csv")
    assert_spi_flash("build/spi3_hw.csv")
    assert_i2s_audio("build/i2s_hw.csv")
    # Note: no display frame assertion for HW (no framebuffer access)

    capture.close()
```

### CSV format normalization

Saleae exports a different CSV format than the sim. A thin adapter normalizes both to the same schema before assertions run:

```python
# conftest.py

def normalize_saleae_spi_csv(path):
    """Convert Saleae SPI analyzer export to sim trace format."""
    # Saleae format: Time [s], Packet ID, MOSI, MISO
    # Sim format:    cycle,type,value
    rows = []
    with open(path) as f:
        reader = csv.DictReader(f)
        for row in reader:
            cycle = int(float(row['Time [s]']) * 16_000_000)  # convert to cycles
            mosi = int(row['MOSI'], 16)
            rows.append(f"{cycle},data,0x{mosi:02X}")
    # Write normalized
    out_path = path.replace('.csv', '_norm.csv')
    with open(out_path, 'w') as f:
        f.write("cycle,type,value\n")
        f.write("\n".join(rows))
    return out_path
```

### What's shared vs what's different

| Component | Sim | Hardware |
|-----------|-----|----------|
| Test firmware | Same binary | Same binary |
| Signal capture | `--trace` (internal) | Saleae (external) |
| CSV format | Native | Normalized via adapter |
| Assertion logic | **Shared** | **Shared** |
| Orchestrator | `test_sim.py` | `test_hw.py` |
| Display frames | ✓ (framebuffer dump) | ✗ (no access to LCD RAM) |
| Requires hardware | No | Yes (NUC + Saleae + board) |
| Execution time | ~5 seconds | ~15 seconds |

### Running

```bash
# Sim only (fast, no hardware needed):
make test-sim

# Hardware only (requires NUC + Saleae + board):
make test-hw

# Both (CI could run sim always, hw on a schedule):
make test-all
```

### Prerequisites for hardware testing

1. NUC with ST-Link V2 connected to board
2. Saleae Logic Pro 16 connected to NUC via USB
3. Saleae Logic 2 software running on NUC (headless mode: `Logic --automation`)
4. `saleae-automation` Python package installed on NUC
5. Saleae probes wired to SPI1, SPI3, I2S2 signals on the board
6. UART adapter connected for "TEST DONE" detection
7. Network access from developer machine to NUC (SSH)
