# gameboy-v4 — Bill of Materials (RK3568 handheld)

**Status:** 🔬 Planning, spin 1. Nothing built or ordered. **Supersedes the retired `rk3568-breakout`
board** — the risk was concentrated in SoC + DDR3 + PMIC + boot, which this board carries anyway, and the
remaining peripherals are all low-risk I²C / I²S / ADC buses. Spin 1 **keeps the bring-up affordances**
(console, boot-mode control, probe pads, maskrom rescue) because there is no breakout to fall back on.

**Goal:** a Mario-Kart-class handheld — RK3568 + Mali-G52 on mainline, 5" DSI touch panel, 1S LiPo.

**Provenance:** the SoC / DRAM / power-tree / clock / boot / display rows carry forward ~8
Rockchip-primary-source review passes originally done for a separate `rk3568-breakout` board, now retired
— that board's purpose is subsumed here, and its ball-level work lives in
[PINOUT.md](PINOUT.md) and [POWER-AND-BLOCKS.md](POWER-AND-BLOCKS.md). The battery rows are ported from
[gameboy-v3/pcb/BOM.md](../../gameboy-v3/pcb/BOM.md) §5, which is datasheet-reviewed and 110/110 sourced.

**Src legend:** ✅ = user has seen the live JLC page · ⚠️ = my check only, confirm on the live page ·
🔲 = not chosen yet.

---

## ⛔ Open power questions the battery introduces

Three things change when the input is a 1S cell (3.0–4.4 V) instead of USB 5 V. **These gate the power
tree and should be settled before schematic capture.**

1. **The 3.3 V rail needs a buck-*boost*, and gb3's part won't carry it.** `vcc3v3_sys` draws **~2.2 A**
   (it feeds all nine RK809 VCC inputs). From a 1S cell the output sits both above and below the input
   across the discharge curve, so a plain buck can't do it. gb3 used **TPS63021 (2 A)** — *insufficient
   here*. ⛔ **Need ≥4 A, not 3**: the AP6256's `VBAT` draws **1 A peak**, on top of `vcc3v3_sys`'s ~2.2 A. New part to source. 🔲
2. **`RK860-0`'s input range is unverified from a 1S cell.** On the breakout it ran from 5 V. Bucking
   3.0–4.4 V down to 0.8–1.15 V should be fine, but **the RK860-0 datasheet is still unobtained** (it was
   already an open item on the breakout), so this is an assumption, not a fact. 🔲
3. **The backlight boost is probably fine.** `TPS61165` takes VIN 3–18 V, so it can run off `SYS`
   (3.0–4.4 V) directly rather than needing a 5 V intermediate — ⚠️ confirm against the datasheet, since
   the breakout specified VIN = 5 V and the duty cycle to 23.8 V is higher from 3.3 V.

> ✅ **PMIC decision: RK809-5 + discrete charger, NOT RK817-5.** RK817-5 integrates a charger and fuel
> gauge, which is why the old draft picked it — but it has **80 units in stock**, worse than the SoC.
> RK809-5 (**1,336**) already provides the rails, RTC **and the audio codec**; the only gaps are the
> charger and fuel gauge, covered by **BQ24073 (30,866)** + **MAX17048 (23,846)**. Keeping RK809-5 also
> means the breakout's entire 16-rail power tree, mandatory-cap enumeration and OTP sequencing analysis
> port unchanged — switching PMIC would have thrown that away.

---

## 1. SoC — ported unchanged

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 1 | SoC | **RK3568B2** | Quad A55, Mali-G52, FCCSP-636L 0.65 mm (no-HDI → JLC POFV via-in-pad). X-ray req, MSL 3. ⚠️ **98 in stock — buy early** | 1 | C3345225 | ✅ |
| 2 | Decoupling | **CC0201KRX5R6BB104** | 100 nF **0201** — one per power domain (HDG §2.1.8), **24 exact**. 0201 because these sit in the via field under the BGA | 24 | C190183 | ⚠️ |
| 3 | Decoupling | **CL05A475KP5NRNC** | 4.7 µF 0402 — **8 exact**: VDD_CPU · VDD_LOGIC · VDD_GPU · DDRPHY ×2 · SYSPLL_0V9 · SARADC/SYSPLL_1V8 · RK809 LDO4 | 8 | C368809 | ⚠️ |
| 4 | Decoupling | CL05A105KA5NQNC | 1 µF 0402 — SoC side, **7 exact** (incl. the DNP `RESETn` slow-release cap) | 7 | C52923 | ⚠️ |
| 5 | Decoupling | CL21A226MAQNNNE | 22 µF — SoC-side bulk, Rockchip's per-rail minima | 9 | C45783 | ✅ |
| 6 | Decoupling | CL21A106KOQNNNE | 10 µF 0805 — one per core rail (LOGIC/GPU/NPU/DDRPHY); lifts LOGIC/GPU/NPU over the mandated 90 µF | 4 | C1713 | ✅ |

