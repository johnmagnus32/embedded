# Optimization 5: Compiler Flags and PGO

Current flags are `-O2 -g -flto`. Significant free performance left on the table. Work in `/home/johmagnu/learning/simple-stm32/sim`. Build with `make` from `sim/`.

## Step 1: Update CFLAGS

Change the Makefile from:
```makefile
CFLAGS = -Wall -O2 -g -flto
```

To:
```makefile
CFLAGS = -Wall -O3 -g -flto -march=native -fomit-frame-pointer -DNDEBUG
```

What each flag does:
- `-O3` — more aggressive inlining, auto-vectorization, loop transforms (vs `-O2`)
- `-march=native` — use host CPU's full instruction set (AVX2, BMI2, etc.)
- `-fomit-frame-pointer` — frees RBP as a general register for the hot loop
- `-DNDEBUG` — disables assert() (good practice for release builds)
- `-flto` — already present, keep it

## Step 2: Add PGO (profile-guided optimization)

Two-pass build that uses real execution data to optimize branch prediction, function layout, and inlining.

Add Makefile targets:

```makefile
# Normal build
all: $(BUILD)/sim-core

# PGO build: ~15-25% faster than regular -O3
pgo: clean pgo-gen pgo-run pgo-use

pgo-gen:
	@echo "=== PGO pass 1: instrumented build ==="
	@mkdir -p $(BUILD)
	$(foreach src,$(SRCS),$(CC) $(CFLAGS) -fprofile-generate $(INCLUDES) -c -o $(patsubst src/%.c,$(BUILD)/%.o,$(src)) $(src);)
	$(CC) -O3 -flto -fprofile-generate -o $(BUILD)/sim-core $(OBJS)

pgo-run:
	@echo "=== PGO pass 2: collecting profile ==="
	./$(BUILD)/sim-core --machine gameboy \
		--firmware ../projects/gameboy/build/gameboy.elf \
		--bench 10000000 --no-chardev

pgo-use:
	@echo "=== PGO pass 3: optimized build ==="
	$(foreach src,$(SRCS),$(CC) $(CFLAGS) -fprofile-use -fprofile-correction $(INCLUDES) -c -o $(patsubst src/%.c,$(BUILD)/%.o,$(src)) $(src);)
	$(CC) -O3 -flto -fprofile-use -fprofile-correction -o $(BUILD)/sim-core $(OBJS)
	@echo "=== PGO build complete ==="
```

`-fprofile-correction` handles minor mismatches between the profiling run and the final build (e.g., if code paths differ slightly between bench mode and normal mode).

Usage:
```bash
make pgo    # full PGO build (~30 seconds)
make        # normal build (for development)
```

## Step 3: Add hot/cold annotations

Mark the hottest functions so GCC places them together for instruction cache locality:

```c
// In armv7m_cpu.c:
__attribute__((hot)) int armv7m_cpu_step(struct armv7m_cpu *c, struct membus *bus);

// In membus.c:
__attribute__((hot)) uint32_t membus_read32(struct membus *bus, uint32_t addr);
__attribute__((hot)) uint16_t membus_read16(struct membus *bus, uint32_t addr);
__attribute__((hot)) void membus_write32(struct membus *bus, uint32_t addr, uint32_t val);

// In armv7m_systick.c:
__attribute__((hot)) void armv7m_systick_check(struct armv7m_systick *st, ...);
```

## Step 4: Add restrict qualifiers

Tell the compiler that cpu and bus pointers don't alias:

```c
int armv7m_cpu_step(struct armv7m_cpu *restrict c, struct membus *restrict bus)
```

This lets GCC keep `c->r[]` values in registers across membus calls instead of reloading from memory after every function call.

## Expected improvement

- `-O3` vs `-O2`: ~10-15%
- `-march=native`: ~5-10%
- PGO: ~15-25% on top
- `hot`/`restrict` annotations: ~3-5%
- Combined: ~30-50% improvement (40 MIPS → ~55-60 MIPS)

## Testing

Run `make perf-bench` (or the MIPS test) at each step:

```
perf: 40.4 MIPS (baseline, -O2 -flto)
perf: 46.2 MIPS (-O3 -march=native -flto)
perf: 58.1 MIPS (-O3 -march=native -flto + PGO)
```

Record in commit message.

## Results (measured 2026-05-01)

Tested on top of OPT2 + OPT3 + OPT4 baseline.

### Step 1: -O3 -march=native -fomit-frame-pointer -DNDEBUG

| Metric | -O2 -flto | -O3 -march=native -flto | Change |
|--------|-----------|------------------------|--------|
| MIPS | 47 | 52 | +11% |
| DMA partial FPS | 70 | 72 | +3% |
| SPI partial FPS | 59 | 62 | +5% |
| Chardev idle FPS | 185 | 198 | +7% |
| Chardev DMA FPS | 136 | 139 | +2% |
| Audio samples/sec | 58,042 | 57,829 | — |

Consistent ~5-11% improvement across the board. Free performance from better compiler flags.

### Step 2: PGO

PGO was tested with two different training workloads:

1. **Gameboy firmware** (`--bench 10000000`): MIPS dropped from 52 to 33 (-37%). The gameboy firmware's instruction mix (RTOS scheduling, display writes) differs significantly from the test firmware's bare-metal patterns. PGO optimized for the wrong workload.

2. **Test firmware** (MIPS + SPI + DMA + IRQ tests): MIPS 51, DMA FPS 76 (+6%), but chardev idle FPS dropped from 198 to 129 (-35%) and audio from 57K to 42K (-27%). PGO improved the hot instruction decode path but hurt the peripheral/chardev paths.

**Conclusion**: PGO is not beneficial for this emulator. The instruction decode chain has too many equally-likely branches (each ARM instruction type is roughly equally common), so PGO's branch prediction hints don't help. The overhead of PGO instrumentation artifacts in cold paths hurts more than the hot path gains.

### Recommendation

Use `-O3 -march=native -fomit-frame-pointer -DNDEBUG -flto` without PGO. This is the current configuration.
