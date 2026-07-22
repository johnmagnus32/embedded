# iCE40UP5K Device Tests

Functional tests for the iCE40UP5K sim device. Each test pairs a trivial STM32 firmware with a trivial FPGA netlist. The firmware drives FPGA input pins via GPIO and asserts FPGA output pins read back correctly. Tests are ordered by complexity — each one exercises one more FPGA primitive.

## Test structure

```
sim/tests/firmware/func/devices/ice40up5k/
├── test_passthrough.c      ← simplest: input pin → output pin (wire)
├── test_inverter.c         ← LUT4 as NOT gate
├── test_and_gate.c         ← LUT4 as AND gate
├── test_or_gate.c          ← LUT4 as OR gate
├── test_dff.c              ← DFF: capture on clock edge
├── test_counter.c          ← DFF chain: 4-bit counter
├── test_bram.c             ← SB_RAM40_4K: write then read
├── test_spram.c            ← SB_SPRAM256KA: write then read
├── test_spi_slave.c        ← full SPI slave (bit-bang from MCU, decode in FPGA)
└── netlists/
    ├── passthrough.json
    ├── inverter.json
    ├── and_gate.json
    ├── or_gate.json
    ├── dff.json
    ├── counter.json
    ├── bram.json
    ├── spram.json
    └── spi_slave.json
```

Each netlist is synthesized from a minimal Verilog module via `yosys -p "synth_ice40 -json"`.

## Test 1: Passthrough (wire)

**What it tests:** Pin I/O works. FPGA passes input directly to output.

**Verilog:**
```verilog
module passthrough (input pin_in, output pin_out);
    assign pin_out = pin_in;
endmodule
```

**Firmware:**
```c
void test_passthrough(void)
{
    /* MCU PA0 → FPGA pin_in, FPGA pin_out → MCU PA1 */
    gpio_set_output(GPIOA, 0);
    gpio_set_input(GPIOA, 1);

    gpio_write(GPIOA, 0, 0);
    fpga_tick(10);
    ASSERT(gpio_read(GPIOA, 1) == 0);

    gpio_write(GPIOA, 0, 1);
    fpga_tick(10);
    ASSERT(gpio_read(GPIOA, 1) == 1);

    PASS("passthrough");
}
```

**Machine wiring:**
```c
// MCU GPIOA pin 0 → FPGA "pin_in"
// FPGA "pin_out" → MCU GPIOA pin 1 (as input)
int fpga_in = ice40up5k_find_pin(&fpga, "pin_in");
int fpga_out = ice40up5k_find_pin(&fpga, "pin_out");
soc.gpio[0].out[0].handler = fpga_input_handler;  // PA0 drives FPGA input
fpga.pins[fpga_out].out.handler = mcu_input_handler;  // FPGA output drives PA1
```

## Test 2: Inverter (LUT4 as NOT)

**What it tests:** LUT4 evaluation with a single-input truth table.

**Verilog:**
```verilog
module inverter (input a, output y);
    assign y = ~a;
endmodule
```

**Firmware:**
```c
void test_inverter(void)
{
    gpio_write(GPIOA, 0, 0);
    fpga_tick(10);
    ASSERT(gpio_read(GPIOA, 1) == 1);  /* ~0 = 1 */

    gpio_write(GPIOA, 0, 1);
    fpga_tick(10);
    ASSERT(gpio_read(GPIOA, 1) == 0);  /* ~1 = 0 */

    PASS("inverter");
}
```

## Test 3: AND gate (LUT4 as 2-input AND)

**What it tests:** LUT4 with two inputs.

**Verilog:**
```verilog
module and_gate (input a, input b, output y);
    assign y = a & b;
endmodule
```

**Firmware:**
```c
void test_and_gate(void)
{
    /* PA0 → a, PA2 → b, FPGA y → PA1 */
    gpio_write(GPIOA, 0, 0); gpio_write(GPIOA, 2, 0); fpga_tick(10);
    ASSERT(gpio_read(GPIOA, 1) == 0);  /* 0 & 0 = 0 */

    gpio_write(GPIOA, 0, 1); gpio_write(GPIOA, 2, 0); fpga_tick(10);
    ASSERT(gpio_read(GPIOA, 1) == 0);  /* 1 & 0 = 0 */

    gpio_write(GPIOA, 0, 0); gpio_write(GPIOA, 2, 1); fpga_tick(10);
    ASSERT(gpio_read(GPIOA, 1) == 0);  /* 0 & 1 = 0 */

    gpio_write(GPIOA, 0, 1); gpio_write(GPIOA, 2, 1); fpga_tick(10);
    ASSERT(gpio_read(GPIOA, 1) == 1);  /* 1 & 1 = 1 */

    PASS("and_gate");
}
```