## 2. DRAM — 2 GB DDR3L

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 7 | DRAM | **MT41K512M16VRP-107 IT:P** | 8 Gbit ×16 DDR3L, FBGA-96. 2 chips = single-rank 32-bit = **2 GB**. Run **DDR3-1066**. ⚠️ **333 in stock and the only viable 8 Gbit ×16 on JLC — buy early.** ✅ Layout-compatible fallback: **MT41K256M16TW-107 (4 Gbit, 4,722 stock)** → 1 GB, same JEDEC ballout, A15 simply unused | 2 | C2831669 | ✅ |
| 8 | DDR | 0603WAF1001T5E | 1 kΩ 1% — VREFCA divider = VDDQ/2 | 2 | C21190 | ✅ |
| 9 | DDR | 0402B102K500NT | 1 nF — VREFDQ + VREFCA + RESET# per chip (6) + 2 on `DDRPHY_VREFOUT` (one to VCC_DDR, one to GND) | 8 | C1523 | ⚠️ |
| 10 | DDR | **0402WGF499JTCE** | 49.9 Ω ×2 — DDR3 CLKP/N AC termination at the T branch | 2 | C25120 | ⚠️ |
| 11 | DDR | **0402CG101J500NT** | 100 pF C0G — centre cap of the CLKP/N RC | 1 | C1546 | ⚠️ |
| 12 | DDR | 0603WAF2400T5E | 240 Ω 1% — DRAM `ZQ`, one per package | 2 | C23350 | ⚠️ |
| 13 | DDR | 0402WGF1200TCE | 120 Ω 1% — SoC `DDR_RZQ` → GND | 1 | C25079 | ⚠️ |
| 14 | Decoupling | CL05B104KB54PNC | 100 nF — 7 per DRAM chip (Rockchip's DDR3 page) | 14 | C307331 | ✅ |
| 15 | Decoupling | CL21A106KOQNNNE | 10 µF — 2 per DRAM chip | 4 | C1713 | ✅ |

## 3. Power — PMIC + regulators (ported; see the open questions above)

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 16 | PMIC | **RK809-5** | 5 bucks + 9 LDOs + 2 switches + RTC + **audio codec (now USED — see §8)**. I²C0 @ **0x20 fixed**. ⚠️ **stock moving: 1,336 (was ~2,471)** — the tightest no-alternate part after the SoC/DRAM | 1 | C2939628 | ⚠️ |
| 17 | Power | **RK860-0** | `VDD_CPU` buck, 0.7125–1.5 V @ 12.5 mV, 6 A, I²C **0x40**, WLCSP-20. Mainline `rockchip,rk8600`. ⚠️ **datasheet still unobtained** + input-range question above | 1 | C19188628 | ⚠️ |
| 18 | Power | 🔲 **3.3 V buck-boost, ≥4 A** | ⛔ **NEW, replaces the breakout's SY8205.** Must hold 3.3 V across a 1S discharge (3.0–4.4 V in), carrying `vcc3v3_sys`'s ~2.2 A. gb3's TPS63021 is 2 A → insufficient | 1 | — | 🔲 |
| 19 | Power | **MWSA0503S-R47MT** | 0.47 µH — RK860-0 + the RK809 bucks. Rockchip specifies one value for every RK809 buck (0.47 µH, Isat ≥3.5 A, DCR ≤20 mΩ). ⚠️ 3,586 in stock for a qty-6 line | 6 | C408353 | ⚠️ |
| 20 | Power | **0402WGF8252TCE** | 82.5 kΩ — BUCK3 FB top. Sets `vcc_ddr` = **1.3500 V** for DDR3L. ⛔ Do **not** copy ROCK 3A's 47 kΩ (that's 1.11 V for LPDDR4) | 1 | C11717 | ⚠️ |
| 21 | Power | **0402WGF1203TCE** | 120 kΩ — BUCK3 FB bottom (the fixed leg in Rockchip's table) | 1 | C25750 | ⚠️ |
| 22 | Power | **0402WGF1000TCE** | 100 Ω — (a) **remote-sense feedback ×4**, *Mandatory* HDG §3.2.2.4: an open FB node **overvolts the rail and kills the SoC** (`VDD_CPU` abs max 1.25 V) · (b) UART2 TX ×1 · (c) SDMMC0_DET ×1 · (d) instrumentation-header bused/GPIO group ×18 (GPIO R3, *recommended*, 22–100 Ω) | 24 | C25076 | ⚠️ |
| 23 | Power | **0402WGJ0220TCE** | 22 Ω — *all Mandatory*: PMIC SCL+SDA (2) · XOUT24M (1) · FSPI_CLK ≤300 mil (1) · SDMMC0_CLK ≤400 mil (1) · SDMMC0 CMD+D0–D3 **at the card** (5) · touch SDA/SCL/RST/INT + panel RESET#/TE **at the FPC** (6) | 16 | C25152 | ⚠️ |
| 24 | Power | **0402WGF1002TCE** | 10 kΩ — `RESETn`/nPOR pull-up to **`vcc3v3_pmu`** (⚠️ must match the PMUIO1 domain, *not* `vcc3v3_sys`) + PMIC_INT + the VDC divider's top leg | 3 | C25744 | ⚠️ |
| 25 | Power | **0402WGF3301TCE** | 3.3 kΩ — VDC divider bottom leg ⇒ 1.24 V. **This is the buttonless power-on**: VDC's rising edge starts the PMIC after a **140 ms de-bounce** (RK809 Rev 2.3 Fig. 4-3) | 1 | C25890 | ⚠️ |
| 26 | Power | **0402WGF5102TCE** | 51 kΩ — RK860-0 EN pull-up to `vcc3v3_pmu`; **all three Rockchip references use this exact value to this rail**, which is what puts VDD_CPU in sequence slot 2A | 1 | C25794 | ⚠️ |
| 27 | Power | **0402WGF1001TCE** | 1 kΩ — ⚠️ **`FLASH_VOL_SEL` (AG25) pull-DOWN, brick-class strap.** Internal pull-up defaults it to 1.8 V but our NOR is 3.3 V; HDG says a mismatch means the chip *"may even be damaged"* and mainline no-ops `vccio2` so there is **no software rescue**. **1 kΩ → 0.19 V ✓; ⛔ 10 kΩ → 1.27 V FAILS** (V_IL 0.8 V). Also SY6280 EN series (2) and ⛔ **UART2 RX ×1 — raised from 100 Ω to stop the console adapter back-powering the board.** A USB-serial adapter idles TX high at 3.3 V and stays powered while the board is off, and `UART2_RX_M0` is ball **AC20 (`GPIO0_D0`) in PMUIO2 ⇒ `vcc3v3_pmu`** — the always-on domain that owns the `RESETn` pull-up and `PMIC_INT`. Through 100 Ω that injects **(3.3−0.7)/100 ≈ 26 mA** into the ESD clamp: at/above per-pin limits, and enough to hold `vcc3v3_pmu` partly up and muddy POR. 1 kΩ ⇒ ≈2.6 mA. `UART2_TX` stays 100 Ω (SoC output is Hi-Z when off). ✅ Not a checklist deviation in spirit — Debug-UART R2's 100 Ω exists for ESD series impedance and 1 kΩ gives *more*; at 1.5 Mbaud it is a ~20 ns edge against a 667 ns bit | 4 | C11702 | ⚠️ |
| 28 | Storage | **0402WGF4701TCE** | 4.7 kΩ — `FLASH_CS0n` pull-up to the **gated** NOR rail + NOR_EN/SD_EN pull-ups. ⛔ **Not 10 kΩ**: those GPIOs reset with a 16–43 kΩ internal pull-*down* and 10 kΩ reaches only 2.03 V worst case | 3 | C25900 | ⚠️ |
| 29 | Power | **BLM18PG121SN1D** | 120 Ω @100 MHz bead, 0603 — **×5, all Mandatory**: `USB_AVDD_0V9`/`_1V8` (2, shared net feeds both USB2+USB3) · `MIPI_DSI_TX0_AVDD_0V9`/`_1V8` (2) · RK809 `VCC_SPK_HP` (1, *"must be reserved"*). Each needs 1 µF source-side + 100 nF at the ball | 5 | C14709 | ⚠️ |
| 30 | Power | **0402WGF1003TCE** | 100 kΩ — TPS61165 `CTRL` external pull-down (*Mandatory*, LCM&TP R3: a floating CTRL can kick the 23.8 V rail before the IO domain is live) | 1 | C25741 | ⚠️ |
| 31 | Storage | **0402WGF1002TCE** | 10 kΩ ×2 — `FSPI_D2` (/WP) + `FSPI_D3` (/HOLD) pull-ups, *"must be reserved"*. ⚠️ **To the GATED NOR rail**, not VCCIO2 — tied to VCCIO2 they'd inject into a deliberately-dead flash and defeat the rescue gate | 2 | C25744 | ⚠️ |
| 32 | Power | **0402WGF2201TCE** | 2.2 kΩ ×2 — I²C0 pull-ups to `vcc3v3_pmu`. Without these the PMIC + CPU-buck bus doesn't work at all | 2 | C25879 | ⚠️ |
| 33 | Power | **0402WGJ0000TCE** | 0 Ω — `TSADC_SHUT_M0` into the `RESETn` net (hardware over-temp path; liftable for debug) | 1 | C21376 | ⚠️ |
| 34 | Power | **CC0402KRX7R9BB103** | 10 nF — buck soft-start cap. ⚠️ Re-derive against whichever buck-boost is chosen | 1 | C60133 | ⚠️ |
| 35 | Decoupling | CL21A106KOQNNNE | 10 µF — RK809 VCC3/4/9 (3) + BUCK3 out (1) + SWOUT1 (1) + buck-boost CIN (1) + RK860-0 CIN (2) | 8 | C1713 | ✅ |
| 36 | Decoupling | CL21A226MAQNNNE | 22 µF — RK809 BUCK1/2/4/5 ×2 each (8) + BUCK3 (1) + RK860-0 4 out/2 in (6) + buck-boost COUT (2) | 17 | C45783 | ✅ |
| 37 | Decoupling | CL05A105KA5NQNC | 1 µF — RK809 VCC5–8 (4) · LDO1,2,3,5,6,7,8,9 outputs (8; **LDO4 is 4.7 µF**) · SWOUT2 (1) · VREF/VCC_RTC/VCC_1P8A/VCC_1P8D (4) · +1 each RK860-0 and the buck-boost | 19 | C52923 | ⚠️ |
| 38 | Decoupling | CL05B104KB54PNC | 100 nF — RK809 PWRON/RESETB/VDC (3; HDG: the RESETB cap *"cannot be deleted at will"*) · BUCK1/2/4 remote-FB taps (3) · RK860-0 (1) · buck-boost PVIN (1) · SoC `nPOR` anti-ESD (1) | 9 | C307331 | ⚠️ |

## 4. Battery — charge + power-path + fuel gauge (1S LiPo) — ported from gb3 §5

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 39 | Battery | **BQ24073RGTR** | Power-path charger, 1.5 A, QFN-16-EP, autonomous. System runs on **OUT = SYS** (regulates ~4.4 V with no cell). ⛔ **Hard-won gb3 findings, carried forward:** `CE`→GND (must not float) · **`EN1`→GND, `EN2`→HIGH strapped to OUT, *not* IN** (EN abs-max 7 V) — otherwise the charger boots at 100 mA and ILIM is inert · **pin 15 is `TD` on the '073** (`ITERM` on the '074) → **TD→GND** for termination at the fixed 10 %; do not leave it unconnected · **EP → VSS with a via array** (heat path) | 1 | C15220 | ✅ |
| 40 | Battery | CL10A475KO8NNNC | 4.7 µF **16 V** — charger `IN`. ⚠️ Must stay **<10 µF** for USB-IF inrush; 16 V for the OVP window | 1 | C19666 | ✅ |
| 41 | Battery | CL21A106KOQNNNE | 10 µF — charger `OUT` (= the SYS node), placed close to the IC | 1 | C1713 | ✅ |
| 42 | Battery | CL21A106KOQNNNE | 10 µF — charger `BAT`, near the cell connector | 1 | C1713 | ✅ |
| 43 | Battery | 0603WAF2001T5E | 2.0 kΩ — `ISET` (charge current) | 1 | C22975 | ✅ |
| 44 | Battery | 0603WAF3601T5E | 3.6 kΩ — `ILIM` (input current limit) | 1 | C22980 | ✅ |
| 45 | Battery | **KNTC0603/10KF3950** | 10 kΩ NTC — `TS` battery-temperature sense | 1 | C2892547 | ✅ |
| 46 | Battery | 0603WAF0000T5E | 0 Ω — `EN2` strap to OUT (see the ⛔ note on the charger) | 1 | C21189 | ✅ |
| 47 | Battery | **S2B-PH-SM4-TB** | JST-PH 2-pin SMD battery connector, side-entry | 1 | C295747 | ✅ |
| 48 | Battery | **MAX17048G+T10** | ⛔ **NEW — I²C fuel gauge** (ModelGauge, no sense resistor). Replaces gb3's resistor-divider-to-ADC approach, and covers the one thing RK809-5 lacks versus RK817-5. 23,846 stock | 1 | C2682616 | ⚠️ |
| 49 | Battery | 🔲 **LiPo cell, 1S** | ⚠️ **Re-size for this board.** gb3 used a 2000 mAh PKCELL LP-803860. At ~5 W sustained gaming a 2000 mAh (7.4 Wh) cell gives **~1.5 h**; a 4000 mAh (14.8 Wh) gives **~3 h**, which is what comparable RK3566 handhelds (RG353, RGB30) ship. Not a JLC part — bought with the enclosure | 1 | — | 🔲 |

