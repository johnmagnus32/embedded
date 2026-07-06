# Breadboard iCE40UP5K Board — Design Requirements

**Status:** Design spec / requirements. No board built yet.
**Audience:** A future engineer (or Claude session with no prior context) who needs to understand
*why* this board exists and *what it must do*, before designing the schematic/PCB.

> **Provenance / how to read the code references.** This board serves a companion firmware project,
> `gameboy-v2`, which at the time of writing lives in `projects/gameboy-v2/` but is **not yet committed
> to git — it is an untracked/uncommitted work tree**. If you are reading this on a clean checkout (or
> a different worktree), those files **will not be present**, and that is expected — do not conclude the
> project "doesn't exist." This document is **self-contained**: every pin assignment, the configuration
> sequence, and the electrical spec are stated inline here and do not require opening any other file to
> trust them. The `projects/gameboy-v2/...` paths are given only so that, *if* you do have that work
> tree, you can cross-check against the source. The facts below are the authority; the file paths are a
> convenience.

---

## 0. In one paragraph

This is a custom breakout board for the **Lattice iCE40UP5K FPGA** (part `iCE40UP5K-SG48I`, the 48-pin
QFN package). In our system the FPGA is used as a graphics coprocessor (a "PPU") and is **configured
by an STM32 microcontroller over SPI at every power-up** — the STM32 streams the FPGA's bitstream in
on boot; there is no configuration flash on the FPGA side. Off-the-shelf iCE40 breakout boards
(iCEBreaker, UPduino) **cannot** be used for this, because they each carry an onboard SPI
configuration flash and a USB programmer wired onto the same SPI bus the STM32 needs to drive —
they contend on that bus and force the FPGA into self-boot ("master") mode. This board therefore
strips all of that out: it exposes just the FPGA, its power, and its config + I/O pins, so an external
STM32 is the sole controller of the configuration bus. Details below.

---

## 1. Why this board exists

### The product: gameboy-v2

`projects/gameboy-v2` is a handheld game console built from **two chips**:

- **STM32F411RE** (ARM Cortex-M4) — runs the game logic, audio, and input. Acts as the "CPU."
- **iCE40UP5K** (Lattice FPGA) — a **Picture Processing Unit (PPU)**. It renders sprites to an
  ILI9341 LCD autonomously. The STM32 sends sprite positions over SPI each frame; the FPGA does all
  pixel work and drives the display.

The defining architectural decision of gameboy-v2: **the STM32 configures the FPGA at every boot.**
The FPGA bitstream is *embedded in the STM32's firmware flash* and streamed into the FPGA over SPI on
power-up. There is **no FPGA-side configuration flash** in the product. (In the firmware this is a
function `fpga_load_bitstream()` that runs first thing in `main()`, before the game starts — the exact
sequence it performs is reproduced in section 2 below, so you do not need the source to follow it.)

Why do it this way? The iCE40 is **SRAM-based**: its logic configuration lives in volatile on-chip
SRAM that is wiped on every power loss. Something must reload it each boot. The gameboy-v2 chooses to
make the STM32 that "something" — one firmware image contains both the CPU program and the FPGA
bitstream, one flash operation programs the whole system, and there is no second flash chip to stock,
place, or program.

### The problem: dev boards don't work this way

The obvious way to prototype is to buy an off-the-shelf iCE40 dev board (iCEBreaker, UPduino) and wire
it to a Nucleo. **This does not faithfully reproduce the gameboy-v2 boot path**, for a concrete
electrical reason:

- Those boards are designed to **self-boot from an onboard SPI config flash** (iCE40 "master mode").
  They also carry an FTDI USB chip that programs that flash.
- Both the flash and the FTDI are wired **onto the FPGA's configuration SPI bus**. The config
  chip-select line (`SPI_SS_B`) is *the same net* as the flash's chip-select.
- If an external STM32 tries to slave-configure the FPGA, it must drive `SPI_SS_B` low — which also
  selects the onboard flash, causing **bus contention** on the shared data line. (This is the same
  reason `iceprog --help` notes that an iCEstick needs its flash chip desoldered for direct SRAM
  programming.)

