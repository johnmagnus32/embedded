# DDR3 Routing — Study Sheet (extracted from the H616 reference layout)

Ground-truth constraints pulled directly from the **Kononenko-K `Allwinner_H616_Devboard`,
`H616_DDR3` variant** KiCad project — the known-good, boots-Linux reference we're copying.
This is "what a working H616 DDR3 layout actually specifies," so you can (a) understand
*why* each number is there and (b) match it on our board. Everything below is read from
the reference's `.kicad_pro`, `.kicad_dru`, `.kicad_pcb`, and `dram.kicad_sch`.

> **The #1 rule:** on the first board, **copy these values, don't invent new ones.** DDR
> succeeds mechanically — match the stackup, placement, ground plane, and the skew/width
> numbers below, and DDR3 training will pass. Cleverness is not required; fidelity is.

---

## 1. Stackup (4-layer, ~1.0 mm — the impedance foundation)

| Layer | Type | Thickness | Material |
|-------|------|-----------|----------|
| **F.Cu** | signal (DDR routes here) | 0.018 mm (½ oz) | copper |
| dielectric 1 | **prepreg** | **0.08 mm** | FR4, εr 4.5 |
| **In1.Cu** | **GROUND plane (DDR reference)** | 0.018 mm | copper |
| dielectric 2 | core | 0.30 mm | FR4, εr 4.5 |
| **In2.Cu** | power/plane | 0.018 mm | copper |
| dielectric 3 | prepreg | 0.08 mm | FR4, εr 4.5 |
| **B.Cu** | signal | 0.018 mm | copper |

**Why it's built this way — the single most important SI fact:** the DDR signals on **F.Cu**
sit only **0.08 mm above a solid ground plane (In1.Cu)**. That tight, close, *unbroken*
ground reference is what controls impedance and gives return current a clean path. **This is
the same principle as the t113-breakout's 90 Ω USB2 pair over In1 GND — DDR is that lesson
scaled up to ~90 signals.** Match this stackup to JLCPCB's controlled-impedance 4-layer
stackup (or the closest, then re-tune widths with their calculator).

> **The rule you must not break:** keep In1.Cu a *continuous, unbroken* ground plane under the
> ENTIRE DDR bus. No splits, no plane gaps, no crossing a void. A break under DDR traces is the
> most common first-board DDR failure.

---

## 2. Net classes — trace widths (from `net_settings`)

| Net class | What it carries | Track width | Notes |
|-----------|-----------------|-------------|-------|
| `DDR3_ADDR` | address bus (`SA*`) | **0.0975 mm** (3.84 mil) | single-ended |
| `DDR3_DATA_LB` | data low byte (`DQ0–7`) | 0.0975 mm | single-ended |
| `DDR3_DATA_HB` | data high byte (`DQ8–15`) | 0.0975 mm | single-ended |
| `DDR3_OTHER` | command/control | 0.0975 mm | see net list below |
| `DDR3_DIFF` | **clock + strobes** | diff pair **0.093 mm width / 0.20 mm gap** | differential |
| USB | USB2 D+/D− | diff 0.08 / 0.08 | (reference only) |

Single-ended DDR = **0.0975 mm (~3.84 mil)** everywhere; the clock/strobe **differential**
pairs are 0.093 mm wide / 0.20 mm gap. These widths are what produce the target impedance
*on this stackup* — if you change the stackup, you must re-solve the widths.

---

## 3. Length / skew matching — the tolerances (from `.kicad_dru`)

These are KiCad **skew** constraints (max length difference *within* a group). This is the
part that creates the "squiggle" meanders on DDR boards.

| Rule | Applies to | Max skew | = mil |
|------|-----------|----------|-------|
| `L_BYTE_LANE_LEN` | low byte lane (`DDR3_DATA_LB`) | **2.54 mm** | 100 mil |
| `H_BYTE_LANE_LEN` | high byte lane (`DDR3_DATA_HB`) | **2.54 mm** | 100 mil |
| `ADDR_LEN` | address bus (`DDR3_ADDR`) | **6.35 mm** | 250 mil |
| `INTER_CLASS_3` | data LB ↔ data HB | 3.175 mm | 125 mil |
| `INTER_CLASS_1/2` | addr ↔ each data byte | 4.445 mm | 175 mil |

**How to read this (the hierarchy that matters):**
- **Tightest = within each data byte lane (±2.54 mm):** each `DQ0–7` group matched to its own
  strobe `DQS`, and `DQ8–15` to its strobe. Data + its strobe travel together, so they must
  arrive together. This is where you spend the most tuning effort.
- **Loosest = the address/command bus (±6.35 mm):** shared fly-by bus, clocked slower, so it
  tolerates more skew. Match it to the clock.
- **Inter-class** rules keep the byte lanes roughly aligned to each other and to address.

Use KiCad's **"Tune length of a single track"** and **"Tune differential pair skew"** router
tools; set these numbers as net-class length/skew rules so the DRC enforces them for you.

---