## 5. Clocks — ported unchanged

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 50 | Clock | **X322524MOB4SI** | 24 MHz, **CL = 12 pF**, SMD3225 | 1 | C70590 | ⚠️ |
| 51 | Clock | **0402CG180J500NT** | **18 pF** C0G — 24 MHz load caps. ⛔ Not 22 pF: that under-capacitates a 12 pF-CL part and runs **+20…+40 ppm fast**, busting Rockchip's *Mandatory* ±20 ppm budget on its own | 2 | C1549 | ✅ |
| 52 | Clock | **0402WGF1004TCE** | 1 MΩ — feedback across XIN24M/XOUT24M. Rockchip: *"cannot be modified at will"* | 1 | C26083 | ⚠️ |
| 53 | Clock | **SC-20S 32.768 kHz** | RK809 RTC crystal, CL 12.5 pF. ⚠️ PMIC R57 (*Mandatory*): it's a metal cylinder, so **the case must be grounded** — add a pad/clip under the can | 1 | C97607 | ✅ |
| 54 | Clock | 0402CG220J500NT | **22 pF** C0G — 32 kHz load caps | 2 | C1555 | ✅ |

## 6. Power in + USB

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 55 | USB | **TYPE-C-31-M-12** | USB-C, 16P, **5 A rated** — charge input + `USB3_OTG0_DP/DM` for maskrom/rockusb/gadget. ⚠️ These are **USB3-namespace balls running at HS**; the USB2 PHY's `USB2_AVDD_*` balls are tied to VSS — OTG0's HS path runs on `USB3_AVDD_*` | 1 | C165948 | ✅ |
| 56 | USB | RC0603FR-075K1L | 5.1 kΩ CC pulldowns (Rd). ⚠️ No CC sensing on-board, so the **source** decides the current: C-to-C charger advertises 3 A, an A→C cable only 500 mA | 2 | C105580 | ✅ |
| 57 | USB | USBLC6-2SC6 | D+/D− ESD + VBUS clamp | 1 | C7519 | ✅ |
| 58 | Decoupling | **RVT1V101M0810** | 100 µF — VBUS input bulk. *Mandatory* >100 µF (PMIC&Power R11) | 1 | C37309 | ⚠️ |
| 59 | Decoupling | **RVT1V101M0810** | 100 µF — ⚠️ **`vcc3v3_sys` bulk, *Mandatory*** (PMIC&Power R12: *"more than 100 µF"*). Also the mitigation for VCC9's 0.3 V shutdown margin | 1 | C37309 | ⚠️ |
| 60 | Power | **RC0805FR-072R2L** | 2.2 Ω — ⚠️ **input RC snubber, *Mandatory*** (PMIC&Power R10), in series with a 2.2 µF across the DC input | 1 | C137549 | ⚠️ |
| 61 | USB | **0402WGF1002TCE + 0402WGF1502TCE** | **VBUSDET divider — *Mandatory* for maskrom.** 10 kΩ (VBUS→M24) + 15 kΩ (M24→GND) = **3.00 V**, centred in the 2.7–3.3 V window. ⛔ **M24 is NOT 5 V tolerant** — direct VBUS damages it | 1+1 | C25744 / C25756 | ⚠️ |
| 62 | USB | CL05B104KB54PNC | 100 nF at ball M24 — *Mandatory*; gives ~0.6 ms attach debounce | 1 | C307331 | ⚠️ |
| 63 | USB | **RC0402FR-072R2L** | 2.2 Ω ×2 series on `USB3_OTG0_DP`/`DM` — *Mandatory* | 2 | C327251 | ⚠️ |
| 64 | USB | CL05B104KB54PNC | 100 nF ×3 on `USB3_AVDD_0V9`/`_1V8`/`_3V3`. ⚠️ HDG §2.2.2.9: **all three must be powered at first power-on** or firmware download over DP/DM doesn't work. ⚠️ The checklist wants these *on the back of the balls* — confirm a 0402 fits beside P22/P23/M23 or swap to 0201 | 3 | C307331 | ⚠️ |