So on a stock dev board you cannot cleanly let the STM32 own the config bus without physically
removing the flash. That is fragile and not representative.

### What this board is

A **minimal iCE40UP5K breakout with no onboard config flash and no USB programmer** — just the FPGA,
its power, decoupling, and headers. The STM32 is the *only* master of the config bus. This lets us:

1. Run the **exact, unmodified gameboy-v2 firmware** (`fpga_load_bitstream()` and all runtime code)
   on a breadboard, with the STM32 configuring the FPGA at boot — bit-for-bit like the final product.
2. Validate the real boot path, SPI protocol, PPU rendering, and LCD output on real silicon before
   committing to the integrated gameboy-v2 PCB.
3. Iterate on hardware cheaply (a ~15-component 2-layer board) with the risky/expensive integration
   (STM32 + FPGA + flash + LCD + audio on one board) deferred until the fundamentals are proven.

**Non-goal:** This board is *not* meant to self-boot or be programmed over USB. It has no persistence
of its own. If it has no STM32 driving it, it does nothing — exactly like the FPGA in the final
product.

---

## 2. The boot / configuration model (must be reproduced exactly)

The board's whole reason for existing is to reproduce this sequence, so it is spelled out in full here
(you do not need any other file to implement it). This is the **iCE40 SPI-slave configuration**
procedure (Lattice TN1248); the gameboy-v2 firmware implements it in `src/fpga_loader.c` — a function
named `fpga_load_bitstream()` called from `main()` before the game starts — but that file is in the
uncommitted work tree, so the canonical description is the numbered steps below:

```
1. CRESET_B  -> low     Clear configuration SRAM.
2. SPI_SS_B  -> low     Select SPI SLAVE config mode (this level is sampled when CRESET releases).
3. (hold ~1 us)
4. CRESET_B  -> high    Release reset. FPGA clears CRAM, then waits for the bitstream.
5. (wait ~1200 us)      Give the chip time to clear config memory before clocking data.
6. Clock the bitstream on MOSI, SPI mode 0 (CPOL=0, CPHA=0), MSB first.
7. Send >=49 trailing dummy clocks (firmware sends 7 bytes = 56 clocks) to finish wake-up.
8. SPI_SS_B  -> high    Release select. FPGA transitions to user mode.
9. CDONE goes high      Success indicator (read by the STM32). If it stays low, config failed.
```

**Boot-mode selection is a single strap:** the level on `SPI_SS_B` at the instant `CRESET_B` is
released decides master vs slave:
- `SPI_SS_B` sampled **low**  -> **slave** mode (wait to be fed) — what gameboy-v2 forces.
- `SPI_SS_B` sampled **high** -> **master** mode (self-load from external flash) — what dev boards do.

On this board there is no flash on the `SPI_SS_B` net, so the STM32 can drive it low at reset to select
slave mode with nothing to contend against. (This is the electrical fix for the dev-board problem in
section 1.)

> Hardware-spec values (min CRESET low pulse, the wait-after-CRESET, exact dummy-clock count, whether
> CDONE needs an external pull-up) are being cross-checked against the Lattice datasheet / TN1248 and
> will be filled into section 5. The firmware currently uses 1 us / 1200 us / 56 clocks, which has
> worked in simulation; confirm against silicon.

---

## 3. Interfaces the board must expose

All signals are **3.3 V** on both the STM32F411 and the iCE40UP5K I/O banks, so **no level shifters
are required**. The pin assignments below are authoritative *as stated here*. They are transcribed
from the gameboy-v2 firmware's device tree (`board.dts`) and the FPGA constraints file
(`fpga/icebreaker.pcf`) — but those files are part of the uncommitted `projects/gameboy-v2/` work tree
(see Provenance note at top), so treat the tables here as the source of record and cross-check the
files only if you have them.

### 3a. Configuration bus (STM32 SPI3 + GPIO -> FPGA config pins)

Used only at boot, by `fpga_load_bitstream()`. This is the interface that stock dev boards cannot
expose cleanly.

