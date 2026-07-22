Add the iCE40UP5K as a generic, bitstream-agnostic device in the MCU simulator. The device loads any netlist, exposes pins via `gpio_line` callbacks, and ticks the gate-level simulation. All protocol knowledge (SPI, LCD parallel) lives in the machine wiring, not in the device. As part of this, merge `sim/mcu` and `sim/fpga` back into a single `sim/` directory with one Makefile. Work in `/home/johmagnu/learning/embedded/sim`. Build with `make`.

## Goal

Model the iCE40UP5K FPGA chip the same way we model every other chip: as a device with pins. The device doesn't know what bitstream is loaded or what's connected to it. The machine file (`gameboy.c`) handles all wiring — connecting MCU SPI output to FPGA input pins, and FPGA output pins to the ILI9341 display.

## Architecture

```
Before (current):
  MCU SPI1 ──────────────────────────── ILI9341 (display)

After:
  MCU SPI1 ──── [board glue] ──── iCE40UP5K pins ──── [board glue] ──── ILI9341
                bit-bang bytes         generic              LCD parallel
                into FPGA pins         tick + pins          decode
```

The ice40up5k device:
1. Loads a netlist (any bitstream)
2. Ticks continuously on its internal clock, reading input pin levels each cycle
3. Fires `gpio_line` callbacks when output pins change
4. Accepts input pin writes from external code
5. Knows nothing about SPI, LCD, ILI9341, or any protocol

## File structure

Merge `sim/mcu` and `sim/fpga` back into a single `sim/` directory with one Makefile:

```
sim/
├── Makefile                ← single Makefile, builds sim-core
├── src/
│   ├── main.c             ← entry point (unchanged)
│   ├── core/              ← membus, chardev, spi_bus, event_queue, elf_load
│   ├── arch/armv7m/       ← CPU, NVIC, SysTick
│   ├── hw/stm32/          ← SPI, GPIO, UART, DMA, etc.
│   ├── soc/               ← stm32f411
│   ├── machine/           ← gameboy, gameboy-v2
│   ├── debug/             ← GDB stub
│   ├── devices/
│   │   ├── ili9341.c/h
│   │   ├── w25q128.c/h
│   │   ├── max98357a.c/h
│   │   └── ice40up5k/     ← NEW: generic FPGA device
│   │       ├── ice40up5k.c/h    ← device interface (pins, tick)
│   │       ├── netlist.c/h      ← Yosys iCE40 JSON netlist parser
│   │       ├── eval.c/h         ← LUT4/DFF/CARRY cell evaluation
│   │       └── hard_cells.c/h   ← SPRAM/BRAM/HFOSC simulation
│   └── sim-web/            ← Python + HTML (unchanged)
├── tests/
└── build/
```

### Makefile

```makefile
CC = gcc
CFLAGS = -Wall -O3 -g -flto -march=native -fomit-frame-pointer -DNDEBUG
BUILD = build

SRC_DIRS = src src/core src/arch/armv7m src/hw/stm32 src/devices src/devices/ice40up5k src/soc src/machine src/debug
INCLUDES = $(addprefix -I,$(SRC_DIRS))

SRCS = src/main.c \
       src/core/membus.c src/core/spi_bus.c src/core/chardev.c \
       src/core/event_queue.c src/core/elf_load.c \
       src/arch/armv7m/armv7m_cpu.c src/arch/armv7m/armv7m_nvic.c \
       src/arch/armv7m/armv7m_systick.c \
       src/hw/stm32/stm32_uart.c src/hw/stm32/stm32_spi.c \
       src/hw/stm32/stm32_gpio.c src/hw/stm32/stm32_exti.c \
       src/hw/stm32/stm32_adc.c src/hw/stm32/stm32_dma.c \
       src/hw/stm32/stm32_syscfg.c \
       src/devices/ili9341.c src/devices/max98357a.c \
       src/devices/trace_dev.c src/devices/w25q128.c \
       src/devices/ice40up5k/ice40up5k.c \
       src/devices/ice40up5k/netlist.c \
       src/devices/ice40up5k/eval.c \
       src/devices/ice40up5k/hard_cells.c \
       src/soc/stm32f411.c \
       src/machine/gameboy.c src/machine/machine.c \
       src/debug/dbg_stub.c

OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

all: $(BUILD)/sim-core

$(BUILD)/sim-core: $(OBJS)
	$(CC) -flto -o $@ $^

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

test: $(BUILD)/sim-core
	python3 tests/run_tests.py

clean:
	rm -rf $(BUILD)

.PHONY: all clean test
```

