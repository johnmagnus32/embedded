Add performance and timing regression tests to the emulator test suite. Work in `/home/johmagnu/learning/simple-stm32/sim/tests`. Read `tests/firmware/test.h`, `tests/run_tests.py`, and the emulator's SysTick/NVIC/SPI/DMA models before making changes. Build firmware with `make` in `tests/firmware/`, run with `python3 tests/run_tests.py`.

## Goal

Tests that verify the emulator's timing behavior matches real hardware expectations. These catch regressions where a refactor or optimization changes how many cycles things take, how fast interrupts fire, or how the emulator performs on the host.

All tests use semihosting for pass/fail. Timing assertions use the emulated cycle count (read via SysTick or a cycle counter), not wall-clock time — we're testing the emulator's internal timing model, not host speed.

## Part 1: Add cycle count reading to test.h

The firmware needs to read the emulated cycle count. The simplest way: read SysTick's current value, or use the DWT cycle counter (CYCCNT) at 0xE0001004. Add a helper:

```c
// In test.h:
#define DWT_CYCCNT  (*(volatile unsigned int *)0xE0001004)
#define DWT_CONTROL (*(volatile unsigned int *)0xE0001000)
#define DEMCR       (*(volatile unsigned int *)0xE000EDFC)

static inline void cycle_counter_init(void)
{
    DEMCR |= (1 << 24);       // TRCENA: enable DWT
    DWT_CYCCNT = 0;
    DWT_CONTROL |= 1;         // CYCCNTENA: enable cycle counter
}

static inline unsigned int cycles(void)
{
    return DWT_CYCCNT;
}

// Assertion with tolerance:
#define CHECK_RANGE(val, min, max) do { \
    unsigned int _v = (val); \
    if (_v < (min) || _v > (max)) { \
        semi_puts("FAIL:"); semi_puts(t_current); \
        semi_puts(": got "); semi_puthex(_v); \
        semi_puts(" expected "); semi_puthex(min); \
        semi_puts("-"); semi_puthex(max); semi_puts("\n"); \
        t_fail_count++; \
    } \
} while (0)

static inline void semi_puthex(unsigned int v)
{
    char buf[9];
    for (int i = 7; i >= 0; i--) {
        buf[i] = "0123456789abcdef"[v & 0xf];
        v >>= 4;
    }
    buf[8] = 0;
    semi_puts(buf);
}
```

The emulator needs to support the DWT CYCCNT register. Add it to the SoC or as a simple membus region that returns `cpu->cycle_count`. Register at 0xE0001000 with size 0x100:

```c
// In stm32f411.c or a new dwt stub:
static uint32_t dwt_read(void *opaque, uint32_t offset) {
    struct armv7m_cpu *cpu = opaque;
    switch (offset) {
    case 0x04: return (uint32_t)cpu->cycle_count;  // CYCCNT
    case 0x00: return 1;  // CTRL: CYCCNTENA
    default: return 0;
    }
}
static void dwt_write(void *opaque, uint32_t offset, uint32_t val) { (void)opaque; (void)offset; (void)val; }

// Register: membus_register(&soc->bus, 0xE0001000, 0x100, dwt_read, dwt_write, &soc->cpu);
// Also register DEMCR at 0xE000EDFC (can be part of the SCB range or a separate stub)
```

## Part 2: Test firmware

### test_perf_mips.c — Host MIPS regression

A firmware that exercises a realistic instruction mix (loads, stores, arithmetic, branches, function calls), then exits via semihosting. The test runner measures wall-clock time externally — the firmware itself does NOT measure time.

