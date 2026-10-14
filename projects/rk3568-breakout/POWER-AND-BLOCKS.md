# POWER-AND-BLOCKS.md — power tree + schematic block plan

**Status:** 🔬 Pre-capture. The two artifacts to settle *before* schematic capture, derived from
[BOM.md](./BOM.md) and [PINOUT.md](./PINOUT.md), then adversarially reviewed against both (20 defects
found and fixed). It mostly *organises* what's already chosen — but it surfaced **4 items that are not
yet BOM rows** — all four are now rows; see the resolved section below.

---

## 1. Power tree

```
USB-C VBUS 5 V   ← the ONLY input, single cable  (≈12.3 W / ~2.5 A peak)
 │   ⚠️ peak needs a 3 A USB-C source (C-to-C charger, or a powered USB-C hub that passes data to the NUC).
 │      If a port folds back, that is a host-side fix — no DC jack, the connector already carries 3 A.
 │   ├─ 100 µF electrolytic ‖ [2.2 Ω + 2.2 µF series-RC to GND] ‖ 100 nF     [bulk + spike absorb]
 │   │
 │   └──► AO4407A P-FET high-side switch  ──► SYS_5V  (everything below)
 │          gate: 100 kΩ to source (default OFF) · 10 kΩ series · 100 nF G-S  [~1 ms soft-start ⇒ inrush ≈1 A]
 │          2N7002 NMOS inverts the control line: PWR_EN HIGH ⇒ board ON
 │          jumper G→GND = permanently ON (standalone bench use; remove for rig operation)
 │          ✅ soft-start is what makes USB-C power practical: USB 2.0 caps port bypass at 10 µF and we
 │             have ~200 µF — hard-switching that is exactly what trips a protected hub port
 │          ⚠️ no reverse-polarity protection (load-switch orientation can't also block reverse)
 │
 │  USB-C also carries data + VBUS sense:
 │   ├─ USB3_OTG0_DP/DM (maskrom, HS) · 2× 2.2 Ω · USBLC6-2SC6 · 2× ESD5451N (CC1/CC2)
 │   ├─ 5.1 kΩ CC pulldowns (Rd) — no CC sensing, so the SOURCE decides: C-to-C 3 A charger = in spec,
 │   │   A→C cable advertises only 500 mA, hub port is empirical
 │   └─ VBUS → 10 k/15 k divider → VBUSDET
 │
 ├──► RK860-0 buck ───────────────► VDD_CPU  0.8–1.15 V (DVFS, I²C 0x40, 6 A)
 │      EN ← 51 kΩ to vcc3v3_pmu (+100 nF footprint, DNP) ⇒ slot 2A, after vdd_logic
 │      VSEL ← PMIC_SLEEP · 0.47 µH (⚠️ ref uses 0.24 µH) · 2× 10 µF + 2× 22 µF in · 4× 22 µF out
 │      FB ← 100 Ω remote sense from the SoC's VDD_CPU ball + 100 nF
 │           ⚠️ open FB ⇒ overvoltage ⇒ dead SoC (abs max 1.25 V)
 │      └─ mandated ≥135 µF total: 66 µF at SoC + 88 µF at buck = **154 µF ✓** (+4.7 µF = 158.7)
 │
 ├──► SY8205FCC buck ─────────────► vcc3v3_sys 3.3 V, 5 A   ⚠️ NOT gated by anything the PMIC makes
 │      EN ← 100 kΩ to its own 5 V PVIN  ("pre-PMIC", before slot 1)   ✅ BOM row
 │      R1 100 k / R2 22.1 kΩ · 2.2 µH · C_BS 100 nF · C_VCC 1 µF · C_SS 10 nF ✅
 │      COUT 2× 22 µF + 10 µF + **100 µF** (>100 µF mandated)
 │      │
 │      └──► RK809-5 PMIC   (VCC1…VCC9 **and VCC_RTC** all from vcc3v3_sys)
 │             VDC ← 10 kΩ/3.3 kΩ off 5 V = 1.24 V  ⇒ **buttonless power-on**
 │             ⚠️ VCC9 doubles as UV sense: VB_OK 3.0 V ⇒ only 0.3 V margin on this rail
 │             │                                        (slot = burned-in OTP order, 2 ms apart)
 │             ├─ BUCK1  vdd_logic  0.9 V  2.5 A  slot 1  FB←100 Ω remote  [2×22+2×22+10 = 98 µF ✓]
 │             ├─ BUCK2  vdd_gpu    0.9 V  2.5 A  slot 2  FB←100 Ω remote  [98 µF ✓]
 │             │        (OTP default 0.9 V; OPP table reaches 1.0 V — the budget below uses 1.0 V)
 │             ├─ BUCK3  vcc_ddr  **1.35 V** 1.5 A slot 3  82.5 k/120 kΩ divider
 │             │        └─► DDRPHY_VDDQ (×8) + DDRPHY_VDDQL (×6) + both DRAM VDD/VDDQ
 │             ├─ BUCK4  vdd_npu   0.5 V*  1.5 A  **OFF**  FB←100 Ω remote  [98 µF ✓]
 │             │        (*OTP idle value; functional NPU needs ~0.9 V set by software)
 │             ├─ BUCK5  vcc_1v8   1.8 V   2.5 A  slot 2
 │             │        └─► header pin 7 only — **no SoC rail uses it** (every IO domain is 3.3 V)
 │             ├─ LDO1   vdda0v9_image 0.9 V 400 mA **OFF** ─bead─► DSI0 AVDD_0V9 (W16) ⚠️
 │             ├─ LDO2   vdda_0v9      0.9 V 400 mA slot 2
 │             │        ├─► SYSPLL_AVDD_0V9 (P11)
 │             │        └─bead─► USB3_AVDD_0V9 (P22)   ⚠️ required for maskrom
 │             ├─ LDO3   vdda0v9_pmu   0.9 V **100 mA** slot 2   ← tightest channel on the board
 │             │        ├─► PMU_VDD_LOGIC_0V9 (V19)
 │             │        └─► PMUPLL_AVDD_0V9 (V21)
 │             ├─ LDO4   vccio_acodec  3.3 V 400 mA **OFF**  — codec unused, channel free
 │             ├─ LDO5   vccio_sd      3.3 V 400 mA slot 4  ─► VCCIO3 (3.3 V only, no UHS)
 │             ├─ LDO6   vcc3v3_pmu    3.3 V 400 mA slot 2
 │             │        ├─► PMUIO1 (Y20, 3.3 V fixed) + PMUIO2 (W19)
 │             │        └─► RESETn/nPOR 10 kΩ, PMIC_INT 10 kΩ, RK860 EN 51 kΩ, SY6280 EN 4.7 kΩ
 │             │            (⚠️ touch I²C pull-ups go to the **touch 3.3 V node**, not here — open item)
 │             ├─ LDO7   vcca_1v8      1.8 V 400 mA slot 2
 │             │        ├─► SARADC_AVDD_1V8 (H22) + SARADC_VIN0 10 kΩ pull-up
 │             │        ├─► SYSPLL_AVDD_1V8 (N10) · OTP_VCC18 (H20)
 │             │        └─bead─► USB3_AVDD_1V8 (P23)  ⚠️ required for maskrom
 │             ├─ LDO8   vcca1v8_pmu   1.8 V 400 mA slot 2 ─► PMUIO0 (= PMUPLL_AVDD_1V8, Y21, fixed)
 │             ├─ LDO9   vcca1v8_image **1.8 V** 400 mA **OFF** ─bead─► DSI0 AVDD_1V8 (Y17) ⚠️
 │             ├─ SW1    vcc_3v3       3.3 V **2.1 A** slot 4   ← the main 3.3 V IO rail
 │             │        ├─► VCCIO1, VCCIO2 (+strap LOW), VCCIO4, VCCIO5, VCCIO6, VCCIO7
 │             │        ├─► USB3_AVDD_3V3 (M23)      ⚠️ required for maskrom
 │             │        ├─► SY6280 #1 ─► NOR VCC                 (gated — rescue)
 │             │        ├─► SY6280 #3 ─► panel VCI + IOVCC       (gated, **default OFF** — mandated)
 │             │        ├─► touch VDDT (ungated) + the touch I²C pull-up rail
 │             │        └─► header +3V3
 │             └─ SW2    vcc3v3_sd     3.3 V 2.1 A slot 4
 │                      └─► SY6280 #2 ─► microSD VDD             (gated — rescue)
 │
 └──► TPS61165 boost ─────────────► VLED+ 19.6–23.8 V @ 40 mA    [panel backlight]
        CTRL ← PWM4 (GPIO0_C3, reset pull-down) + **100 kΩ pull-down**
        10 µH · SS16 · 2.2 µF/50 V · Rset 5.1 Ω · 220 nF COMP · LED1−+LED2− tied (2 strings × 7)

RESETn net (wired-OR, open-drain): nPOR (AH27) · RK809 RESETB · TSADC_SHUT (0 Ω link) · header tap
        10 kΩ pull-up → **vcc3v3_pmu** (must match PMUIO1) · 100 nF at nPOR · 100 nF at RESETB
        ⚠️ must release **≥10 ms** after the last rail — measure on board #1
```

