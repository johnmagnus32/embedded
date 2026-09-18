# gameboy-v4 PCB — Bill of Materials (WORKING DRAFT)

**Status:** Early draft / planning. Nothing built, nothing ordered.
The gameboy-v4 handheld: an **ST STM32MP157AAC3** (dual Cortex-A7 @650 MHz + Cortex-M4,
Vivante GC Nano GLES2 GPU) running mainline Linux, driving the reused **gb3 5" 800×480
RGB666 parallel panel** via LTDC. See [`../REQUIREMENTS.md`](../REQUIREMENTS.md) for why
this chip. (Chip choice history: H616 → H700 → RK3288 → **STM32MP157**; the STM32MP157
won because it's the only candidate that drives the gb3 RGB panel on mainline + has a
clone-able ST reference design + easy 16-bit DDR3.)

**Reference design:** ST publishes complete design files (schematics + layout project +
gerbers + BOM) for the **STM32MP157F-DK2** (board MB1272, 16-bit DDR3L) and **EV1**
(MB1262, has a CN11 RGB/LTDC connector). These are the clone-able references for the
**DDR3 layout + STPMIC1 power tree** — the hard, must-be-exact core. Copy them; add our
handheld peripherals on top.

**Sourcing legend:**
- ✅ **Proven** — carried over from a board that booted on silicon (gb3 `t113-breakout` / v1 / v2); LCSC # confirmed.
- ⚠️ **Verify** — real MPN chosen, but **confirm LCSC #, stock, and JLC tier before ordering.**
- 🔲 **TODO** — value/decision not finalized (e.g., exact passive counts pending a KiCad BOM export of ST's reference).

> ⚠️ Passive quantities/values are **estimates** until a netlist BOM is exported from ST's
> DK2/EV1 design files. LCSC #s marked ⚠️ are **not yet live-confirmed** — resolve before ordering.

---

## 1. Core SoC + DDR3 + Power + Clock (clone ST's DK2/EV1 reference)

The DDR3 topology + STPMIC1 power tree are where a layout mistake means a dead board —
copy them net-for-net from ST's published board files.

| # | Group | Part (MPN) | Description | Qty | LCSC # | Src |
|---|-------|------------|-------------|-----|--------|-----|
| 1 | SoC | **STM32MP157AAC3** | Dual Cortex-A7 @650 MHz + Cortex-M4, GC Nano GPU (GLES2), **TFBGA-361** 12×12 mm 0.5 mm, **16-bit DDR3**, −40…+125 °C | 1 | **C511448** | ⚠️ (LCSC-listed) |
| 2 | PMIC | **STPMIC1A/1B** (ST) | Companion PMIC for the STM32MP1 — all SoC/DDR/IO rails, power sequencing (ST reference approach; do NOT hand-roll the rail sequencing) | 1 | — | ⚠️ |
| 3 | DRAM | **16-bit DDR3L, x16** (e.g. 4-Gbit / 512 MB — match ST DK2's part class: Micron/Nanya/Winbond DDR3L) | Single x16 DDR3L device = the simplest bus. 512 MB (4-Gbit) or 256 MB is ample | 1 | — | ⚠️ (DDR3 well-stocked at LCSC) |
| 4 | Clock | **24 MHz HSE crystal** | Main oscillator (SMD, CL per ST reference) | 1 | — | ⚠️ |
| 5 | Clock | **32.768 kHz LSE crystal** | RTC oscillator | 1 | — | ⚠️ |
| 6 | Clock | 18–22 pF C0G 0402 | Crystal load caps (per crystal datasheet) | 4 | — | 🔲 |
| 7 | DDR term | **240 Ω** ZQ-cal + **VREF divider** (2×) + VTT if used | DDR3L ZQ + VREF (VDDQ/2); short-trace single-chip layout may lean on ODT | ~several | — | 🔲 |
| 8 | Decoupling | 100 nF 0402 X7R | Per-power-pin (SoC + DDR + STPMIC1) | ~many | — | 🔲 |
| 9 | Decoupling | 1 µF / 4.7 µF 0402/0603 | DDR + rail decoupling | ~many | — | 🔲 |
| 10 | Bulk | 10 µF / 22 µF | STPMIC1 rail outputs + VBUS input bulk (**Rev-A lesson: don't omit the +5V bulk cap**, see gb3 t113-breakout REV-B) | ~several | — | 🔲 |
| 11 | Power inductors | per STPMIC1 buck datasheet | For the STPMIC1 switching rails | ~several | — | 🔲 |

> **DDR width:** the TFBGA-361 package is **16-bit DDR only** (single x16 chip) — the
> simplest bus, and exactly what the DK2 reference uses. (The larger LFBGA-354/448
> packages do 32-bit DDR but are bigger + harder to route — not chosen.)

---

## 2. Boot / recovery (software-side facts)

- **Boot chain (mainline):** BootROM → TF-A BL2 (does DDR3 init — open + documented by ST)
  → OP-TEE/SP-MIN → U-Boot (BL33) → kernel. Reference: `stm32mp157?-dk2` / `-ev1` U-Boot
  + kernel DTs. All components mainline/open.
- **Recovery:** BootROM **USB DFU** (`dfu-util` / STM32CubeProgrammer) — pushes code to a
  flash-less board over USB-C; cleaner than the T113 FEL dance. Plus UART boot.
- **Boot media:** QSPI-NOR / eMMC / microSD (BOOT pins + OTP select). NOR boot works like gb3.
- **DDR bring-up aid:** ST's **DDR tuning/test tool** (via STM32CubeProgrammer) validates
  the interface after layout — a real advantage for a first external-DDR board.

---

## 3. Display — reuse the gb3 panel (via LTDC, mainline)

| # | Role | Part | Interface | LCSC # | Src |
|---|------|------|-----------|--------|-----|
| 12 | Panel | **gb3 5" 800×480 IPS, RGB666 parallel** | LTDC DPI, `ltdc_pins_f` (18-bit RGB666, ~22 pins) → `panel-dpi`/`panel-simple` on mainline | (module) | ✅ reuse gb3 |
| 13 | Touch | **FocalTech FT7311** (gb3 silicon-validated; I²C @0x38) | I²C | (on-FPC) | ✅ reuse gb3 |
| 14 | FPC connector | 40-pin 0.5 mm FFC/FPC (match panel) | mechanical | — | 🔲 |
| 15 | Backlight driver | I²C boost+dimmer (gb3 choice, e.g. KTZ8866/LM3630A) | WLED boost + I²C | verify | ⚠️ |

> LTDC drives parallel RGB666 turnkey on mainline (`st,stm32-ltdc`, proven in-tree on
> `stm32mp135f-dk`). NOTE: LTDC has a single output — RGB and DSI are mutually exclusive.

---

## 4. Feature parts — carried over from gb3 (chip-agnostic I²C/I²S/etc.)

These reuse the gb3 selections; interfaces all map to STM32MP157 (see REQUIREMENTS R5).

| # | Role | Part (MPN) | Interface | LCSC # | Src |
|---|------|------------|-----------|--------|-----|
| 16 | IMU (6-axis) | **LSM6DSOXTR** | I²C 0x6A/0x6B | C481766 | ✅ (gb3-confirmed) |
| 17 | Haptic driver | **DRV2605LDGSR** | I²C 0x5A | C527464 | ✅ (gb3-confirmed) |
| 18 | LRA actuator | **Vybronics VG0832022D** (8 mm, 235 Hz) | via DRV2605L OUT± | (mech) | ✅ (gb3) |
| 19 | Audio amp | **MAX98357A** (I²S Class-D) + speaker | I²S / SAI | (from v1/v2) | ✅ carryover |
| 20 | Button expander | **PCA9555** + tactiles | I²C | verify | ⚠️ (gb3) |
| 21 | Analog stick | 2-axis + external I²C ADC (ADS1015-class) or digital-out module | I²C / ADC | verify | 🔲 |
| 22 | Bluetooth/WiFi | mainline module (RTL8723DS-class, `rtw88`) — **avoid out-of-tree radios** | SDIO + UART | verify | ⚠️ |
| 23 | Boot NOR | **W25Q128JVSIQ** | QSPI | C97521 | ✅ (gb3) |
| 24 | microSD | push-push socket w/ card-detect | SDMMC | C22467599 | ✅ (gb3) |
| 25 | Battery/charge | 1S LiPo + **BQ24074** power-path (gb3) | — | verify | ⚠️ (gb3) |
| 26 | USB-C | receptacle + CC pulldowns (5 V in + USB2 + DFU) | USB OTG | C165948 | ✅ (gb3/v1) |

---

## Next steps before this BOM is orderable

1. **Get ST's DK2 (MB1272) + EV1 (MB1262) design files** and export their DDR3 + STPMIC1
   BOM → replace the placeholder passive counts/values here.
2. **Resolve every ⚠️/🔲 to a live LCSC # + stock + JLC tier** (STPMIC1, DDR3L, crystals,
   backlight driver, PCA9555, BT module, charger).
3. **Order an STM32MP157F-DK2 now** — prototype mainline Linux + Etnaviv/GLES2 + the
   `forge` port while the board is designed (per REQUIREMENTS R2 / the build plan).
4. **Prove the DDR3 + STPMIC1 core** (clone ST's reference) before adding peripherals.