The standalone `fpga-sim` binary is gone. If you want to run a netlist without the MCU, you use `--machine fpga-only` (a machine variant that just ticks the FPGA and produces VCD output — no ARM CPU, no firmware). Same binary, different mode.

## Device interface (ice40up5k.h)

```c
#ifndef ICE40UP5K_H
#define ICE40UP5K_H

#include <stdint.h>
#include "gpio_line.h"

struct sim_state;

#define ICE40_MAX_PINS 48  /* SG48 package */

struct ice40_pin {
    char name[32];
    int net_id;             /* internal net this pin drives/reads */
    int direction;          /* 0=input, 1=output */
    uint8_t level;          /* current level (for change detection on outputs) */
    struct gpio_line out;   /* callback fired when output changes */
};

struct ice40up5k {
    struct sim_state *fpga;
    struct ice40_pin pins[ICE40_MAX_PINS];
    int num_pins;
};

/* Load a netlist. Pins are auto-discovered from the netlist's ports. */
void ice40up5k_init(struct ice40up5k *dev, const char *netlist_path);
void ice40up5k_free(struct ice40up5k *dev);

/* Advance simulation by one FPGA clock cycle */
void ice40up5k_tick(struct ice40up5k *dev);

/* Advance simulation by N FPGA clock cycles */
void ice40up5k_tick_n(struct ice40up5k *dev, int n);

/* Set an input pin level (by index) */
void ice40up5k_set_pin(struct ice40up5k *dev, int pin_idx, uint8_t level);

/* Find pin index by name. Returns -1 if not found. */
int ice40up5k_find_pin(struct ice40up5k *dev, const char *name);

#endif
```

## Device implementation (ice40up5k.c)

```c
#include "ice40up5k.h"
#include "netlist.h"
#include "eval.h"
#include <string.h>
#include <stdio.h>

void ice40up5k_init(struct ice40up5k *dev, const char *netlist_path)
{
    memset(dev, 0, sizeof(*dev));

    dev->fpga = netlist_load(netlist_path, NULL);
    memset(dev->fpga->nets, 0, sizeof(dev->fpga->nets));

    /* Discover pins from netlist ports */
    struct sim_state *s = dev->fpga;
    for (int i = 0; i < s->num_ports; i++) {
        struct port_info *p = &s->ports[i];
        if (p->width == 1) {
            /* Single-bit port → one pin */
            struct ice40_pin *pin = &dev->pins[dev->num_pins++];
            strncpy(pin->name, p->name, sizeof(pin->name));
            pin->net_id = p->bits[0];
            pin->direction = p->direction;  /* from netlist: input/output */
            pin->level = 0;
        } else {
            /* Multi-bit port → one pin per bit */
            for (int b = 0; b < p->width; b++) {
                struct ice40_pin *pin = &dev->pins[dev->num_pins++];
                snprintf(pin->name, sizeof(pin->name), "%s[%d]", p->name, b);
                pin->net_id = p->bits[b];
                pin->direction = p->direction;
                pin->level = 0;
            }
        }
    }

    /* Let FPGA settle */
    ice40up5k_tick_n(dev, 100);

    fprintf(stderr, "[ice40up5k] Loaded: %d cells, %d nets, %d pins\n",
            s->num_cells, s->num_nets, dev->num_pins);
}

void ice40up5k_tick(struct ice40up5k *dev)
{
    struct sim_state *s = dev->fpga;

    /* Rising edge */
    s->nets[s->clk_net] = 1;
    eval_clock_edge(s);
    eval_combinational(s);

    /* Falling edge — just toggle clock, no re-evaluation needed
     * unless the design uses clk combinationally (PPU does not) */
    s->nets[s->clk_net] = 0;

    /* Check output pins for changes, fire callbacks */
    for (int i = 0; i < dev->num_pins; i++) {
        struct ice40_pin *p = &dev->pins[i];
        if (p->direction != 1) continue;
        uint8_t new_level = (p->net_id >= 0) ? s->nets[p->net_id] : 0;
        if (new_level != p->level) {
            p->level = new_level;
            gpio_set(&p->out, new_level);
        }
    }
}

void ice40up5k_tick_n(struct ice40up5k *dev, int n)
{
    for (int i = 0; i < n; i++)
        ice40up5k_tick(dev);
}

void ice40up5k_set_pin(struct ice40up5k *dev, int pin_idx, uint8_t level)
{
    if (pin_idx < 0 || pin_idx >= dev->num_pins) return;
    struct ice40_pin *p = &dev->pins[pin_idx];
    if (p->net_id >= 0)
        dev->fpga->nets[p->net_id] = level;
}

int ice40up5k_find_pin(struct ice40up5k *dev, const char *name)
{
    for (int i = 0; i < dev->num_pins; i++)
        if (strcmp(dev->pins[i].name, name) == 0)
            return i;
    return -1;
}

void ice40up5k_free(struct ice40up5k *dev)
{
    netlist_free(dev->fpga);
}
```