## 4. Net → class mapping (from `netclass_patterns` — the DDR3 signal groups)

| Class | Net pattern | Meaning |
|-------|-------------|---------|
| `DDR3_ADDR` | `/DRAM/SA*` | address lines A0..An |
| `DDR3_DATA_LB` | `/DRAM/DQ[0-7]` | data byte 0 |
| `DDR3_DATA_HB` | `/DRAM/DQ[8-9]`, `/DRAM/DQ1x` | data byte 1 |
| `DDR3_DIFF` | `/DRAM/SCK?` (clock), `/DRAM/SDQS*` (strobes) | differential clock + DQS |
| `DDR3_OTHER` | `SBA*` (bank), `SCKE*` (clk-enable), `SRAS/SCAS/SWE` (command), `SCS*` (chip-sel), `SODT*` (on-die-term ctrl), `SDQM*` (data mask) | command/control |

**Topology this implies (study it in the reference PCB):**
- **Data (`DQ`/`DQS`/`DQM`) = point-to-point** — each byte lane routes SoC → its own DRAM chip.
  (This board has **2× DDR3 chips**, each ×16-ish, forming the 32-bit bus as two 16-bit halves.)
- **Address/command/clock = shared "fly-by"** — daisy-chained past both DRAM chips.
- That split is *exactly why* the skew budgets differ: point-to-point data is tight, shared
  fly-by address is loose.

---

## 5. Power / reference network (from `dram.kicad_sch`)

The support network that makes DDR3 work — small, but every part has a reason:

- **VDDQ** — the DDR3 supply rail (from the **AXP305 PMIC**). DDR3 = 1.5 V, DDR3L = 1.35 V;
  confirm which against the AXP305 config + DRAM chip.
- **+1V8** — the SoC's DRAM-controller / IO supply (heavily decoupled: many `1u` + `100n`).
- **VREF (VREFDQ / VREFCA)** — generated by a **2 kΩ / 2 kΩ divider = VDDQ/2**, then filtered
  (`1u` + `100n`). This is the reference the DRAM compares signals against — keep it clean and
  routed as a quiet, star-fed net, not a noisy trace near switching.
- **ZQ calibration** — **240 Ω to GND** (the DDR3 `RZQ` standard value) sets the DRAM's
  on-die drive/termination impedance. One per chip (the 3× 240R seen = ZQ + related).
- **Termination:** the reference relies on the DRAM/SoC **on-die termination (ODT** — note the
  `SODT*` control nets) rather than an external VTT termination rail. That's a valid
  simplification for short, 2-chip layouts — **and it makes your first board easier** (no VTT
  regulator to design). Keep the fly-by address short so ODT is sufficient.
- **Decoupling:** lots of `1u` (×11) + `100n` caps distributed on VDDQ/+1V8 right at the pins.
  Place these tight to the DRAM and SoC power balls — copy their placement.

---

## 6. Your learning workflow with this sheet

1. **Read §1 + §5 first** — understand the stackup (why ground is 0.08 mm below signal) and
   the VREF/ZQ/ODT network. Pair with Rick Hartley's return-current talk for the *why*.
2. **Open the reference PCB** (`H616_devboard.kicad_pcb`) and *find each thing above in the
   actual layout*: the ground plane under DDR, the meandered byte lanes, the fly-by address
   daisy-chain, the ZQ/VREF resistors near the chips. Watching a Robert Feranec DDR routing
   video alongside this makes it click.
3. **Reproduce the board unchanged** (build-plan step A1) — re-derive these constraints as you
   go; don't just copy blindly, but don't deviate either.
4. **Set these as KiCad net-class rules** on our board so DRC enforces the skew/width budgets.
5. **Your software safety net:** after fab, the U-Boot/custom DRAM init runs training and
   reports bus health — you can read/log/tune the init (your home turf) far more than a
   PCB-only person can. A marginal layout can often be tuned in the init, not respun.

---

### Quick-reference card (copy these onto our board)

```
Stackup:     4-layer, signal(F.Cu) / 0.08mm prepreg / GND(In1) / 0.30mm core / PWR(In2) / 0.08mm / B.Cu, FR4 er4.5
SE width:    0.0975 mm (3.84 mil)  — all single-ended DDR
Diff pair:   0.093 mm width / 0.20 mm gap  — clock + DQS
Skew:        byte lane ±2.54mm (100mil) | address ±6.35mm (250mil) | LB↔HB ±3.175mm | addr↔data ±4.445mm
Topology:    data = point-to-point per byte;  addr/cmd/clk = shared fly-by (2 DRAM chips)
Ground:      In1.Cu = solid, UNBROKEN under the entire DDR bus  ← the rule you cannot break
VREF:        2k/2k divider = VDDQ/2, filtered   |   ZQ: 240Ω to GND   |   Termination: on-die (ODT), no external VTT
```

*Source: Kononenko-K/Allwinner_H616_Devboard, H616_DDR3 variant (CERN-OHL-P). All values read
from the KiCad project files; verify against the live project + JLCPCB stackup before ordering.*
