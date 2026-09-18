# gameboy-v4 — STM32MP157 3D-capable handheld (RESEARCH / PLANNING)

**Status:** 🔬 Research + planning. **Nothing built, nothing on silicon, no board ordered.**
This directory holds the hardware planning for a v4 that adds **real 3D graphics**
(a Mario-Kart-64-class game) on top of the v3 Linux handheld.

**Selected SoC: STM32MP157AAC3** (LCSC C511448). See
[`REQUIREMENTS.md`](REQUIREMENTS.md) for the requirements and [`pcb/BOM.md`](pcb/BOM.md)
for the parts.

## Where this sits in the series

| | Compute | Graphics | OS |
|---|---|---|---|
| v1 | STM32F411 (Cortex-M4) | software 2D | own RTOS, bare-metal |
| v2 | STM32F411 + iCE40 FPGA | hardware sprite PPU (CPU + coprocessor split) | own RTOS |
| v3 | Allwinner T113-S3 (dual A7, ARMv7, 128 MB **SiP** DDR3) | CPU/framebuffer, **no GPU** | mainline Linux **+** full from-scratch stack |
| **v4** | **ST STM32MP157AAC3** (dual Cortex-A7 @650 MHz + **Cortex-M4**, external 16-bit DDR3) | **Vivante GC Nano + open Etnaviv (GLES 2.0)** | mainline Linux (GPU) + from-scratch stack (ARMv7, ports from v3) |

Nice continuity: the Cortex-M4 in the STM32MP157 is the *same core family as v1* and
revives v2's CPU+coprocessor split — v4 closes the v1→v4 loop in one chip.

## Why STM32MP157 (the short version)

The chip search ran H616 → H700 → RK3288 → **STM32MP157** (full history in project
memory). The decisive facts:

- **It drives the gb3 RGB666 panel on mainline — turnkey.** The STM32MP157's **LTDC**
  natively outputs parallel RGB666 (`st,stm32-ltdc` + `panel-dpi`, RGB666 is the
  predefined `ltdc_pins_f` pinmux, **proven in-tree on `stm32mp135f-dk`**). This is the
  capability the **H616 and R40 lacked** (no mainline parallel-RGB) — and it's the single
  biggest reason those were rejected. **We reuse the v3 5" 800×480 panel as-is.**
- **DDR3, the simplest way.** The chosen TFBGA-361 package runs a **16-bit DDR3** bus
  (one x16 chip) — the simplest external-DRAM layout — and ST publishes the **DK2/EV1
  board design files** as a clone-able reference (plus a DDR tuning tool).
- **JLCPCB-buildable.** LCSC **C511448**, TFBGA-361 / 0.5 mm → JLC-standard fab (no HDI),
  JLC-assembled. ~$7, in stock.
- **Real 3D + best learning API.** GC Nano / **OpenGL ES 2.0** on the open **Etnaviv**
  driver = N64/PS1-class textured 3D (Mario-Kart-64 tier), and GLES 2.0 is the ideal
  foundation (~85–90% of what you learn transfers to GLES 3.x / Vulkan).
- **Most open vendor + ARMv7.** Mainline TF-A/U-Boot/OP-TEE/Linux + Etnaviv (no blob),
  world-class no-NDA docs, USB-DFU recovery. ARMv7-A → the from-scratch
  bootloader/kernel/libc port from v3 with minimal ISA churn.

**The accepted tradeoff:** the GC Nano is the *weakest* GPU of the candidates — N64-class,
**not** modern-shader 3D. That's fine for a Mario-Kart-64-style game (the target). If v4
ever needed modern GLES3 3D, that's the RK3288, at the cost of a much harder board
(dual-channel DDR3, no reference). See [`REQUIREMENTS.md`](REQUIREMENTS.md) → Accepted
tradeoffs.

## Reference design (the starting point)

- **ST STM32MP157F-DK2** (board MB1272) and **STM32MP157F-EV1** (MB1262). ST publishes
  full **schematics + layout project + gerbers + BOM** for both — a genuine clone-able
  reference for the **16-bit DDR3L layout + STPMIC1 power tree** (the hard core).
- The **DK2** (~$90) is the development board (see build plan). The **EV1** additionally
  breaks out a **CN11 RGB/LTDC connector** — the only ST board you can wire an external
  parallel-RGB panel to (the DK2 is DSI/HDMI only).

## Build plan (de-risking order)

1. **Order a DK2 now** → prototype mainline Linux boot + the `forge` board port on real silicon.
2. **GLES2 renderer on desktop → then the DK2** (its bundled DSI screen) — learn graphics, chip-agnostic, zero board risk.
3. **Clone ST's DDR3 + STPMIC1 core** onto the v4 board → prove fab/assembly/DDR bring-up.
4. **Graft handheld peripherals** (the gb3 RGB666 panel via LTDC, buttons, analog stick, audio, battery) onto the proven core.
5. **Etnaviv/GLES2 + our renderer on the v4 board** → first 3D on the real panel.
6. **From-scratch stack port** (ARMv7 — reuses most of v3) as a parallel OS-understanding track.

## Contents

- [`REQUIREMENTS.md`](REQUIREMENTS.md) — the v4 requirements + how the STM32MP157AAC3 meets each.
- [`pcb/BOM.md`](pcb/BOM.md) — the BOM (SoC/DDR/power core + reused gb3 peripherals).
- [`pcb/DDR3-ROUTING-STUDY.md`](pcb/DDR3-ROUTING-STUDY.md) — DDR3 routing fundamentals
  (⚠️ currently written against the old H616 reference; the *principles* hold, but the
  specific numbers must be redone from ST's DK2/EV1 16-bit DDR3L layout).