```c
#include "test.h"

/* Realistic workload: array operations with function calls, branches, memory access */
__attribute__((noinline))
static int compute(int *buf, int n)
{
    int sum = 0;
    for (int i = 0; i < n; i++) {
        buf[i] = i * 3 + 7;           // arithmetic + store
        sum += buf[i];                  // load + add
        if (buf[i] > 100) sum -= 10;   // branch + conditional arithmetic
    }
    return sum;
}

__attribute__((noinline))
static int nested_calls(int x)
{
    return compute((int[64]){0}, 64) + x;
}

static int workload_buf[256];

void test_main(void)
{
    /* Run a fixed number of iterations of a realistic workload.
     * The runner measures wall-clock time for this process to complete. */
    volatile int result = 0;
    for (int iter = 0; iter < 100; iter++) {
        result += compute(workload_buf, 256);
        result += nested_calls(iter);
    }

    /* Prevent dead code elimination */
    if (result == 0x12345678) semi_puts("unlikely\n");

    semi_exit(0);
}
```

The test runner handles timing and baseline comparison (see Part 5 below).

### test_perf_systick.c — SysTick timing accuracy

Configures SysTick, runs for a known number of instructions, verifies the tick count matches expectations.

```c
#include "test.h"

#define SYST_CSR (*(volatile unsigned int *)0xE000E010)
#define SYST_RVR (*(volatile unsigned int *)0xE000E014)
#define SYST_CVR (*(volatile unsigned int *)0xE000E018)

static volatile int tick_count;

void systick_handler(void) {
    tick_count++;
    SYST_CSR = 0;  // disable after one tick for precise measurement
}

#define TEST_CUSTOM_VECTORS
#include "test.h"

// Custom vector table with SysTick handler
extern unsigned int _stack_top;
void __attribute__((naked)) reset_handler(void) { __asm volatile("bl test_main\n b .\n"); }
__attribute__((section(".vector_table"), used))
void (* const vectors[])(void) = {
    (void (*)(void))&_stack_top, reset_handler,
    0, 0, 0, 0, 0, 0, 0, 0, 0,  // vectors 2-10
    0,  // SVCall
    0, 0,
    0,  // PendSV
    systick_handler,  // SysTick
};

void test_main(void)
{
    cycle_counter_init();

    TEST("systick_period");
    tick_count = 0;
    SYST_RVR = 1000;  // fire every 1000 cycles
    SYST_CVR = 0;

    unsigned int start = cycles();
    SYST_CSR = 0x07;  // enable + interrupt + processor clock

    // Wait for SysTick to fire
    while (tick_count == 0) {}

    unsigned int elapsed = cycles() - start;

    // Should fire after ~1000 cycles (plus a few for interrupt entry)
    // Allow 1000-1050 to account for interrupt latency
    CHECK_RANGE(elapsed, 1000, 1050);

    semi_puts("systick elapsed: "); semi_puthex(elapsed); semi_puts(" cycles\n");

    TEST_DONE("perf_systick");
}
```

### test_perf_irq_latency.c — Interrupt entry latency

Measures cycles from PendSV trigger to handler execution. Real Cortex-M4 is 12 cycles.

```c
#include "test.h"

#define SCB_ICSR  (*(volatile unsigned int *)0xE000ED04)
#define SCB_SHPR3 (*(volatile unsigned int *)0xE000ED20)

static volatile unsigned int handler_entry_cycle;

void pendsv_handler(void) {
    handler_entry_cycle = cycles();
}

#define TEST_CUSTOM_VECTORS
// ... vector table with pendsv_handler ...

void test_main(void)
{
    cycle_counter_init();
    SCB_SHPR3 |= (0xFF << 16);  // PendSV lowest priority

    TEST("irq_latency");
    unsigned int trigger_cycle = cycles();
    SCB_ICSR = (1 << 28);  // PENDSVSET

    // Spin until handler runs
    while (handler_entry_cycle == 0) {}

    unsigned int latency = handler_entry_cycle - trigger_cycle;

    // Real hardware: 12 cycles for stacking
    // Emulator: should be close (12-20 cycles depending on implementation)
    CHECK_RANGE(latency, 10, 30);

    semi_puts("irq latency: "); semi_puthex(latency); semi_puts(" cycles\n");

    TEST_DONE("perf_irq_latency");
}
```

