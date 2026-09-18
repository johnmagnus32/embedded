# RK3568 DDR3 Routing — Study Sheet

Ground-truth constraints for a **custom RK3568 board with a single 32-bit DDR3 channel**, pulled
from **Rockchip's own reference documents** — the numbers you'd set as KiCad net-class + length
rules. This is the "self-route it carefully" path from the [chip comparison](CHIP-COMPARISON.md):
RK3568 is 0.65 mm no-HDI, DDR3-capable, and Rockchip publishes an **editable Allegro DDR3-1066
core-board template** to clone the topology from.

> **The #1 rule (same as the H616 sheet):** on a first board, **copy Rockchip's values, don't
> invent them.** Match the stackup, impedance, topology, and the length/skew budgets below and DDR3
> training passes. Run **DDR3-1066** (not 2133) for margin. Fidelity beats cleverness.

**Sources (all Rockchip primary, verbatim):**
- **RK3568 High-Speed PCB Design Guide V1.0** (2021-04-12) — DDR impedance, length/skew tables, general rules.
- **RK3568 Hardware Design Guide V1.2** (2022-01-26) — stackup (§3.1), DDR power/VREF/ZQ/topology (§2.1.7).
- **RK3568 Datasheet V1.2** — controller max rate.
- **RK3568 EVB reference kit** (mirrored on GitHub) — editable Allegro `.brd` + OrCAD `.DSN` + PADS DDR core-board templates.

> ⚠️ **Legal-gray reference:** the mirrored EVB kit carries **no license** and the docs are marked
> "Rockchip Confidential / Copyright." Read it for the numbers; **confirm reuse rights** (Rockchip or
> an official partner) before shipping a derivative layout.

---

## 0. Template reality check (is there really a cloneable DDR3 island?)

