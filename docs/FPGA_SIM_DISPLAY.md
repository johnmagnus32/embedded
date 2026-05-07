Add visual display output to the FPGA simulator by tapping the LCD SPI signals and feeding them to the existing ILI9341 model + sim-web UI. Make the simulation backend pluggable so either our gate-level simulator or Verilator can drive the same display pipeline. Work in `/home/johmagnu/learning/simple-stm32/sim/fpga`. Build with `make`.

## Goal

Run the PPU design in simulation and see its rendered output in the browser — same sim-web UI used by the MCU simulator. The display pipeline (ILI9341 model → chardev → sim-web) is reused from `sim/mcu`. The simulation backend is swappable between our gate-level interpreter (slow, ~1 FPS) and Verilator (fast, ~40 FPS).

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│ sim/fpga                                                     │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐     │
│  │ Backend (pluggable)                                 │     │
│  │                                                     │     │
│  │  Option A: gate-level sim (our sim_state + eval)    │     │
│  │  Option B: Verilator (compiled C++ from Verilog)    │     │
│  │                                                     │     │
│  │  Interface: step(), get_port_bit(), set_port_bit()  │     │
│  └──────────────────────┬──────────────────────────────┘     │
│                         │ port values each cycle             │
│                         ▼                                    │
│  ┌──────────────────────────────────────────────────────┐    │
│  │ LCD tap (lcd_tap.c)                                  │    │
│  │                                                      │    │
│  │ Watches LCD_SCLK/MOSI/CS/DC nets each cycle          │    │
│  │ Decodes SPI bytes                                    │    │
│  │ Calls ili9341_transfer() for each byte               │    │
│  └──────────────────────┬───────────────────────────────┘    │
│                         │                                    │
│                         ▼                                    │
│  ┌──────────────────────────────────────────────────────┐    │
│  │ ili9341.c + chardev.c (reused from sim/mcu)          │    │
│  │                                                      │    │
│  │ Builds framebuffer from decoded commands/pixels       │    │
│  │ Flushes to TCP chardev socket on refresh interval    │    │
│  └──────────────────────┬───────────────────────────────┘    │
│                         │ TCP :9004                           │
└─────────────────────────┼────────────────────────────────────┘
                          │
                          ▼
┌──────────────────────────────────────────────────────────────┐
│ sim-web (unchanged — same Python + HTML from sim/mcu)        │
│                                                              │
│ Connects to display chardev, renders framebuffer in canvas   │
└──────────────────────────────────────────────────────────────┘
```

## New file structure

```
sim/fpga/
├── src/
│   ├── main.c              ← updated: backend selection, simulation loop
│   ├── netlist.c/h         ← unchanged
│   ├── eval.c/h            ← unchanged
│   ├── vcd.c/h             ← unchanged
│   ├── backend.h           ← NEW: pluggable backend interface
│   ├── backend_gate.c      ← NEW: wraps our gate-level sim as a backend
│   ├── backend_verilator.cpp ← NEW: wraps Verilator model as a backend
│   ├── lcd_tap.c/h         ← NEW: SPI decoder → ILI9341 (standalone module, future sim_node)
│   ├── spi_driver.c/h      ← NEW: bit-bangs bytes into backend SPI ports (replaceable by MCU node later)
│   └── display.c/h         ← NEW: ILI9341 + chardev init/tick (thin wrapper)
├── Makefile                ← updated: two build targets (gate vs verilator)
└── ...
```

Key separation:
- `lcd_tap.c` only talks to the backend via `get_port()` — never accesses internal state
- `spi_driver.c` only talks to the backend via `set_port()` — easily replaced by a cosim wire
- `main.c` orchestrates the loop but doesn't contain domain logic
- Each module could become a `sim_node` in a future cosim scheduler with zero API changes

## Design for future co-simulation

The backend interface is designed to map directly to a future `sim_node` in a co-simulation scheduler. Keep these principles:

1. **Backends are stateless from the caller's perspective** — the caller only interacts via `step()`, `get_port()`, `set_port()`. No reaching into internal state.

2. **Ports are the only interface between nodes** — all communication happens through named ports. The LCD tap reads ports, it doesn't peek at internal nets. This means the LCD tap could later become a separate `sim_node` (an ILI9341 node) wired to the FPGA node via the scheduler.

3. **No hardcoded clock relationships** — the backend's `step()` advances by one of its own clock cycles. It doesn't know or care what clock rate other components run at. The caller (or future scheduler) decides how to interleave.

4. **The LCD tap should be a separate module, not embedded in main.c** — it observes ports and drives the ILI9341 model. In a future cosim, it becomes its own node with input ports (SCLK, MOSI, CS, DC) and a chardev output. Keep it decoupled now so the refactor is trivial later.

5. **SPI input driving (testbench) should also be a separate module** — `spi_driver.c` that takes a byte buffer and bit-bangs it into the backend via `set_port()`. In a future cosim, this gets replaced by the MCU node's SPI output wired through the scheduler. The interface is the same — just the source of bytes changes.

### How the current design maps to future cosim

```
Current (single FPGA sim):              Future (cosim scheduler):
─────────────────────────               ─────────────────────────
main.c drives SPI inputs        →       MCU sim_node drives SPI via wires
backend->step() advances FPGA   →       scheduler calls fpga_node->step()
lcd_tap reads LCD ports          →       ILI9341 sim_node reads via wires
ili9341 + chardev → sim-web     →       same, just owned by the ILI9341 node
```

No architectural changes needed when adding cosim — just lift the existing modules into nodes and let the scheduler call them instead of main.c's loop.

## Backend interface

```c
// backend.h
#ifndef BACKEND_H
#define BACKEND_H