## Test 4: OR gate (LUT4 as 2-input OR)

**What it tests:** Different LUT4 truth table.

**Verilog:**
```verilog
module or_gate (input a, input b, output y);
    assign y = a | b;
endmodule
```

**Firmware:**
```c
void test_or_gate(void)
{
    gpio_write(GPIOA, 0, 0); gpio_write(GPIOA, 2, 0); fpga_tick(10);
    ASSERT(gpio_read(GPIOA, 1) == 0);

    gpio_write(GPIOA, 0, 1); gpio_write(GPIOA, 2, 0); fpga_tick(10);
    ASSERT(gpio_read(GPIOA, 1) == 1);

    gpio_write(GPIOA, 0, 0); gpio_write(GPIOA, 2, 1); fpga_tick(10);
    ASSERT(gpio_read(GPIOA, 1) == 1);

    gpio_write(GPIOA, 0, 1); gpio_write(GPIOA, 2, 1); fpga_tick(10);
    ASSERT(gpio_read(GPIOA, 1) == 1);

    PASS("or_gate");
}
```

## Test 5: D flip-flop (SB_DFF)

**What it tests:** Sequential logic — output only changes on clock edge.

**Verilog:**
```verilog
module dff_test (input clk, input d, output reg q);
    always @(posedge clk) q <= d;
endmodule
```

**Firmware:**
```c
void test_dff(void)
{
    /* Set D=1, clock hasn't risen yet → Q should still be 0 */
    gpio_write(GPIOA, 0, 1);  /* D */
    fpga_tick(1);  /* combinational settles, but no clock edge */
    ASSERT(gpio_read(GPIOA, 1) == 0);  /* Q unchanged */

    /* Tick the FPGA clock (rising edge) → Q captures D */
    fpga_tick(1);  /* this is the rising edge */
    ASSERT(gpio_read(GPIOA, 1) == 1);  /* Q = D = 1 */

    /* Change D to 0, no clock edge yet */
    gpio_write(GPIOA, 0, 0);
    fpga_tick(1);
    ASSERT(gpio_read(GPIOA, 1) == 1);  /* Q holds */

    /* Clock edge → Q captures new D */
    fpga_tick(1);
    ASSERT(gpio_read(GPIOA, 1) == 0);  /* Q = D = 0 */

    PASS("dff");
}
```

## Test 6: 4-bit counter (DFF chain)

**What it tests:** Multiple DFFs, carry logic, multi-bit output.

**Verilog:**
```verilog
module counter (input clk, input rst, output [3:0] count);
    reg [3:0] cnt = 0;
    always @(posedge clk)
        if (rst) cnt <= 0;
        else cnt <= cnt + 1;
    assign count = cnt;
endmodule
```

**Firmware:**
```c
void test_counter(void)
{
    /* PA0 → rst, FPGA count[3:0] → PA1-PA4 */
    gpio_write(GPIOA, 0, 1);  /* assert reset */
    fpga_tick(2);
    ASSERT(read_4bit() == 0);

    gpio_write(GPIOA, 0, 0);  /* release reset */
    fpga_tick(2);  /* count = 1 */
    ASSERT(read_4bit() == 1);

    fpga_tick(2);  /* count = 2 */
    ASSERT(read_4bit() == 2);

    for (int i = 0; i < 13; i++) fpga_tick(2);
    ASSERT(read_4bit() == 15);  /* count = 15 */

    fpga_tick(2);  /* count wraps to 0 */
    ASSERT(read_4bit() == 0);

    PASS("counter");
}
```

## Test 7: Block RAM (SB_RAM40_4K)

**What it tests:** BRAM write and read with address/data buses.