## 7. Boot storage + remote rescue — ported unchanged

> BootROM order is **FSPI → NAND → eMMC → SDMMC0 → USB(maskrom)**, hardcoded. Gating a medium's VCC makes
> its probe fail, so **both media off ⇒ maskrom, enforced in hardware**. Ported from the
> [t113-breakout Fix 5](../../t113-breakout/REV-B-FIXES.md) pattern, bench-confirmed there.

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 65 | Storage | W25Q128JVSIQ | 128 Mbit SPI-NOR on FSPI — the remote-flashable boot target. **3.3 V part ⇒ the flash IO domain must be 3.3 V** (the 1 kΩ `FLASH_VOL_SEL` strap) | 1 | C97521 | ✅ |
| 66 | Storage | **A-MicroTF-1.85A** | microSD socket w/ card-detect → `sdmmc0`. ⛔ **`SDMMC0_DET` (Y22) is a reset-sampled strap for the whole VCCIO3 bank**: HIGH = ARM JTAG (internal pull-up default), LOW = SDMMC0. Float it and **the card simply won't work** | 1 | C22467599 | ⚠️ |
| 67 | Storage | SP3004-04XTG | 4-ch ESD, 0.85 pF — covers SD CLK/CMD/DAT0–3 | 2 | C207280 | ⚠️ |
| 68 | Storage | RC0603JR-0710KL | 10 kΩ — SD CMD/DAT pull-ups (5) + `SARADC_VIN0` recovery pull-up **to `vcca_1v8`** (1). ⚠️ Must be 1.8 V: `SARADC_VIN0`'s max is `SARADC_AVDD_1V8` | 6 | C99198 | ✅ |
| 69 | Decoupling | CC0603KRX7R9BB104 | 100 nF — NOR VCC + SD VDD | 2 | C14663 | ✅ |
| 70 | Decoupling | CL21A106KOQNNNE | 10 µF — SD VDD bulk at the socket | 1 | C1713 | ✅ |
| 71 | Recovery | **SY6280AAC** ×3 | Boot-media power switches + the panel rail. Active-high EN, 2.4–5.5 V, 80 mΩ, SOT-23-5. **Switch #1** → NOR VCC, **#2** → microSD VDD (both off ⇒ maskrom), **#3** → panel VCI+IOVCC. ⚠️ *Mandatory*, LCM&TP R8: *"the power supply of the screen **must be controllable**… when powered on it is **not provided by default**"* ⇒ **#3 gets no pull-up** and is driven from `GPIO0_C5` (AC21), whose reset pull is **down** and whose Rockchip pin-usage text is literally *"LCD1 panel power enable, active high"* | 3 | C55136 | ⚠️ |
| 72 | Recovery | **0402WGF6801TCE** | 6.8 kΩ ×3 — *Mandatory* `ISET`, one per SY6280. **I_lim = 6800/R** ⇒ 1 A. Without it the limit is undefined | 3 | C25917 | ⚠️ |
| 73 | Recovery | CL21A106KOQNNNE + CL05B104KB54PNC | 10 µF + 100 nF downstream of switch #3 (the panel rail); SD's and NOR's pairs are counted at their own loads | 2 | C1713 / C307331 | ✅ |

## 8. Bring-up affordances — ⛔ all right-angle to keep the stack height down