#include <stdint.h>

struct fpga_backend {
    void *ctx;

    /* Advance simulation by one clock cycle (rising + falling edge) */
    void (*step)(void *ctx);

    /* Read a 1-bit output port value */
    uint8_t (*get_port)(void *ctx, int port_id);

    /* Set a 1-bit input port value */
    void (*set_port)(void *ctx, int port_id, uint8_t val);

    /* Look up port ID by name (called once during init) */
    int (*find_port)(void *ctx, const char *name);

    /* Clean up */
    void (*destroy)(void *ctx);
};

/* Backend constructors */
struct fpga_backend *backend_gate_create(const char *netlist_json, const char *module);
struct fpga_backend *backend_verilator_create(void);

#endif
```

## Backend: gate-level (backend_gate.c)

Wraps the existing `sim_state` + `eval_combinational` + `eval_clock_edge`:

```c
#include "backend.h"
#include "netlist.h"
#include "eval.h"

struct gate_ctx {
    struct sim_state *state;
};

static void gate_step(void *ctx) {
    struct gate_ctx *g = ctx;
    struct sim_state *s = g->state;
    s->nets[s->clk_net] = 1;
    eval_clock_edge(s);
    eval_combinational(s);
    s->nets[s->clk_net] = 0;
    eval_combinational(s);
}

static uint8_t gate_get_port(void *ctx, int port_id) {
    struct gate_ctx *g = ctx;
    return get_net(g->state, port_id);
}

static void gate_set_port(void *ctx, int port_id, uint8_t val) {
    struct gate_ctx *g = ctx;
    if (port_id >= 0) g->state->nets[port_id] = val;
}

static int gate_find_port(void *ctx, const char *name) {
    struct gate_ctx *g = ctx;
    for (int i = 0; i < g->state->num_ports; i++)
        if (strcmp(g->state->ports[i].name, name) == 0)
            return g->state->ports[i].bits[0];
    return -1;
}

struct fpga_backend *backend_gate_create(const char *netlist, const char *module) {
    struct gate_ctx *g = malloc(sizeof(*g));
    g->state = netlist_load(netlist, module);
    memset(g->state->nets, 0, sizeof(g->state->nets));

    struct fpga_backend *b = malloc(sizeof(*b));
    b->ctx = g;
    b->step = gate_step;
    b->get_port = gate_get_port;
    b->set_port = gate_set_port;
    b->find_port = gate_find_port;
    return b;
}
```

## Backend: Verilator (backend_verilator.cpp)

```cpp
#include "Vppu_top.h"
#include "verilated.h"

extern "C" {
#include "backend.h"
}

struct verilator_ctx {
    Vppu_top *model;
};