**Verilog:**
```verilog
module bram_test (
    input clk,
    input we,
    input [7:0] addr,
    input [7:0] wdata,
    output [7:0] rdata
);
    reg [7:0] mem [0:255];
    reg [7:0] rdata_reg;
    always @(posedge clk) begin
        if (we) mem[addr] <= wdata;
        rdata_reg <= mem[addr];
    end
    assign rdata = rdata_reg;
endmodule
```

**Firmware:**
```c
void test_bram(void)
{
    /* Write 0xAB to address 0x10 */
    set_addr(0x10);
    set_wdata(0xAB);
    set_we(1);
    fpga_tick(2);  /* clock edge → write */
    set_we(0);

    /* Read back from address 0x10 */
    set_addr(0x10);
    fpga_tick(2);  /* clock edge → read latches */
    ASSERT(get_rdata() == 0xAB);

    /* Write 0x55 to address 0x20, verify 0x10 unchanged */
    set_addr(0x20);
    set_wdata(0x55);
    set_we(1);
    fpga_tick(2);
    set_we(0);

    set_addr(0x10);
    fpga_tick(2);
    ASSERT(get_rdata() == 0xAB);  /* still 0xAB */

    set_addr(0x20);
    fpga_tick(2);
    ASSERT(get_rdata() == 0x55);

    PASS("bram");
}
```

## Test 8: SPRAM (SB_SPRAM256KA)

**What it tests:** 16-bit wide SPRAM with mask write.

**Verilog:**
```verilog
module spram_test (
    input clk,
    input [13:0] addr,
    input [15:0] wdata,
    input [3:0] mask,
    input we, cs,
    output [15:0] rdata
);
    // Instantiates SB_SPRAM256KA
    SB_SPRAM256KA spram (
        .ADDRESS(addr), .DATAIN(wdata), .DATAOUT(rdata),
        .MASKWREN(mask), .WREN(we), .CHIPSELECT(cs),
        .CLOCK(clk), .STANDBY(1'b0), .SLEEP(1'b0), .POWEROFF(1'b1)
    );
endmodule
```

**Firmware:**
```c
void test_spram(void)
{
    /* Write 0xDEAD to address 0 */
    set_addr(0);
    set_wdata(0xDEAD);
    set_mask(0xF);  /* all nibbles */
    set_we(1); set_cs(1);
    fpga_tick(2);
    set_we(0);

    /* Read back */
    set_addr(0);
    fpga_tick(2);
    ASSERT(get_rdata() == 0xDEAD);

    /* Masked write: only low nibble */
    set_addr(0);
    set_wdata(0x1234);
    set_mask(0x1);  /* only nibble 0 */
    set_we(1);
    fpga_tick(2);
    set_we(0);

    fpga_tick(2);
    ASSERT(get_rdata() == 0xDEA4);  /* only low nibble changed */

    PASS("spram");
}
```

## Test 9: SPI slave (full protocol)

**What it tests:** Complete SPI slave implementation in the FPGA — the MCU bit-bangs SPI, the FPGA decodes it and stores a register value. MCU reads the register back via a separate output bus.

**Verilog:**
```verilog
module spi_slave (
    input clk,
    input spi_clk,
    input spi_mosi,
    input spi_cs,
    output [7:0] reg_out  /* last byte received */
);
    reg [2:0] spi_clk_sync;
    reg [1:0] spi_cs_sync;
    always @(posedge clk) begin
        spi_clk_sync <= {spi_clk_sync[1:0], spi_clk};
        spi_cs_sync <= {spi_cs_sync[0], spi_cs};
    end
    wire spi_clk_rise = (spi_clk_sync[2:1] == 2'b01);
    wire cs_active = ~spi_cs_sync[1];

    reg [7:0] shift_reg;
    reg [2:0] bit_cnt;
    reg [7:0] received;

    always @(posedge clk) begin
        if (!cs_active) begin
            bit_cnt <= 0;
        end else if (spi_clk_rise) begin
            shift_reg <= {shift_reg[6:0], spi_mosi};
            bit_cnt <= bit_cnt + 1;
            if (bit_cnt == 7)
                received <= {shift_reg[6:0], spi_mosi};
        end
    end

    assign reg_out = received;
endmodule
```