### Rail budget on `vcc3v3_sys` (the one rail that carries almost everything)

| Downstream | Load | Draw on 3.3 V |
|---|---|---|
| vdd_logic (BUCK1 ~0.9 V) | ~1.5 A | ~0.45 A |
| vdd_gpu (BUCK2, 1.0 V worst-case OPP) | ~1.5 A | ~0.50 A |
| vcc_ddr (BUCK3 1.35 V, 2× DDR3L) | ~1.0 A | ~0.45 A |
| vcc_1v8 (BUCK5) — ⚠️ **no SoC load; header pin only.** Budgeted as margin | ~0.3 A | ~0.18 A |
| LDO1–9 (pass-through) | ~0.3 A | ~0.30 A |
| vcc_3v3 (SW1: IO domains, NOR, panel, pull-ups) — *average*; **peak ≈0.6 A** with the SD card | ~0.3 A | ~0.30 A |
| vdd_npu (BUCK4) — NPU unused | ~0 | ~0 |
| **Total** | | **≈ 2.2 A** → 5 A part ✓ |

**Not on this rail:** VDD_CPU (own buck off 5 V) and the backlight boost (5 V).
**Input (corrected for conversion loss — the three terms are *output* powers):**
7.25/0.90 + 3.0/0.90 + 0.95/0.85 ≈ **12.3 W ⇒ ~2.5 A at 5 V** = 83 % of a 15 W supply.