// Port IDs are just enum indices
enum { PORT_SPI_CLK=0, PORT_SPI_MOSI, PORT_SPI_CS,
       PORT_LCD_SCLK, PORT_LCD_MOSI, PORT_LCD_CS, PORT_LCD_DC };

static void veri_step(void *ctx) {
    auto *v = (verilator_ctx *)ctx;
    v->model->clk = 1; v->model->eval();
    v->model->clk = 0; v->model->eval();
}

static uint8_t veri_get_port(void *ctx, int port_id) {
    auto *v = (verilator_ctx *)ctx;
    switch (port_id) {
    case PORT_LCD_SCLK: return v->model->LCD_SCLK;
    case PORT_LCD_MOSI: return v->model->LCD_MOSI;
    case PORT_LCD_CS:   return v->model->LCD_CS;
    case PORT_LCD_DC:   return v->model->LCD_DC;
    default: return 0;
    }
}

static void veri_set_port(void *ctx, int port_id, uint8_t val) {
    auto *v = (verilator_ctx *)ctx;
    switch (port_id) {
    case PORT_SPI_CLK:  v->model->SPI_CLK = val; break;
    case PORT_SPI_MOSI: v->model->SPI_MOSI = val; break;
    case PORT_SPI_CS:   v->model->SPI_CS = val; break;
    }
}

static int veri_find_port(void *ctx, const char *name) {
    if (strcmp(name, "SPI_CLK") == 0)  return PORT_SPI_CLK;
    if (strcmp(name, "SPI_MOSI") == 0) return PORT_SPI_MOSI;
    if (strcmp(name, "SPI_CS") == 0)   return PORT_SPI_CS;
    if (strcmp(name, "LCD_SCLK") == 0) return PORT_LCD_SCLK;
    if (strcmp(name, "LCD_MOSI") == 0) return PORT_LCD_MOSI;
    if (strcmp(name, "LCD_CS") == 0)   return PORT_LCD_CS;
    if (strcmp(name, "LCD_DC") == 0)   return PORT_LCD_DC;
    return -1;
}

extern "C" struct fpga_backend *backend_verilator_create(void) {
    auto *v = new verilator_ctx;
    v->model = new Vppu_top;

    auto *b = (fpga_backend *)malloc(sizeof(fpga_backend));
    b->ctx = v;
    b->step = veri_step;
    b->get_port = veri_get_port;
    b->set_port = veri_set_port;
    b->find_port = veri_find_port;
    return b;
}
```

## LCD tap (lcd_tap.c)

```c
// lcd_tap.h
#ifndef LCD_TAP_H
#define LCD_TAP_H

#include "backend.h"
#include "ili9341.h"

struct lcd_tap {
    int sclk_port, mosi_port, cs_port, dc_port;
    uint8_t prev_sclk, prev_cs;
    uint8_t shift;
    int bit_count;
    struct ili9341 *display;
};

void lcd_tap_init(struct lcd_tap *t, struct fpga_backend *b, struct ili9341 *display);
void lcd_tap_cycle(struct lcd_tap *t, struct fpga_backend *b);

#endif
```

```c
// lcd_tap.c
#include "lcd_tap.h"

void lcd_tap_init(struct lcd_tap *t, struct fpga_backend *b, struct ili9341 *display) {
    t->sclk_port = b->find_port(b->ctx, "LCD_SCLK");
    t->mosi_port = b->find_port(b->ctx, "LCD_MOSI");
    t->cs_port   = b->find_port(b->ctx, "LCD_CS");
    t->dc_port   = b->find_port(b->ctx, "LCD_DC");
    t->prev_sclk = 0;
    t->prev_cs = 1;
    t->shift = 0;
    t->bit_count = 0;
    t->display = display;
}

