# gameboy-v4 PCB — Bill of Materials (WORKING DRAFT — CORE ONLY)

**Status:** Early draft / planning. This is the **base SoC + DDR + power + clock core**
of the gameboy-v4 handheld: an **Allwinner H616** (quad Cortex-A53, Mali-G31 GPU)
running mainline Linux + Panfrost. Nothing built, nothing ordered.

**What this covers:** ONLY the "sacred core" — the parts that must be copied
net-for-net from the reference design (the SoC, its DDR3 bus, the PMIC power tree,
the crystal, and boot storage). The handheld feature parts (screen, buttons, analog
stick, audio, battery, haptics) are **TODO** and get added on top of this proven
core exactly like the [gameboy-v3 BOM](../../gameboy-v3/pcb/BOM.md) did — see
[§Feature parts](#feature-parts--todo).

**Source of these parts:** extracted from the reference design
[Kononenko-K `Allwinner_H616_Devboard`](https://github.com/Kononenko-K/Allwinner_H616_Devboard),
**`H616_DDR3` variant** (native KiCad 8, CERN-OHL-P, 4-layer, derived from Orange Pi
Zero 2/3). Manufacturing spec from that design: **4-layer, 3.5/3.5 mil trace/space,
0.35/0.15 mm vias, 80 µm prepreg** — all inside JLCPCB standard capability (no HDI).

**Sourcing legend:**
- ✅ **Proven** — carried over from a prior board that booted on silicon (`t113-breakout`/gameboy-v1/v2); LCSC # confirmed.
- ⚠️ **Verify** — named in the reference schematic (real MPN) but **LCSC #, stock, and JLC tier must be confirmed before ordering.**
- 🔲 **TODO** — value/passive present in the reference but not a specific ordering part yet, or a decision still open.

> ⚠️ **Important — the reference design carries NO LCSC part numbers.** It is an
> educational design whose passives are generic values ("C", "R", "1u", "100nF") and
> whose ICs are named by MPN only. Every LCSC # below is therefore marked **⚠️ Verify**
> or left blank — **none are fabricated.** Resolving MPN → LCSC #, stock, and JLC
> Basic/Extended tier is a required step before any order. Passive counts are
> **estimates** pending a proper netlist BOM export from KiCad.

---

## 1. Core SoC + DDR3 + Power + Clock + Storage (from the `H616_DDR3` reference)

Copy this whole block net-for-net from the reference — especially the DDR3 topology
and the AXP305 power tree. This is where a layout mistake means a dead board; the
reference already solved it.

| # | Group | Part (MPN) | Description | Qty | LCSC # | Src |
|---|-------|------------|-------------|-----|--------|-----|
| 1 | SoC | **Allwinner H616** | Quad Cortex-A53 @1.5 GHz, Mali-G31 MP2 GPU, TFBGA284 14×12 mm 0.65 mm pitch | 1 | C5365289 | ⚠️ |
| 2 | DRAM | **AS4C256M16D3** (Alliance Memory) | DDR3, 256M×16 (4 Gbit), FBGA-96 — **2× = 1 GB on a 32-bit bus (2×16-bit)** | 2 | — | ⚠️ |
| 2-alt | DRAM (alt) | **K4B8G1646D** (Samsung) | Alternate DDR3 FBGA-96 named in the reference; verify which is in-stock/JLC | (2) | — | ⚠️ |
| 3 | Power (PMIC) | **AXP305** (X-Powers) | Primary PMIC — generates the SoC core / DDR / IO rails for the H616 | 1 | — | ⚠️ |
| 4 | Clock | **24 MHz crystal** (Y1) | Main SoC reference crystal (SMD, CL per reference) | 1 | — | ⚠️ |
| 5 | Clock | 32.768 kHz crystal | RTC crystal (confirm present in reference power/RTC net) | 1 | — | 🔲 |
| 6 | ESD | **SP0502BAHT** (Littelfuse) | Low-cap TVS/ESD protection (USB/data lines) | ≥1 | — | ⚠️ |
| 7 | USB-UART | **CP2102** (Silicon Labs) | Integrated USB-UART debug console converter (on-board) | 1 | — | ⚠️ |
| 8 | Storage | microSD socket | Push-push microSD (primary boot medium for the dev loop) | 1 | — | 🔲 |
| 9 | DDR term | ZQ-cal + VTT/VREF resistors | DDR3 ZQ-calibration + termination/reference network (per reference `dram` sheet) | ~several | — | 🔲 |
| 10 | Decoupling | 100 nF 0402 X7R | Per-power-pin decoupling (SoC + DDR + PMIC) | ~many | — | 🔲 |
| 11 | Decoupling | 1 µF 0402 | DDR/rail decoupling (seen throughout the `dram` sheet) | ~many | — | 🔲 |
| 12 | Bulk | 10 µF / 22 µF | Bulk caps on the PMIC rails + VBUS input | ~several | — | 🔲 |
| 13 | Power inductors | 2520 power inductors | Buck inductors for the AXP305 rails (STEP model `Inductor_2520` in reference) | ~several | — | 🔲 |

> **Passive quantities are placeholders.** The real counts + exact values come from a
> KiCad **BOM export** of the reference `H616_DDR3` project (`Tools → Generate BOM`),
> not from this hand-read. Do that export before ordering.

> **DDR3 vs LPDDR4 choice:** we use the **DDR3 variant** (this table) — DDR3 is the
> well-stocked LCSC memory and the simpler bring-up. The reference's LPDDR4 variant
> (4 GB, AXP313A PMIC) is the higher-capacity option but LPDDR4 discrete sourcing is
> thin at JLC/LCSC. For a kart game, 1 GB is ample (RAM was never the 3D blocker).

---

## 2. Boot / recovery (software-side facts, no BOM parts)

- **Recovery:** `xfel` FEL over USB (H616 chip id `0x00182300`) — FEL + DRAM-over-USB
  works; **note `xfel` does NOT flash NOR/NAND on H616**, so plan boot-from-SD for the
  dev loop (matches the reference).
- **Boot chain (mainline):** BROM → U-Boot SPL (open sunxi DDR3 init) → TF-A BL31
  (`sun50i_h616`) → U-Boot → kernel. Reference U-Boot target: `orangepi_zero2_defconfig`;
  kernel DTB `sun50i-h616-orangepi-zero2.dtb` (starting point for our board DT).

---

## Feature parts — TODO

Everything that makes it a *handheld* is deferred and gets added on top of the core
above, in the style of the [gameboy-v3 BOM](../../gameboy-v3/pcb/BOM.md):

- 🔲 **Screen** — panel + interface (⚠️ H616 mainline display path — HDMI works
  upstream; MIPI-DSI/RGB DT support is thinner — **confirm before choosing a panel**).
- 🔲 **Buttons / D-pad** — GPIO (carried over from v1/v2).
- 🔲 **Analog stick** — 2-axis via external I²C ADC (ADS1015-class, mainline IIO).
- 🔲 **Audio** — I²S Class-D amp (**MAX98357A** carries over from v1/v2) + speaker.
- 🔲 **Battery** — 1S LiPo + charge/power-path (re-budget for the H616's higher draw).
- 🔲 **USB-C** — power in + boot/recovery data.

---

## Next steps before this BOM is orderable

1. **Open the reference in KiCad 8+** (`Hardware/H616_DDR3/H616_devboard.kicad_pro`)
   and run **Generate BOM** → get the real passive counts + values.
2. **Resolve every ⚠️/🔲 to a concrete LCSC #** with live stock + JLC tier.
3. **Reproduce the reference board unchanged first** (build-plan step 1) to prove the
   fab/assembly/DDR pipeline before adding any handheld feature.