### Rails that must be powered regardless (HDG Table 2-11) — **every one is sourced in the tree above**

`PMUPLL_AVDD_0V9/1V8` (V21/Y21) · `SYSPLL_AVDD_0V9/1V8` (P11/N10) · `PMU_VDD_LOGIC_0V9` (V19) ·
**`PMUIO1`** (Y20, 3.3 V fixed) · `PMUIO2` (W19) · **`VCCIO2`** (H18) · **`VCCIO3`** (L22) ·
`SARADC_AVDD_1V8` (H22) · `OTP_VCC18` (H20) · **all three `USB3_AVDD`** (P22/P23/M23 — needed for
maskrom even though USB3 is unused) · `MIPI_DSI_TX0_AVDD_0V9/1V8` (W16/Y17) · DDRPHY_VDDQ + VDDQL
(**both 1.35 V**).

### Four sequencing rules (the only hard ones — see BOM.md for sources)

1. **No inter-module order is mandated** (*"no timing requirement between different modules"*), so the
   three-source topology is fine. Recommended order: `PMU_0V9 → VDD_LOGIC → {CPU, GPU, NPU}`, 3.3 V IO last.
2. **RESETn released ≥10 ms after the last rail.** ⚠️ unverified — measure.
3. **Power-down: RESETn low BEFORE any rail drops.** PMIC does this; on a droop the lead is only 60 µs.
4. **VCC_RTC must be the highest / first PMIC supply** — satisfied by sharing `vcc3v3_sys`.