void lcd_tap_cycle(struct lcd_tap *t, struct fpga_backend *b) {
    uint8_t sclk = b->get_port(b->ctx, t->sclk_port);
    uint8_t cs   = b->get_port(b->ctx, t->cs_port);
    uint8_t mosi = b->get_port(b->ctx, t->mosi_port);
    uint8_t dc   = b->get_port(b->ctx, t->dc_port);

    // Rising edge of SCLK while CS active (low)
    if (sclk && !t->prev_sclk && !cs) {
        t->shift = (t->shift << 1) | mosi;
        t->bit_count++;
        if (t->bit_count == 8) {
            ili9341_set_dc(t->display, dc);
            ili9341_transfer(t->display, t->shift);
            t->bit_count = 0;
        }
    }

    // CS deassert resets bit counter
    if (cs && !t->prev_cs) {
        t->bit_count = 0;
    }

    t->prev_sclk = sclk;
    t->prev_cs = cs;
}
```

## Updated main loop

```c
int main(int argc, char **argv) {
    // Parse args: --backend gate|verilator, --netlist file, --display port
    const char *backend_type = "gate";
    const char *netlist = NULL;
    int display_port = 0;  // 0 = no display

    // ... arg parsing ...

    // Create backend
    struct fpga_backend *backend;
    if (strcmp(backend_type, "gate") == 0)
        backend = backend_gate_create(netlist, module);
    else
        backend = backend_verilator_create();

    // Set up display (if --display specified)
    struct ili9341 display;
    struct chardev *display_cd = NULL;
    struct lcd_tap tap;
    if (display_port > 0) {
        ili9341_init(&display);
        display_cd = chardev_create("display", display_port);
        chardev_listen(display_cd);
        display.chardev = display_cd;
        lcd_tap_init(&tap, backend, &display);
        fprintf(stderr, "[fpga-sim] Display on port %d\n", display_port);
    }

    // Simulation loop
    for (uint64_t cyc = 0; cyc < max_cycles; cyc++) {
        backend->step(backend->ctx);

        if (display_port > 0) {
            lcd_tap_cycle(&tap, backend);
            ili9341_tick(&display);
        }
    }

    // Cleanup
    if (display_cd) chardev_close(display_cd);
    backend->destroy(backend->ctx);
}
```

## Running it

```bash
# Terminal 1: FPGA sim with display output (gate-level backend, slow)
cd sim/fpga
./build/fpga-sim ../../projects/fpga-ppu/build/ppu_netlist.json \
    --backend gate --display 9004 --cycles 50000000

# Terminal 1 (alternative): Verilator backend (fast, ~real-time)
./build/fpga-sim-verilator --display 9004

# Terminal 2: sim-web (unchanged)
python3 ../mcu/src/sim-web/sim-web.py --display-port 9004

# Browser: http://localhost:3000 — see PPU output
```

## Build system

```makefile
# Two build targets:

# Gate-level backend (default, no Verilator needed)
fpga-sim: src/main.c src/backend_gate.c src/lcd_tap.c src/display.c \
          src/netlist.c src/eval.c src/vcd.c \
          $(MCU_SIM)/devices/ili9341.c $(MCU_SIM)/core/chardev.c
    $(CC) $(CFLAGS) -I$(MCU_SIM)/devices -I$(MCU_SIM)/core -o $@ $^