## Board wiring (gameboy.c)

All protocol knowledge lives here — the machine file knows what's connected to what:

```c
#include "ice40up5k.h"
#include "ili9341.h"

/* --- SPI-to-pins glue (MCU SPI byte → FPGA input pins) --- */

#define FPGA_CYCLES_PER_SPI_PHASE 6

struct spi_fpga_bridge {
    struct ice40up5k *fpga;
    struct gameboy *board;  /* to set fpga_ticked_this_cycle flag */
    int clk_pin;
    int mosi_pin;
    int cs_pin;
};

static uint8_t spi_to_fpga_transfer(void *opaque, uint8_t byte)
{
    struct spi_fpga_bridge *b = opaque;

    for (int bit = 7; bit >= 0; bit--) {
        ice40up5k_set_pin(b->fpga, b->mosi_pin, (byte >> bit) & 1);
        ice40up5k_set_pin(b->fpga, b->clk_pin, 0);
        ice40up5k_tick_n(b->fpga, FPGA_CYCLES_PER_SPI_PHASE);
        ice40up5k_set_pin(b->fpga, b->clk_pin, 1);
        ice40up5k_tick_n(b->fpga, FPGA_CYCLES_PER_SPI_PHASE);
    }
    ice40up5k_set_pin(b->fpga, b->clk_pin, 0);

    /* Prevent background ticker from double-ticking this cycle */
    b->board->fpga_ticked_this_cycle = 1;
    return 0;
}

static void spi_to_fpga_cs(void *opaque, int level)
{
    struct spi_fpga_bridge *b = opaque;
    ice40up5k_set_pin(b->fpga, b->cs_pin, level);
    ice40up5k_tick_n(b->fpga, 10);
}

/* --- LCD parallel glue (FPGA output pins → ILI9341) --- */

struct lcd_parallel {
    struct ili9341 *display;
    struct ice40up5k *fpga;
    int data_pins[8];
    int dc_pin;
    uint8_t dc;
};

static void lcd_wr_handler(void *opaque, int level)
{
    struct lcd_parallel *l = opaque;
    if (level == 0) {  /* falling edge of WR = data strobe */
        uint8_t byte = 0;
        for (int i = 0; i < 8; i++)
            byte |= (l->fpga->pins[l->data_pins[i]].level << i);
        ili9341_set_dc(l->display, l->dc);
        ili9341_transfer(l->display, byte);
    }
}

static void lcd_dc_handler(void *opaque, int level)
{
    ((struct lcd_parallel *)opaque)->dc = level;
}

/* --- Machine init --- */

static struct ice40up5k fpga;
static struct spi_fpga_bridge spi_bridge;
static struct lcd_parallel lcd;

void gameboy_v2_init(struct gameboy *b, struct chardev_table *chardevs)
{
    stm32f411_init(&b->soc, BOARD_SYSCLK_HZ);

    /* iCE40UP5K — netlist loaded later via --device fpga0=... */
    /* (init deferred, but struct is allocated) */

    /* ILI9341 display (separate device, wired to FPGA outputs) */
    static struct ili9341 display;
    ili9341_init(&display);
    display.chardev = chardevs ? chardev_find(chardevs, "display") : NULL;
    display.cycle_count_ptr = &b->soc.cpu.cycle_count;
    b->display = &display;

    /* ... rest of init (UART, flash, audio, etc.) ... */
}

/* Called from gameboy_load_device() when --device fpga0=path is parsed */
static void wire_fpga(struct gameboy *b, const char *netlist_path)
{
    ice40up5k_init(&fpga, netlist_path);

    /* Wire MCU SPI1 → FPGA SPI input pins */
    spi_bridge.fpga = &fpga;
    spi_bridge.clk_pin  = ice40up5k_find_pin(&fpga, "SPI_CLK");
    spi_bridge.mosi_pin = ice40up5k_find_pin(&fpga, "SPI_MOSI");
    spi_bridge.cs_pin   = ice40up5k_find_pin(&fpga, "SPI_CS");

    int idx = spi_bus_attach(&b->soc.spis[0].bus, &spi_bridge, spi_to_fpga_transfer);
    b->soc.spis[0].bus.slaves[idx].cs_active = 0;

    /* CS driven by GPIO PA4 */
    b->soc.gpio[0].out[4].handler = spi_to_fpga_cs;
    b->soc.gpio[0].out[4].opaque = &spi_bridge;

    /* Wire FPGA LCD output pins → ILI9341 */
    lcd.display = b->display;
    lcd.fpga = &fpga;
    lcd.dc = 0;

    for (int i = 0; i < 8; i++) {
        char name[16];
        snprintf(name, sizeof(name), "LCD_D[%d]", i);
        lcd.data_pins[i] = ice40up5k_find_pin(&fpga, name);
    }
    lcd.dc_pin = ice40up5k_find_pin(&fpga, "LCD_DC");

    int wr_pin = ice40up5k_find_pin(&fpga, "LCD_WR");
    fpga.pins[wr_pin].out.handler = lcd_wr_handler;
    fpga.pins[wr_pin].out.opaque = &lcd;

    int dc_pin = ice40up5k_find_pin(&fpga, "LCD_DC");
    fpga.pins[dc_pin].out.handler = lcd_dc_handler;
    fpga.pins[dc_pin].out.opaque = &lcd;

    /* Schedule background FPGA ticking */
    /* (handled in gameboy_v2_tick — no event needed) */
}
```