### Open power items

- ⚠️ **Discharge time.** The rescue loop is "both media off → power-cycle → maskrom", and bulk is now
  ~100 µF (input) + ~100 µF (3V3). **Measure decay; make the power-cycle off-time exceed it**, else add a
  bleeder (PMIC R13).
- ⚠️ **LDO1 + LDO9 are OFF at reset** and feed the DSI0 PHY ⇒ **panel is dark until software enables them.**
- ⚠️ **RK860-0 inductor** may want 0.24 µH (reference) rather than our shared 0.47 µH — pending its datasheet.

---

## 2. Schematic block plan (`subcircuit` values for the CSV flow)

One page per block. These are the literal `subcircuit` strings for `bom.csv`; a net crossing blocks
becomes a global label, a net inside one stays local — so **block boundaries are the net-naming plan too.**

| `subcircuit` | Contents | ~Parts | Notes |
|---|---|---|---|
| `soc-core` | RK3568B2, core/analog power balls + their 17× 0201 / 1 µF / 4.7 µF / 10 µF / 22 µF decoupling, PLL rails, OTP_VCC18, TVSS, **the 4 analog ferrite beads (2× USB3_AVDD + 2× DSI0_AVDD) with their 1 µF source-side + 100 nF at-ball pairs** | ~50 | Biggest page. Power balls only — no signals. The beads were in no block before review. |
| `soc-io` | the VCCIOn / PMUIOn domain balls + their 10× 0201 caps, FLASH_VOL_SEL strap (1 kΩ), **the whole RESETn network** (nPOR 100 nF, **+1 µF slow-release cap fitted DNP** for the ≥10 ms reset-release rule, 10 kΩ pull-up to `vcc3v3_pmu`, 0 Ω TSADC_SHUT link, PMIC_INT 10 kΩ) | ~14 | Keeps the domain-voltage story on one sheet, which is where mistakes happen. |
| `ddr` | 2× MT41K512M16 + 7×100 nF + 2×10 µF each, VREFCA divider, **all 8× 1 nF** (VREFDQ/VREFCA/RESET# per chip + 2 on DDRPHY_VREFOUT: one to VCC_DDR, one to GND), ZQ 240 Ω, DDR_RZQ 120 Ω, **CLKP/N RC (2× 49.9 Ω + 100 pF)** | 36 | ✅ **No longer blocked.** Byte lanes + DQ-within-byte are free to permute, **CA is fixed** — map in [PINOUT.md §5](./PINOUT.md). T-topology + RC at the branch. |
| `pmic` | RK809-5, VCC1–9 input caps, all buck/LDO output caps + 5× 0.47 µH, VDC divider, PWRON/RESETB caps, I²C0 22 Ω + 2.2 kΩ pull-ups, BUCK3 divider, **32 kHz crystal + its 2× 22 pF and a grounded pad/clip under the metal can** (*Mandatory*, PMIC R57), VCC_SPK_HP bead + 10 µF | ~58 | |
| `cpu-buck` | RK860-0, 0.47 µH, **2× 10 µF + 2× 22 µF in, 4× 22 µF out**, EN 51 kΩ + 100 nF (DNP), 100 Ω remote FB + its own 100 nF | ~14 | Separate page: it's the one rail whose feedback error destroys the SoC. |
| `sys-buck` | SY8205FCC, 2.2 µH, FB divider, COUT incl. 100 µF, C_BS 100 nF, C_VCC 1 µF, **C_SS 10 nF** (`CC0402KRX7R9BB103`, C60133), **EN 100 kΩ pull-up to its own 5 V PVIN** (shares the backlight 100 kΩ reel) | ~11 | ✅ both now BOM rows. ⚠️ No SY8205 datasheet in `cad/docs` — re-derive t_SS and confirm EN abs-max ≥ 5 V when it lands. |
| `power-in` | USB-C (**sole power + data**), **AO4407A P-FET + 2N7002 + gate network (100 kΩ / 10 kΩ / 10 kΩ / 100 nF) + always-ON jumper**, CC pulldowns, USBLC6, CC TVS, 100 µF input bulk, RC snubber, VBUSDET divider + 100 nF, DP/DM 2.2 Ω | ~21 | The P-FET **is** the remote power-cycle; its soft-start is what lets a USB port swallow ~200 µF of bulk. |
| `boot-flash` | W25Q128JVSIQ, CS 4.7 kΩ, D2/D3 10 kΩ **to the gated rail**, FSPI_CLK 22 Ω, 100 nF, maskrom test pad | ~10 | |
| `microsd` | socket, 5× 22 Ω at the card, CLK 22 Ω, DET 100 Ω, 2× SP3004 ESD, pull-ups, VDD caps | ~18 | |
| `rescue` | 3× SY6280 + 3× ISET 6.8 kΩ, **EN: 4.7 kΩ pull-ups to `vcc3v3_pmu` (omit on #3) + 1 kΩ series from each SoC GPIO**, panel-switch downstream bulk, **6-pin JST-XH boot-mode connector (keyed) — `PWR_EN`, `NOR_EN`, `SD_EN`, `RESETn`, `RECOVERY`(=`SARADC_VIN0`, pull-to-GND only), GND** + their 100 Ω/TVS + the `SARADC_VIN0` 1 nF | ~26 | All three switches on one page, and now all host control too. ⛔ `PWRON`/`RESETn`/`SARADC_VIN0` moved here from `headers` — one tap, one driver. |
| `display` | 40-pin + 6-pin FPC connectors, **touch 2.0 kΩ pull-ups to the touch 3.3 V node** (not `vcc3v3_pmu`), 6× 22 Ω + 6× TVS, panel rail caps, 🔲 DSI common-mode-choke footprints bridged by 0 Ω (5 pairs — footprint-only per LCM&TP R13) | ~25 | |
| `backlight` | TPS61165, 10 µH, SS16, Cout/Cin, Rset, COMP, **CTRL 100 kΩ pull-down** | ~8 | |
| `console` | 1×3 header, UART2 2× 100 Ω, 2× TVS, power LED + 510 Ω | ~8 | |
| `headers` | 2× 1×25 (⛔ control taps moved to `rescue`), **18 × 100 Ω series R on the bused/GPIO group** ✅ (now folded into the 100 Ω row), 🔲 SP3004 arrays (BOM files these under "reserve, no parts") | ~20 | |
| `clock` | 24 MHz crystal (12 pF CL), 2× 18 pF, 22 Ω, 1 MΩ | ~5 | |


### ✅ The four missing parts — now BOM rows

1. **SY8205 `C_SS` soft-start cap** — added as a new row: **10 nF 50 V X7R 0402, `CC0402KRX7R9BB103`
   (C60133)**, the only 10 nF on the board. ⚠️ Value carried from the BOM's own SY8205 row, **not** a
   local datasheet — there is still no SY8205 datasheet in `cad/docs`, so re-derive t_SS against the
   final ~154 µF COUT when it arrives.
2. **SY8205 EN pull-up** — folded into the existing **100 kΩ** row (`0402WGF1003TCE`, C25741), quantity
   1 → 2, with the pull going to the buck's **own 5 V PVIN**, not EXT_EN. ⚠️ Confirm `EN` abs-max ≥ 5 V
   from the datasheet.
3. **Header series resistors** — folded into the existing **100 Ω** row (C25076), quantity 10 → **28**,
   allocating **18** to the 2×25 header's bused/GPIO group (spare I²C 2, SPI0 4, spare UART 2, PWM 2,
   8 plain GPIO). ⛔ **Correction to this document's earlier claim:** GPIO R3 is *"it is **recommended**
   to connect a resistor … between 22 Ω-100 Ω … and reserve TVS devices"* — **recommended, not
   Mandatory**, and it specifies a *range*. 100 Ω chosen over 22 Ω for maximum surge impedance, since
   these are low-speed insurance pins. PWRON/RESETn/SARADC_VIN0/UART2 already had their own 100 Ω.
4. **Snubber cap** — folded into the **2.2 µF 50 V** row (`CC0805KKX7R9BB225`, C125847), quantity
   1 → 2. That row already said "reuse the backlight reel"; only the count was missing.

### ✅ Both carried-forward items — now closed

**`USB2_AVDD_*` → tie R8/P9/P10 to VSS; the 0201 domain-cap count drops 27 → 24.** Settled by the ball
*namespace*, not by guesswork: OTG0's high-speed pins are **`USB3_OTG0_DP/DM`, annotated `_HS/FS/LS`** in
the pinout sheet, so OTG0's HS path is powered by `USB3_AVDD_*` (which stays powered — mandatory for
maskrom). The only USB2-namespace signals are `USB2_HOST2/3_DP/DM`, unwired here, and Rockchip's
unused-pins workbook Part V *"USB2.0 HOST2/USB2.0 HOST3"* lists **R8/P9/P10 = `Tie VSS`** verbatim.
Mainline already defaults `usb_host1_ehci`/`ohci` to `status = "disabled"`, so nothing probes the dead
PHY. ⛔ *The tree above assumed the shared-net branch — that assumption is now withdrawn.* Note
`USB3_AVDD_*` still needs its three mandatory 100 nF, budgeted as 0402s in the BOM's USB section; the
checklist wants them "on the back of the corresponding power pins", so layout must confirm a 0402 fits
beside P22/P23/M23 or swap them to 0201.