| Role | STM32 pin | STM32 function | iCE40 config pin |
|------|-----------|----------------|------------------|
| Config SPI clock | PC10 | SPI3_SCK (AF6) | SPI_SCK |
| Config SPI data  | PC12 | SPI3_MOSI (AF6) | SPI_SI (a.k.a. SDI) |
| Config select    | PB6  | GPIO out       | SPI_SS_B |
| Config reset     | PB1  | GPIO out       | CRESET_B |
| Config done      | PB2  | GPIO in        | CDONE |

Notes:
- SPI3_MISO (PC11) is wired in `board.dts` but configuration is write-only from the STM32's side; the
  FPGA's `SPI_SO` is not needed for slave config. (Confirm whether `SPI_SO` should be left floating or
  pulled — see section 5.)
- On the real gameboy-v2 this same SPI3 bus is also shared with a W25Q128 flash (for audio). **This
  breadboard board does not need that flash** — it is only for the config path. Audio flash can be
  added later or omitted during bring-up.

### 3b. Runtime game-data bus (STM32 SPI1 -> FPGA) — the PPU command channel

Used every frame after boot. One-way (STM32 -> FPGA), so no MISO. CS is bit-banged as a plain GPIO in
`projects/gameboy-v2/src/ppu.c` (not hardware NSS).

| Role | STM32 pin | STM32 function | iCE40 pin (from .pcf) |
|------|-----------|----------------|------------------------|
| SPI clock | PA5 | SPI1_SCK (AF5) | 4  (net `SPI_CLK`) |
| SPI MOSI  | PA7 | SPI1_MOSI (AF5) | 2  (net `SPI_MOSI`) |
| SPI CS    | PA4 | GPIO out       | 47 (net `SPI_CS`) |

### 3c. Runtime display bus (FPGA -> ILI9341, 8-bit parallel / Intel 8080)

The FPGA drives the LCD directly. This is a **parallel 8080 bus**, *not* SPI — most cheap ILI9341
breakouts are SPI-only, so a parallel-capable module is required.

| Signal | iCE40 pin (from .pcf) |
|--------|------------------------|
| LCD_D0..D3 | 43, 38, 34, 31 |
| LCD_D4..D7 | 28, 27, 26, 25 |
| LCD_WR (write strobe) | 21 |
| LCD_DC (data/command) | 20 |
| LCD_CS (chip select)  | 19 |

Display wiring gotchas (the FPGA only ever *writes*):
- Tie the display's **RD** high (3.3 V) — the FPGA never reads.
- Tie/drive **RESET** high (3.3 V, or via RC, or a spare GPIO).
- Wire the backlight per the module (usually 3.3 V through a resistor).

> The pin numbers above are the iCE40 package ball/pad numbers as used in the current `.pcf`, which
> targets the iCEBreaker's PMOD layout. **When laying out a custom board these can be reassigned to
> whatever routes cleanly** — but if they change, `icebreaker.pcf` must be updated to match, and the
> bitstream re-synthesized. Keep a board-specific `.pcf` alongside the design.

---

## 4. Board requirements (what the PCB must provide)

Functional requirements, in priority order:

1. **iCE40UP5K-SG48** (QFN48) with complete, correctly-decoupled power (see section 5 for rails).
2. **No onboard configuration flash** on the config SPI bus. This is the core differentiator.
3. **No onboard USB/FTDI programmer.** The STM32 is the sole config master.
4. **Config pins broken out** to a labeled header: SPI_SCK, SPI_SI, SPI_SS_B, CRESET_B, CDONE (per
   3a); include SPI_SO too, in case the STM32 ever programs NVCM.
5. **Runtime I/O broken out** to labeled headers: SPI_CLK/MOSI/CS in (3b), and the 8 LCD data +
   WR/DC/CS out (3c), plus grounds.
6. **Break out ALL remaining usable GPIO** to a generic spare-header. This is a *prototyping* board —
   the marginal cost of a few extra header positions is trivial, and having every one of the ~39 user
   I/O accessible means you can add signals (extra buttons to the FPGA, debug/status pins, a
   logic-analyzer tap, repurpose the board for other FPGA experiments) without a respin. Put the
   functional nets (config, runtime SPI, LCD) on their own labeled headers so the pinout is
   self-documenting, and route the leftover GPIO to a plain 0.1" header. Also expose **3.3 V, 1.2 V,
   and GND at multiple points** for easy probing/powering.