## Background ticking

The FPGA ticks at the correct ratio relative to the MCU clock. The machine's `tick()` function advances both:

```c
static uint64_t fpga_accum = 0;
#define MCU_HZ   100000000
#define FPGA_HZ   24000000

int gameboy_v2_tick(struct gameboy *b)
{
    int r = stm32f411_tick(&b->soc);

    /* Tick FPGA at correct clock ratio — but only if the SPI bridge
     * didn't already tick it this MCU cycle. The SPI bridge sets
     * fpga_ticked_this_cycle when it bit-bangs a transfer. */
    if (!b->fpga_ticked_this_cycle) {
        fpga_accum += FPGA_HZ;
        while (fpga_accum >= MCU_HZ) {
            ice40up5k_tick(&b->fpga_dev);
            fpga_accum -= MCU_HZ;
        }
    }
    b->fpga_ticked_this_cycle = 0;

    return r;
}
```

**Double-tick prevention**: The SPI bridge calls `ice40up5k_tick_n()` directly during a transfer (to clock in bits). It sets a flag so the background ratio ticker skips that MCU cycle. This prevents the FPGA from running at 2× speed during SPI activity. The FPGA accumulator is also advanced by the number of ticks the bridge consumed, keeping the ratio correct over time.

This gives exactly 24 FPGA ticks per 100 MCU ticks on average — derived from the actual clock speeds. Output pin callbacks fire during these ticks, feeding LCD data to the ILI9341.

**WFI / idle handling**: The MCU sim's `stm32f411_tick()` still returns and advances the cycle count during WFI — it just doesn't execute instructions. The FPGA keeps ticking at the correct ratio regardless of whether the CPU is sleeping or executing.

## Why this design is correct

The ice40up5k device has the same shape as every other chip in the sim:

| Device | Inputs | Outputs | Internal state |
|--------|--------|---------|----------------|
| `w25q128` | SPI bytes + CS | SPI MISO byte | 16MB flash array |
| `ili9341` | SPI bytes + DC | chardev frames | 240×320 framebuffer |
| `ice40up5k` | pin levels | pin level callbacks | netlist + net array |

It doesn't know:
- What bitstream is loaded (PPU? UART bridge? PWM controller?)
- What protocol its pins carry (SPI? I2C? parallel?)
- What's connected to its outputs (display? LEDs? nothing?)

