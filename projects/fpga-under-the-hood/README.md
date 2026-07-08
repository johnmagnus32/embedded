# fpga-under-the-hood

An **illustrative** Verilog model of what happens *inside* an SRAM-based FPGA
(modeled on the Lattice iCE40) from power-up through configuration to running
your design.

> **This is a teaching model, not real hardware and not runnable as a bitstream.**
> You cannot synthesize these files onto an iCE40 — they *describe* the fixed
> silicon that makes synthesis possible in the first place. Read them top to
> bottom to understand the mechanism; don't try to build them.

## The core idea

An SRAM-based FPGA is **volatile**: it forgets its entire personality on every
power-off. So each boot it must be *reconfigured* — a big binary **bitstream**
is streamed in and stored in on-chip **configuration RAM (CRAM)**. Every bit of
CRAM sets one switch in the reprogrammable **fabric** (LUTs + flip-flops +
routing). Once CRAM is full, the fabric *is* your circuit.

The circuit that performs this loading — the **config controller** — is NOT
made of fabric (it has to exist before the fabric works). It is fixed silicon,
laid down at the factory. That is what most of these files model.

## Real silicon vs. this teaching model

| Concept in these files | Real on iCE40? |
|------------------------|----------------|
| CRAM = SRAM whose bits control the fabric | ✅ Real |
| CRAM is DISTRIBUTED — the cells live inside the LUTs / routing / cells, not in a separate block | ✅ Real |
| A LUT is a tiny SRAM (truth table), and those cells ARE part of CRAM | ✅ Real |
| Routing = config-bit-controlled muxes (those bits are CRAM too) | ✅ Real |
| Fabric = a regular ARRAY of identical tiles, addressed by position | ✅ Real |
| Each cell input picks its source from a mux of nearby tracks (local routing) | ✅ Real (shape) |
| Config controller is fixed silicon beside the fabric | ✅ Real |
| POR gates config on VCC/SPI_VCCIO1/VPP_2V5 | ✅ Real |
| CDONE is open-drain (pull low or release) | ✅ Real |
| Boot mode chosen by SPI_SS_B level at CRESET rising | ✅ Real |
| Config sequence (reset → clear → load → startup → done) | ✅ Real behavior |
| **Exact** state encodings, addresses, frame sizes, bit counts | ⚠️ Invented for clarity |
| **Exact** bitstream byte format (sync word, opcodes) | ⚠️ Not reproduced (partly proprietary; see Project IceStorm) |

The **behavior** is faithful; the **exact numbers/encodings** are simplified so
the mechanism is legible.

## How the real chip was authored (the honest version)

- The **digital** hard blocks here (config FSM, loader, CDONE logic) were very
  likely written by Lattice engineers in real Verilog/VHDL — then pushed through
  an **ASIC flow** (synthesis → standard cells → photomask → fixed transistors),
  NOT an FPGA bitstream flow. Same language, permanent silicon.
- The **analog** hard blocks (PLL, the POR voltage detector, the `SB_HFOSC`
  oscillator, the `SB_RGBA_DRV` LED-driver current sinks) were NOT Verilog at
  all — they are transistor-level schematics designed and verified in SPICE.
  You only get a *simulation model* + a placement primitive for those.

So "you can't author this" means *you, the FPGA user, can't change it* — not
that no one wrote it in an HDL.

## Reading order

1. **`fabric_cell.v`** — the LUT, FF, routing switch, and routing **mux**, each
   owning its own config cells. Start here: it shows that "the LUTs" and "CRAM"
   are the same silicon — configuration = writing those in-place cells.
2. **`cram.v`** — explains that CRAM is **distributed, not a block**; contains
   only the shared config *write bus* (no storage), and says why.
3. **`por.v`** — the power-on-reset detector that gates everything.
4. **`cram_loader.v`** — the serial-bit → address/data datapath (the "LOAD" step)
   that drives the write bus.
5. **`cdone_pin.v`** — the open-drain success flag (explains your board's LED).
6. **`config_controller.v`** — the master FSM tying it all together.
7. **`fpga_top.v`** — the whole die: controller + loader + write bus fanned out
   to a **`generate`d array of N identical tiles** (LUT+FF+routing-mux each,
   addressed by position) + pins; the blank → configure → user-mode lifecycle.
   Scale the fabric with the `N_CELLS` localparam (default 100).

## The lifecycle in one picture

```
POWER ON
   │  POR waits for VCC / SPI_VCCIO1 / VPP_2V5 valid
   ▼
FABRIC BLANK  (CRAM empty — chip does nothing yet)
   │  CRESET_B pulsed; SPI_SS_B sampled at its rising edge → master/slave
   ▼
CONFIGURATION  ── bitstream shifted in via SPI_SCK/SPI_SI → written to CRAM
   │
   ▼
STARTUP  (≥49 wake-up clocks; fabric reset released)
   │
   ▼
CDONE ── HIGH   "configured OK, your design is live"
   │  SPI config pins flip role → become ordinary user GPIO
   ▼
USER MODE  (the fabric now runs your Verilog)
   │
   ▼
POWER OFF ──► CRAM forgotten ──► repeat every boot
```