> Spin 1 populates these; later spins depopulate. **All connectors are right-angle / side-entry** so
> nothing protrudes vertically into the enclosure.

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 74 | Debug | **PZ254R-11-03P** | 1×3 2.54 mm **right-angle** pin header — UART2 console TX/RX/GND. ⛔ Right-angle (was vertical on the breakout) per the height constraint. ⚠️ Through-hole: hand-solder | 1 | C492411 | ⚠️ |
| 75 | Recovery | **S3B-XH-A** | 3-pin JST-XH **side-entry**, keyed — boot-mode connector. **1** `NOR_EN` · **2** `SD_EN` · **3** `GND`. That's the whole remote-control surface and it's irreducible: nothing else selects a boot source. Power-cycling is `uhubctl` cutting VBUS. ⚠️ Pitch is **2.5 mm** despite being sold as "XH2.54" | 1 | C157928 | ⚠️ |
| 76 | Debug | **PZ254R-11-40P** | 1×40 **right-angle** header, **snapped to length** — the instrumentation block (~2×25 equivalent). Spare I²C / SPI0 / UART, PWM, ~8 GPIO, rails, GND. ⛔ With no breakout board, this is the **only** way to get a probe or logic analyser onto a bus — it stopped being insurance. ⚠️ 615 strips in stock | 2 | C2925569 | ⚠️ |
| 77 | Debug | Test points / 1×1 pads | Probe pads on **every rail** (5 V, SYS, 3V3_SYS, VDD_CPU, vdd_logic, vdd_gpu, vcc_ddr, vcc_1v8, vcc3v3_pmu, vcca_1v8, VBUSDET) **+ `PWRON`, `RESETn`, `RECOVERY`**. ⚠️ **VCC9 has only ~0.3 V of shutdown margin**, so measuring droop is not optional. Footprint-only | ~14 | — | ✅ |
| 78 | Status | 0603WAF5100T5E | 510 Ω — power-LED limiter | 1 | C23193 | ✅ |
| 79 | Status | 19-213SYGC | Green power LED — the only "is it alive" signal besides UART. ⚠️ **23 in stock**; any 0603 green substitutes | 1 | C2986027 | ⚠️ |
| 80 | Display | **ESD5451N-MS** | Bidirectional 0.4 pF ESD, 0402 — ***Mandatory* ×10**: touch SDA/SCL/RST/INT + panel RESET#/TE (6, LCM&TP R12) · UART2 RX/TX (2, Debug-UART R2) · USB-C CC1/CC2 (2, USB2 R10 — the USBLC6 only covers DP/DM+VBUS). Topology is always **connector → TVS → series R → SoC** | 10 | C2802295 | ⚠️ |

## 9. Display — Winstar WF50DTYA3MNG10 (5" 720×1280 DSI + touch)

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 81 | Display | **JUSHUO AFC24-S40FIA-00** | Display tail: 0.5 mm × **40 pos**, right-angle, flip-top, **double-sided contacts** (mates fingers-up *or* -down, which removes the unverified contact-side risk). Carries 4× DSI pairs + CLK, VCI, IOVCC, RESET, TE, LED+/LED1−/LED2−, GND | 1 | C262686 | ⚠️ |
| 82 | Display | **JUSHUO AFC24-S06FIA-00** | Touch tail: 0.5 mm × **6 pos** — SDA, SCL, RST, INT, VDDT, GND | 1 | C262679 | ⚠️ |
| 83 | Display | 0603WAF2001T5E | 2.0 kΩ — **GT928** touch I²C pull-ups. ⚠️ LCM&TP R10 wants these on the **touch 3.3 V node**, not `vcc3v3_pmu`. ⚠️ The GT928's I²C address is **strapped at power-on by RST/INT**, so route both to GPIOs | 2 | C22975 | ✅ |
| 84 | Decoupling | CL05B104KB54PNC + CL05A105KA5NQNC | 100 nF + 1 µF — local decoupling at both FPC connectors (VCI / IOVCC / VDDT) | 6 | C307331 / C52923 | ⚠️ |
| 85 | Backlight | TPS61165DBVR | WLED constant-current boost → 19.6–23.8 V @ 40 mA. `CTRL` ← **PWM4 = GPIO0_C3 (AE23)**, reset pull-down. ⚠️ **VIN is now SYS (3.0–4.4 V), not 5 V** — see open question 3 | 1 | C58756 | ✅ |
| 86 | Backlight | SMNR4020-10UH | 10 µH boost inductor | 1 | C135263 | ⚠️ |
| 87 | Backlight | SS16 | 60 V 1 A Schottky | 1 | C2481 | ✅ |
| 88 | Backlight | CC0805KKX7R9BB225 | 2.2 µF 50 V — TPS61165 Cout **and** the input RC snubber's cap | 2 | C125847 | ✅ |
| 89 | Backlight | CC0603KRX5R6BB475 | 4.7 µF Cin | 1 | C109456 | ✅ |
| 90 | Backlight | RC0402FR-075R1L | 5.1 Ω Rset — panel ILED = **40 mA**; 200 mV / 5.1 Ω = 39 mA ✓ | 1 | C137955 | ✅ |
| 91 | Backlight | CC0402KRX7R7BB224 | 220 nF COMP (mandatory loop comp) | 1 | C326590 | ✅ |

## 10. Radio — WiFi + Bluetooth (one module, one antenna)

⛔ **NEW. The board previously had no network interface at all** — USB-C is OTG-only and gb3's Bluetooth is
not a network, so web browsing was impossible. Spec confirmed against
`cad/docs/AP6256-wifi-bt-module.pdf`.