All of that is the machine file's job.

## Bitstream loading in sim

On real hardware, the firmware calls `fpga_load_bitstream()` to clock the bitstream into the iCE40 over SPI. In the simulator, the FPGA netlist is already loaded via `--device fpga0=...` at startup. The firmware's bitstream loading code still runs — it sends bytes over SPI2 and checks CDONE — but the sim handles it gracefully:

- **SPI config bus**: The `gameboy-v2` machine wires SPI3 to a dummy sink (bytes are accepted and discarded). The firmware's `spi_write()` calls succeed without error.
- **CRESET/CDONE GPIO pins**: The machine init sets CDONE high immediately. When firmware reads CDONE after the "load", it sees success.
- **No behavioral change**: The firmware code is identical between sim and real hardware. It doesn't need `#ifdef SIM` or any conditional logic.

```c
// In gameboy_v2_init():

/* CDONE pin always reads high in sim (FPGA is "already configured") */
stm32_gpio_set_input(&b->soc.gpio[1], CDONE_PIN, 1);

/* SPI3 config bus — attach a null sink so writes don't fault */
spi_bus_attach(&b->soc.spis[2].bus, NULL, null_spi_transfer);
b->soc.spis[2].bus.slaves[0].cs_active = 1;
```

This means the same firmware binary works in both sim and on real hardware with zero changes.

## Netlist path

Loaded via `--device fpga0=path/to/netlist.json`, same pattern as `--device flash0=...`:

```c
// In gameboy_load_device():
if (strcmp(name, "fpga0") == 0) {
    wire_fpga(b, path);
    return 0;
}
```

## Build system

Single Makefile at `sim/Makefile` builds one binary: `sim-core`. The iCE40 gate-level simulation engine lives entirely within `src/devices/ice40up5k/` — it's an implementation detail of that device, not a shared library.

## Machine variant

```c
const struct machine_desc gameboy_v2_machine = {
    .name        = "gameboy-v2",
    .description = "STM32F411 + iCE40UP5K + ILI9341",
    .board_size  = sizeof(struct gameboy),
    .init        = gameboy_v2_init_wrap,
    .tick        = gameboy_v2_tick_wrap,  // ticks both MCU and FPGA
    .get_cpu     = gameboy_get_cpu,
    .get_bus     = gameboy_get_bus,
    .get_flash   = gameboy_get_flash,
    .get_ram     = gameboy_get_ram,
    .get_sysclk  = gameboy_get_sysclk,
    .load_device = gameboy_v2_load_device,
};
```

`--machine gameboy` still works as before (direct ILI9341 on SPI). `--machine gameboy-v2` uses the FPGA device.

## Running it

```bash
cd sim && make

# MCU standalone (direct ILI9341, no FPGA)
./build/sim-core \
    --machine gameboy \
    --firmware ../projects/gameboy/build/gameboy.elf \
    --chardev display=9004 --realtime

# MCU + FPGA (full system)
./build/sim-core \
    --machine gameboy-v2 \
    --firmware ../projects/gameboy-v2/build/gameboy-v2.elf \
    --device fpga0=../projects/gameboy-v2/build/ppu_top.json \
    --chardev display=9004 --realtime

# FPGA standalone (iCEBreaker dev board — no MCU, just tick a netlist with VCD output)
./build/sim-core \
    --machine icebreaker \
    --device fpga0=../projects/gameboy-v2/build/ppu_top.json \
    --cycles 2200000 --vcd build/ppu_debug.vcd

# sim-web (unchanged)
python3 src/sim-web/sim-web.py --display-port 9004
```

## Performance

### Gate-level cost estimate

| Per FPGA tick | Cost |
|---------------|------|
| eval_clock_edge (DFFs) | ~50ns (200 DFFs) |
| eval_hard_cells (SPRAM) | ~20ns |
| eval_combinational (LUTs, 1 pass) | ~300ns (~1500 LUTs, topo-sorted) |
| Output pin change detection | ~5ns |
| **Total per FPGA tick** | ~375ns |

| Per MCU tick | Cost |
|--------------|------|
| ARM CPU decode + execute | ~200ns |
| FPGA ticks (0.24 avg) | ~90ns |
| **Total per MCU tick** | ~290ns → ~3.4M MCU ticks/sec |

