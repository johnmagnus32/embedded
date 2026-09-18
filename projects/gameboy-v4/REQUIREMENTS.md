# gameboy-v4 — Requirements

The hard requirements driving the v4 SoC + board selection, and how the selected
chip meets each. These emerged from the v4 chip-selection research (see the
assistant's project memory for the full decision history: H616 → H700 → RK3288 →
**STM32MP157**, and why each earlier candidate was ruled out).

**Selected SoC: STMicroelectronics STM32MP157AAC3** (LCSC **C511448**).
Decode: `A` = 650 MHz, no crypto/secure-boot · `A` = family marker ·
`C` = **TFBGA-361** package (12×12 mm, 0.5 mm pitch, **16-bit DDR3**) · `3` = −40…+125 °C.
Dual Cortex-A7 @650 MHz + Cortex-M4 coprocessor + Vivante GC Nano GPU (GLES 2.0),
~$7, in stock. See [`pcb/BOM.md`](pcb/BOM.md).

**Status legend:** ✅ met · ⚠️ met with a caveat/tradeoff · 🔲 verify before commit.

---

## Requirements

### R1 — DDR3 memory (not DDR4 / LPDDR4)  ✅
**Why:** JLCPCB must be able to source + assemble it, and DDR3 is the simplest external
memory bus for a first external-DRAM board (DDR3 chips are well-stocked at LCSC;
LPDDR4-era parts tend to force finer-pitch/HDI boards).
**How met:** The STM32MP157 supports DDR3/DDR3L. The chosen **TFBGA-361 package runs a
16-bit DDR3 bus** — the *simplest* width (one x16 DDR3L chip, ~half the nets of a 32-bit
bus). This is exactly the config on ST's own reference board (DK2 = 4-Gbit DDR3L, 16-bit,
533 MHz), so there's a proven layout to follow.
**Note:** external DDR3 routing is still a genuinely new skill vs the v3 T113 (which had
in-package SiP DRAM = zero DDR routing). Mitigated by R2's reference design.

### R2 — Development board for parallel software work  ✅ (strongest fit of any candidate)
**Why:** prototype the software (mainline Linux boot, `forge` board port, GPU/GLES2,
input/audio) on real silicon *while* the custom board is designed and fabbed.
**How met:** **STM32MP157F-DK2** Discovery Kit (~$90) — dual A7 + M4, DDR3L, a bundled
4" DSI touchscreen, HDMI, WiFi/BT, Gigabit Ethernet, USB-C, microSD, **on-board ST-LINK
debugger**; runs mainline Linux / OpenSTLinux out of the box. **Also: ST publishes the
full board design files** (schematics + layout project + gerbers + BOM) for the DK2 and
EV1 — a genuine clone-able reference, which the other candidates lacked.
**Caveat (⚠️):** the DK2 exposes display over DSI+HDMI only — it **cannot** be wired to
the gb3 *parallel-RGB* panel. To test the RGB666 panel on a dev board you need the
**STM32MP157F-EV1** (has a CN11 RGB/LTDC connector, ~$400), or validate the panel
directly on the custom v4 board. Plan: DK2 for software/GPU bring-up; RGB panel proven
on the v4 board (or EV1).

### R3 — Compatible with the gb3 screen (5" 800×480 RGB666 parallel + touch)  ✅ (on mainline, turnkey)
**Why:** reuse the [gameboy-v3](../gameboy-v3/pcb/BOM.md) panel (5" 800×480 IPS, RGB666
parallel/DPI) and its touch — no new panel selection, no mechanical change.
**How met:** The STM32MP157's **LTDC** controller natively drives parallel RGB
(RGB565/RGB666/RGB888) and is **turnkey on mainline** — driver `st,stm32-ltdc` +
`panel-dpi`/`panel-simple`, with RGB666 a *predefined pinmux group* (`ltdc_pins_f`).
**Proven in-tree**: `stm32mp135f-dk.dts` drives a Rocktech RGB *parallel* panel via this
exact path (MP135 shares the LTDC IP with MP157). Pixel clock 800×480@60 ≈ 30 MHz vs
LTDC's ~90 MHz ceiling. This is the capability the H616 and R40 *lacked* (no mainline
parallel-RGB path) — a decisive reason the STM32MP157 won.
**Caveat (⚠️):** the gb3 panel's *silicon-validated* touch is a **FocalTech FT7311**
(I²C @0x38), not the Goodix GT911 that was a BOM candidate — use the FocalTech mainline
driver. Either way, touch is device-tree-only work.