7. **Internal oscillator only** — the design uses `SB_HFOSC` (see `fpga/ppu_top.v`), so **no external
   crystal is required.**
8. **CRESET_B**: 10 kΩ pull-up to SPI_VCCIO1 (3.3 V) + optional reset button, so the pin has a defined
   idle state and the STM32 can still drive it (see 5d).
9. **CDONE**: external ~10 kΩ pull-up to the STM32 3.3 V rail + optional LED indicator ("configured
   OK") (see 5d).
10. Common ground with the STM32 board — obvious but non-negotiable on a breadboard; call it out on the
    header silkscreen.

Explicitly **out of scope** for this board (kept on the STM32/Nucleo or added later):
- Audio (I2S2 -> MAX98357A): STM32 pins WS=PB12, SCK=PB13, SD=PB15.
- Buttons: STM32 PC0–PC7 with internal pull-downs (see `src/buttons.c`).
- W25Q128 audio flash on SPI3.

### 4a. Board realization (physical build decisions)

These are the concrete build choices for this board (they are not derivable from any firmware — they
are design decisions made for the prototype):

- **Power input: a `3V3` header pin, source-agnostic.** The board is fed **3.3 V** through a header pin
  (jumper it from the Nucleo's 3V3, a bench supply, or anywhere convenient) alongside a `GND` pin.
  Because the input is already 3.3 V, the board needs **only one on-board regulator: a 1.2 V LDO
  derived from the 3.3 V input** (feeding VCC + VCCPLL). The 3.3 V input directly supplies VCCIO_0,
  SPI_VCCIO1, VCCIO_2, and VPP_2V5 (VPP_2V5's ≥2.30 V requirement is satisfied by 3.3 V). Section 5a
  describes the general UP5K rail scheme (both a 3.3 V and a 1.2 V rail); *this* board takes 3.3 V in
  on the header and generates only 1.2 V on-board. **Do not** add a 3.3 V regulator or a 5 V input.
  Provide the 3.3 V input cap and the 1.2 V LDO in/out caps per section 5.
- **Power sequencing: simple cascade, no sequencing hardware (accepted for this prototype).** Deriving
  1.2 V from the 3.3 V input means the 3.3 V rails necessarily come up *before* the 1.2 V core — the
  reverse of Lattice's recommended core-first order (§4.5 / section 5b). **This is accepted for this
  board:** the iCE40UP5K is the lenient family (power-up order is *recommended, not required*; POR gates
  configuration on VCC anyway), so it should configure reliably despite the inversion. No load switch,
  supervisor, or Power-Good circuitry is included — that keeps the board a ~15-part single-LDO design.
  If config ever proves unreliable on silicon, section 5b documents an optional load-switch fix to
  restore core-first order; it is deliberately **not** built into this revision.
- **Assembly: full JLCPCB turnkey (PCBA).** JLC places the QFN48, all passives, and the 1.2 V LDO.
  This means the design must produce a complete **BOM + CPL (placement) file** with JLC/LCSC part
  numbers for every component, including the iCE40UP5K, chosen from parts JLC can source. Prefer JLC
  "Basic"/preferred parts where possible to avoid extended-part fees — but note the FPGA itself is
  almost certainly an Extended part (confirm LCSC stock before committing; it is the one component that
  cannot be swapped). **Do not count on JLC placing the 0.1" through-hole headers** — generic THT
  headers are often not offered by the assembly service, so plan to hand-solder the headers yourself
  (they are the easy part). Keep the QFN land pattern and thermal-pad vias correct per section 5c
  regardless.
- **Form factor: standalone board with 0.1" (2.54 mm) header rows, jumper-wire friendly.** Model it on
  the Nucleo's Morpho headers: plain 0.1"-pitch pin headers you push jumper wires onto. It is **not** a
  DIP module that plugs into a solderless breadboard, and **not** a Nucleo shield. Every functional net
  and all spare GPIO (requirements 4–6) land on these headers; group and silkscreen-label them by
  function (CONFIG, SPI, LCD, GPIO, PWR) so the pinout is self-documenting. Expose `3V3`, `1V2`, and
  multiple `GND` pins on the power header for probing.

---

## 5. Verified hardware facts (iCE40UP5K-SG48)

Sources: **iCE40 UltraPlus Family Data Sheet FPGA-DS-02008 v1.4** and **iCE40 Programming and
Configuration FPGA-TN-02001 (TN1248) rev 3.4**, both from latticesemi.com; cross-checked against
`projects/gameboy-v2/src/fpga_loader.c`. Items still needing confirmation are listed in section 5f.

### 5a. Power rails and voltages

The SG48 has these supply pins (DS §5.1.1): **VCC** (core, ×2 pins), **VCCIO_0 / SPI_VCCIO1 /
VCCIO_2** (I/O bank 0/1/2), **VPP_2V5** (NVCM + operating), **VCCPLL** (PLL). Note: the SPI-config
supply is **not** a separate "VCC_SPI" pin — it *is* the Bank-1 I/O supply, **SPI_VCCIO1**.

| Rail | Voltage (recommended) | Notes |
|------|----------------------|-------|
| VCC (core) | **1.14–1.26 V, nom 1.2 V** (abs max 1.42 V) | Chip-killer rail. Needs a 1.2 V regulator; **never feed core from 3.3 V.** |
| VCCPLL | 1.14–1.26 V, nom 1.2 V | Tie to the 1.2 V rail through an RC / ferrite noise filter (DS Note 1). |
| VCCIO_0 / SPI_VCCIO1 / VCCIO_2 | 1.71–3.46 V per bank | All three → **3.3 V** here, so config + runtime I/O match the STM32. Unused-bank VCCIO ties to VCC. |
| VPP_2V5 | Slave-SPI: 1.714–3.46 V, but **≥2.30 V required here** | Because we use `SB_HFOSC` (and/or LED driver), DS Table 4.2 Note 4 forces VPP_2V5 ≥ 2.30 V → put it on **3.3 V**. |

**Two rails feed the FPGA: 3.3 V and 1.2 V.** 1.2 V → VCC + VCCPLL(filtered); 3.3 V →
VCCIO_0, SPI_VCCIO1, VCCIO_2, VPP_2V5. This matches both the iCEBreaker and UPduino v3 (which both
generate exactly these two FPGA rails). **On *this* board (per section 4a) the 3.3 V arrives on a
header pin, so only the 1.2 V LDO is on-board** — derived from the 3.3 V input. The general two-LDO
(5 V → 3.3 V + 1.2 V) topology described here is the reference case, not what this board builds.

Currents (Table 4.6, typical, blank pattern, 0 MHz, PLL off): static core 75 µA; startup **peak**
core 12 mA, SPI_VCCIO1 9.0 mA, VPP_2V5 2.5 mA, VCCIO 2.0 mA. Runtime dynamic current is
design-specific — use the Lattice Power Calculator for the actual PPU bitstream.

### 5b. Power sequencing (DS §4.5)

An order is recommended but **no inter-rail delay is required** — each rail need only reach ≥0.5 V
before the next: (1) VCC + VCCPLL (1.2 V), (2) SPI_VCCIO1, (3) VPP_2V5, (4) VCCIO_0/VCCIO_2 (any time
after VCC/VCCPLL ≥0.5 V). Ramp 0.6–10 V/ms, monotonic. Far more lenient than older iCE40. Only VCC,
SPI_VCCIO1, VPP_2V5 are POR-monitored, but **all** supply pins must be connected.

The recommended order is **core (1.2 V) first, then the 3.3 V I/O rails.** This board takes 3.3 V in
and derives 1.2 V from it, so the cascade brings the 3.3 V rails up **first** — the reverse of the
recommendation. **This board accepts that inversion** (section 4a): the UP5K tolerates it (order is
recommended, not required; POR waits for VCC before configuring), so no sequencing hardware is fitted.

> **Optional fix if config proves unreliable (not built in this revision).** To restore core-first
> order, gate the FPGA's 3.3 V I/O rails behind a **load switch** whose active-high enable is driven by
> a **"1.2 V-good"** signal — a voltage supervisor on the 1.2 V rail with a push-pull, HIGH-when-good
> output (so it drives the enable directly, no inverter). Feed the 1.2 V LDO straight from the 3.3 V
> input so the core still comes up first, and gate only the I/O rails. A verified JLC-sourceable parts
> set for this (TPS22918 load switch + TPS3839 supervisor + the 1.2 V LDO) is preserved in the project
> history if it is ever needed — but the current design deliberately omits it to stay minimal.

### 5c. Package / assembly

**48-pin QFN, 7×7 mm, 0.5 mm pitch** (part `iCE40UP5K-SG48I`). Pin budget: 39 user I/O (Bank 0=17,
1=14, 2=8), VCC ×2, VCCIO ×3, VCCPLL ×1, VPP_2V5 ×1, config ×2, **GND balls = 0**. **The exposed
center paddle is the ONLY ground connection and MUST be tied to GND** (DS Table 5.2 Note 1) with
thermal vias. Because of the center pad, this is **reflow / hot-air only — not reliably
iron-solderable.** Consider JLCPCB assembly for the QFN; passives and SOT-23-5 LDOs are hand-solderable.

### 5d. Config pins — pull-ups and wiring (DS §5.1.2–5.1.3)

- **CRESET_B**: input, **no internal pull-up**. Add **10 kΩ pull-up to SPI_VCCIO1** (3.3 V). STM32
  push-pull GPIO drives it; the pull-up gives a defined level before the MCU boots.
- **CDONE**: open-drain, weak internal pull-up to SPI_VCCIO1. Add an **external ~10 kΩ pull-up to the
  STM32 3.3 V I/O rail**, plus an optional status LED (~2.2 kΩ, as the dev boards use).
- **SPI_SS_B**: **has an internal pull-up → idles HIGH → the chip would default to MASTER boot.** For
  our slave-config design the STM32 must drive SPI_SS_B **LOW** when releasing CRESET_B. Do **not**
  add a pull-up (the dev boards' 10 kΩ pull-up is exactly what forces master boot); an optional 10 kΩ
  **pull-down** is fine. There is **no CBSEL pin** — boot mode is selected solely by the SPI_SS level
  at CRESET release.
- **SPI_SO** (FPGA output, IOB_32a): not used while streaming a slave bitstream; wire it to the STM32
  only if the STM32 will ever program NVCM. Otherwise may be left unconnected.
- Config pins are all in **Bank 1, referenced to SPI_VCCIO1**: SPI_SCK=IOB_34a, SPI_SO=IOB_32a,
  SPI_SI=IOB_33b, SPI_SS_B=IOB_35b.

### 5e. Config timing (DS §4.28; procedure TN1248 §13.2)

| Parameter | Value | Meaning |
|-----------|-------|---------|
| CRESET_B low pulse (min) | **200 ns** | Firmware uses ~1 µs — fine. |
| CRESET_B high → first SCK (min) | **1200 µs** | Config-memory clear. Firmware waits 1200 µs — exact. |
| Trailing clocks after CDONE high (min) | **49** (rising-to-rising) | Firmware sends 56 (7 bytes) — fine. |
| Slave SPI clock | **1 MHz min – 25 MHz max** | **Do not run config SCK below 1 MHz.** Confirm the STM32 prescaler lands in range. |
| SPI mode | **Mode 0, MSB-first** (CPOL=0, CPHA=0) | STM32 drives on falling edge, FPGA samples on rising. |

TN1248 also documents a pre-stream step the firmware omits: after the 1200 µs wait, pulse
**SPI_SS high → 8 dummy clocks → SPI_SS low**, *then* stream the image. Many designs configure fine
without it, but if config is ever flaky on real silicon, add it.

### 5f. CONFIRM BEFORE BUILD (could not be fully verified)

- **TN1252 "iCE40 Hardware Checklist" was not retrieved.** The per-rail decoupling table and the exact
  VCCPLL RC-filter R/C values here come from the open-source iCEBreaker/UPduino designs, *not* from
  TN1252. Practical starting point (from those boards): **0.1 µF per supply pin + 4.7 µF (or 10 µF)
  bulk per rail**, and a VCCPLL filter of ~100 Ω series (UPduino) or a 600 Ω ferrite bead (iCEBreaker)
  with 0.1 µF + 4.7 µF. **Fetch TN1252 to confirm before finalizing the BOM.**
- **Regulator choice:** this board needs only a **1.2 V LDO** (3.3 V in → 1.2 V out; see section 4a).
  UPduino v3 uses an AP2127K-1.2 (SOT-23-5) for this rail; pick any 1.2 V LDO with headroom for the
  ~12 mA core startup peak, 4.7 µF in/out. Since assembly is full JLC turnkey (4a), choose an LDO JLC
  stocks. (The AP2127K-3.3 the reference boards use is *not* needed here — 3.3 V comes in on a header.)
- **SG48 land pattern / thermal-via count:** consult the mechanical drawing + FPGA-TN-02044.
- **Pull-up reference supply:** this doc cites **SPI_VCCIO1** per the UP5K datasheet (the generic
  TN1248 says VCCIO_2 — wrong for this part). Harmless while all banks are 3.3 V, but do not split
  bank voltages without re-checking.

### 5g. Firmware notes surfaced by this review (`fpga_loader.c`)

- **In spec:** CRESET/SS ordering, 1200 µs wait, MSB-first mode 0, CDONE check, and 56 trailing clocks
  (> 49 min) are all correct.
- **Comment bug:** the "49 dummy clocks (7 bytes)" comment is wrong arithmetic — 7 bytes = 56 clocks.
  Behavior is fine; only the comment misleads.
- **Omitted step:** the SS-high / 8-clock / SS-low pre-stream step (5e) is skipped. Low-risk; add it if
  config proves flaky.
- **Verify config SCK rate is 1–25 MHz** — it is set elsewhere (SPI3 prescaler), not in this file.

---

## 6. Bring-up order (once the board is built)

De-risk one link at a time; do not wire everything at once.

1. **Power + grounds.** Feed 3.3 V into the header; meter the 3.3 V input and the on-board 1.2 V (core)
   rail at their final values before connecting anything else. Establish common ground with the STM32.
2. **Config path.** Run gameboy-v2 firmware; watch the UART for `FPGA: loading N bytes...` then
   `FPGA: config OK`. Confirm CDONE goes high / the CDONE LED lights. This alone proves the board's
   reason for existing.
3. **Display, no game data.** The FPGA's `lcd_driver` free-runs after config — with just the display
   wired and a background color set, you should see a solid background. Proves the parallel bus.
4. **Game-data path.** Let the STM32 upload tiles and send the title sprite over SPI1; watch the title
   appear.
5. **Audio + buttons** (on the STM32 side) last.

---

## 7. Relationship to the simulator

The simulator (`sim/`, machine `gameboy-v2`) models this exact system: the STM32 (`gameboy_v2.c`),
the gate-level iCE40 (`devices/ice40up5k/`), and the ILI9341. In sim, the config transfer is faked —
the netlist is pre-loaded via `--device fpga0=...` and only CDONE-going-high (when firmware deasserts
SS/PB6) is modeled. **This board is where the real config transfer actually gets exercised.**

Note: the simulator's gate-level FPGA is very slow (a full frame takes minutes of wall time), which is
a *simulation-speed artifact only*. On real silicon the iCE40 runs at ~24 MHz (`SB_HFOSC`), so a full
320x240 frame is on the order of tens of microseconds — frames render in real time. Any "frames are
slow / only one frame appears" behavior seen in the sim is not expected on this hardware.

---

## 8. Progression of options (context for the build-vs-buy decision)

For the record, three tiers were considered:

- **Tier 1 — flash-less UP5K breakout, no PCB work:** buy a bare/flash-less UP5K module (or a UPduino
  with its config flash disabled) and wire it to the Nucleo. Fastest way to validate the firmware path;
  the UPduino caveat is that it *also* has a config flash on the SPI bus that must be kept out of the
  way. Recommended first step.
- **Tier 2 — this board:** the minimal custom FPGA-only board described here. The clean, repeatable
  bench target.
- **Tier 3 — integrated gameboy-v2 dev board:** STM32 + iCE40 + W25Q128 + LCD connector + audio on one
  PCB. The end goal; de-risked by Tiers 1–2.

This document specifies **Tier 2**.