At 100MHz target, that's **~29× slower than real-time**. The game is not interactive at this speed — it's a correctness verification tool, not a playable experience.

### Why eval_combinational is called once per tick, not twice

The earlier design called eval_combinational on both rising and falling edges. For the PPU design (no combinational use of the clock net), the falling edge re-evaluation is unnecessary. The tick function only evaluates combinational logic once:

```c
void ice40up5k_tick(struct ice40up5k *dev) {
    s->nets[s->clk_net] = 1;
    eval_clock_edge(s);       // DFFs capture
    eval_hard_cells(s);       // SPRAM read/write
    eval_combinational(s);    // LUTs settle (single pass)
    s->nets[s->clk_net] = 0;
    // No second eval_combinational — clock isn't used combinationally
    // Check output pins for changes...
}
```

This halves the LUT evaluation cost. If a future design uses the clock combinationally, add a flag to enable the second pass.

### Interactive use: Verilator backend

For playable speed, the `ice40up5k_tick` internals can be swapped to call a Verilator-compiled model. The device interface (set pins, tick, read pins) is unchanged — only the evaluation engine differs:

| Backend | FPGA ticks/sec | MCU sim speed | Interactive? |
|---------|---------------|---------------|-------------|
| Gate-level | ~2.7M | ~29× slow | No — verification only |
| Verilator | ~100M+ | ~1× real-time | Yes — playable |

The gate-level backend is the default (no build dependencies). Verilator is opt-in for developers who need interactive testing.

### Netlist size

The full `ppu_top` design is estimated at:
- ~1500 LUTs, ~200 DFFs, ~50 carry cells
- ~4000–6000 nets (multi-bit buses expand net count)
- 1–2 SPRAM instances, 4–8 BRAM instances

`MAX_NETS` = 8192 should be sufficient. If it's not, the netlist loader will print an error and exit — easy to bump.

## Prerequisite: Hard IP support in the FPGA simulator

The gate-level simulator currently only handles `SB_LUT4`, `SB_DFF*`, and `SB_CARRY`. Full top-level designs like `ppu_top` use hard IP that must be supported before the device can simulate them.

### Primitives to add

| Primitive | Used in | Behavior |
|-----------|---------|----------|
| `SB_HFOSC` | `spi_led_top`, `ppu_top` | Internal oscillator — output net becomes the clock |
| `SB_SPRAM256KA` | `ppu_top` (sprite_mem) | 16K×16 single-port synchronous RAM with mask write |
| `SB_RAM40_4K` | `ppu_top` (if Yosys maps register files to BRAM) | 256×16 or 512×8 block RAM |

### Architecture change to sim/fpga

The current cell model assumes 1-bit-in/1-bit-out stateless cells. Hard IP cells have multi-bit ports and internal state. Introduce a second cell category:

```c
// Primitive cells (existing — unchanged, fast path)
struct cell { ... };  // LUT4, DFF, CARRY

// Memory/hard-IP cells (new — evaluated separately)
struct hard_cell {
    enum hard_type type;    // HARD_HFOSC, HARD_SPRAM, HARD_BRAM
    int ports[64];          // net IDs for all connections (flat array)
    int port_widths[8];     // width of each logical port
    void *state;            // internal state (RAM contents, config)
};
```

Evaluation order:

```
1. Rising edge:  eval_clock_edge()       — DFFs capture D→Q
                 eval_hard_cells()       — SPRAM/BRAM perform read/write
                 eval_combinational()    — LUTs/CARRYs settle (single topo-sorted pass)
2. Falling edge: just toggle clock net (no re-evaluation for PPU design)
```

### SB_HFOSC

**Ports**: `CLKHFEN` (input), `CLKHFPU` (input), `CLKHF` (output)
**Parameter**: `CLKHF_DIV` — divider setting (ignored in sim, we just drive the clock)

No evaluation needed. During netlist load, record the `CLKHF` output net ID. Use this as the clock net when no `clk` port exists:

```c
// In sim_state:
int hfosc_clk_net;  // -1 if no HFOSC present

// Clock detection fallback:
if (s->clk_net < 0 && s->hfosc_clk_net >= 0)
    s->clk_net = s->hfosc_clk_net;
```

### SB_SPRAM256KA