### R4 — GPU: enough for basic 3D + good for learning GPU programming  ⚠️ (yes, at N64-class)
**Why:** the headline v4 feature is real 3D (a Mario-Kart-esque game), and a goal is to
*learn* GPU programming with reusable fundamentals.
**How met:** Vivante **GC Nano** GPU, **OpenGL ES 2.0**, on the open mainline **Etnaviv**
driver (blob-free). Enough for **N64 / PS1-class textured, Z-buffered 3D** (Mario Kart 64
tier). GLES 2.0 is the *ideal* learning API — the minimal-but-complete programmable
pipeline; ~85–90% of what you learn (shaders/GLSL, buffers, textures, the MVP transform
chain, depth/blend, EGL+GBM/KMS) transfers directly to GLES 3.x / Vulkan / desktop GL.
**Accepted tradeoff (⚠️):** the GC Nano is the *weakest* GPU of the candidates
(marginally N64-class; below Mali-400). It does **not** reach modern-shader ("Tier-3")
3D — no MRT/compute/instancing, no dynamic-lighting/post-fx-heavy looks. If v4 ever needs
modern 3D, that's the RK3288 (GLES 3.1) at the cost of a much harder board. **Decision:
accepted — N64-class is the target.**

### R5 — Supports all gb3 peripherals  ✅
**Why:** carry over the gb3 feature set without re-selecting interfaces.
**How met:** the STM32MP157 is a peripheral-rich HMI/industrial MPU — it has every bus
the gb3 board uses, with headroom:

| gb3 peripheral | Interface | STM32MP157 provides |
|---|---|---|
| Screen (RGB666 parallel) | DPI | ✅ LTDC (R3) |
| Touch (FT7311) | I²C | ✅ multiple I²C |
| IMU (LSM6DSOX) | I²C | ✅ I²C |
| Haptics (DRV2605L + LRA) | I²C | ✅ I²C |
| Button expander (PCA9555) | I²C | ✅ I²C |
| Audio amp (MAX98357A-class) | I²S / SAI | ✅ SAI (I²S) |
| Analog stick (2-axis) | ADC | ✅ on-chip ADC |
| Boot NOR (W25Q128) | (Q)SPI | ✅ QSPI (a boot source) |
| microSD / eMMC | SDMMC | ✅ SDMMC |
| Bluetooth module | UART / SDIO | ✅ UART + SDIO |
| USB-C | USB OTG | ✅ USB OTG |

### R6 — Runs Linux (mainline + open-source drivers, no vendor blob)  ✅
**Why:** the project's core value — mainline Linux on an open stack (this is what
disqualified the H616 for display, and what the STM32MP157 wins on). Also: ARMv7-A
(Cortex-A7), so the from-scratch bootloader/kernel/libc stack ports from v3 with minimal
ISA churn (a bonus, not a hard requirement).
**How met:** ST is the most open vendor in this class — fully public docs (no NDA),
mainline TF-A + U-Boot + OP-TEE + Linux, documented DDR init + a DDR tuning tool, and the
open **Etnaviv** GPU driver. The GC Nano GPU runs blob-free on mainline Mesa.

### R7 — Available on JLCPCB  ✅
**Why:** the board must be fabbable + assemblable at JLCPCB (LCSC-sourceable, no HDI).
**How met:** **STM32MP157AAC3 = LCSC C511448**, in stock (~249 units, ~$7). Package
**TFBGA-361, 0.5 mm pitch → JLC-standard fab, NO HDI** (through-hole vias, JLC SMT places
it). The pricier list variants only buy 800 MHz (D/F) and/or secure-boot crypto (C/F)
we don't need; AAC3 is both the best technical fit and the cheapest.

---

## Accepted tradeoffs (eyes open)

1. **Weakest GPU** — GC Nano / GLES 2.0 = N64/PS1-class 3D, not modern-shader. Accepted:
   the target is a Mario-Kart-64-style game, and GLES 2.0 is the better *learning* API.
2. **External DDR3 routing** — a new skill vs the v3 SiP DRAM. Mitigated by the 16-bit bus
   (simplest) + ST's published DK2 reference layout + ST's DDR tuning tool.
3. **Dev board can't drive the parallel panel** — the DK2 is DSI/HDMI only; the RGB666
   panel gets validated on the EV1 or the v4 board itself.
4. **Touch is FT7311, not GT911** — FocalTech mainline driver; DT-only.

## Verification note

The load-bearing facts above (LTDC parallel-RGB on mainline, GC Nano/Etnaviv GLES2, DDR3
16-bit, LCSC availability, dev-board display breakout) were researched and cross-checked
against primary sources this session (ST datasheets/RM0436, mainline kernel source
`drivers/gpu/drm/stm/ltdc.c` + `stm32mp135f-dk.dts`, Mesa docs, ST board pages, the LCSC
listing). Re-confirm exact LCSC #s + stock and the full pinmux against the final board
before ordering.