**One module, one antenna, both radios.** Datasheet: *"Supports 1 antenna with one for WLAN & Bluetooth
shared port… shared Bluetooth and WLAN receive signal path eliminates the need for an external power
splitter."* So no diplexer, no splitter, no second antenna, and no separate Bluetooth module.

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 92 | Radio | **AP6256** | **WiFi + BT combo module**, QFN-44 12×12, $5.31. **802.11a/b/g/n/ac 1×1 SISO, 433.3 Mbps**, 2.4 + 5 GHz, 20/40/80 MHz · **BT 5.2** (Classic + LE) · **SDIO v3.0** (v2.0-host compatible). ⚠️ JLC's describe field claims "IEEE802.11b/g/n" — **wrong**, the datasheet is explicit about `ac`. ✅ Mainline driver confirmed: `brcmfmac/sdio.c` has `BRCMF_FW_ENTRY(BRCM_CC_4345_CHIP_ID, 0x00000200, 43456)` | 1 | C5248079 | ⚠️ |
| 93 | Radio | **U.FL-R-SMT(10)** | **Board-side U.FL/IPEX receptacle**, SMD, 1.25 mm tall, 50 Ω to 6 GHz — the antenna connector. Genuine Hirose; a clone at **C5299419** ($0.023, 210 k stock) is the cost-down. ⚠️ **~30 mating cycles** — treat as mate-once at assembly, and use a **U.FL removal tool** (never pull the cable, it rips the centre pin or lifts the pad). ⚠️ Mated stack ≈ **2.5 mm** plus coax bend radius — budget it in the enclosure | 1 | C434808 | ⚠️ |
| 94 | Radio | CL21A106KOQNNNE | 10 µF — `VBAT` bulk, local to the module. ⚠️ **`VBAT` draws 1 A peak** (datasheet §2.2), so this is not token decoupling | 1 | C1713 | ⚠️ |
| 95 | Radio | CL05B104KB54PNC | 100 nF — `VBAT` HF bypass | 1 | C307331 | ⚠️ |
| 96 | Radio | CL05A105KA5NQNC | 1 µF — `VDDIO` bypass | 1 | C52923 | ⚠️ |
| 97 | Radio | CL05B104KB54PNC | 100 nF — `VDDIO` HF bypass | 1 | C307331 | ⚠️ |
| 98 | Radio | **0402WGJ0000TCE** | 0 Ω — **`SDIO_VSEL` strap to GND**. Pin 29: *"1: when VDDIO = 1.8 V, 0: when VDDIO = 3.3 V."* Our IO domains are all 3.3 V, so **VDDIO = 3.3 V ⇒ strap LOW**. A 0 Ω rather than a hard tie so it can be lifted if the SDIO domain ever moves to 1.8 V | 1 | C21376 | ⚠️ |
| 99 | Radio | **0402WGF1002TCE** | 10 kΩ ×2 — pull-downs on **`WL_REG_ON`** and **`BT_REG_ON`** so both radios are **off at reset** until software enables them. Driven from SoC GPIOs | 2 | C25744 | ⚠️ |
| 100 | Radio | 🔲 **coax pigtail, U.FL → U.FL** | ~1.13 mm coax, board to antenna. **Not a JLC part** — hand-fitted after assembly, like the battery and enclosure | 1 | — | 🔲 |
| 101 | Radio | 🔲 **dual-band FPC antenna** | 2.4 + 5 GHz flex antenna, adhesive-backed, U.FL terminated. Pick one with a **published VSWR curve**. **Not a JLC part** | 1 | — | 🔲 |

**Why U.FL + a flex antenna rather than a chip or trace antenna.** Pin 2 is `WL_BT_ANT` ("RF I/O port")
flanked by GND on pins 1 and 3 — a ground-signal-ground arrangement, so feed it with a 50 Ω coplanar
waveguide. ⛔ **The datasheet publishes no reference antenna** — no recommended type, no VSWR, no dBi, no
keep-out. With no vendor tuning to copy, a **pre-tuned commercial antenna is the only option that doesn't
require a VNA**, and a cable is the only way to *place* the radiator away from the battery, the LCD frame
and the user's hands — which is the dominant variable in a handheld. This is what the Switch, the Steam
Deck and the Anbernic/Powkiddy RK3566 handhelds all do. ⛔ **Pin 4 is NC and must float** — *"Don't
connected to ground."*

**Digital connections:** 6 SDIO lines (`SDIO_DATA_0–3`, `_CMD`, `_CLK`) + SDIO IRQ + `WL_REG_ON` +
`BT_REG_ON` + `WL_HOST_WAKE` + `BT_HOST_WAKE` + `LPO` + the BT UART pair. Optional `PCM_IN/OUT/CLK/SYNC`
for routing BT audio in hardware.

**✅ Two decisions taken from Rockchip's reference** (`rk3568-rock-3a.dts`) rather than invented:
- **`sdmmc2`, M0 pinmux.** The reference puts microSD on `sdmmc0` and SDIO WiFi on `&sdmmc2` with
  `cap-sdio-irq` + `non-removable` + `keep-power-in-suspend` + `mmc-pwrseq`, on `sdmmc2m0_*` pins. M0 lands
  on the unused CIF camera bus; **M1 would collide with `LCDC_D8/D12/D13`**. It declares `sd-uhs-sdr104`
  (UHS-I, 208 MHz) — vastly above the ~8 Mbit/s a 1080p H.264 stream needs.
- **`CLK32K` finally has a consumer.** The reference wires `clocks = <&rk809 1>` as the pwrseq's
  `ext_clock` — the RK809 pin the breakout had written off as unused. Route it to `LPO` and no separate
  32 kHz oscillator is needed.

**⚠️ Three host-side blobs, all in `/lib/firmware/brcm/`:** `brcmfmac43456-sdio.bin` (WiFi firmware,
**mandatory**) · `brcmfmac43456-sdio.txt` (WiFi **NVRAM**, **mandatory** — only the `.clm_blob` is flagged
`BRCMF_FW_REQF_OPTIONAL`) · `BCM4345C5.hcd` (**Bluetooth patchram**, uploaded over UART by `btbcm`). Also
assign a BD address — `btbcm.c` knows `BDADDR_BCM4345C5` as the unset placeholder. **The NVRAM is the real
integration risk**, not the firmware: it is board-flavoured (`boardrev`, `devpath`, `ccode`, `macaddr`,
antenna fields). Because the AP6256 is a *module*, the hard RF numbers (PA gain tables, crystal trim,
temperature curves) belong to the module and travel with it — so an NVRAM from any SBC using an AP6256 is
mostly correct, and what's left is `ccode`, `macaddr` and antenna fields.

**⚠️ Relaxes R6.** `brcmfmac` and `btbcm` load **firmware blobs** to the module. That is *device* firmware
on a separate chip, not a kernel blob on our CPU — the same category as the `ddr.bin` already accepted for
DDR init, and as the RK3588's `mali_csffw.bin` noted in [CHIP-COMPARISON.md](CHIP-COMPARISON.md). **No WiFi
chip from any vendor is blob-free** (the one exception, Atheros `ath9k` on PCIe, is 2.4 GHz-only and
mini-PCIe — absurd in a handheld), so the alternative is no WiFi. **Amend R6 rather than break it quietly.**

**Cost-down / fallback:** **AP6212** (C2798300, 1,648 stock, $3.02, same QFN-44 12×12) is BCM43430-based —
the Pi Zero W's chip, so mainline support is about as certain as it gets — but **2.4 GHz only, BT 4.1**.
⚠️ **Its `VBAT` max is 3.8 V**, so it must come off the regulated 3.3 V rail, never `SYS` (a full 4.2 V
cell would exceed it). Feeding the AP6256 from 3.3 V too keeps the two interchangeable. ⚠️ Verify
pin-compatibility before committing the footprint.

## 11. 🔲 Handheld peripherals — to source (ported from gb3 unless marked NEW)

Named here so the interface budget is visible; **none is sourced yet.** gb3's picks are datasheet-reviewed
and 110/110 sourced, so most of this is transcription, not design.