**Firmware:**
```c
void test_spi_slave(void)
{
    /* Bit-bang SPI: send 0xA5 to FPGA */
    set_cs(0);  /* CS active low */
    fpga_tick(10);

    uint8_t tx = 0xA5;
    for (int bit = 7; bit >= 0; bit--) {
        set_mosi((tx >> bit) & 1);
        set_sclk(0);
        fpga_tick(6);  /* setup time */
        set_sclk(1);
        fpga_tick(6);  /* hold time */
    }
    set_sclk(0);
    fpga_tick(10);
    set_cs(1);
    fpga_tick(10);

    /* Read reg_out from FPGA output pins */
    ASSERT(get_reg_out() == 0xA5);

    /* Send another byte: 0x3C */
    set_cs(0);
    fpga_tick(10);
    tx = 0x3C;
    for (int bit = 7; bit >= 0; bit--) {
        set_mosi((tx >> bit) & 1);
        set_sclk(0); fpga_tick(6);
        set_sclk(1); fpga_tick(6);
    }
    set_sclk(0); fpga_tick(10);
    set_cs(1); fpga_tick(10);

    ASSERT(get_reg_out() == 0x3C);

    PASS("spi_slave");
}
```

## How tests run

Each test is a standalone firmware that:
1. Loads a specific netlist via `--device fpga0=netlists/xxx.json`
2. Uses a test-specific machine variant (`--machine fpga-test`) that wires MCU GPIO ↔ FPGA pins
3. Drives inputs, ticks the FPGA, reads outputs, asserts
4. Exits via semihosting with PASS/FAIL

```bash
# Run all FPGA device tests:
cd sim
for test in passthrough inverter and_gate or_gate dff counter bram spram spi_slave; do
    ./build/sim-core --machine fpga-test \
        --firmware tests/firmware/func/devices/ice40up5k/build/test_${test}.elf \
        --device fpga0=tests/firmware/func/devices/ice40up5k/netlists/${test}.json
done
```

## Machine: fpga-test

A minimal machine with just an STM32 + iCE40UP5K, GPIO pins wired between them:

```c
/* MCU GPIO port A pins 0-7 ↔ FPGA pins (directly connected) */
/* PA0-PA3: MCU outputs → FPGA inputs */
/* PA4-PA7: FPGA outputs → MCU inputs */

// MCU output → FPGA input
for (int i = 0; i < 4; i++) {
    int fpga_pin = ice40up5k_find_pin(&fpga, input_names[i]);
    soc.gpio[0].out[i].handler = fpga_pin_driver;
    soc.gpio[0].out[i].opaque = &fpga_pins[fpga_pin];
}

// FPGA output → MCU input
for (int i = 0; i < 4; i++) {
    int fpga_pin = ice40up5k_find_pin(&fpga, output_names[i]);
    fpga.pins[fpga_pin].out.handler = mcu_pin_driver;
    fpga.pins[fpga_pin].out.opaque = &soc.gpio[0]; // drives PA4+i
}
```

## Generating netlists

Each Verilog file is synthesized to a JSON netlist:

```bash
cd sim/tests/firmware/func/devices/ice40up5k
for v in verilog/*.v; do
    name=$(basename $v .v)
    yosys -p "read_verilog $v; synth_ice40 -json netlists/${name}.json"
done
```

