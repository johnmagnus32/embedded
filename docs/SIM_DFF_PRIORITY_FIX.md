# SB_DFFESR Enable Priority Bug — Root Cause and Fix

## Root Cause

The gate-level simulator's DFF evaluation had incorrect priority ordering for `SB_DFFESR` (DFF with Enable + Sync Reset) and `SB_DFFESS` (DFF with Enable + Sync Set) cells.

**Our code (wrong):**
```c
if (reset && R)       Q = 0;      // reset wins unconditionally
else if (enable)      Q = D;
else                  Q = hold;
```

**Real iCE40 hardware (from cells_sim.v):**
```verilog
always @(posedge C)
    if (E) begin          // enable gates EVERYTHING
        if (R) Q <= 0;   // reset only fires when enabled
        else   Q <= D;
    end
    // else: Q holds regardless of R
```

**The difference:** In real hardware, when `E=0`, the DFF holds its value no matter what `R` or `S` are. Our code applied reset even when enable was inactive, forcing the DFF to 0 when it should have held.

## How It Manifested

Yosys uses `SB_DFFESR` for counters inside state machines. The enable signal is active only in the state where the counter should change. The reset signal is derived from "not in the incrementing state" logic. With correct priority (enable gates reset), the counter holds when enable=0 despite reset=1. With our wrong priority, the counter got reset to 0 every tick it wasn't being incremented.

This caused `pixel_x` in `lcd_driver` to oscillate between 0 and 1 instead of counting to 239, and `init_idx` to behave incorrectly, preventing the PPU from ever rendering sprites.

## The Fix

One change in `eval.c` — check enable first:

```c
int enabled = (c->enable < 0 || get_net(s, c->enable));
if (!enabled) {
    new_q[i] = s->nets[c->output];  // hold
} else if (c->reset >= 0 && get_net(s, c->reset)) {
    new_q[i] = 0;
} else if (c->set >= 0 && get_net(s, c->set)) {
    new_q[i] = 1;
} else {
    new_q[i] = get_net(s, c->inputs[0]);
}
```

## Additional Fix: DFF Initialization

All iCE40 DFFs initialize to 0 regardless of variant. The `SB_DFFESS` "SS" suffix means "has a synchronous set PORT" — not "starts at 1". The `cells_sim.v` macro `SB_DFF_INIT` is `initial Q = 0` for all variants. Our code previously initialized `SB_DFFESS`/`SB_DFFSS` to 1, which was wrong.

## Regression Tests Needed

### 1. Counter with state-machine gating (reproduces the exact bug)

A state machine that increments a counter only in one specific state. Verifies enable-gates-reset behavior.

**File:** `sim/tests/firmware/func/hw/ice40up5k/test_fpga_counter_sm.v` + `.c`

```verilog
module counter_sm (input clk, input start, output reg [8:0] count, ...);
    // 5-state machine, counter increments only in S_LO
    // Synthesizes to SB_DFFESR with E=0 in non-LO states, R=1 in IDLE
endmodule
```

**Assertion:** After `start` pulse + 50 ticks, `count >= 8`.

### 2. DFF enable-vs-reset priority test

Directly tests that a DFF with both E and R holds when E=0, R=1.

**File:** `sim/tests/firmware/func/hw/ice40up5k/test_fpga_dff_priority.v` + `.c`

```verilog
module dff_priority (input clk, input d, input enable, input reset, output reg q);
    always @(posedge clk)
        if (enable) begin
            if (reset) q <= 0;
            else q <= d;
        end
endmodule
```

**Assertions:**
- Set D=1, E=1, R=0, tick → Q=1
- Set E=0, R=1, tick → Q still 1 (hold, not reset)
- Set E=1, R=1, tick → Q=0 (now reset fires)

### 3. DFF initialization test

Verifies all DFF variants start at 0.

**File:** `sim/tests/firmware/func/hw/ice40up5k/test_fpga_dff_init.v` + `.c`

```verilog
module dff_init (input clk, input d, output reg q_plain, output reg q_ss);
    always @(posedge clk) q_plain <= d;
    always @(posedge clk) if (d) q_ss <= 1;  // forces SB_DFFESS inference
endmodule
```

**Assertion:** Both outputs are 0 before any clock edge.

### 4. PPU e2e gate-level test

The full PPU netlist test that was failing. Should be a permanent regression test.

**File:** `sim/tests/firmware/func/hw/ice40up5k/test_ppu_gate.c`

**Assertion:** After uploading sprite data and ticking 500K cycles, `non_bg > 0`.

## Files Modified

- `sim/src/devices/ice40up5k/eval.c` — DFF priority fix
- `sim/src/devices/ice40up5k/ice40up5k.c` — removed incorrect init_val=1 initialization