### test_perf_spi.c — SPI throughput

Measures how many cycles it takes to transfer N bytes via SPI. Without pacing, this should be ~2 cycles per byte (write DR + read SR). With pacing, it should match the configured SPI clock.

```c
#include "test.h"

#define SPI1_CR1 (*(volatile unsigned int *)0x40013000)
#define SPI1_SR  (*(volatile unsigned int *)0x40013008)
#define SPI1_DR  (*(volatile unsigned int *)0x4001300C)

void test_main(void)
{
    cycle_counter_init();
    SPI1_CR1 = (1 << 6) | (1 << 2);  // SPE + MSTR

    TEST("spi_throughput_256");
    unsigned int start = cycles();
    for (int i = 0; i < 256; i++) {
        while (!(SPI1_SR & (1 << 1))) {}  // wait TXE
        SPI1_DR = (unsigned int)i;
    }
    unsigned int elapsed = cycles() - start;

    // Without SPI pacing: ~2-4 cycles per byte (instant TXE)
    // With SPI pacing: ~16 cycles per byte at /2 prescaler
    // Use wide range to pass either way, but catch catastrophic regressions
    CHECK(elapsed > 256);       // at least 1 cycle per byte
    CHECK(elapsed < 256 * 50);  // no more than 50 cycles per byte

    semi_puts("spi 256 bytes: "); semi_puthex(elapsed); semi_puts(" cycles\n");

    TEST_DONE("perf_spi");
}
```

### test_perf_membus.c — Memory access throughput

Measures cycles for sequential RAM reads/writes. Catches membus regressions.

```c
#include "test.h"

static volatile unsigned int ram_buf[256];

void test_main(void)
{
    cycle_counter_init();

    TEST("ram_write_1k");
    unsigned int start = cycles();
    for (int i = 0; i < 256; i++)
        ram_buf[i] = (unsigned int)i;
    unsigned int elapsed = cycles() - start;

    // ~2-3 cycles per word write (STR + loop overhead)
    CHECK_RANGE(elapsed, 256, 256 * 10);
    semi_puts("ram write 1KB: "); semi_puthex(elapsed); semi_puts(" cycles\n");

    TEST("ram_read_1k");
    start = cycles();
    volatile unsigned int sum = 0;
    for (int i = 0; i < 256; i++)
        sum += ram_buf[i];
    elapsed = cycles() - start;

    CHECK_RANGE(elapsed, 256, 256 * 10);
    semi_puts("ram read 1KB: "); semi_puthex(elapsed); semi_puts(" cycles\n");

    TEST_DONE("perf_membus");
}
```

## Part 3: Add DWT CYCCNT support to the emulator

Register the DWT region in `stm32f411_init`:

```c
membus_register(&soc->bus, 0xE0001000, 0x100, dwt_read, dwt_write, &soc->cpu);
```

Also ensure DEMCR (0xE000EDFC) is writable — it may fall in the existing SCB range or need its own region. The firmware writes `DEMCR |= (1 << 24)` to enable DWT. The emulator can just accept the write and always return CYCCNT from `cpu->cycle_count`.

## Part 4: Update the test list

Add to `tests/firmware/Makefile`:

```makefile
TESTS = hello test_alu test_it test_call test_ldm_stm test_mem test_shift \
        test_alu2 test_wide test_arith test_irq \
        test_perf_mips test_perf_systick test_perf_irq_latency \
        test_perf_spi test_perf_membus
```

Add to `tests/run_tests.py`:

```python
tests = [
    # ... existing correctness tests ...

    # Performance / timing regression tests
    ("perf_mips",        "test_perf_mips"),
    ("perf_systick",     "test_perf_systick"),
    ("perf_irq_latency", "test_perf_irq_latency"),
    ("perf_spi",         "test_perf_spi"),
    ("perf_membus",      "test_perf_membus"),
]
```

## Part 5: MIPS baseline system in run_tests.py