This is done once and the JSON files are checked into the repo (they're small, ~1-5KB each).

## Implementation order

1. Create directory structure and Verilog source files
2. Synthesize all netlists with Yosys
3. Create `fpga-test` machine variant
4. Implement `test_passthrough.c` — validates basic pin I/O
5. Implement `test_inverter.c` — validates LUT4
6. Implement `test_and_gate.c` and `test_or_gate.c` — validates multi-input LUT4
7. Implement `test_dff.c` — validates sequential logic
8. Implement `test_counter.c` — validates DFF chains + carry
9. Implement `test_bram.c` — validates SB_RAM40_4K hard IP
10. Implement `test_spram.c` — validates SB_SPRAM256KA hard IP
11. Implement `test_spi_slave.c` — validates full protocol decode (integration test)
12. Add to `run_tests.py` test runner

## Test 10: HFOSC (internal oscillator, self-clocking)

**What it tests:** SB_HFOSC — FPGA runs on its own internal clock without MCU driving a clock pin. The MCU just reads output pins that change autonomously.

**Verilog:**
```verilog
module hfosc_counter (output [3:0] count);
    wire clk;
    SB_HFOSC #(.CLKHF_DIV("0b10")) osc (
        .CLKHFEN(1'b1), .CLKHFPU(1'b1), .CLKHF(clk)
    );

    reg [3:0] cnt = 0;
    always @(posedge clk) cnt <= cnt + 1;
    assign count = cnt;
endmodule
```

**Firmware:**
```c
void test_hfosc(void)
{
    /* No clock pin driven by MCU — FPGA clocks itself via SB_HFOSC */
    /* Just tick the FPGA and observe the counter incrementing */

    uint8_t val1 = read_4bit();  /* PA4-PA7 */
    fpga_tick(100);  /* let HFOSC run */
    uint8_t val2 = read_4bit();

    /* Counter should have advanced */
    ASSERT(val2 != val1);

    fpga_tick(100);
    uint8_t val3 = read_4bit();
    ASSERT(val3 != val2);

    PASS("hfosc");
}
```

**What this validates in the sim:**
- `SB_HFOSC` cell is parsed from the netlist
- The HFOSC output net is identified as the clock source
- `ice40up5k_tick()` uses the HFOSC net as the clock (not an external pin)
- DFFs clocked by HFOSC advance correctly

## Test 11: Parallel output bus (8-bit)

**What it tests:** FPGA drives an 8-bit output bus simultaneously. MCU reads all 8 pins and verifies the value. Validates multi-bit port handling and that all output pin callbacks fire correctly.

**Verilog:**
```verilog
module parallel_out (
    input clk,
    input [7:0] din,
    input load,
    output [7:0] dout
);
    reg [7:0] reg_out = 0;
    always @(posedge clk)
        if (load) reg_out <= din;
    assign dout = reg_out;
endmodule
```

**Firmware:**
```c
void test_parallel_out(void)
{
    /* PA0-PA7 → FPGA din[7:0], PA8 → load
     * FPGA dout[7:0] → PB0-PB7 (MCU reads) */

    /* Load 0xA5 into FPGA register */
    set_din(0xA5);
    set_load(1);
    fpga_tick(2);  /* clock edge captures */
    set_load(0);

    /* Read 8-bit output */
    ASSERT(read_dout() == 0xA5);

    /* Load 0x3C */
    set_din(0x3C);
    set_load(1);
    fpga_tick(2);
    set_load(0);

    ASSERT(read_dout() == 0x3C);

    /* Verify individual bits */
    set_din(0x01);
    set_load(1);
    fpga_tick(2);
    set_load(0);
    ASSERT(read_dout() == 0x01);  /* only bit 0 high */

    set_din(0x80);
    set_load(1);
    fpga_tick(2);
    set_load(0);
    ASSERT(read_dout() == 0x80);  /* only bit 7 high */

    PASS("parallel_out");
}
```

**What this validates in the sim:**
- Multi-bit port discovery from netlist (8 individual pin callbacks)
- All 8 output pin `gpio_set` callbacks fire with correct levels
- MCU can read all 8 bits simultaneously
- Matches the LCD data bus pattern used in the real PPU (`LCD_D[7:0]`)

## Coverage summary

| Primitive | Test | PPU module that uses it |
|-----------|------|------------------------|
| Wire (assign) | test_passthrough | everywhere |
| SB_LUT4 (1 input) | test_inverter | everywhere |
| SB_LUT4 (2 input) | test_and, test_or | everywhere |
| SB_DFF | test_dff | spi_cmd, pixel_gen, lcd_driver |
| SB_DFF + SB_CARRY | test_counter | pixel_gen frame timer, lcd_driver cursor |
| SB_RAM40_4K | test_bram | sprite_table, tile_table |
| SB_SPRAM256KA | test_spram | sprite_mem |
| SPI clock domain crossing | test_spi_slave | spi_cmd |
| SB_HFOSC | test_hfosc | ppu_top |
| Multi-bit parallel I/O | test_parallel_out | lcd_driver (LCD_D[7:0]) |

All primitives and patterns used by the gameboy-v2 PPU are covered.

## Addressing concerns

### 1. DFF test timing

`ice40up5k_tick(1)` = one full clock cycle (rising edge + falling edge + combinational settle). You cannot observe "D set but clock hasn't risen" because the tick IS the clock edge. The DFF test should be rewritten:

```c
void test_dff(void)
{
    /* After init, Q = 0 (reset state) */
    ASSERT(gpio_read(GPIOA, 1) == 0);

    /* Set D=1, tick → rising edge captures D into Q */
    gpio_write(GPIOA, 0, 1);
    fpga_tick(1);
    ASSERT(gpio_read(GPIOA, 1) == 1);  /* Q = 1 */

    /* Set D=0, but DON'T tick yet — Q should still be 1 */
    gpio_write(GPIOA, 0, 0);
    /* No tick here — Q holds previous value */
    ASSERT(gpio_read(GPIOA, 1) == 1);  /* Q unchanged (no clock edge) */

    /* Now tick → Q captures new D */
    fpga_tick(1);
    ASSERT(gpio_read(GPIOA, 1) == 0);  /* Q = 0 */

    PASS("dff");
}
```

The key insight: setting a GPIO pin is instantaneous (no tick needed). Reading an output pin reflects the state after the last tick. Only `fpga_tick()` advances the clock.

### 2. Machine wiring: generic index-based approach

The `fpga-test` machine uses **index-based wiring**, not name-based. All FPGA input ports map to MCU GPIO outputs by discovery order, all FPGA output ports map to MCU GPIO inputs by discovery order:

```c
void fpga_test_wire(struct fpga_test_board *b)
{
    int out_idx = 0;  /* MCU output pin index (PA0, PA1, ...) */
    int in_idx = 0;   /* MCU input pin index (PB0, PB1, ...) */

    for (int i = 0; i < b->fpga.num_pins; i++) {
        if (b->fpga.pins[i].direction == 0) {
            /* FPGA input ← MCU output (GPIOA) */
            b->fpga_input_map[out_idx] = i;
            b->soc.gpio[0].out[out_idx].handler = fpga_pin_driver;
            b->soc.gpio[0].out[out_idx].opaque = &b->fpga.pins[i];
            out_idx++;
        } else {
            /* FPGA output → MCU input (GPIOB) */
            b->fpga.pins[i].out.handler = mcu_input_driver;
            b->fpga.pins[i].out.opaque = &b->soc.gpio[1];  /* GPIOB */
            b->fpga_output_map[in_idx] = i;
            b->mcu_input_pin[in_idx] = in_idx;
            in_idx++;
        }
    }
}
```

The firmware uses GPIO port A for outputs (driving FPGA inputs) and GPIO port B for inputs (reading FPGA outputs). Pin ordering matches the netlist's port declaration order. Each test firmware knows its own port layout (e.g., "my first output is `a`, second is `b`") and uses the corresponding GPIO pin index.

No per-test machine variants needed. One machine, generic wiring, firmware knows the pin mapping.

### 3. BRAM: implement as part of this work

The `SB_RAM40_4K` evaluation must be implemented before the BRAM test can pass. This is included in the implementation order:

**Phase 3 (from ICE40UP5K_DEVICE.md) is a prerequisite:**
1. Parse `SB_RAM40_4K` connections in `netlist.c`
2. Decode geometry from `READ_MODE`/`WRITE_MODE` parameters
3. Implement `eval_bram()` — dual-port synchronous RAM
4. Allocate internal state (`uint16_t mem[256]` or configured geometry)
5. Evaluate on clock edge: if write enable, store data; always output read data

The test_bram test validates this implementation. It is NOT expected-fail — it must pass before this work is considered complete.

### 4. Port direction discovery

The netlist parser must correctly identify port direction (input vs output) from the Yosys JSON. In Yosys JSON format:

```json
"ports": {
    "pin_in": { "direction": "input", "bits": [2] },
    "pin_out": { "direction": "output", "bits": [5] }
}
```

The `test_passthrough` test implicitly validates this: if directions are swapped, the MCU would try to drive an output pin (no effect) and read an input pin (always 0). The test would fail with "expected 1, got 0".

To make this explicit, add a direction sanity check in `ice40up5k_init()`:

```c
/* After pin discovery, log directions for debugging */
for (int i = 0; i < dev->num_pins; i++) {
    fprintf(stderr, "[ice40] pin %d: %s (%s)\n",
            i, dev->pins[i].name,
            dev->pins[i].direction ? "output" : "input");
}
```

If the passthrough test fails, this log immediately reveals whether directions were parsed wrong.