**Ports**:
- `ADDRESS[13:0]` — read/write address
- `DATAIN[15:0]` — write data
- `DATAOUT[15:0]` — read data (registered, 1-cycle latency)
- `MASKWREN[3:0]` — nibble write mask
- `WREN` — write enable
- `CHIPSELECT` — chip select
- `CLOCK` — clock (same as system clock)
- `STANDBY`, `SLEEP`, `POWEROFF` — power modes (ignored in sim)

**Internal state**: `uint16_t mem[16384]`

**Evaluation** (on clock edge, when CHIPSELECT=1):
```c
void eval_spram(struct hard_cell *hc, uint8_t *nets) {
    uint16_t addr = read_port(nets, hc, PORT_ADDRESS, 14);
    uint16_t din  = read_port(nets, hc, PORT_DATAIN, 16);
    uint8_t  mask = read_port(nets, hc, PORT_MASKWREN, 4);
    uint8_t  wren = read_port_bit(nets, hc, PORT_WREN);
    uint8_t  cs   = read_port_bit(nets, hc, PORT_CHIPSELECT);
    uint16_t *mem = (uint16_t *)hc->state;

    if (cs) {
        if (wren) {
            if (mask & 1) mem[addr] = (mem[addr] & 0xFFF0) | (din & 0x000F);
            if (mask & 2) mem[addr] = (mem[addr] & 0xFF0F) | (din & 0x00F0);
            if (mask & 4) mem[addr] = (mem[addr] & 0xF0FF) | (din & 0x0F00);
            if (mask & 8) mem[addr] = (mem[addr] & 0x0FFF) | (din & 0xF000);
        }
        write_port(nets, hc, PORT_DATAOUT, 16, mem[addr]);
    }
}
```

### SB_RAM40_4K

**Ports**: Similar to SPRAM but smaller (256×16 or configurable geometry). Two independent ports (read + write) with separate clocks and addresses.

**Internal state**: `uint16_t mem[256]` (or other geometry based on parameters)

**Evaluation**: Same pattern as SPRAM — read/write on clock edge. Port mapping depends on `READ_MODE` and `WRITE_MODE` parameters from Yosys.

### Multi-bit port helpers

```c
static inline uint32_t read_port(uint8_t *nets, int *port_nets, int width) {
    uint32_t val = 0;
    for (int i = 0; i < width; i++) {
        int net = port_nets[i];
        if (net == NET_CONST_1) val |= (1u << i);
        else if (net >= 0) val |= ((uint32_t)nets[net] << i);
    }
    return val;
}

static inline void write_port(uint8_t *nets, int *port_nets, int width, uint32_t val) {
    for (int i = 0; i < width; i++) {
        if (port_nets[i] >= 0)
            nets[port_nets[i]] = (val >> i) & 1;
    }
}
```

### File changes in sim/

```
sim/src/devices/ice40up5k/
├── ice40up5k.h ← add ice40_pin struct, device API
├── ice40up5k.c ← pin discovery, tick, set_pin, find_pin
├── netlist.h   ← add hard_cell struct, hfosc_clk_net field
├── netlist.c   ← parse SB_HFOSC/SB_SPRAM256KA/SB_RAM40_4K cells
├── eval.h      ← add eval_hard_cells() declaration
├── eval.c      ← add eval_hard_cells(), eval_spram(), eval_bram()
└── hard_cells.c/h ← NEW: SPRAM/BRAM evaluation logic
```

### Limits

Increase `MAX_NETS` from 4096 to 8192 — the full PPU design uses more nets due to multi-bit buses.

## Implementation order

### Phase 0: Merge sim/mcu and sim/fpga into sim/

1. Move `sim/mcu/src/*` → `sim/src/`
2. Move `sim/fpga/src/*` → `sim/src/devices/ice40up5k/` (drop the old `main.c`)
3. Move `sim/mcu/tests/` → `sim/tests/`
4. Create unified `sim/Makefile` (builds single `sim-core` binary)
5. Add `icebreaker` machine variant (ticks FPGA, produces VCD, supports `--assert`)
6. Delete `sim/mcu/` and `sim/fpga/`
7. Verify: `make` builds, `make test` passes all existing tests

### Phase 1: Hard IP — SB_HFOSC (unlocks `spi_led_top`)