**Yes — genuine editable CAD, and a DDR3 variant.** The EVB kit ships native `*.brd` (Allegro),
`*.DSN` (OrCAD), `*.pcb`/`*.asc` (PADS) per DDR type — not PDFs. The DDR3 one is
`RK3568_Template_DDR3P416DD6_44X41_1066MHz` (V1.0, 2021-01-29). Name decode: **DDR3**, RK3568
package, **D**ual-**d**ie/2-rank, **6**-layer; **44×41 mm** board; **1066 Mbps**. (Full family also
present: DDR3+ECC, DDR4-1600, LPDDR3-1066, LPDDR4X-1600.) This is the freeze-paste-able island —
in **Allegro/OrCAD/PADS format** (you'd re-draw in KiCad following its rules, not import directly).

---

## 1. Stackup — 6-layer, ~1.6 mm (Rockchip's recommendation)

Built for **50 Ω single-ended / 100 Ω differential**, Dk ≈ 4.2 @ 1 GHz, 1 oz copper all layers.

| Layer | Role | Below-it dielectric | µm | mil |
|-------|------|---------------------|----|-----|
| **L1** | **TOP — signal (route DDR here)** | PP 1080 (RC64%) | 80 | **3.15** |
| **L2** | **GND (DDR reference)** | PP 2116 (RC50%) | 102 | 4.02 |
| **L3** | POWER | core | 1008 | 39.69 |
| **L4** | inner signal | PP 2116 (RC50%) | 102 | 4.02 |
| **L5** | **GND (DDR reference)** | PP 1080 (RC64%) | 80 | 3.15 |
| **L6** | **BOTTOM — signal (route DDR here)** | — | 35 | 1.38 |

**The key SI fact:** L1 references **L2 GND** just **80 µm / 3.15 mil** away (and L6 references L5
GND). Route DDR data/strobe on **L1 or L6** over that solid, close, *unbroken* ground. Rule Rockchip
states: *"the layer neighbouring the chip must be ground; keep every signal layer adjacent to a
ground layer; never run two adjacent signal layers."* A 4-layer alt exists (TOP/GND/POWER/BOTTOM)
but **6-layer is the recommendation** — plan for 6.

---

## 2. Impedance targets (DDR3 tables — identical in both guides)

- **Single-ended** (DQ / DM / address / command / control): **50 Ω ±10%**
- **Differential** (DQS pairs, CK): **100 Ω ±10%**

**Trace widths are NOT tabulated** — Rockchip gives impedance + Dk + stackup and lets your fab's
field solver set the width. On this 6-layer/Dk-4.2 stack, 50 Ω outer ≈ **4–4.5 mil**. The only
literal width printed is **BGA fan-out = 4 mil** (outer two ball rings). Re-solve widths against
**JLCPCB's actual 6-layer controlled-impedance stackup** before routing.

---

## 3. Length / skew budgets — the core of the sheet

Set these as KiCad length-tuning / net-class rules. **DDR3 write-leveling + per-lane training make
most of these loose** — the tight ones are the intra-pair diffs and the un-trained control lines.

| Constraint | Rockchip spec | mm | Notes |
|---|---|---|---|
| **DQ ↔ DQS (within a byte lane)** | ≤ **600 mil** | ≤ 15.24 | *not* tightly matched — "keep DQ short"; leveling absorbs it |
| **DM ↔ DQS (within a byte)** | ≤ 600 mil | ≤ 15.24 | same |
| **DQS_P ↔ DQS_N (intra-pair)** | < **12 mil** | < 0.305 | **tight** |
| **CK_P ↔ CK_N (intra-pair)** | < **12 mil** | < 0.305 | **tight** |
| **DQS ↔ CK** | < **1500 mil** | < 38.1 | the only loose cross-lane data budget |
| **CSn / CKE / ODT ↔ CK** | < **30 mil** | < 0.762 | **tightest single-ended** — these aren't trained |
| **Other address/command ↔ CK** | < **600 mil** | < 15.24 | loose — CA is trained |
| **T-branch arm balance (L2a ↔ L2b)** | ≤ **20 mil** | ≤ 0.508 | the two stub arms of a T |
| **T stub length (CA/CTRL/CLK)** | ≤ **600 mil** | ≤ 15.24 | "as short as possible" |

**Spacing** (as ×trace-width): DQ↔DQ ≥2×; byte↔byte ≥2×; **DQ↔DQS ≥3×** (min 2×); **CK↔all ≥3×**
(min 2×); CA↔CA ≥2×. On surface layers: guard **DQS with GND + GND vias ≤200 mil apart**; route
DQ/DM as **G-S-S-G** with GND vias ≤200 mil apart.

> **Two numbers Rockchip does NOT give (don't invent them):** there is **no byte-lane-to-byte-lane
> skew match** (byte↔byte is a *spacing* rule only — write-leveling handles arrival), and **no
> absolute DDR3 net-length ceiling** (only "as short as possible" + the ≤600 mil stubs). The
> "<6 inch" caps in the guide are for USB/PCIe/HDMI, not DDR3.

---

## 4. Topology (§2.1.7.4)

Single **32-bit** channel. **DDR3 x16 is the widest JEDEC part, so 32-bit = 2 chips minimum.**
- **Simplest hobbyist build:** **2× x16, single rank (1 CS)** — DQ point-to-point per chip.
- Rockchip's reference: 4× x16 / 2-rank → **DQ = T-topology "one-drive-two"**, **CA/CMD = double-T
  "one-drive-four."** Keep both T-arms short and equal (≤20 mil).
- **Termination:** non-ECC T-topology uses an **RC at the branch point** to shape the clock — **no
  VTT rail**; **ODT** does the work. (A VTT rail + 39/43 Ω series appears *only* in the fly-by/ECC
  high-fan-out template — you don't need it.) Optional 2 pF reserved across CK_P/CK_N near the SoC.

---

## 5. Power / reference network (§2.1.7.2-3, .5)

- **VDDQ:** DDR3 = **1.5 V**; **DDR3L = 1.35 V** (set via the RK809 PMIC BUCK or a discrete DCDC).
- **VREF:** the SoC pin **DDR_VREFOUT** drives **VREFDQ** to the DRAM = **0.75 V (DDR3) / 0.675 V
  (DDR3L)**. **VREFCA is made externally = VDDQ/2 by two 1 kΩ 1% resistors.** Every VREF pin needs a
  **1 nF** decoupling cap (mandatory).
- **ZQ:** DRAM ZQ pin → **240 Ω 1% to GND**; SoC PHY pin **DDR_RZQ → 120 Ω 1% to GND**.
- **RESET#:** reserve a **1 nF** cap (ESD).
- **Vias:** 1 via per DRAM power/GND pad; 1 via per cap pad; **≥6 vias** at the VCC_DDR DCDC output;
  **≥12 vias** on the SoC VCC_DDR ball field.

---

## 6. Reference-plane / ground rules

- Keep the reference plane **continuous under the entire DDR bus** — no slits/splits in the return
  path. *(This is the "keep In1.Cu unbroken" rule from the H616 sheet, scaled to ~71 nets.)*
- Layer change staying over GND → **stitching GND via within 25 mil** of the signal via.
- Layer change that swaps GND↔POWER reference → **100 nF stitching cap**, one per 3-4 signal vias.
- Route **≥12 mil from any plane edge**, **≥6 mil from via holes/pads**, **≥3× width from a GND pour**.
- Remove unused via pads; any via stub **>12 mil → simulate**.

---

## 7. Recommended speed + net count

- **Controller ceiling:** DDR3-2133. **Target DDR3-1066** on a first board — it's the speed of the
  provided template and buys large timing margin (difficulty climbs steeply toward 3200).
- **~71 nets** for a single-rank 32-bit channel: 32 DQ + 8 DQS (4 diff) + 4 DM + ~15 addr + 3 bank +
  3 cmd + 4 control + 2 CK. (Split ≈ 44 data / 25 addr-cmd-ctrl / 2 clock.) Add 3 per extra rank.

---

### KiCad net-class starter (from §2–§3)

```
DDR3_DQ   (DQ, DM):        50Ω SE ±10% | DQ-DQ ≥2×w, DQ-DQS ≥3×w | match to DQS within lane ≤600 mil (soft)
DDR3_DQS  (diff):         100Ω ±10%   | intra-pair ≤12 mil | GND-guarded on surface layers
DDR3_CK   (diff):         100Ω ±10%   | intra-pair ≤12 mil | ≥3×w to all | DQS↔CK ≤1500 mil
DDR3_CTRL (CS#/CKE/ODT):   50Ω SE ±10% | ↔CK ≤30 mil   ← tightest single-ended
DDR3_CA   (addr/BA/RAS/CAS/WE): 50Ω SE ±10% | ↔CK ≤600 mil | T-arms L2a↔L2b ≤20 mil, stubs ≤600 mil
Stackup: 6-layer, DDR on L1/L6 over L2/L5 GND (80µm/3.15mil), 50Ω SE / 100Ω diff, Dk≈4.2
Power:   VDDQ 1.5V (DDR3)/1.35V (DDR3L) | VREFCA = 1k/1k = VDDQ/2, 1nF each | ZQ 240Ω, RZQ 120Ω | no VTT (ODT)
Speed:   DDR3-1066, single rank, 2× x16
```

---

## 8. How this compares to the ST DK2 / H616 sheets

- **Harder than the STM32MP157** (single **16-bit** chip, clone ST's *editable, validated gerbers*):
  RK3568 is **32-bit = 2 chips + T-topology**, ~71 nets, **6 layers**, and the reference is
  Allegro/PADS (re-draw in KiCad, not import).
- **Easier than RK3588** (LPDDR4X, 200+ nets, HDI): RK3568 is **through-hole no-HDI**, DDR3 (slower,
  forgiving), and has an actual DDR3 template to follow.
- **Your software safety net still applies:** Rockchip's DDR init runs training and reports margins —
  a marginal layout can often be diagnosed/tuned rather than blindly respun. *(Note: RK3568 DDR init
  uses Rockchip's `ddr.bin` blob — an ally here, but a small dent in the no-blob ethos.)*

*Verify every number against the live Rockchip PDFs + JLCPCB's actual 6-layer impedance stackup
before ordering; confirm reference-reuse licensing.*