| Role | Part | Interface | Status vs gb3 |
|---|---|---|---|
| Button expander | **PCA9555PWR** | I²C | port |
| D-pad, A/B/X/Y, Start/Select | ⛔ **conductive silicone membrane + plastic caps** (Switch Lite replacement sets) | → expander | ⛔ **CHANGED from gb3's tactiles** |
| **L/R, ZL/ZR shoulders** | ⛔ **membrane too** — the Switch Lite set includes them | → expander | ✅ confirmed from the product photos |
| Volume ± | **ALPS SKRTLBE010** ×2 — the only tactiles left | → expander | board edge; lever awkward, feel irrelevant |
| IMU (6-axis) | **LSM6DSOXTR** | I²C | port |
| Haptic driver + actuator | **DRV2605LDGSR** + **Vybronics VG0832022D** LRA | I²C | port |
| Speaker amp + speaker | **MAX98357AETE+T** + **Same Sky CES-2704-088L050** | I²S | ⚠️ see codec note |
| Headphone chain (gb3 "Tier-2") | **PCM5102APWR** DAC → **TPA6132A2RTER** amp → **PJ-327C-4A** jack | I²S | port — ~18 parts with passives |
| **Analog stick ×2** | **Hall-effect Joy-Con replacement** (AliExpress, ~$1–7 per 2) | SoC ADC (4 ch) + FFC | ⛔ **NEW — gb3 has none.** ✅ Hall = **no drift** |
| Soft power button | — | RK809 `PWRON` | ✅ **part saved** — but see the `VDC` note below |
| microSD | ✅ done (socket row above) | SDMMC | |

**Five things that are *not* a straight port:**

- ⛔ **All buttons move to conductive silicone membrane except volume.** **There is no switch component**:
  a conductive-carbon pill on a silicone dome bridges **two exposed copper pads on our PCB**. So
  `TS-1187A-B-A-B` x10 is **deleted** and `ALPS SKRTLBE010` drops to **x2 (volume only)** — those sit at a
  board edge where a lever is awkward and feel is irrelevant.
  ✅ **Resolved from the product photographs** (two earlier revisions of this note got it wrong — first
  deleting *all* tactiles, then proposing a tactile/membrane hybrid for the shoulders on the theory that a
  lever-driven point force suits a switch better). **Nintendo uses membrane throughout**, and the Switch
  Lite replacement set contains, visibly: a **one-piece D-pad cross with hinge slots** in its centre · a
  **4-dome A/B/X/Y diamond** · **2 square pads with raised square centres (L/R shoulders)** · **2 round
  dark-centred domes (ZL/ZR triggers)** · a **4-dome Home/Capture/Plus/Minus strip** · a clear plastic
  power-button piece. *(Text sources had conflicted: a GBAtemp thread on "Joy Con ZL tactile switch specs"
  versus eXtremeRate describing the ZL/ZR assembly as conductive rubber domes. The photo settles it.)*
  ⚠️ **The cost of going all-membrane: with only volume tactile, essentially no buttons are testable until
  the enclosure exists.** That is precisely what
  [controls-breakout](../../controls-breakout/README.md) is for — settle the stack there before this
  board's layout freezes. Electrically identical to a switch shorting to ground, so the **PCA9555
  is unchanged**.
  ⛔ **This forces the board finish to ENIG.** JLC's default HASL is unusable here — a builder who tried it
  found *"HASL finish is very sensitive to dust"*, and the consensus is *"ENIG because it's flat and
  remains conductive."* ENIG is an upcharge on the **whole board**, not just those pads. (Mass production
  uses **carbon ink** over the pads, more wear- and oxidation-resistant than gold, but it is not a standard
  JLC option.)
  **Pads:** interdigitated fingers, "as tight as the fab allows", **no solder mask**.
  ⚠️ **The real cost is mechanical, not electrical.** A tactile switch works on a bare board; a membrane
  only works when the enclosure holds the silicone at the right compression — too loose and nothing
  actuates, too tight and it is permanently pressed. **Buttons cannot be tested until the shell exists.**
  ⛔ **Buying a donor membrane inherits its button spacing.** The rubber is molded one piece per group, so
  the ABXY cluster, the D-pad cross and their relative positions come from Nintendo's design and our pads
  and shell openings must match *it*. Get the rubber **in hand before placing anything** in that corner.
  ✅ **Donor: Switch Lite.** Its sets include a proper one-piece **D-pad cross** (Joy-Cons use four separate
  buttons) and it is the closest form factor. Buy the **conductive rubber set** *and* a **plastic button
  set** — then only the copper pads and shell openings are ours; print custom caps later as a cosmetic
  change, not a functional unknown.
  ⚠️ **To measure on arrival:** pill centre-to-centre spacing (→ pad centres), pill diameter (→ pad
  diameter), uncompressed dome height + travel before contact (→ the shell standoff that decides whether
  buttons work at all), and whether the set covers shoulder buttons.
  ✅ **De-risked separately by [controls-breakout](../../controls-breakout/README.md)** — a trivial 2-layer ENIG
  board (pad pairs + one LED each, zero firmware) that settles the standoff height, pad geometry, D-pad
  feel and cap fit against a printed shell, **in parallel with this board's layout**. If it validates the
  membrane stack before v4's layout freezes, v4 commits to membranes with no hedge; otherwise carry
  tactile footprints on spin 1. Pads and tactile footprints cannot share the same area, so it is one or
  the other per spin.

