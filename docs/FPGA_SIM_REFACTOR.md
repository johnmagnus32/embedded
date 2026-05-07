Refactor the FPGA gate-level simulator into clean, testable modules. Work in `/home/johmagnu/learning/simple-stm32/sim/fpga`. Build with `make`.

## Goal

Split the single-file `sim.c` (383 lines) into separate files by responsibility. Remove global state in favor of a simulation context struct. Replace the multi-pass evaluation hack with a proper topological sort. Auto-detect the target module from the netlist.

## Current structure

```
sim/fpga/
├── sim.c       ← everything in one file (parser, eval, VCD, main)
└── Makefile
```

## New structure

```
sim/fpga/
├── src/
│   ├── main.c       ← arg parsing, main simulation loop, assertions
│   ├── netlist.c    ← JSON parsing, cell/net construction
│   ├── netlist.h    ← data structures: sim_state, cell, port_info
│   ├── eval.c       ← simulation core: clock_edge, eval_combinational
│   ├── eval.h       ← eval API
│   ├── vcd.c        ← VCD file writer
│   └── vcd.h        ← VCD API
├── Makefile
└── README.md
```

## Key changes

### 1. Simulation state struct (no globals)

Replace all global arrays with a single context:

```c
// netlist.h
struct sim_state {
    uint8_t *nets;          // dynamically sized to num_nets
    int      num_nets;

    struct cell *cells;
    int      num_cells;

    int     *eval_order;    // indices of combinational cells (topo-sorted)
    int      num_eval;
    int     *dff_list;      // indices of DFF cells
    int      num_dffs;

    struct port_info *ports;
    int      num_ports;

    int      clk_net;       // net ID of the clock port
};
```

All functions take `struct sim_state *` as first argument.

### 2. Topological sort for evaluation order

Replace the 3-pass hack in `eval_combinational` with a proper topological sort computed once during `build_eval_order`:

1. Build a dependency graph: for each combinational cell, record which other combinational cells drive its inputs.
2. Topological sort (Kahn's algorithm): repeatedly find cells with no unsatisfied dependencies, add them to eval_order, mark their outputs as "available."
3. `eval_combinational` then runs a single pass through `eval_order` — guaranteed correct for any netlist.

DFF outputs are always "available" at the start of evaluation (they were captured on the clock edge), so they have no dependencies in the combinational graph.

### 3. Auto-detect target module

Instead of requiring `--module counter`, scan the modules in the JSON and pick the one that:
- Doesn't start with `$` (internal Yosys modules)
- Isn't a known cell library name (`SB_*`, `ICESTORM_*`)
- Has cells (not an empty definition)

If multiple candidates exist, require `--module`. If exactly one, use it automatically.

### 4. VCD writer abstraction

```c
// vcd.h
struct vcd_writer;

struct vcd_writer *vcd_open(const char *path, struct port_info *ports, int num_ports);
void vcd_sample(struct vcd_writer *w, uint64_t time_ns, uint8_t *nets, uint8_t *prev);
void vcd_close(struct vcd_writer *w);
```

Encapsulates file handle, signal IDs, and change detection. Main loop just calls `vcd_sample` without knowing VCD format details.

### 5. Netlist loader API

```c
// netlist.h
struct sim_state *netlist_load(const char *json_path, const char *module_name);
void netlist_free(struct sim_state *s);
```

Returns a fully constructed, topo-sorted simulation state ready to run. `module_name` can be NULL for auto-detect.

## Updated Makefile

```makefile
CC = gcc
CFLAGS = -Wall -O2 -g
BUILD = build
SRCS = src/main.c src/netlist.c src/eval.c src/vcd.c
OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

all: $(BUILD)/fpga-sim

$(BUILD)/fpga-sim: $(OBJS)
	$(CC) -o $@ $^

$(BUILD)/%.o: src/%.c src/*.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -Isrc -c -o $@ $<

test: $(BUILD)/fpga-sim
	@echo "=== Test 1: counter increments (led_b=1 after 2.2M cycles) ==="
	$(BUILD)/fpga-sim ../../projects/fpga-blink/build/counter_netlist.json \
		--cycles 2200000 --vcd $(BUILD)/counter.vcd \
		--assert led_b=1 --assert led_r=0 --assert led_g=0
	@echo ""
	@echo "=== Test 2: counter too early (led_b=0 after 100 cycles) ==="
	$(BUILD)/fpga-sim ../../projects/fpga-blink/build/counter_netlist.json \
		--cycles 100 --vcd /dev/null \
		--assert led_b=0 --assert led_r=0 --assert led_g=0
	@echo ""
	@echo "=== Test 3: auto-detect module (no --module flag) ==="
	$(BUILD)/fpga-sim ../../projects/fpga-blink/build/counter_netlist.json \
		--cycles 10 --vcd /dev/null
	@echo ""
	@echo "All tests passed."

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
```

## Testing

### Existing tests (must still pass)

| Test | Command | Expected |
|------|---------|----------|
| Counter increments | `--cycles 2200000 --assert led_b=1` | PASS (bit 21 toggled) |
| Counter too early | `--cycles 100 --assert led_b=0` | PASS (not enough cycles) |

### New tests

| Test | What it verifies |
|------|-----------------|
| Auto-detect module | Run without `--module` flag, should find `counter` automatically |
| Single-pass correctness | After refactor, results must match the multi-pass version bit-for-bit. Compare VCD output of old vs new for 1000 cycles. |
| Topological sort | Verify `eval_order` is a valid topological ordering: for each cell in the order, all its input-driving cells appear earlier. Print order with `--verbose` flag. |

### Regression test script

```bash
# Compare old and new simulator output
OLD_VCD=$(mktemp)
NEW_VCD=$(mktemp)

# Run old (before refactor, save VCD)
git stash
make clean && make
build/fpga-sim ../../projects/fpga-blink/build/counter_netlist.json \
    --cycles 1000 --vcd $OLD_VCD

# Run new (after refactor)
git stash pop
make clean && make
build/fpga-sim ../../projects/fpga-blink/build/counter_netlist.json \
    --cycles 1000 --vcd $NEW_VCD

# Compare (should be identical)
diff $OLD_VCD $NEW_VCD && echo "PASS: VCD output matches" || echo "FAIL: output differs"
rm $OLD_VCD $NEW_VCD
```

## Verification checklist

- [ ] `make` builds without warnings
- [ ] `make test` passes all 3 tests
- [ ] VCD output is identical to pre-refactor for 1000 cycles
- [ ] No global mutable state (all state in `struct sim_state`)
- [ ] `eval_combinational` runs exactly 1 pass (no multi-pass)
- [ ] `--module` flag still works for explicit selection
- [ ] Auto-detect works when `--module` is omitted
- [ ] `valgrind --leak-check=full build/fpga-sim ...` reports no leaks