1. Add `hfosc_clk_net` field to `sim_state`
2. Parse `SB_HFOSC` in `netlist.c` — extract `CLKHF` output net
3. In `main.c`, fall back to `hfosc_clk_net` when no `clk` port found
4. Test: synthesize full `spi_led_top` and simulate

### Phase 2: Hard IP — SB_SPRAM256KA (unlocks `ppu_top`)

1. Add `hard_cell` struct and `hard_cells[]` array to `sim_state`
2. Add multi-bit port read/write helpers
3. Parse `SB_SPRAM256KA` connections in `netlist.c`
4. Implement `eval_spram()` — called on clock edge
5. Allocate and zero-initialize 32KB state per SPRAM instance
6. Test: synthesize `ppu_top` with SPRAM and run basic SPI write + readback

### Phase 3: Hard IP — SB_RAM40_4K (unlocks Yosys BRAM inference)

1. Parse `SB_RAM40_4K` and variants
2. Decode geometry from `READ_MODE`/`WRITE_MODE` parameters
3. Implement `eval_bram()` — dual-port, separate read/write clocks
4. Test: force Yosys to map `tile_table` to BRAM, verify read/write

### Phase 4: Device skeleton

1. Create `ice40up5k.h` and `ice40up5k.c`
2. Implement init (load netlist, discover pins), tick, set_pin, find_pin
3. Test: load a simple netlist, set input pins, verify output pin callbacks fire

### Phase 5: Board wiring — SPI bridge

1. Write `spi_to_fpga_transfer()` glue in machine file
2. Wire MCU SPI1 → FPGA input pins
3. Test: MCU sends SPI byte → FPGA's internal spi_cmd module receives it

### Phase 6: Board wiring — LCD parallel → ILI9341

1. Write `lcd_wr_handler` / `lcd_dc_handler` glue in machine file
2. Connect FPGA output pin callbacks to ILI9341
3. Test: FPGA drives LCD_WR low → ILI9341 receives byte → chardev outputs frame

### Phase 7: Background ticking + end-to-end

1. Implement clock-ratio accumulator in `gameboy_v2_tick()` with double-tick prevention
2. Wire `--device fpga0=...` loading
3. Test: game firmware → SPI → FPGA renders → display shows game

## Testing

| Test | What it verifies |
|------|-----------------|
| `make test` | MCU tests + FPGA counter assertions all pass |
| HFOSC clock detection | `spi_led_top` netlist loads without "No 'clk' port" error |
| SPRAM write+read | Drive address/data/wren nets, verify DATAOUT after 1 cycle |
| SPRAM mask write | Partial mask → only masked nibbles change |
| `ppu_top` netlist loads | No unknown-cell errors |
| `--machine gameboy` still works | No regression |
| Pin discovery | `ice40up5k_find_pin("SPI_CLK")` returns valid index |
| Output callbacks | Setting input pins + ticking → output pin callbacks fire |
| SPI bridge | MCU sends byte → FPGA receives correct bits |
| LCD decode | FPGA LCD_WR strobe → ILI9341 gets correct byte |
| Display output | sim-web shows rendered frame |
| Different bitstream | Load spi_led_top netlist instead → LED output pins toggle (no ILI9341 needed) |

## Verification checklist

- [ ] `make` builds `sim-core` without warnings
- [ ] `make test` passes (MCU tests + FPGA assertions via `--machine icebreaker`)
- [ ] `spi_led_top` netlist loads and simulates (HFOSC works)
- [ ] SPRAM read/write produces correct values
- [ ] SPRAM mask write is nibble-granular
- [ ] Full `ppu_top` netlist loads without unknown-cell errors
- [ ] Counter design (2.2M cycles) not measurably slower after hard IP changes
- [ ] `--machine gameboy` still works (no regression)
- [ ] `--machine gameboy-v2` boots firmware
- [ ] `--machine icebreaker` produces correct VCD and assertions pass
- [ ] ice40up5k device has no `#include` of `ili9341.h` or any protocol header
- [ ] Loading a different netlist works without changing device code
- [ ] SPI transfer reaches FPGA internal nets
- [ ] FPGA output pin changes fire gpio_line callbacks
- [ ] ILI9341 receives correct LCD data via board wiring
- [ ] Display visible in sim-web
- [ ] No memory leaks (`valgrind`)
- [ ] Old `sim/mcu/` and `sim/fpga/` directories removed