**Panel-switch enable GPIO → `GPIO0_C5`, ball `AC21`.** The pinout sheet gives its reset pull as **down**
(16 K–43 K, typ 26 K) with reset direction **input**, so an active-high SY6280 EN is **off through
power-on** — which is what LCM&TP R8 requires. Rockchip's own pin-usage column for AC21 reads
*"LCD1 panel power enable, active high"*, so this is the vendor-intended pin. `GPIO0_C6` (AD20) is the
equally-valid alternate (also pull-down; Rockchip earmarks it "4G module power enable"). ⛔ This EN
**must get no pull-up** — the default-OFF behaviour depends entirely on the internal pull-down.

**Why these boundaries.** Three principles: (1) a regulator lives with its own inductor, divider and
caps, so each page can be checked against its datasheet alone; (2) the two pages where an error is
*destructive* — `cpu-buck` (open feedback ⇒ overvoltage) and `soc-io` (domain-voltage mismatch ⇒ IO
damage) — are deliberately small and isolated; (3) `rescue` keeps all three load switches together
because they are one mechanism, even though one of them powers the display.

### Capture order (lowest-risk first)

1. `clock`, `console`, `backlight`, `power-in` — small, self-contained, exercise the CSV→KiCad flow
2. `sys-buck`, `cpu-buck`, `pmic` — the power tree, checkable against §1 above
3. `soc-core`, `soc-io` — the big ball-count pages
4. `ddr` — ✅ unblocked: CA map is fixed and tabulated ([PINOUT.md §5](./PINOUT.md)); byte lanes are free
5. `boot-flash`, `microsd`, `rescue`, `display`, `headers`

Validate after each block (`generate_schematic.py --subcircuit <name>`) rather than at the end.