# Verilator backend (optional, requires verilator installed)
fpga-sim-verilator: src/main.c src/backend_verilator.cpp src/lcd_tap.c src/display.c \
                    $(MCU_SIM)/devices/ili9341.c $(MCU_SIM)/core/chardev.c
    verilator --cc ../../projects/fpga-ppu/src/*.v --exe --build \
        src/main.c src/backend_verilator.cpp src/lcd_tap.c \
        $(MCU_SIM)/devices/ili9341.c $(MCU_SIM)/core/chardev.c \
        -o $@
```

## Driving SPI inputs (simulating the STM32)

The testbench also needs to drive the SPI input ports (send sprite data + frame updates). This is done via `backend->set_port()`:

```c
// Helper: send one byte over SPI to the PPU
void spi_send_byte(struct fpga_backend *b, int clk_port, int mosi_port, int cs_port, uint8_t byte) {
    for (int bit = 7; bit >= 0; bit--) {
        b->set_port(b->ctx, mosi_port, (byte >> bit) & 1);
        // Several step() calls per SPI bit (system clock is faster than SPI)
        for (int i = 0; i < 8; i++) b->step(b->ctx);
        b->set_port(b->ctx, clk_port, 1);
        for (int i = 0; i < 8; i++) b->step(b->ctx);
        b->set_port(b->ctx, clk_port, 0);
    }
}

// Upload sprites, then send frame updates in a loop
void run_simulation(struct fpga_backend *b) {
    int spi_clk = b->find_port(b->ctx, "SPI_CLK");
    int spi_mosi = b->find_port(b->ctx, "SPI_MOSI");
    int spi_cs = b->find_port(b->ctx, "SPI_CS");

    // Upload sprite data
    b->set_port(b->ctx, spi_cs, 0);
    spi_send_byte(b, spi_clk, spi_mosi, spi_cs, 0x02);  // sprite upload cmd
    spi_send_byte(b, spi_clk, spi_mosi, spi_cs, 0x00);  // addr high
    spi_send_byte(b, spi_clk, spi_mosi, spi_cs, 0x00);  // addr low
    // ... send sprite pixel data ...
    b->set_port(b->ctx, spi_cs, 1);

    // Send frame updates
    while (1) {
        b->set_port(b->ctx, spi_cs, 0);
        spi_send_byte(b, spi_clk, spi_mosi, spi_cs, 0x01);  // frame update cmd
        spi_send_byte(b, spi_clk, spi_mosi, spi_cs, dino_y);
        spi_send_byte(b, spi_clk, spi_mosi, spi_cs, dino_frame);
        // ... obstacles ...
        b->set_port(b->ctx, spi_cs, 1);

        // Let PPU render a full frame (~2.5M cycles)
        for (int i = 0; i < 2500000; i++) {
            b->step(b->ctx);
            lcd_tap_cycle(&tap, b);
            ili9341_tick(&display);
        }
    }
}
```

## Testing

### Existing tests (must still pass)

| Test | What it verifies |
|------|-----------------|
| `make test` (current) | Counter netlist assertions still work |

### New tests

| Test | What it verifies |
|------|-----------------|
| LCD tap decodes bytes | Feed known SPI waveform to lcd_tap, verify ili9341 receives correct bytes |
| Backend interface | Both gate and verilator backends produce same port values for same inputs |
| Display output | Run PPU for one frame, capture framebuffer, verify sky=0x867D at (120,10) and ground=0x79E0 at (120,250) |
| End-to-end | Start sim with --display, connect sim-web, verify image appears in browser |

### Regression

```bash
# Verify gate-level sim still passes original tests
make test

# Verify display mode works (manual: check browser shows colored pixels)
./build/fpga-sim ../../projects/fpga-ppu/build/ppu_netlist.json \
    --backend gate --display 9004 --cycles 5000000 &
python3 ../mcu/src/sim-web/sim-web.py --display-port 9004
# Open browser → should see sky blue + brown ground
```

## Dependencies on sim/mcu code

Files reused (linked, not copied):

| File | What it provides |
|------|-----------------|
| `sim/mcu/src/devices/ili9341.c` | ILI9341 command decoder, framebuffer, flush |
| `sim/mcu/src/devices/ili9341.h` | ILI9341 struct and API |
| `sim/mcu/src/core/chardev.c` | TCP chardev (listen, accept, write) |
| `sim/mcu/src/core/chardev.h` | Chardev API |

These files may need minor modifications to compile standalone:
- Remove any `#include` of MCU-sim-specific headers not needed by the FPGA sim
- Ensure `ili9341_set_dc` is callable as a standalone function (currently it's a void* opaque callback — may need a direct-call wrapper)
- Chardev may reference headers like `spi_bus.h` transitively — verify and stub if needed

## Performance expectations

| Backend | Cycles/sec | Frames/sec | Use case |
|---------|-----------|------------|----------|
| Gate-level | ~2M | ~1 | Debugging, verifying correctness |
| Verilator | ~100M | ~40 | Interactive development, visual feedback |

## Verification checklist

- [ ] `make` builds gate-level backend with display support
- [ ] `make test` still passes (counter assertions)
- [ ] `--display 9004` starts chardev listener
- [ ] sim-web connects and shows framebuffer
- [ ] Sky pixels are blue (0x867D), ground pixels are brown (0x79E0)
- [ ] Sprite upload via SPI input → sprite appears in rendered frame
- [ ] Frame update via SPI input → dino position changes in rendered output
- [ ] Verilator backend (optional) produces same visual output as gate backend
- [ ] No changes to sim-web code
- [ ] ili9341.c/chardev.c linked from sim/mcu, not copied