- ⛔ **The analog stick is new.** `REQUIREMENTS.md` R5 lists "Analog stick (2-axis)" as a gb3 peripheral,
  but **gb3's BOM has no stick** — its controls are entirely tactile (D-pad, face, Start/Select, volume,
  bumpers). For a kart game a stick is arguably essential, so this is a genuine addition to design, not
  copy. ✅ **Part chosen: a Hall-effect Joy-Con replacement stick** (~$1–7 per 2 on AliExpress) over the
  documented ALPS **RKJXV122400R** (LCSC `C918854`, $0.69) — Hall is **contactless, so it cannot drift**,
  which is the exact wear-out failure that made Joy-Con drift infamous and that the ALPS part's carbon-film
  track is subject to. It is also far lower-profile, matching the Switch form factor. ⚠️ Keep RKJXV as the
  documented fallback: the Hall part has **no datasheet at all**.
  ⛔ **Supply 3.3 V and divide each axis output — 4 axes x 2 resistors = 8.** This reverses an earlier note
  here that said to supply from `vcca_1v8` so the output stayed under `SARADC_AVDD_1V8` with no divider.
  That shortcut only applies to a *bare* pot whose supply you control. **Any Joy-Con-pinout part expects
  the Joy-Con's ~3.3 V supply** — and a pot across 3.3 V swings its wiper 0-3.3 V just as a Hall part
  drives 0-3.3 V, so **either way the output exceeds the 1.8 V ADC limit.** Supplying 3.3 V + dividing is
  the one arrangement that works for *both* technologies and keeps them swappable.
  ✅ **Carbon-film and Hall versions are very likely pin-compatible** — both advertise as 1:1 replacements
  in the same Joy-Con socket. Verify on arrival; if so, one FFC footprint serves both.
  **Prototype with the carbon-film 4-pack** (Amazon, days not weeks, **and it includes 4 thumbstick caps**
  plus Y/+ screws, which settles the cap question), **ship with Hall** (no drift).
  ✅ **INTERFACE FULLY SPECIFIED — no measuring needed.** Community-documented (4layertech's breakout
  pinout + an independent forum confirmation), so the symbol and footprint can be drawn today:
  **5 contacts at 0.5 mm pitch**, pinout **`V+` (3.3 V) · `G` · `X` · `Y` · `S`** (centre-click switch,
  **shorts to ground when pressed**).
  ⛔ **The flex CANNOT be soldered.** *"These joysticks use a carbon membrane flex connector which cannot
  be soldered to"* — the traces are **carbon, not copper**. An FFC/ZIF connector is mandatory, not a
  convenience. (iFixit Guide 113182 independently confirms a **ZIF** connector in the stock Joy-Con.)
  ✅ **Connector: `FPC-0.5FX-05PH20`, LCSC `C19077699`** — 5P 0.5 mm, right-angle, **double-sided
  contacts, top and bottom entry**, 3,830 stock, $0.038. ⛔ Double-sided is deliberate: 4layertech warns
  *"install with the graphite contact side facing down… contact side up reverses the pinout"*, and a
  dual-contact connector mates either way — the same reasoning that picked the JUSHUO display connectors.
  **This connector IS the BOM row and the footprint**; the stick itself needs no land pattern.
  ✅ **Mechanical:** held by **2x Phillips #00 screws** (iFixit) so the enclosure needs two threaded bosses,
  with a **thin gasket** around the shaft hole in the shell. **4layertech publishes a STEP file** for their
  breakout as a geometry reference.
  ⚠️ **Verify on arrival, as confirmation rather than discovery:** pitch and contact count with calipers,
  and the **output range** — TMR sensing may not swing a clean 0-3.3 V, which sets the divider ratio.
  ⚠️ Expect per-axis software calibration (centre/min/max at first boot); reviewers report needing it even
  in a real Joy-Con.
  stick FPC pitch", Printables/GrabCAD for a scanned model) before measuring from scratch.
  ⚠️ **2 sticks = 4 ADC channels, and that makes `SARADC_VIN*` *used*** — so the checklist's **1 nF per
  channel becomes *Mandatory*** (+4), and its routing rule applies: keep them away from the LCD and DRAM
  buses, no vias near high-speed signals. ⚠️ Expect per-axis software calibration (centre/min/max at first
  boot) — reviewers report needing it even in a real Joy-Con.
- ✅ **The power button gets simpler.** gb3 needed an **STM6601** push-button power controller because the
  T113 had no PMIC button input. RK809-5 has **`PWRON`** natively, so that IC and its two passives
  disappear. Datasheet Rev 2.3 §4.1.2/4.1.3: **100 ms** low to turn on (de-bounce 500/100 ms, I²C-set),
  **6/8/10/12 s** low to force off. ⛔ **Gotcha: long-press-off is gated on software** — *"writing register
  bit `0xF7<6>=0`, and then keeping low level at PWRON for 6/8/10/12 S would turn off the PMIC."* If the
  board hangs before anything writes that bit, **the long press does nothing**. Set it in U-Boot, not just
  in the kernel.
- ⛔ **`VDC` must move from `SYS` to `VBUS`.** On the breakout the VDC divider hung off the always-on 5 V as
  a deliberate *buttonless* power-on. With a battery, `SYS` is always present, so a VDC rising edge happens
  the moment a cell is connected and the board boots itself. Tie the divider to **`VBUS`** instead and the
  behaviour becomes conventional: insert battery ⇒ nothing · plug charger ⇒ wakes · press `PWRON` ⇒ boots.
  *(A long-press-off still latches off correctly either way — VDC stays high, so there is no new rising
  edge to restart it.)*
- ⚠️ **The speaker path may collapse into the PMIC.** gb3's §4 is explicit about *why* it used an external
  I²S amp: *"on-chip codec has no mainline driver."* RK809-5 **has** a codec, and this BOM already
  bead-feeds `VCC_SPK_HP` because Rockchip mandates reserving it. **Verify RK809 codec support in mainline
  ASoC before committing** — if it's there, MAX98357A and possibly the whole PCM5102A/TPA6132A2 headphone
  chain (~20 parts) go away. If not, port gb3's chain unchanged.

---

## Status

**101 rows · 38 ✅ / 59 ⚠️ / 4 🔲** · 2 datasheets outstanding (RK860-0, and whichever buck-boost lands).
Open 🔲: the **≥4 A buck-boost**, the **LiPo cell**, and the two non-JLC radio items (coax pigtail + FPC
antenna). §11's handheld peripherals are named but not yet sourced.

**Buy early — the four no-alternate parts, ~$30 total, all with moving stock:**
`RK3568B2` **98** · `MT41K512M16` **333** · `RK809-5` **1,336** · `RK860-0` **4,587**.

**Next:** settle the three open power questions (above), then the netlist CSVs
([schematic-csv-spec.md](../../../cad/schematic-csv-spec.md)) — which are blocked on the
RK3568 symbol's 7-unit support and a hand-made JEDEC ×16 FBGA-96 DDR3L symbol.

**Board:** 6-layer (Rockchip's recommendation, DDR on L1/L6 over solid GND), 0.65 mm FCCSP → via-in-pad
(JLC POFV, **not** HDI), X-ray inspection, bake MSL3 parts. Run **DDR3-1066** first.
Routing rules come straight from `cad/docs/RK3568-hardware-design-guide-EN-V1.2.pdf` §2.1.7 (topology, VREF/ZQ, stackup §3.1) + `RK3568-high-speed-pcb-design-guide-V1.0.pdf` (impedance, length/skew tables) — **DDR3/DDR3L section**: 50 Ω SE / 100 Ω diff, intra-pair ≤12 mil, CS/CKE/ODT↔CK ≤30 mil, T-arm balance ≤20 mil, ~71 nets. ⚠️ Rockchip does **not** tabulate trace widths — re-solve them against JLC's actual 6-layer controlled-impedance stackup.
Ball-level pinout + the fixed DDR3 CA map: [PINOUT.md](PINOUT.md). Schematic block plan: [POWER-AND-BLOCKS.md](POWER-AND-BLOCKS.md).