The MIPS test is different from the others — it measures wall-clock time externally and compares against a stored baseline.

### Baseline file: `tests/perf_baseline.json`

```json
{
    "mips_elapsed_ms": 245,
    "host": "x86_64",
    "date": "2026-04-30",
    "note": "Initial baseline after clock_gettime fix"
}
```

This file is gitignored (machine-specific). Created on first run.

### Runner logic for MIPS test:

```python
import json, time

BASELINE_FILE = os.path.join(os.path.dirname(__file__), "perf_baseline.json")
REGRESSION_THRESHOLD = 0.20  # fail if >20% slower than baseline

def run_mips_test(elf_path, update_baseline=False):
    print(f"  perf_mips...", end=" ", flush=True)

    start = time.monotonic()
    result = subprocess.run(
        [SIM_CORE, "--machine", "gameboy", "--firmware", elf_path, "--no-chardev"],
        timeout=30, capture_output=True
    )
    elapsed_ms = (time.monotonic() - start) * 1000

    if result.returncode != 0:
        print(f"FAIL (exit code {result.returncode})")
        return False

    # Load or create baseline
    if update_baseline or not os.path.exists(BASELINE_FILE):
        baseline = {"mips_elapsed_ms": elapsed_ms, "host": os.uname().machine,
                     "date": time.strftime("%Y-%m-%d")}
        with open(BASELINE_FILE, "w") as f:
            json.dump(baseline, f, indent=2)
        print(f"PASS (baseline set: {elapsed_ms:.0f}ms)")
        return True

    with open(BASELINE_FILE) as f:
        baseline = json.load(f)

    baseline_ms = baseline["mips_elapsed_ms"]
    regression = (elapsed_ms - baseline_ms) / baseline_ms

    if regression > REGRESSION_THRESHOLD:
        print(f"FAIL ({elapsed_ms:.0f}ms vs baseline {baseline_ms:.0f}ms, "
              f"{regression*100:.1f}% regression)")
        return False
    else:
        print(f"PASS ({elapsed_ms:.0f}ms vs baseline {baseline_ms:.0f}ms, "
              f"{regression*100:+.1f}%)")
        return True
```

### CLI flags:

```python
# In main():
if "--update-baseline" in sys.argv:
    run_mips_test(elf, update_baseline=True)
    return
```

Usage:
```bash
# First run or after intentional changes:
python3 tests/run_tests.py --update-baseline

# Normal CI run:
python3 tests/run_tests.py
# perf_mips... PASS (248ms vs baseline 245ms, +1.2%)
# perf_mips... FAIL (412ms vs baseline 245ms, 68.2% regression)
```

### .gitignore entry:

```
tests/perf_baseline.json
```

## Part 6: CI-friendly output

The runner should print a summary that's easy to grep for regressions:

```
Running tests:
  hello_uart...       PASS
  alu_flags...        PASS
  ...
  perf_mips...        PASS  (elapsed: 2 cs)
  perf_systick...     PASS  (systick elapsed: 0x000003f2 cycles)
  perf_irq_latency... PASS  (irq latency: 0x0000000e cycles)
  perf_spi...         PASS  (spi 256 bytes: 0x00000400 cycles)
  perf_membus...      PASS  (ram write 1KB: 0x00000380 cycles)

15 passed, 0 failed
```

Capture the semihosting stderr output from each test and print the timing values alongside PASS/FAIL. These values can be included in commit messages for tracking:

```
perf: irq_latency=14 cycles, systick_period=1012 cycles, spi_256=1024 cycles
```

## What these tests catch

| Test | Catches |
|------|---------|
| perf_mips | Host-side regressions (clock_gettime overhead, slow membus, etc.) |
| perf_systick | SysTick firing at wrong rate relative to instruction count |
| perf_irq_latency | Interrupt entry taking too many/few cycles |
| perf_spi | SPI pacing changes (if added later) |
| perf_membus | Membus lookup regressions (TLB changes, region scan slowdowns) |
