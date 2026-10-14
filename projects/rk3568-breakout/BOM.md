# RK3568 Breakout PCB — Bill of Materials

**Status:** 🔬 Planning. Nothing built or ordered. Minimal board: **SoC + 2 GB DDR3L + power + boot
storage + console + the DSI panel connector**, plus a small insurance header. Same role as [t113-breakout](../t113-breakout/BOM.md) — prove the
hard core (SoC + DDR3 + PMIC + boot) before the handheld board. Software bring-up can start earlier on a
Radxa ROCK 3A (same SoC, and the power tree below is copied from it).

**Goal:** validate the chip **boots** and the **GPU draws to the gameboy-v4 panel**. Nothing else.

**Scope — deliberately omitted:** buttons, LEDs, battery/charger, audio (RK809's codec left unused),
Wi-Fi/BT, Ethernet, HDMI, USB3/PCIe/SATA, camera, eMMC. Unused high-speed PHY pins (HDMI, USB3/PCIe/SATA
combo PHYs, DSI1) stay unconnected/terminated per the datasheet.

**Src legend:** ✅ = user has seen the live JLC page (this session, or verified for t113-breakout/gb3) ·
⚠️ = my check only / confirm on the live page · 🔲 = part not chosen yet.

## 1. SoC

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 1 | SoC | **RK3568B2** | Quad A55, Mali-G52, FCCSP-636L 0.65 mm (no-HDI). X-ray req, MSL 3 | 1 | C3345225 | ✅ |
| 2 | Decoupling | **CC0201KRX5R6BB104** | 100 nF 10 V X5R **0201** — **one per power *domain*** (HDG §2.1.8), **24 exact** for our interface set (enumerated in the note below; was 27 — ✅ the `USB2_AVDD_*` question is resolved below and drops 3). **0201, not 0402**: these sit in the via field *under* the BGA (checklist: *"must be placed on the back of the corresponding power pins"*) and 0402 won't fit between 0.65 mm vias | 24 | C190183 | ⚠️ |
| 3 | Decoupling | **CL05A475KP5NRNC** | 4.7 µF 10 V X5R **0402** (798 k stock) — **8 exact**: VDD_CPU 1 · VDD_LOGIC 1 · VDD_GPU 1 · DDRPHY 2 · SYSPLL_0V9 1 · SARADC/SYSPLL_1V8 1 · RK809 **LDO4** output 1 | 8 | C368809 | ⚠️ |
| 4 | Decoupling | CL05A105KA5NQNC | 1 µF 25 V X5R **0402** (LCSC **Basic**) — SoC side, **6 exact**: VCCIO6 1 · PMU_VDD_LOGIC_0V9 + PMUPLL_AVDD_0V9 2 · PMUPLL_AVDD_1V8 1 · SYSPLL_0V9 1 · SARADC/SYSPLL_1V8 1 · **+1 `RESETn`→GND slow-release cap, fitted as DNP** (see the reset-timing note) | 7 | C52923 | ⚠️ |
| 5 | Decoupling | CL21A226MAQNNNE | **22 µF** — SoC-side bulk meeting Rockchip's **mandatory per-rail totals**: VDD_CPU **3**, VDD_LOGIC **2**, VDD_GPU **2**, VDD_NPU **2**. ⚠️ **Not optional** — HDG requires **VDD_CPU > 135 µF** and **> 90 µF** each on LOGIC/GPU/NPU. Place the VDD_CPU caps on the **back of the die**, as close as possible | 9 | C45783 | ✅ |
| 6 | Decoupling | CL21A106KOQNNNE | 10 µF 0805 — SoC-side bulk, **one per core rail**: VDD_LOGIC, VDD_GPU, VDD_NPU, DDRPHY *(Rockchip uses 0603)*. ⛔ **Was duplicated as two identical qty-4 rows — merged.** These 4 are what lift LOGIC/GPU/NPU over the mandated 90 µF: 2× 22 µF at the SoC + 2× 22 µF at the buck = 88 µF **fails**; **+10 µF = 98 µF ✓** | 4 | C1713 | ✅ |

## 2. DRAM — 2 GB DDR3L (support network per [RK3568-DDR3-ROUTING-STUDY.md](../gameboy-v4/pcb/RK3568-DDR3-ROUTING-STUDY.md))

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 7 | DRAM | **MT41K512M16VRP-107 IT:P** | 8 Gbit ×16 DDR3L (1.35 V), FBGA-96. 2 chips = single-rank 32-bit = **2 GB** | 2 | C2831669 | ✅ |
| 8 | DDR | 0603WAF1001T5E | 1 kΩ 1% — VREFCA divider = VDDQ/2. 0603 (sits beside the DRAM, not under the BGA); reused from gb3 | 2 | C21190 | ✅ |
| 9 | DDR | 0402B102K500NT | 1 nF 50 V X7R 0402 (LCSC **Basic**) — **9 total: VREFDQ + VREFCA + RESET# per chip (6), plus 2 on `DDRPHY_VREFOUT`, plus ⚠️ 1 on `SARADC_VIN0` (NEW, *Mandatory* — SARADC checklist: "if used, the 1 nF capacitor must not be deleted"; place it hard against the ball).** HDG: the VREFOUT caps *"cannot be deleted or modified randomly; the VREF pins of DDR must be with a 1nF decoupling capacitor"*. ⚠️ **Rockchip specifies 1 nF, not 100 nF** (Micron's 0.1 µF VREF figure is from a DDR1-era app note) | 9 | C1523 | ⚠️ |
| 10 | DDR | **0402WGF499JTCE** | **49.9 Ω ±1 %** ×2 (LCSC **Basic**) — ⚠️ **DDR3 CLKP/N AC termination, Mandatory and was missing.** DRAM checklist R13/R43: *"Using T topology requires an additional **RC circuit between DDR3_CLKP/N** to improve signal quality and reduce EMI"* and *"must be at the branch point."* Circuit: CLKP–[49.9 Ω]–•–[100 pF]–GND–•–[49.9 Ω]–CLKN. Place at the T branch | 2 | C25120 | ⚠️ |
| 11 | DDR | **0402CG101J500NT** | **100 pF C0G 50 V 0402** (LCSC **Basic**) — centre cap of the CLKP/N RC above. Our other C0G reels are 18/22 pF — neither fits | 1 | C1546 | ⚠️ |
| 12 | DDR | 0603WAF2400T5E | 240 Ω 1% — DRAM ZQ. **One per *package*, not per die**: `MT41K512M16VRP` is a **TwinDie** (two 4 Gbit ×8 die in one package) and only **one external ZQ ball** is bonded out → 2 chips = 2 resistors | 2 | C23350 | ✅ |
| 13 | DDR | 0402WGF1200TCE | 120 Ω ±1% 0402 (LCSC **Basic**) — SoC `DDR_RZQ` → GND | 1 | C25079 | ⚠️ |
| 14 | Decoupling | CL05B104KB54PNC | 100 nF 50 V X7R **0402** — **7 per DRAM chip = 14** (Rockchip's DDR3 reference fits exactly 7 per device; 0402 is fine here — beside the chip, not under a BGA). ⚠️ **Micron explicitly rejects a cap-per-ball rule** (TN-41-13: *"It is not necessary to allocate a capacitor for every pin pair"*) and sets a floor of **≥4 low-ESL caps per device** (TN-46-14) — Rockchip's 7 clears it. Place at the package corners/edges with vias on the device side | 14 | C307331 | ⚠️ |
| 15 | Decoupling | CL21A106KOQNNNE | 10 µF 0805 — **2 per DRAM chip = 4** (Rockchip fits 2× 10 µF per device alongside the 7× 100 nF). **No 1 µF on the DRAM** — the reference fits none | 4 | C1713 | ✅ |
| 16 | DDR | *(see note)* | `vcc_ddr` bulk is covered by the per-chip 10 µF above + the RK809 BUCK3 output caps — no separate part | — | — | ✅ |

## 3. Power — PMIC + regulators

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 17 | PMIC | **RK809-5** | RK3568 companion PMU: 5 bucks + 9 LDOs + 2 switches + RTC (codec unused). **No charger** — right for a no-battery board. I²C0 @ **0x20 fixed** (no address strap). ✅ **Die question resolved: this code really is the `-5`** — `-5` and `-5A` are *separate orderable devices* with distinct top markings (datasheet §2.2/2.3), LCSC catalogues this as model `RK809-5` with its own `-5` datasheet, and no `-5A` is listed at all. ⚠️ **stock is moving: 1344 (was ~2471 a few turns ago)** | 1 | C2939628 | ⚠️ |
| 18 | Power | **RK860-0** | VDD_CPU buck: 5 V in, 0.7125–1.5 V @ 12.5 mV, **6 A**, I²C **0x40**, WLCSP-20 0.4 mm. Mainline `rockchip,rk8600`. ⛔ **Not the ROCK 3A's TCS4525 — that part is dead on LCSC** (0 stock, 4526 delisted). Dual-source **SYR827PKC (C111839, ~40 % cheaper)** drops into the *same footprint, same address, same DTS* | 1 | C19188628 | ⚠️ |
| 19 | Power | **SY8205FCC** | `vcc3v3_sys` 5 V → 3.3 V sync buck, **5 A**, 500 kHz, **SOIC-8-EP (θJA 30–36 °C/W)**. Feeds all nine RK809 VCC inputs + VCC_RTC. FB: **R1 100 kΩ / R2 22.1 kΩ**; COUT **≥47 µF**; C_BS 100 nF; C_VCC 1 µF; C_SS 10 nF. ⚠️ **EN must be pulled to its own 5 V input, NOT to EXT_EN** (see power-on note) | 1 | C111875 | ⚠️ |
| 20 | Power | **MWSA0603S-2R2MT** | 2.2 µH 10 A/9.5 A, 15 mΩ — inductor for the SY8205 3.3 V buck (datasheet wants <10 mΩ DCR; this clears it) | 1 | C408445 | ⚠️ |
| 21 | Power | **MWSA0503S-R47MT** | **0.47 µH** 11 A/15 A, 8.5 mΩ — inductor for **all six** remaining bucks: RK860-0 CPU buck + the 5 RK809 bucks. Rockchip's HDG specifies *one value for every RK809 buck*: **0.47 µH, Isat ≥3.5 A, DCR ≤20 mΩ**, and 0.47 µH also suits RK860-0/SYR827/TCS4525 → **one part number ×6, survives any regulator swap**. Rockchip's own boards use a **2520 (2.5×2.0 mm)** package; this part is 5.4×5.2 mm — electrically better (8.5 mΩ) but ~6× the area, so switch to a 2520 0.47 µH if the layout gets tight | 6 | C408353 | ⚠️ |
| 22 | Power | **0402WGF8252TCE** | **82.5 kΩ ±1 %** — BUCK3 FB top divider. **Sets `vcc_ddr` = 1.3500 V exactly for DDR3L** per Rockchip's table (RH = (V−0.8)·RL/0.8, RL = 120 kΩ). E96 82.5 kΩ is exact; plain 82 kΩ (**C4142**) gives 1.3467 V and is also fine. ⛔ **Do NOT copy ROCK 3A's 47 kΩ** — that's 1.11 V for its LPDDR4 and would under-volt our DDR3L | 1 | C11717 | ⚠️ |
| 23 | Power | **0402WGF1203TCE** | **120 kΩ ±1 %** — BUCK3 FB bottom divider (the fixed leg in Rockchip's table). JLC **Preferred**, 550 k stock | 1 | C25750 | ⚠️ |
| 24 | Power | **0402WGF1000TCE** | **100 Ω ±1 %** (LCSC **Basic**) — (a) **remote-sense feedback ×4** (BUCK1/2/4 + CPU rail). **Mandatory, HDG §3.2.2.4** — an open feedback node **overvolts the rail and destroys the SoC** (VDD_CPU abs max 1.25 V). (b) **UART2 TX ×1** (SoC→adapter; ⛔ **RX moved to 1 kΩ** — see the console back-powering note). (c) **SARADC_VIN0 series ×1**. (d) ⚠️ **SDMMC0_DET ×1** (Micro-SD R2). (e) ⚠️ **PWRON + RESETn header taps ×2** — Clock&Reset R24 mandates *button → TVS → 100 Ω → cap → SoC*. (f) ⚠️ **2×25-header bused/GPIO group ×18** — spare I²C (2), SPI0 (4), spare UART (2), PWM (2), 8 plain GPIO. GPIO R3 (*recommended*, not Mandatory): *"when the board-to-board connection is realized through the connector … connect a resistor between 22 Ω-100 Ω … and reserve TVS devices."* 100 Ω chosen over 22 Ω for maximum surge impedance — these are low-speed insurance pins | 27 | C25076 | ⚠️ |
| 25 | Power | **0402WGJ0220TCE** | **22 Ω ±5 %** — series resistors, *all Mandatory*: **SCL + SDA** at the PMIC (2) · **XOUT24M** (1) · **FSPI_CLK** ≤300 mil from the SoC (1) · **SDMMC0_CLK** ≤400 mil (1) · ⚠️ **SDMMC0 CMD + D0–D3 at the *card* end** (5 — Micro-SD R4, was missing) · ⚠️ **touch SDA/SCL/RST/INT and panel RESET#/TE at the FPC** (6 — LCM&TP R12, was missing) | 16 | C25152 | ⚠️ |
| 26 | Power | **0402WGF1002TCE** | **10 kΩ ±1 %** (LCSC **Basic**, 22 M stock) — **RESETn/nPOR pull-up to `vcc3v3_pmu`** (⚠️ **must** match the PMUIO1 domain the nPOR pin sits in — hard rule, not `vcc3v3_sys`) + **PMIC_INT pull-up** (open-drain) + the 10 kΩ leg of the VDC divider. Reset net is the wired-OR of nPOR, RK809 RESETB and TSADC_SHUT | 3 | C25744 | ⚠️ |
| 27 | Power | **0402WGF3301TCE** | **3.3 kΩ ±1 %** (LCSC **Basic**) — bottom leg of the **VDC start-up divider**; with the 10 kΩ top leg off the always-on 5 V this puts VDC (pin 61) at **1.24 V**, right on Rockchip's ~1.2 V recommendation. **This divider is the buttonless power-on mechanism** (see note) | 1 | C25890 | ⚠️ |
| 28 | Power | **0402WGF5102TCE** | **51 kΩ ±1 % — RK860-0 EN pull-up to `vcc3v3_pmu`** (RK809 LDO6, sequence slot 2). This *is* the VDD_CPU sequencing scheme — **all three Rockchip reference designs use exactly this value to this rail**, which is what puts VDD_CPU at "slot 2A", after `vdd_logic`. Pair with a 100 nF EN→GND footprint (populate for a ~5 ms ramp as ROCK 3A does, else DNP) | 1 | C25794 | ⚠️ |
| 29 | Power | **0402WGF1001TCE** | **1 kΩ ±1 %** (LCSC **Basic**) — ⚠️ **`FLASH_VOL_SEL` (ball AG25) pull-DOWN; brick-class strap.** Internal pull-**up** (16–43 kΩ) defaults it **HIGH = 1.8 V**, but our NOR is **3.3 V**. HDG: a mismatch means *"the function will be abnormal or the chip may even be damaged"*, and **mainline's io-domain driver no-ops `vccio2`, so there is no software rescue.** Sizing: worst-case 16 kΩ pull-up vs V_IL = 0.8 V ⇒ R ≤ 5.1 kΩ. **1 kΩ → 0.19 V (>4× margin); ⛔ 10 kΩ → 1.27 V FAILS.** Also used for the 1 kΩ series from each SoC GPIO into the SY6280 EN nets (2), plus ⛔ **UART2 RX ×1 — raised from 100 Ω to stop the console adapter back-powering the board** (see the note) | 4 | C11702 | ⚠️ |
| 30 | Storage | **0402WGF4701TCE** | **4.7 kΩ ±1 %** (LCSC **Basic**) — (a) `FLASH_CS0n` pull-up to the **gated NOR rail** (*"must be reserved"*). (b) ⛔ **NOR_EN / SD_EN pull-ups — 4.7 kΩ, not 10 kΩ**: those SoC GPIOs reset with a 16–43 kΩ internal pull-**down**, and 10 kΩ only reaches **2.03 V** worst case vs 2.55 V for 4.7 kΩ | 3 | C25900 | ⚠️ |
| 31 | Power | **BLM18PG121SN1D** | **120 Ω @100 MHz ferrite bead**, 2 A, 50 mΩ, 0603 (LCSC **Basic**, 1.4 M stock) — **×4, all Mandatory**: isolate **`USB3_AVDD_0V9` + `_1V8`** from VDDA_0V9/VCCA_1V8 (2), and **`MIPI_DSI_TX0_AVDD_0V9` + `_1V8`** (2, *"must be reserved"*) · ⚠️ **RK809 pin 33 `VCC_SPK_HP` from 5 V (1)** — PMIC&Power R25 says that pin's circuit *"must be reserved"* even with the codec unused (I had wrongly concluded "leave unconnected"); reference fits bead + 10 µF. Each bead needs 1 µF source-side + 100 nF at the ball | 5 | C14709 | ⚠️ |
| 32 | Display | **ESD5451N-MS** | Bidirectional 0.4 pF-class ESD diode, 0402 — ⚠️ ***Mandatory* TVS, ×13, was entirely missing.** LCM&TP R12 (touch SDA/SCL/RST/INT + panel RESET#/TE = 6) · Debug-UART R2 (UART2 RX/TX = 2) · Clock&Reset R24 + SARADC R13 (PWRON, RESETn, SARADC_VIN0 = 3) · USB2 R10 (USB-C **CC1/CC2** = 2 — the USBLC6 only covers DP/DM+VBUS). Topology is always **connector → TVS → series R → SoC**, TVS closest to the connector | 13 | C2802295 | ⚠️ |
| 33 | Power | **0402WGF1003TCE** | **100 kΩ** (LCSC **Basic**) — **two uses.** (a) ⚠️ **external pull-down on the TPS61165 CTRL pin, Mandatory.** LCM&TP R3: *"select the internal pull-down GPIO, and **connect the pull-down resistor externally** to avoid the screen flicker phenomenon during power-on."* PWM4's internal pull-down alone isn't enough before the IO domain is live — a floating CTRL can kick the 23.8 V rail. (b) **SY8205 `EN` pull-up to its own 5 V PVIN** — the buck must self-enable from 5 V, *not* from the PMIC's EXT_EN (see the power-on note). ⚠️ Confirm `EN` abs-max ≥ 5 V when the SY8205 datasheet lands — it is **not** in `cad/docs` | 2 | C25741 | ⚠️ |
| 34 | Storage | **0402WGF1002TCE** | **10 kΩ ±1 %** ×2 — pull-ups on **FSPI_D2 (NOR /WP)** and **FSPI_D3 (NOR /HOLD)**; checklist: *"the resistors connected to FSPI_D2 and FSPI_D3 must be reserved"*. FSPI_D2 resets pull-**down**, so /WP comes up asserted without this. ⚠️ **These must go to the GATED NOR rail, not VCCIO2** — a deviation from Rockchip's reference (where the nets are the same); tied to VCCIO2 they'd inject current into a deliberately-dead flash and partly defeat the rescue gate. Same reel as the 10 kΩ row | 2 | C25744 | ⚠️ |
| 35 | Power | **0402WGF2201TCE** | **2.2 kΩ ±1 %** ×2 — **I²C0 pull-ups to `vcc3v3_pmu`**. The ball description says *"need external pull-up"*; Rockchip's reference fits 2.2 kΩ. Without these the PMIC + CPU-buck bus doesn't work at all | 2 | C25879 | ⚠️ |
| 36 | Power | **0402WGJ0000TCE** | **0 Ω** — `TSADC_SHUT_M0` into the RESETn net (the over-temp path; Rockchip fits a 0 Ω link here so it can be lifted for debug) | 1 | C21376 | ⚠️ |
| 37 | Power | **CC0402KRX7R9BB103** | **10 nF 50 V X7R** 0402 — **SY8205 `C_SS` soft-start**, the only 10 nF on the board. Without it SS is unprogrammed and the buck ramps at its internal default into ~154 µF of `vcc3v3_sys` bulk. ⚠️ Value is from this BOM's own SY8205 row, **not a local datasheet** (none in `cad/docs`) — re-derive t_SS against the final COUT when it lands | 1 | C60133 | ⚠️ |
| 38 | Decoupling | CL21A106KOQNNNE | 10 µF 16 V X5R 0805 — **exact count**. The RK809 VCC inputs are **not uniform** (Rockchip ref): **VCC3, VCC4, VCC9 = 10 µF each** (3) · **BUCK3 output** 1×10 µF (1) · **SWOUT1** (1) · SY8205 CIN (1) · RK860-0 CIN 2× (2) = **8**. *(VCC1 takes a 22 µF — see that row — and VCC2 shares it; VCC5/6/7/8 take the 1 µF row)* | 8 | C1713 | ✅ |
| 39 | Decoupling | CL21A226MAQNNNE | 22 µF X5R 0805 — **exact count**: RK809 **BUCK1/2/4/5 = 2× each** (8) + **BUCK3 = 1×** (1, its second cap is a 10 µF from this row) · **RK860-0** 4× out + 2× in (6) · **SY8205** COUT 2× (2, with a 10 µF → 54 µF, clears its ≥47 µF) = **17**. 2×22 µF clears every HDG buck minimum incl. BUCK5's 33 µF. *Rockchip fits 22 µF **6.3 V 0603**; this 25 V 0805 derates far less, at the cost of area* | 17 | C45783 | ✅ |
| 40 | Decoupling | CL05A105KA5NQNC | 1 µF 0402 — **exact count, from Rockchip's reference**: VCC5/6/7/8 inputs (4) · LDO1,2,3,5,6,7,8,9 outputs (8 — **LDO4 is 4.7 µF**) · SWOUT2 (1) · **VREF, VCC_RTC, VCC_1P8A, VCC_1P8D** (4) = 17 for the RK809. Codec caps omitted (codec unused). +1 each for RK860-0 and the SY8205 C_VCC = **19** | 19 | C52923 | ⚠️ |
| 41 | Decoupling | CL05B104KB54PNC | 100 nF 0402 — **exact count**: RK809 **PWRON, RESETB, VDC** (3 — HDG: the RESETB cap *"cannot be deleted at will"*) · **BUCK1/2/4 remote-FB taps** (3) · RK860-0 (1) · SY8205 at PVIN (1) · SoC **nPOR** anti-ESD (1) = **9** | 9 | C307331 | ⚠️ |
## 4. Clocks

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 42 | Clock | **X322524MOB4SI** | 24 MHz, **CL = 12 pF**, SMD3225 **4-pad** (both GND pads grounded — mandatory for ESD), ±10 ppm, −40…+85 °C, 90 k stock. ⛔ **Replaces the 18 pF-CL X322524MRB4SI carried over from the T113 board** — that part was mismatched to its own load caps. Back-calculated from Rockchip's 18 pF reference loads: CL = 18·18/36 + stray ≈ **12 pF**. ⚠️ **ESR deviation: 50 Ω vs Rockchip's Table 2-1 max of 40 Ω** — *every* 24 MHz SMD3225 on JLC is 50 Ω-max, and Rockchip's own schematics never publish their crystal's ESR, so compliance is unverifiable either way. Accept for bring-up; if the oscillator is marginal, move to a larger package (5032/7050) where <40 Ω is achievable | 1 | C70590 | ⚠️ |
| 43 | Clock | **0402CG180J500NT** | **18 pF** C0G 0402 — 24 MHz load caps. ⛔ **Changed from 22 pF**: 22 pF with an 18 pF-CL crystal under-capacitates it (C_eff ≈ 14–16 pF) and runs **+20…+40 ppm fast**, busting Rockchip's *Mandatory* ±20 ppm room-temp budget on its own. 18 pF + a 12 pF-CL crystal is exactly what **all five** Rockchip reference designs fit. Reused from gb3 | 2 | C1549 | ✅ |
| 44 | Clock | **0402WGF1004TCE** | **1 MΩ ±1 %** 0402 (LCSC **Basic**) — feedback resistor across XIN24M/XOUT24M. Rockchip: *"cannot be modified at will"*; present as `R1202` in every reference design | 1 | C26083 | ⚠️ |
| 45 | Clock | *(superseded)* | ⛔ The 22 pF 24 MHz load caps were **replaced by 18 pF** when the crystal moved to a 12 pF-CL part (see the 18 pF row). Row kept so the change stays visible; **do not order** | — | — | ✅ |
| 46 | Clock | SC-20S 32.768 kHz | RK809 RTC crystal, **CL = 12.5 pF — confirmed correct**. The RK809 datasheet specifies *no* CL/ESR target (checked revs 1.01/1.7/2.1); the HDG says match the crystal's own CL, ≥18 pF, **22 pF recommended** — which is exactly a 12.5 pF-class part. (Feeding `clk32k` to the SoC is optional — see note) | 1 | C97607 | ✅ |
| 47 | Clock | 0402CG220J500NT | **22 pF** C0G 0402 — 32 kHz load caps. ⛔ **Changed from 18 pF**: with 22 pF + stray, C_eff ≈ 12.5–14 pF ≈ the SC-20S's 12.5 pF CL. 18 pF sits at Rockchip's stated *floor* (C_eff ≈ 10.5–12 pF → runs fast, risks unstable start-up). Only 22 pF in the BOM — same MPN as the superseded 24 MHz load-cap row | 2 | C1555 | ✅ |

## 5. Power in + USB (power + maskrom)

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 48 | USB | TYPE-C-31-M-12 | **USB-C — power *and* data, the primary input** (single cable). VBUS → the P-FET below; `USB3_OTG0_DP/DM` → maskrom / rockusb / gadget (⚠️ **USB3-namespace balls running at HS** — the USB2 PHY is grounded); VBUS also taps the VBUSDET divider. ⚠️ **~2.5 A peak is above a spec USB-A port (500 mA USB2 / 900 mA USB3).** Fine from a **USB-C source**: a C-to-C cable to a 3 A charger advertises 3 A via Rp and is fully in spec. From a **plain USB-A port it is empirical** — early bring-up (console + NOR flashing, GPU and backlight idle) draws far less than peak, so a 900 mA port may well do. ⛔ **If peak load folds the port, the fix is host-side** (a 3 A USB-C charger, or a powered USB-C hub that passes data through to the NUC and sources from its own brick) — **not a board change**: the connector and the P-FET already carry 3 A | 1 | C165948 | ✅ |
| 49 | USB | RC0603FR-075K1L | 5.1 kΩ CC pulldowns (Rd, sink) — makes a Type-C source enable VBUS. ⚠️ **No CC-voltage sensing on this board**, so it cannot *read* the advertised current and will draw what it needs regardless. That is why the source matters: a **USB-C charger via C-to-C advertises 3 A** and is in spec, while an **A→C cable advertises only 500 mA** (56 kΩ Rp in the cable) even off a port that can physically deliver more | 2 | C105580 | ✅ |
| 50 | USB | USBLC6-2SC6 | D+/D− ESD + VBUS clamp | 1 | C7519 | ✅ |
| 51 | Decoupling | **RVT1V101M0810** | **100 µF 35 V** electrolytic, SMD 8 mm — DC/VBUS input bulk. ⛔ **Was 47 µF; PMIC&Power R11 (*Mandatory*) wants >100 µF at the DC input.** ⚠️ Note USB 2.0 nominally limits a port's VBUS bypass to 10 µF, so expect inrush on plug-in — pair with the input snubber below and an inrush limiter if a host trips | 1 | C37309 | ⚠️ |
| 52 | Decoupling | **RVT1V101M0810** | **100 µF 35 V** ×1 more — ⚠️ **`vcc3v3_sys` bulk, Mandatory.** PMIC&Power R12: *"VCC3V3_SYS … place capacitors with a capacity of **more than 100 µF** to obtain the best power ripple."* Our 2×22 + 10 µF = 54 µF was short. **This is also the mitigation for VCC9's 0.3 V shutdown margin.** Re-check SY8205 soft-start inrush with the bigger COUT | 1 | C37309 | ⚠️ |
| 53 | Power in | **AO4407A** | ⛔ **NEW — high-side load switch, the remote power-cycle element.** P-channel, **30 V / 14 A, ~11 mΩ @ V_GS −10 V** (budget ~20 mΩ at our −5 V), **SOIC-8**. At 2.5 A that is **~0.13 W** → a few °C rise. ⛔ **A SOT-23 part will not do**: AO3401A at 60 mΩ dissipates **0.375 W**, ≈ +94 °C in SOT-23. Source → VBUS, drain → `SYS_5V`, so when off its body diode blocks forward current | 1 | C2841482 | ⚠️ |
| 54 | Power in | **2N7002** | ⛔ **NEW — gate-driver NMOS** (LCSC **Basic**, 1.9 M stock), SOT-23. Inverts the 3.3 V control line onto the P-FET gate: **control HIGH ⇒ board ON**. Gate network reuses existing reels — **100 kΩ** gate↔source pull-up (default **OFF**), **10 kΩ** control-line pull-down, **10 kΩ** in series to the P-FET gate, **100 nF** gate↔source. That last RC is the **soft-start**, and it matters *more* on USB than on a brick: ~1 ms ramp holds inrush into ~200 µF to ≈1 A instead of an unbounded spike. ⚠️ **USB 2.0 nominally caps a port's bypass capacitance at 10 µF and we have ~200 µF** — switching that in hard is exactly what trips a protected hub port, so the soft-start is what makes USB-C power practical at all. ✅ **This also means `PWR_EN` is a *better* power-cycle than uhubctl**: on-board, soft-started, and independent of whether the hub implements per-port switching | 1 | C8545 | ⚠️ |
| 55 | Power | **RC0805FR-072R2L** | **2.2 Ω 0805** — ⚠️ **input RC snubber, Mandatory.** PMIC&Power R10: *"place an **RC circuit at the DC input end**, which can absorb the peak voltage at the moment of DC insertion."* In series with a 2.2 µF (reuse the backlight reel) across the input to GND. 0805 for pulse energy — not the 0402 2.2 Ω used on USB DP/DM | 1 | C137549 | ⚠️ |
| 56 | USB | **0402WGF1002TCE + 0402WGF1502TCE** | **VBUSDET divider — MANDATORY for maskrom.** **10 kΩ** (VBUS → `USB3_OTG0_VBUSDET`/ball M24) + **15 kΩ** (M24 → GND) = **3.00 V**, dead-centre in the pin's 2.7–3.3 V window. ⛔ **M24 is NOT 5 V tolerant** (the only 5 V-tolerant pin on the RK3568 is HDMI_TX_HPDIN) — direct VBUS would damage it. Identical values in **four** independent reference designs. The 10 kΩ shares the `0402WGF1002TCE` reel | 1+1 | C25744 / C25756 | ⚠️ |
| 57 | USB | CL05B104KB54PNC | **100 nF at ball M24** — mandatory per Rockchip's review checklist; with the 6 kΩ Thevenin source it gives ~0.6 ms of attach debounce. Same 100 nF reel | 1 | C307331 | ⚠️ |
| 58 | USB | **RC0402FR-072R2L** | **2.2 Ω ×2 series on `USB3_OTG0_DP`/`DM`** — *Mandatory* per the review checklist (row 9) on the download pair | 2 | C327251 | ⚠️ |
| 59 | USB | CL05B104KB54PNC | 100 nF ×3 — one each on **`USB3_AVDD_0V9` / `_1V8` / `_3V3`**. ⚠️ HDG §2.2.2.9: all three **must be powered at first power-on** or firmware download over DP/DM doesn't work. Same 100 nF reel | 3 | C307331 | ⚠️ |

## 6. Boot storage + remote rescue

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 60 | Storage | W25Q128JVSIQ | 128 Mbit SPI-NOR on FSPI — **the remote-flashable boot target** (written over USB maskrom, no SD swapping). **3.3 V part → flash IO domain must be 3.3 V** (see notes) | 1 | C97521 | ✅ |
| 61 | Storage | A-MicroTF-1.85A | microSD socket w/ card-detect → `sdmmc0` (det = GPIO0_A4) | 1 | C22467599 | ✅ |
| 62 | Storage | SP3004-04XTG | 4-ch ESD, 0.85 pF — 2× covers SD CLK/CMD/DAT0–3 | 2 | C207280 | ✅ |
| 63 | Storage | RC0603JR-0710KL | 10 kΩ — SD CMD/DAT pull-ups (5) + SARADC_VIN0 recovery-input pull-up to vcca_1v8 (1) | 6 | C99198 | ✅ |
| 64 | Decoupling | CC0603KRX7R9BB104 | 100 nF 0603 — NOR VCC + SD VDD (not under the BGA, so 0603 is fine) | 2 | C14663 | ✅ |
| 65 | Decoupling | CL21A106KOQNNNE | 10 µF 0805 — SD VDD bulk at the socket. ⚠️ **Counted here, not in the media-switch row** — the switch sits in series with SD VDD, so this *is* its downstream bulk; don't double-count | 1 | C1713 | ✅ |
| 66 | Recovery | **SY6280AAC** ×3 | **Boot-media power switches + the panel rail.** Active-high-enable current-limited load switch, 2.4–5.5 V, 80 mΩ, SOT-23-5. **switch #1 in series with NOR VCC, switch #2 with microSD VDD** → both off ⇒ **maskrom, guaranteed in hardware** (ported from [t113-breakout Fix 5](../t113-breakout/REV-B-FIXES.md), bench-confirmed there). **Switch #3 gates the panel VCI+IOVCC** — ⚠️ *Mandatory*, LCM&TP R8: *"the power supply of the screen **must be controllable**. When powered on, it is **not provided by default**."* So switch #3's EN gets **no pull-up** (default OFF) and is driven from a reset-pull-down PMUIO2 GPIO — ✅ **now allocated: `GPIO0_C5` / ball `AC21`**, whose reset pull is **down** (16 K–43 K) and whose Rockchip pin-usage text is literally *"LCD1 panel power enable, active high"*. **Touch VDDT stays on `vcc_3v3`** — the reference doesn't switch it | 3 | C55136 | ⚠️ |
| 67 | Recovery | **0402WGF6801TCE** | **6.8 kΩ ±1 %** ×3 (JLC **Preferred**) — ⚠️ **mandatory `ISET` resistor, one per SY6280** (ISET → GND). Current limit is *programmed* by it: **I_lim = 6800/R_set** ⇒ 6.8 kΩ = **1 A**, above the NOR (~20 mA), microSD (~500 mA peak) and panel (~55 mA) loads and inside the 0.4–2 A range. **Without it the limit is undefined** | 3 | C25917 | ⚠️ |
| 68 | Recovery | *(the 4.7 kΩ row)* | EN pull-ups moved to **4.7 kΩ** — 10 kΩ has no margin against the SoC's internal pull-down. **Two drivers share each EN net** (SoC GPIO + the rig's DTR/RTS), so the SoC drives through **1 kΩ** and the rig overrides it; both floating ⇒ the pull-up holds EN high (= media ON, board behaves like a product standalone) | — | — | ✅ |
| 69 | Recovery | CL21A106KOQNNNE + CL05B104KB54PNC | 10 µF + 100 nF downstream of the switches — expect inrush when re-powering a medium. ⚠️ **SD's pair is counted at the socket and NOR's at the flash**, so this row covers the **panel** switch (switch #3) only. Same reels | 2 | C1713 / C307331 | ✅ |
| 68 | Recovery | **XH-6A** | ⛔ **Boot-mode / control connector — 6-pin JST-XH, `keyed`, 101 k stock.** Every line the NUC needs on one plug, and ⛔ **keyed so it cannot be inserted reversed** — it carries GND and `PWR_EN`, so a reversed plain pin header is a bricked board. Replaces the old 2×2 (`NOR_EN`/`SD_EN` only) so alternate boot modes are host-selected, not set by hand. Pins **1** `PWR_EN` (board on/off) · **2** `NOR_EN` · **3** `SD_EN` · **4** `RESETn` · **5** `RECOVERY` = `SARADC_VIN0` · **6** `GND`. **Boot source = which media are powered** (BootROM order FSPI → NAND → eMMC → SDMMC0 → USB): NOR on / SD off = production NOR boot · NOR off / SD on = SD boot · **both off = maskrom**. ⛔ **Pin 5 is pull-to-GND ONLY — never drive it high**: `SARADC_VIN0`'s maximum is `SARADC_AVDD_1V8`, so a 3.3 V host line exceeds it (SARADC checklist). One GND is sufficient — every line here is µA–mA and the 2.5 A return goes through USB-C, not this connector. ⚠️ **Through-hole: hand-solder.** ⚠️ **Pitch is 2.5 mm** even though the ecosystem sells it as "XH2.54" — buy header and cable from the same family. **Cable: an off-the-shelf 6-pin JST-XH pre-crimped pigtail**, bare ends soldered to the Pico = a one-piece keyed harness, no loose jumpers. ⚠️ **Host side: one USB GPIO adapter** (a Pico, or FT232H-class) — five lines will not fit on a serial adapter's DTR/RTS pair. **Standalone bench use:** jumper the P-FET gate to GND to force the board permanently ON; remove it for rig operation | 1 | C5663 | ⚠️ |

*Decoupling counts are now confirmed against Rockchip's reference (DRAM: 2× 10 µF + 7× 100 nF per device, verified). The 0402
caps/resistors above are all LCSC **Basic** (no feeder fee); JLC bills a 20-piece minimum + 10-piece
attrition on 0402s, so small quantities round up.*

## 7. Console

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 71 | Debug | PZ254V-11-03P | 1×3 2.54 mm pin header, **through-hole** (hand-solder — JLC won't place THT on a standard SMT order). UART2 console: TX/RX/GND (also broken out on the insurance header) | 1 | C2937625 | ⚠️ |
| 72 | Status | 0603WAF5100T5E | 510 Ω — **power-LED current limit only**. ⛔ The UART series resistors moved to **100 Ω** (Rockchip's checklist specifies 100 Ω on RX/TX and says they *"must not be deleted"*) — see the 100 Ω row | 1 | C23193 | ✅ |
| 73 | Status | 19-213SYGC | Green power LED — on a board you can't physically reach, the only "is it alive" signal besides UART. Reuses t113/gb3 parts (510 Ω limiter is the row above) | 1 | C2986027 | ✅ |
| 74 | Debug | Test points / 1×1 pads | ⛔ **+1 pad on `PWRON`** (dropped from the 6-pin control connector — `PWR_EN` cuts 5 V outright, which is strictly deeper than a PMIC button press, so it was the most redundant line). Probe pads on **every rail**: 5 V, 3V3_SYS, VDD_CPU, vdd_logic, vdd_gpu, vcc_ddr (1.35 V), vcc_1v8, vcc3v3_pmu, vcca_1v8, VBUSDET. ⚠️ **VCC9 has only ~0.3 V of shutdown margin**, so measuring droop is not optional. Footprint-only — no part to buy | ~11 | — | ✅ |
| 75 | Thermal | **Wakefield 658-60AB** | **27.9 × 27.9 × 15.2 mm** extruded aluminium, **2.0 °C/W @ 500 LFM**, $1.65, 2.8 k stock (Digikey). Interim pick — swap after measuring; it's a $2 part, not a respin. ⚠️ **adhesive not included** — add thermal tape or a pad. ⚠️ **Check clearance in layout: 28 mm square overhangs a 19 mm package by ~4.5 mm/side and the DRAM sits close to the SoC for DDR routing.** Rationale: over-temp routes to `TSADC_SHUT → RK809 reset`, so overheating looks like a *silent shutdown*. Alternative: an **ODROID-M1 / ROCK 3A heatsink**, sized for this exact 19×19 mm RK3568 and often fan-equipped | 1 | — | ⚠️ |

## 8. Display — Winstar WF50DTYA3MNG10 (DSI + touch)

All numbers below are from [`cad/docs/WF50DTYA3MNG10.pdf`](../../cad/docs/WF50DTYA3MNG10.pdf) (spec rev E).
5.0" 720×1280 IPS · **ILI9881C** · **4-lane MIPI DSI** · **GT928** cap touch · module 73.30 × 127.60 ×
3.45 mm, active area 62.10 × 110.40 mm, **4× ⌀1.0 mm mounting holes**.

**Two separate flex tails** → two connectors. DSI must be routed as 100 Ω differential pairs, short and
length-matched, SoC → connector; it cannot go through 2.54 mm headers.

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 76 | Display | **JUSHUO AFC24-S40FIA-00** | Display tail: 0.5 mm × **40 pos**, right-angle, flip-top lid, **double-sided contacts** (mates fingers-up *or* fingers-down → removes the unverified contact-side risk, see note). Flex 20.50 mm wide × 0.30 mm. Carries 4× DSI pairs + CLK pair, VCI, IOVCC, RESET, TE, LED+/LED1−/LED2−, GND | 1 | C262686 | ⚠️ |
| 77 | Display | **JUSHUO AFC24-S06FIA-00** | Touch tail: 0.5 mm × **6 pos**, right-angle, flip-top, double-sided contacts. Flex 3.50 mm wide × 0.30 mm. Pins 1-6 = SDA, SCL, RST, INT, VDDT, GND | 1 | C262679 | ⚠️ |
| 78 | Display | 0603WAF2001T5E | 2.0 kΩ 1% — GT928 I²C pull-ups on SDA/SCL (value per GT928 spec); reused from gb3 | 2 | C22975 | ✅ |
| 79 | Decoupling | CL05B104KB54PNC + CL05A105KA5NQNC | 100 nF + 1 µF 0402 — local decoupling at both FPC connectors (VCI / IOVCC / VDDT); same reels as the 100 nF and 1 µF 0402 rows | 6 | C307331 / C52923 | ⚠️ |
| 80 | Backlight | TPS61165DBVR | WLED constant-current boost, VIN = 5 V. **CTRL ← a SoC PWM** (mainline `pwm-backlight`) | 1 | C58756 | ✅ |
| 81 | Backlight | SMNR4020-10UH | 10 µH boost inductor | 1 | C135263 | ✅ |
| 82 | Backlight | SS16 | 60 V 1 A Schottky | 1 | C2481 | ✅ |
| 83 | Backlight | CC0805KKX7R9BB225 | 2.2 µF 50 V — **2 uses**: TPS61165 Cout (≥50 V clears the 23.8 V VLED+) **and the DC-input RC snubber's cap**, in series with the 2.2 Ω (that row said "reuse the backlight reel" but the quantity was never incremented) | 2 | C125847 | ✅ |
| 84 | Backlight | CC0603KRX5R6BB475 | 4.7 µF Cin | 1 | C109456 | ✅ |
| 85 | Backlight | RC0402FR-075R1L | **5.1 Ω Rset — unchanged from gb3, confirmed correct:** panel ILED = **40 mA** total, and 200 mV / 5.1 Ω = 39 mA ✓ | 1 | C137955 | ✅ |
| 86 | Backlight | CC0402KRX7R7BB224 | 220 nF COMP cap (mandatory loop comp) | 1 | C326590 | ✅ |

> **✅ Panel DSI pair order — RESOLVED (from the local datasheet).** Panel flex by ascending pin:
> **D3 (22/23) · D2 (25/26) · CLK (28/29) · D1 (31/32) · D0 (34/35)**, GND between each pair. The SoC's
> ball edge runs **D0 · D1 · CLK · D2 · D3** (AH17→AH13). **The two are exact mirrors**, so orient the
> connector with flex pin 40 nearest the SoC's AH13 end and all five pairs route straight across with
> **no crossings** — get it backwards and all five cross. See [PINOUT.md](./PINOUT.md) §1.3.

> **FPC contact side — the one thing the datasheet can't answer.** Winstar's section view labels
> "Stiffener" on one face and "Contact side" on the other, but has no section arrows tying either to the
> front/back of the module, so **which way the gold fingers face after the flex folds behind the panel is
> UNVERIFIED** — look at the physical panel before locking footprints. Both chosen connectors
> (**AFC24**, double-sided) wire both slot walls to the same pad, so they mate either way **at identical
> pin positions**, for the same price as single-sided. That matters because contact side and pin order are
> coupled: flipping the flex to change which face presents the fingers also moves flex-pin-1 to the other
> end of the housing — get it wrong and the DSI pairs reverse **and LED+ (19.6–23.8 V) lands on
> LED2−/GND**. Flip-top lids were also preferred over slide-lock: the actuator stays captive during rework.
> Winstar's tail geometry (19.50/20.50 mm and 2.50/3.50 mm) exactly matches the standard 0.5 mm FPC
> profile, so any conforming connector fits. Single-sided alternates if ever needed: bottom-contact
> AFC01-S40FCA-00 / AFC01-S06FCA-00, or BOOMELE C9160 (the gb3 part — real specs confirmed: 40P 0.5 mm
> bottom-contact right-angle, but thinner stock and a tighter 0.3 ±0.03 mm flex spec).

> **Panel power — one 3.3 V rail does all three.** VCI (analog) 2.5–3.6 V · IOVCC (logic) **1.65–3.6 V** ·
> VDDT (touch) 2.8–3.3 V. All three ranges include 3.3 V, so **no 1.8 V panel rail is needed** — tie VCI,
> IOVCC and VDDT to `vcc_3v3` and keep RESET/TE/touch GPIO in a **3.3 V** IO domain. IDD ≈ 44 mA + 40 mA
> backlight.

> **Power-on sequence (datasheet §8):** IOVCC and VCI may rise in either order (skew > 0) → one shared
> 3.3 V rail satisfies it. Then **RESET high ≥ 5 ms after rails**, **reset low pulse ≥ 10 µs**, **≥ 10 ms
> reset → first DSI command**. Off: RESET low, then ≥ 1 ms before dropping the rails. All of this is the
> mainline panel driver's job — hardware just needs the shared rail + one RESET GPIO.

> **Backlight string (datasheet §5.2):** **14 LEDs = 2 parallel strings of 7**, cathodes brought out
> separately as LED1− / LED2−. VLED+ = 19.6–23.8 V, ILED = 40 mA total (20 mA per string). The TPS61165 is
> single-channel → **tie LED1− and LED2− together** into its FB/sense node so one 5.1 Ω resistor sets the
> full 40 mA. 23.8 V max is well inside the TPS61165's ~38 V OVP.

> **Touch = GT928, not GT911.** Mainline-supported — `goodix,gt928` is in `goodix.c`'s OF table (verified
> in the local kernel tree) and maps to the GT911 chip data. I²C ≤ 400 kbps. Its **address is strapped at
> power-on by RST/INT**: 0xBA/0xBB (7-bit **0x5D**) or 0x28/0x29 (7-bit **0x14**) — so route **both** RST
> and INT to GPIOs (the driver drives them for the reset/addr-select dance).

## 9. Insurance header

| # | Category | Part | Description | Qty | LCSC # | Src |
|---|----------|------|-------------|-----|--------|-----|
| 87 | Header | 2044-1X25G00SA | 1×25 female, **2×** = one 2×25 block (50 positions). Not needed for the goal — spare pins to add probes/helpers if bring-up needs them (see below) | 2 | C49569761 | ✅ |

> **What goes on the 50 pins:** UART2 (console, also on the 1×3 header) · one spare I²C, one SPI, one spare
> UART · 2 PWM · ~8 plain GPIO · 5 V / 3V3 / 1V8 · ~12 GND. All from **3.3 V** IO domains so nothing on the
> header is 1.8 V. ⛔ **`PWRON` / `RESETn` / `SARADC_VIN0` moved off this block** onto the 2×5 boot-mode
> header — they are rig-control lines, not insurance pins, and duplicating a tap on two connectors invites
> two drivers fighting over one net. Their 100 Ω + TVS stay with them.

---

> **Power tree — cloned from mainline `rk3568-rock-3a.dts` (verified in the local kernel tree):**
> USB-C 5 V → **RK860-0** (VDD_CPU) · → **3.3 V sys buck** → all RK809 VCC1–9 · → backlight boost (5 V).
> RK809: **BUCK1** vdd_logic · **BUCK2** vdd_gpu · **BUCK3** vcc_ddr · **BUCK4** vdd_npu · **BUCK5**
> vcc_1v8 · **LDO1** vdda0v9_image · **LDO2** vdda_0v9 · **LDO3** vdda0v9_pmu · **LDO4** vccio_acodec
> (3.3) · **LDO5** vccio_sd · **LDO6** vcc3v3_pmu · **LDO7** vcca_1v8 · **LDO8** vcca1v8_pmu · **LDO9**
> vcca1v8_image · **SWITCH1** vcc_3v3 · **SWITCH2** vcc3v3_sd.
> **One deviation:** ROCK 3A runs LPDDR4, so `vcc_ddr` (set by BUCK3's FB divider) must be set for
> **DDR3L = 1.35 V** — take the divider from Rockchip's DDR3 template, not the ROCK 3A.

> **⚠️ Unused-pin handling — Rockchip's rule is "GROUND the unused PHY supply balls", not "leave floating".**
> Read from `RK3568_Processing instructions for unused pins_V10_20201218_CN` + the review checklist. Signal
> pins float; **analog supply balls get tied to VSS**: `PCIE30_AVDD_0V9/1V8` (U21/U20/U22) ·
> `MULTI_PHY_AVDD_0V9/1V8` (R20/R21/R22) · `HDMI_TX_AVDD_0V9/1V8` (V17/V18/W18) · `EDP_TX_AVDD` (M20/M22) ·
> `MIPI_CSI_RX_AVDD` (W14/Y14) · `MIPI_DSI_TX1_AVDD` (W15/Y15) · `USB2_AVDD_0V9/1V8/3V3` (R8/P9/P10, the
> HOST2/3 PHY). Also: **`PCIE30_REFCLKP/N_IN` (Y25/AA25) must be tied to VSS, not floated**, and **`TVSS`
> (AB24) must be directly grounded** (Mandatory). `PCIE30_RESREF` floats — **do NOT fit the 200 Ω** (that's
> a PCIe-in-use rule). DDR ECC's 11 signals float. Three savings fall out: no PCIe REXT resistor, no DSI1
> beads, no `REFCLK_OUT` 22 Ω.
>
> ⛔ **The trap: `USB3_AVDD_0V9 / _1V8 / _3V3` (P22/P23/M23) MUST BE POWERED even though we use no USB3.**
> HDG §2.2.2.9: *"Since the firmware of the RK3568 chip must be downloaded from the USB3_OTG0_DP/M
> interface, USB3_AVDD_0V9, USB3_AVDD_1V8, USB3_AVDD_3V3 must be powered in the first time power-on."* The
> unused-pins workbook says `Tie VSS` for these — **that applies only if OTG0 is unused entirely, which is
> not our case.** Grounding them would kill maskrom flashing.
>
> ✅ **`USB2_AVDD_*` (R8/P9/P10) — RESOLVED: tie to VSS.** The ball *namespace* settles it: OTG0's
> high-speed pins are **`USB3_OTG0_DP/DM`, annotated `_HS/FS/LS` in the pinout sheet** — i.e. HS/FS/LS for
> OTG0 lives in the **USB3** group and is powered by `USB3_AVDD_*`. The only USB2-namespace signals are
> `USB2_HOST2_DP/DM` (R2/R1) and `USB2_HOST3_DP/DM` (T2/T1), which this board does not wire. Rockchip's
> unused-pins workbook, Part V *"USB2.0 HOST2/USB2.0 HOST3"*, lists **R8/P9/P10 = `Tie VSS`** verbatim.
> *(Don't be misled by mainline's `usb2phy0_otg` phy-node name — that is a PHY instance name, not a claim
> about which AVDD balls feed it.)* Software is already safe: `usb_host1_ehci`/`ohci` default to
> **`status = "disabled"`** in mainline `rk356x.dtsi`, so nothing probes the dead PHY. **Saves 3 caps.**
>
> ⛔ **`SDMMC0_DET` (ball Y22) is a reset-sampled strap that selects the whole VCCIO3 bank's function:
> HIGH = ARM JTAG (the default, via an internal pull-up), LOW = SDMMC0.** Leave it floating and **the
> microSD simply will not work**. It must be driven by the socket's card-detect (high = no card). Tying it
> permanently low loses JTAG. There are exactly **two** reset-sampled straps on the RK3568 — this and
> `FLASH_VOL_SEL` — and both are easy to get wrong.
>
> **Rails that must be supplied regardless** (inverse of HDG Table 2-11): `PMUPLL_AVDD_0V9/1V8` (V21/Y21 —
> note **`PMUIO0` *is* `PMUPLL_AVDD_1V8`**, 1.8 V fixed) · `SYSPLL_AVDD_0V9/1V8` (P11/N10) ·
> `PMU_VDD_LOGIC_0V9` (V19) · `PMUIO1` (Y20, **3.3 V only and must be powered**) · `PMUIO2` (W19) ·
> `VCCIO2` (H18) · `VCCIO3` (L22) · `SARADC_AVDD_1V8` (H22) · `OTP_VCC18` (H20) · the three `USB3_AVDD` ·
> `MIPI_DSI_TX0_AVDD_0V9/1V8` (W16/Y17) · DDRPHY_VDDQ/VDDQL (**both 1.35 V for DDR3L**). May be left
> unpowered: `VCCIO1/4/5/6/7` — power only the banks whose headers get populated.
>
> **Boot-probe hazard:** the BootROM probes **FSPI → NAND → eMMC → SDMMC0**, driving those IOs during each
> probe. Rockchip therefore advises **not** using the VCCIO2 (flash) bank as ordinary GPIO — check the
> insurance header doesn't tap it.

> **✅ Power sequencing — RESOLVED, and my earlier damage worry was WRONG.** HDG §2.2.1.2 is explicit:
> *"for the same module, low voltage power on first, high voltage later … **there is no timing requirement
> between different modules**"* (the Chinese original adds *"intervals ≥ 0 µs"*). So our three-source
> topology is **not** a sequencing violation — it's structurally identical to Rockchip's own AIoT reference,
> which is also three-source. **Back-powering is likewise a non-issue:** HDG Table 2-12 lists VDD_CPU,
> VDD_GPU, VDD_NPU, VDD_LOGIC *and* VCCIO1–7 as *"can support power off"* while the PMU/DDR rails stay up —
> i.e. **3.3 V IO energised with the core rails fully off is the normal standby state**, a supported mode,
> not a latch-up hazard. There are **no cross-rail relative abs-max limits** in the datasheet and **no
> ramp-rate, slew or monotonicity requirements** anywhere.
>
> **But four things ARE mandatory, and one bites us:**
> 1. ⚠️ **`RESETn` must be released ≥ 10 ms after the last rail is stable.** Our note said the RK809
>    releases RESETB 2 ms after the last rail — **that would violate the rule.** HDG Fig 2-63 and the AIoT
>    schematic both label `RK809_RESETn` as **"Slot 4+5"** = 5 × 2 ms = 10 ms, which reconciles it, but the
>    RK809 datasheet's generic waveform just says *T = 2 ms*. **Unresolved — scope slot-4 rails against
>    nPOR on board #1.** ✅ **The hedge is now fitted as DNP** rather than left to rework: a **1 µF 0402 footprint across `RESETn`→GND, marked DNP** (`CL05A105KA5NQNC`, the existing 1 µF reel, +1 qty). With the 10 kΩ pull-up that is ≈10 ms; populate it only if the measurement comes back short. ⛔ Without the footprint the fix is a rework wire on a 0.65 mm BGA board. If it measures ~4 ms, populate that cap or add
>    an open-drain supervisor per HDG Fig 2-4. *This is the single most consequential open item here.*
> 2. **Power-DOWN: `RESETn` must be pulled low BEFORE any rail drops** (§2.2.1.3). The RK809 does this by
>    construction (RESETB low → 2 ms → all channels off together). Caveat: on a **SYS under-voltage** the
>    lead shrinks to **60 µs** — another reason to keep `vcc3v3_sys` droop tight.
> 3. **The RESETn pull-up must come from the same IO domain as the nPOR pin — `PMUIO1`, i.e. `vcc3v3_pmu`,
>    NOT `vcc3v3_sys`.** Hard rule, stated as such.
> 4. **Don't re-allocate the RK809-5's rail→channel mapping** — its sequence is burned in.
>
> **Recommended (not mandatory) order**, which our rails already follow:
> `PMU_VDD_LOGIC_0V9 → VDD_LOGIC → {VDD_CPU, VDD_GPU, VDD_NPU}`; `VDD_LOGIC → OTP_VCC18` and
> `→ SARADC_AVDD_1V8`; USB `0V9 → 1V8 → 3V3`; MIPI DSI `0V9 → 1V8`; 3.3 V IO rails last.
>
> ⚠️ **`vcc3v3_sys` must NOT feed any RK3568 pin directly.** In all the references the SoC's 3.3 V IO comes
> *only* from RK809 SWOUT1/SWOUT2/LDO5 (slot 4); the system rail feeds the PMIC and the regulators. Keeping
> that split is what makes the "3.3 V IO last" ordering happen.
>
> ⚠️ **The 100 Ω remote-sense resistors are damage-prevention, not tuning.** Checklist row 85: with an open
> feedback node the DC-DC *"loses control of the output voltage, which will lead to **overvoltage damage to
> the main control**"* — and VDD_CPU's abs max is 1.25 V. The 100 Ω + 100 nF **must not be deleted**.
>
> ⚠️ **RK860-0 is less verified than the rest of the power tree.** No RK860 datasheet is in Rockchip's kit
> and rockchip.com was unreachable, so its **EN threshold vs a 51 kΩ pull-up, VSEL pin, soft-start, and
> whether its register map matches the kernel's `rk8600` path are all unconfirmed**. All three references
> use the **TCS4525** instead — but that part is 0-stock on LCSC (see the CPU-buck note), so RK860-0 stands
> as the only sourceable option. Get its datasheet before layout. Also wire **PMIC_SLEEP → RK860 VSEL** if
> standby voltage scaling is wanted (all three references do).

> **✅ Buttonless power-on — RESOLVED (Rockchip RK3568 Hardware Design Guide §2.2.3.5 + RK809 DS rev 2.1).**
> The RK809 has **exactly three turn-on sources** (`ON_PWRON`, `ON_PLUG_IN`, `ON_RTC` in reg 0xF5) and
> **no OTP "auto-start on VCC" bit** — so simply applying VCC does *not* start it. The buttonless path is a
> **rising edge on VDC (pin 61)**: VCC_RTC must already be >3.0 V, then VDC crossing **>0.55 V** (Rockchip
> recommends ~1.2 V) starts the PMIC, and **VCC9 must exceed 3.0 V within 1.5 ms of EXT_EN going high or it
> won't latch on**. So: feed VDC from a **10 kΩ/3.3 kΩ divider off the always-on 5 V + 100 nF** (the 10 kΩ, 3.3 kΩ and 100 nF rows).
> **⛔ Do not enable the 3.3 V sys buck from EXT_EN** — enable it from its own 5 V input (exactly what
> Rockchip's reference and the ROCK 3A do; the ROCK 3A leaves the EXT_EN option resistor unfitted). Gating it
> on EXT_EN creates a chicken-and-egg against the 1.5 ms rule. **⛔ Do not strap PWRON low** — a sustained
> low is the 6 s force-shutdown; leave it with just a 100 nF (it has an internal 17 kΩ pull-up). Still break
> **PWRON and RESETB** out to the insurance header for jumper/rig control.

> **VCC_RTC ordering is a hard constraint.** HDG: VCC_RTC *"must be the highest voltage among all the supply
> pins… must be powered on first, or together with other supplies. It is not allowed to supply other power
> sources before VCC_RTC."* → tie **VCC_RTC and VCC1…VCC9 all to `vcc3v3_sys`** (what every reference board
> does). Automatic if the SY8205 is the only 3.3 V source.

> **⚠️ VCC9 has only ~0.3 V of shutdown margin.** VCC9 doubles as the under/over-voltage sense pin, and the
> RK809-5's `VB_OK` default is **3.0 V** (confirmed, Rev 2.3 Table 4-4; range 2.8–3.6 V via `VB_OK_SEL`) — *"if the voltage of VCC9 is lower than 3.0 V after power-on, it
> will automatically shut down."* On a 3.3 V rail that's 0.3 V. Size `vcc3v3_sys` generously and keep its
> droop tight under load transients, or expect spurious shutdowns. (Another reason for the 5 A SY8205.)

> **Default-OFF rails (RK809-5 OTP, fixed and non-reprogrammable).** `vdd_npu` (BUCK4), `vdda0v9_image`
> (LDO1), `vccio_acodec` (LDO4) and `vcca1v8_image` (LDO9) are **OFF at reset** — software enables them.
> Sequence: BUCK1 → BUCK2/BUCK5/LDO2/3/6/7/8 → BUCK3 → LDO5/SWOUT1/SWOUT2, 2 ms apart, RESETB released 2 ms
> after the last rail. **The -5 sequence is burned in and must not be mixed with -1/-2/-3 variants.**

> **Unused blocks — pin disposition (Rockchip reference note).** Codec unused → **tie pins 14, 15, 16, 17,
> 19, 40 to GND**; **leave 18, 32, 34, 35, 36, 37, 38, 39, 41, 42, 43 floating** (⚠️ pin 33 VCC_SPK_HP is in
> neither list — leave unconnected). This deletes ~8 charge-pump/speaker caps. Gas gauge unused → **ground
> BATDIV (56), SNSP (62), SNSN (63)**. Both are right for a no-battery board.

> **The 27 under-BGA 100 nF, enumerated.** Rockchip's rule is one 100 nF per power *domain*, so the count
> follows from which domains we actually power. Ball counts from `RK3568_PinOut_V1.1`; groupings follow
> Rockchip's reference schematic:
>
> | Domain | Caps | | Domain | Caps |
> |---|---|---|---|---|
> | VDD_CPU (10 balls) | 1 | | PMUIO1 + PMUIO2 | 2 |
> | VDD_LOGIC (10) | 1 | | PMU_VDD_LOGIC_0V9 + PMUPLL_0V9 | 2 |
> | VDD_GPU (5) | 1 | | PMUPLL_AVDD_1V8 | 1 |
> | VDD_NPU (5) | 1 | | OTP_VCC18 | 1 |
> | DDRPHY_VDDQ + VDDQL (14) | 2 | | SYSPLL_AVDD_0V9 | 1 |
> | VCCIO1 / 2 / 3 / 4 | 4 | | SARADC_1V8 + SYSPLL_1V8 | 2 |
> | VCCIO5 + VCCIO7 | 2 | | MIPI **DSI0** _AVDD_0V9 / _1V8 | 2 |
> | VCCIO6 | 1 | | ~~USB2_AVDD_0V9 / _1V8 / _3V3~~ | **0** ✅ |
> | | | | **Total** | **24** |
>
> **Dropped vs Rockchip's 42** — the interfaces this board doesn't wire: PCIe3.0 (2 domains), MULTI_PHY
> (2), HDMI_TX (2), eDP (2), MIPI CSI + DSI1 (4), **USB2 HOST2/3 (3 — grounded, see below)**. `USB3_AVDD_*` **is** powered and **does** need its 3 mandatory caps — they are budgeted as 0402s in the USB section instead of here. ⚠️ *The checklist wants those three "on the back of the corresponding power pins"; confirm in layout that a 0402 fits beside P22/P23/M23 or swap them to 0201.* HDG says an entirely-unused power domain may be
> left **unpowered and floating**, so those take no caps. **Two assumptions behind the 24:** (a) unused
> analog domains stay unpowered — if any ends up tied to a rail, add one 100 nF for it; (b) VDD_NPU is
> decoupled even though BUCK4 is OFF by default and the NPU is unused, so the rail can be enabled later
> for ~4 parts. Also note **OTP_VCC18 is not optional** — the review checklist says it *"must be powered,
> and a 0.1uF decoupling capacitor should be placed."*

> **⛔ The ROCK 3A's CPU buck is not buildable — this is a real deviation.** Mainline's
> `rk3568-rock-3a.dts` uses a `tcs,tcs4525` at I²C **0x1c**, but **TCS4525 is 0-stock on LCSC/JLC and the
> TCS4526 page is delisted**; every FAN53555 variant is also 0-stock (9 of them marked "no longer
> manufactured"). So the CPU buck is **RK860-0** — Rockchip's own part, 4.6 k in stock, mainline
> `rockchip,rk8600`. **DTS change required: `reg` 0x1c → 0x40**; keep 0.8–1.15 V (exact steps on the
> 712.5 mV + 12.5 mV grid).
>
> **One footprint, three sources.** RK860-0 / SYR827PKC / TCS4525 are **ball-identical** 20-ball 0.4 mm
> WLCSPs (only deltas: TCS4525's B3 power-good instead of PGND, and a separate analog VIN on D1 — both
> benign if B3→GND and D1→VIN). RK860-0 and SYR827PKC are *both* at **0x40 with die ID 8 and identical
> kernel voltage tables*, so a single DTS node drives either chip with no software change. Lay out one
> land pattern and keep SYR827PKC (**C111839**) as the cheaper second source.
>
> **0.4 mm pitch but still no HDI** — worth stating since 0.4 mm normally forces it: on all three parts
> the four *interior* balls (B2/B3/C2/C3) are **all GND**, VIN is a 2×2 corner block and SW/LX the
> opposite corner, so every real signal (VSEL, EN, SCL, SDA, VOUT) is on the perimeter. Drop GND vias
> straight into the interior and escape the signals on the top layer → **stays on JLC's standard 4-layer
> process.** Route the A4 VOUT sense trace to the output-cap terminal, not off the inductor, and **don't
> let EN float** (all three datasheets).
>
> **No Basic/Preferred synchronous buck exists for the 3.3 V rail** — a ~600-part sweep of JLC's
> adjustable bucks found only non-synchronous Basic/Preferred parts (TPS5430/TPS54331/MC34063/XL1509),
> and TPS5430's 5.5 V minimum input rules it out from 5 V anyway. So the 3.3 V buck is Extended regardless; pick on
> stock + thermals instead. Cost-down alternate if the measured draw is ≤2.5 A: **TPS563201DDCR
> (C116592, $0.07, 100 k stock)** — but only 3.3 A min current limit and 92.6 °C/W, i.e. ~100 °C junction
> at a hard 3 A. Second source: **AP62300TWU-7 (C1883519)**. The FP6161's bigger sibling **FP6165ADXR-G1
> (C77233, 3 A, SOP-8-EP)** also works and reuses the same 0.6 V divider math, but stock is only ~1.4 k.

> **3.3 V rail current budget (my estimate — recompute against the RK809 + RK3568 datasheets).**
> Because all nine RK809 VCC inputs hang off `vcc3v3_sys`, everything except the CPU core and the
> backlight flows through this one buck. Reflecting each downstream rail back through its converter
> (bucks at ~90 %, LDOs 1:1):
>
> | Downstream rail | Load | Draw on 3.3 V |
> |---|---|---|
> | `vdd_logic` (BUCK1, ~0.9 V) | ~1.5 A | ~0.45 A |
> | `vdd_gpu` (BUCK2, ~1.0 V, 200–800 MHz @ 0.85–1.0 V per the OPP table) | ~1.5 A | ~0.50 A |
> | `vcc_ddr` (BUCK3, 1.35 V, 2× DDR3L ×16) | ~1.0 A | ~0.45 A |
> | `vcc_1v8` (BUCK5) | ~0.3 A | ~0.18 A |
> | LDO1–9 (0.9 / 1.8 / 3.3 V analog) | ~0.3 A | ~0.30 A |
> | `vcc_3v3` switch (SD, NOR, panel VCI/IOVCC/VDDT, pull-ups) | ~0.3 A | ~0.30 A |
> | `vdd_npu` (BUCK4) — NPU unused on this board | ~0 | ~0 |
> | **Total** | | **≈ 2.2 A** |
>
> → pick a **≥4 A** part (headroom for transients + the unbudgeted NPU). **Not** on this rail: the CPU
> core (the RK860-0 runs straight off 5 V) and the backlight boost (5 V).
>
> **Input side (corrected for conversion loss).** ⛔ The three figures above are *output* powers, so each
> must be divided by its converter's efficiency — summing them and dividing by 5 V assumed lossless
> conversion: 7.25 W/0.90 + 3 W/0.90 + 0.95 W/0.85 ≈ **12.3 W ⇒ ~2.5 A at 5 V** (was stated as 11 W /
> 2.2 A). 2.5 A is **83 % of a 15 W supply, not 73 %** — in spec from a USB-C charger over C-to-C (3 A
> advertised), above spec for a USB-A port (500 mA USB2 / 900 mA USB3). ⚠️ **Under-powering is not a clean
> failure:** with no CC sensing the board draws unconditionally, and the RK809's **VCC9 under-voltage sense
> trips at 3.0 V on a 3.3 V rail**, so a weak source yields a *silent shutdown loop* rather than a
> diagnosable error — budget generously and watch the `vcc3v3_sys` test point.
>
> ⛔ **Note 2.5 A is the GPU-and-panel-at-full-tilt peak, not the bring-up draw.** Boot-to-console + NOR
> flashing, with the backlight and GPU idle, is a small fraction of it. So a hub port is plausible for the
> early milestones, and **a 3 A USB-C source (C-to-C charger, or a powered USB-C hub that passes data
> through to the NUC) is the answer** for the draw-to-the-screen goal — a host-side swap, not a board
> change. Measure it on
> board #1 before assuming either way. **Power-cycling does not depend on this** — that is `PWR_EN` on the
> on-board P-FET, which works regardless of the source.
>
> ✅ **Cross-checked against the RK809's per-rail maxima** (datasheet **Rev 2.3** Table 4-4 — newer than the
> Rev 2.1 used earlier): BUCK1 **2.5 A**, BUCK2 **2.5 A**, BUCK3 **1.5 A**, BUCK4 **1.5 A**, BUCK5 **2.5 A**,
> LDO1/2/4–9 **400 mA** each, LDO3 **100 mA**. Every load assumed above sits inside its rail's capability, so
> the ≈2.2 A figure is a realistic load, not an over-reach. **But note the 5 A buck is sized for the
> realistic case, not for every rail at its datasheet maximum simultaneously** — that hypothetical totals
> ~7 A on the 3.3 V input (the nine LDOs alone could pass ~3.3 A) and is unreachable with this board's
> loads (no NPU use, tiny analog draws, one 2 GB DDR3L bus). If peripherals are ever added to the LDO
> rails, redo this.

> **IO domains — the classic RK356x brick risk.** Each domain's pin voltage must match its physical rail,
> and the DT `pmu_io_domains` must match both. ROCK 3A: pmuio1/2 = 3.3 V, vccio1 = 3.3 V, **vccio2 =
> 1.8 V**, vccio3 = vccio_sd, vccio4 = 1.8 V, vccio5 = 3.3 V, vccio6 = 1.8 V, vccio7 = 3.3 V.
> **Change for this board:** ROCK 3A's 1.8 V flash domain exists for its eMMC. We have no eMMC and a 3.3 V
> NOR (FSPI = GPIO1_C7, GPIO1_D0–D4), so run the flash domain at **3.3 V** (and strap `FLASH_VOL_SEL` to match) —
> or keep 1.8 V and swap the **W25Q128JVSIQ** for the 1.8 V **W25Q128JW**. Confirm the domain/pin map in the RK3568 datasheet.

> **⚠️ Thermal is under-determined — three stacked unknowns.** (a) The 658-60AB's **natural-convection θ is
> not published** in the Digikey data; the 8–10 °C/W figure behind the choice is an *extrapolation* from its
> 2.0 °C/W @ 500 LFM (the one part listing both showed a 2.8× forced→natural penalty, and from 500 LFM it
> would be larger). (b) **The SoC's real dissipation is unverified** — the 4–6 W working figure comes from
> *our own* 11 W board budget reflected back through the converters, not from measurement or a Rockchip
> thermal spec. (c) Adhesive tape adds unbudgeted resistance (~0.8 W/mK). **Published small-sink numbers are
> sobering**: a 17 mm sink is **23.9 °C/W natural** (≈120 °C rise at 5 W) and a 15 mm one is 62.5 °C/W — so
> *small passive sinks do not work here*, and vendor "8 °C/W" headlines are all at 200–500 LFM, i.e. with a
> fan. **Plan for airflow if sustained GPU load is intended**; a 25–30 mm fan takes this from marginal to
> comfortable. Measure die temp via the RK3568 TSADC on the first board before trusting any of the above.

> **✅ USB VBUSDET — RESOLVED, and it was a real gap.** `USB3_OTG0_VBUSDET` is **ball M24**, the *only*
> VBUS-detect input on the chip (note the naming trap: the USB2 OTG0 PHY sits on the `USB3_OTG0_*` ball
> group; the `USB2_HOST2/3` pins have no VBUSDET at all). **It is fatal to the rescue path if omitted.**
> Rockchip's review checklist marks it *Mandatory* twice — *"`USB3_OTG0_VBUSDET` must also be connected"* —
> and the HDG explains the mechanism: before U-Boot the port is **device mode by default**, and in device
> mode the controller *"just judge[s] whether the VBUSDET pin is high, if it is high, DP will be pulled up
> and enumeration will start."* No VBUSDET ⇒ no DP pull-up ⇒ **the host never sees a device ⇒ no maskrom**.
> Mainline agrees: `phy-rockchip-inno-usb2.c` reads `utmi_bvalid` from USB2PHY0_GRF `0xC0` bit 9 as a
> **read-only status derived from M24 — there is no software override**.
>
> **Circuit (HDG Fig. 2-83; identical in Rockchip's AIoT ref, the Rockchip Tablet ref, ROCK 3A and
> Firefly ROC-RK3568-PC):** `VBUS —[10 kΩ]— M24`, with `15 kΩ` and `100 nF` from M24 to GND. 5 V × 15/25 =
> **3.00 V** = the checklist's stated TYP; 4.75–5.25 V VBUS → 2.85–3.15 V, inside the 2.7–3.3 V window.
>
> ⚠️ **Tap the connector's VBUS, not a board-internal always-on 5 V rail** — all four references tap the
> node the attached host actually drives. Off a permanent rail, VBUSDET reads high forever and Linux
> always believes a host is attached. **If VBUS could ever be PD-negotiated above 5 V**, add the Tablet
> reference's clamps (470 Ω + 5.1 V Zener on the 5 V side); at 9 V this divider would put 5.4 V on M24.
>
> **`USB3_OTG0_ID` (L23) can be left floating** — ~200 kΩ internal pull-up to `USB3_AVDD_1V8` reads high =
> device mode, which is what a rescue port wants. Device mode needs **no ID pin**: 5.1 kΩ CC pulldowns + VBUSDET is sufficient. The RK3568 has **no on-chip CC/TCPC**, so host mode on this port would
> need a real TCPC (Firefly uses an FUSB302B) — not wanted here.
>
> ✅ Also confirmed mandatory and already in the BOM: **`SARADC_VIN0`'s 10 kΩ pull-up to `vcca_1v8`** — it's
> the hard-wired, non-remappable recovery key, and the HDG says the pull-up *"must be reserved and cannot
> be deleted to ensure the default normal booting judgment."*

> **📋 Full checklist sweep done — all 31 sheets, 682 rows.** Rockchip's review checklist was walked
> end-to-end (not sampled): ~313 applicable rows, **647 marked *Mandatory***, ~367 N/A on unused
> interfaces. It found **14 Mandatory gaps**, all now in the rows above, plus these deviations to settle
> at capture:
>
> - **CPU-buck inductor:** the reference uses **0.24 µH** for VDD_CPU (PMIC R27: *"required to refer to the
>   reference diagram"*). Our single 0.47 µH covers all six bucks; the 0.47 µH/Isat≥3.5 A/DCR<50 mΩ spec is
>   for the **RK809** bucks only. Resolve against the RK860-0 datasheet (still unobtained).
> - ✅ **`USB2_AVDD_*` — CLOSED (ground R8/P9/P10; 0201 count 27 → 24).** Rockchip's unused-pins workbook
>   says `Tie VSS` for exactly these three balls under *"USB2.0 HOST2/USB2.0 HOST3"*, the only USB2-namespace
>   signals are HOST2/HOST3 (unwired here), and OTG0's HS path is `USB3_OTG0_DP/DM` on `USB3_AVDD_*`. Full
>   reasoning in the unused-pin note above. *(The references drive these nets only because they populate
>   USB-A host ports; we don't, so diverging is deliberate.)*
> - **Touch I²C pull-ups:** LCM&TP R10 wants them pulled to the **touch 3.3 V node** specifically. Our
>   2.0 kΩ value is fine (GT928 spec); make the *rail* explicit.
> - **`DDRPHY_VREFOUT`:** DRAM R10 wants **1 nF to VCC_DDR *and* 1 nF to GND** — pin both connections down
>   rather than "2 on VREFOUT".
> - **`PMIC_INT` pull-up** must go to `vcc3v3_pmu` (its PMUIO1 domain), not any 3.3 V rail.
> - **32.768 kHz crystal:** PMIC R57 (*Mandatory*) — *"if a metal cylinder is used, the metal case should be
>   grounded."* The SC-20S is a cylinder → add a grounded pad/clip under the can, or move to an SMD 3215.
> - **Reserve, no parts:** 4-pad **common-mode-choke footprints bridged by 0 Ω** on the 5 DSI pairs
>   (LCM&TP R13 says *reserve*); a **0.7 mm test pad on FSPI_CLK** promoted to an explicit line item
>   (FLASH R6 — the second maskrom entry), routed **daisy-chain not branch**; **SP3004-04XTG arrays** on the
>   2×25 header's SPI0 + GPIO group (GPIO R3/LCDC R17).
> - ⚠️ **Discharge circuit (PMIC R13, conditional — and the condition is met).** Our rescue loop *is*
>   "both-off → power-cycle → maskrom", and items above just raised the bulk to ~100 µF + ~100 µF.
>   **Measure the decay time and make the `PWR_EN` off-time exceed it**, or add a bleeder/active discharge. *(The on-board P-FET is the power-cycle now; uhubctl remains available as a coarser hard cycle.)*
> - ✅ **DRAM decoupling confirmed exact** against the reference's 2-chip DDR3 page (2× 10 µF + 7× 100 nF
>   per device) — the "estimates" caveat below can go.
> - ✅ Our **82.5 kΩ** BUCK3 divider and **1 kΩ** FLASH_VOL_SEL pull-down are both *more* conservative than
>   the reference's 82 kΩ / 2.2 kΩ — no change.
>
> The sweep also produced a long list of **layout-only** mandates (per-rail copper widths and via counts
> under the BGA, DDR skew/guard rules, 90 Ω USB / 100 Ω DSI differential targets, crystal guard-via
> spacing, TVS-at-connector topology). Those belong to the layout phase, not the BOM.

> **✅ SY6280 EN threshold — RESOLVED, and the 4.7 kΩ choice was necessary.** The datasheet gives an
> **absolute** threshold, not a fraction of V_IN (the worry that would have forced level shifters):
> **V_EN(H) = 2 V min**, V_EN(L) = 0.8 V max, EN leakage ≤1 µA, and *"Pull high to enable IC. **Do not
> float.**"* Against the SoC pins' worst-case 16 kΩ internal pull-down:
>
> | pull-up to `vcc3v3_pmu` | V at EN | vs 2 V threshold |
> |---|---|---|
> | **4.7 kΩ** | **2.55 V** | ✅ 0.55 V margin |
> | 10 kΩ | 2.03 V | ⛔ **0.03 V — effectively fails** |
> | 2.2 kΩ | 2.90 V | ✅ more margin if wanted |
>
> So the 10 kΩ I originally specified would have been unreliable, not merely tight. Other confirmed
> specs: **IN UVLO 2.3 V max** (fine at 3.3 V), I_Q 25 µA enabled / 0.1 µA off.

> **📄 Pin assignment is now done — see [PINOUT.md](./PINOUT.md).** All 636 balls cross-checked between
> Rockchip's two pinout sheets, every mainline `rockchip,pins` triple verified, 52 assignments on 52
> unique balls with **zero collisions**. ✅ **No function lands in a 1.8 V-only domain, so no level
> shifters are needed** — the only 1.8 V signal we use is `SARADC_VIN0` (a dedicated analog ball, which
> gets a warning label on the header). Every selectable IO domain is set to **3.3 V**, uniformly, which is
> itself the defence against mis-setting one. Two findings there are brick-class and are reflected in the
> rows above: **`FLASH_VOL_SEL` needs a 1 kΩ pull-down** (10 kΩ computes to 1.27 V and fails V_IL, and
> there is no software rescue), and **`GPIO0_A5` is driven LOW before U-Boot** — had it been used for
> `NOR_EN` the board could never have booted from NOR. **`SDMMC0_DET` also turns out to be a second boot
> strap** (card-detect selects SDMMC0 vs ARM JTAG on the whole VCCIO3 bank), which promotes the socket's
> CD contact from convenience to correctness — and hands us free **SWD on balls H26/J23**.
>
> ⚠️ **The DSI0 PHY rails are OFF at power-on.** `MIPI_DSI_TX0_AVDD_0V9/1V8` come (via the mandated
> ferrite beads) from RK809-5 **LDO1** and **LDO9**, both **OFF by OTP default** — so **software must
> enable them before the panel can light.** Expect a dark panel on first boot until they're marked
> `regulator-always-on`.
>
> **Free second maskrom entry:** Rockchip's reference annotates a test point on **FSPI_CLK (ball A22)** —
> *"if FSPI_CLK = 0 V at/after power on, the system will enter Maskrom mode"* — and the checklist explains
> it's there for *"when the wrong firmware is programmed (the RECOVERY key does not work at this time)."*
> One 0.7 mm pad buys a second hardware rescue path alongside the SY6280 gating. Independent confirmation
> of this board's core thesis that recovery must live below your code.

> **Boot + recovery — production-faithful NOR boot *with* a hardware rescue.** The BootROM order is
> **FSPI → NAND → eMMC → SDMMC0 → USB maskrom**, fixed in silicon (we fit neither NAND nor eMMC, so those two probes fail on unpopulated pins and fall through). The danger isn't a *corrupt* NOR image (the BootROM
> validates the header, rejects it and falls through on its own) — it's a **valid-but-buggy** one: it passes
> validation, gets jumped to, hangs in DDR init, and the BootROM is already gone. No retry, no software path
> back. **The ROM's fallback is a load-failure guard, not a bad-logic guard** — exactly the STM32MP157
> `fsbl1`/`fsbl2` lesson. Recovery therefore has to live *below* your code, and entering maskrom is a
> hardware decision made before any user code runs, so it must be a hardware act.
>
> **The mechanism (the three SY6280 load switches): gate each boot medium's power.** Kill a medium's VCC and the BootROM's probe of it
> fails. This beats the usual community trick of shorting a flash pin to GND — no bus contention with the
> SoC, and it needs no unverified pin guess.
>
> | NOR pwr | SD pwr | Result |
> |---|---|---|
> | **ON** | OFF | boots **NOR** ← the production-faithful case you want to test |
> | OFF | ON | boots **SD** |
> | **OFF** | **OFF** | **maskrom** — deterministic rescue, independent of anything flashed |
> | ON | ON | boots NOR (higher priority); also lets you test the priority itself |
>
> **Remote loop:** both-off → power-cycle (**`PWR_EN` low→high on the on-board P-FET** — better than uhubctl: soft-started, and it works whether or not the hub implements per-port switching. uhubctl still works as a coarser hard cycle) → maskrom → `rkdeveloptool ul` the
> new NOR image → NOR-on/SD-off → power-cycle → **boots from NOR exactly as production would**. Unbrickable
> and repeatable.
>
> **Driving the two control lines costs no extra rig hardware:** take them from the **console USB-serial
> adapter's DTR and RTS** (3.3 V logic on a CP2102N/FT232, software-togglable) — the same trick ESP32 boards
> use for auto-reset. The adapter is already wired for the console.
>
> ⚠️ **Verify on the bench:** (a) an unpowered flash can be weakly back-fed through its ESD diodes from the
> SoC still driving FSPI CLK/CS — add series resistors or confirm the BootROM really sees "no flash";
> (b) the T113 work flagged that an **SD card can be parasitically back-fed through CLK/CMD/DAT**, so
> VDD-off alone may not hide it — gate SD CLK too if it doesn't. **NOR gating is the clean one; SD is the
> one to validate.**
>
> **Iterating without risking NOR at all:** `rkdeveloptool` has **`db <loader>` = DownloadBoot, which runs a
> bootloader from RAM and writes nothing** (verified in its source; distinct from `ul`/`wl`, which do write).
> Use `db` for fast iteration, then `ul` to NOR once a build is proven — same shape as your T113 FEL loop.
>
> **Stage chain:** TPL (**Rockchip `ddr.bin` blob** — no open RK3568 DDR init) → SPL → TF-A BL31 → U-Boot →
> kernel. **Operational rule until the media switches are populated: do not write a bootable image to NOR** — with no media
> switches there is no remote way back from a hung bootloader.

> **32.768 kHz clock to the SoC is OPTIONAL.** RK809 pin 68 `CLK32K` (open-drain, enabled by default) can
> feed the SoC's `CLK32K_IN` on **GPIO0_B0** (a pinmux function, not a dedicated pin) — it only improves
> deep-sleep power; otherwise the SoC uses its own oscillator. **None of Rockchip's three reference boards
> actually routes it to the SoC.** If used: 10 kΩ pull-up to `vcc3v3_pmu` + 22 Ω series, and the amplitude
> must satisfy the PMUIO2 domain. **Debug trap (HDG):** the RTC oscillator is deliberately weak — *"using an
> ordinary oscilloscope on XOUT or XIN cannot detect the oscillation, or the probe will stop oscillation when
> it touches it. Please test the CLK32K pin."* Layout: guard XIN/XOUT with ground the whole way, GND via
> every 400 mil, nothing high-speed underneath.

> ⛔ **The debug UART will back-power the board through its RX pin — fixed with one resistor value.**
> A USB-serial adapter idles its **TX high at 3.3 V**, and the console stays plugged in while `PWR_EN`
> cycles board power, so this is *continuous*, not a momentary plug-in event. `UART2_RX_M0` is **ball AC20
> (`GPIO0_D0`), which sits in PMUIO2 ⇒ `vcc3v3_pmu`** — the always-on PMU domain that also owns the
> `RESETn` pull-up and `PMIC_INT`. With the board off that rail is 0 V, so through a 100 Ω series the
> adapter injects **(3.3 − 0.7) / 100 ≈ 26 mA** into the SoC's ESD clamp: at or above typical per-pin
> injection limits, and enough to hold `vcc3v3_pmu` partially up and muddy POR — the worst domain to do it
> in. **Fix: `UART2_RX` series = 1 kΩ, not 100 Ω** ⇒ ≈2.6 mA. `UART2_TX` stays at 100 Ω (SoC→adapter; the
> SoC's output is high-Z when off, so that direction injects nothing). ✅ **This is not a checklist
> deviation in spirit** — Debug-UART R2's 100 Ω exists for ESD/surge series impedance, and 1 kΩ provides
> *more*. At 1.5 Mbaud, 1 kΩ into ~20 pF is a ~20 ns edge against a 667 ns bit, so there is no SI cost.
> *(Host-side alternative, no board change: put the console adapter on a uhubctl port and cut it together
> with `PWR_EN`. The resistor is better — passive, and it cannot be forgotten.)*
>
> **Pins (mainline pinctrl):** console **UART2_M0: TX = GPIO0_D1, RX = GPIO0_D0, 1,500,000 8N1, 3.3 V**
> (use a CP2102N/FT232 — many CH340s can't do 1.5 Mbaud). **I²C0: SCL = GPIO0_B1, SDA = GPIO0_B2**
> (RK809 0x20 + RK860-0 **0x40**, not the ROCK 3A's 0x1c). **PMIC_INT = GPIO0_A3**, **PMIC_SLEEP = GPIO0_A2**, **TSADC_SHUT
> (GPIO0_A1) → RK809 reset** (hardware over-temp shutdown). SD card-detect = GPIO0_A4.

> **SD:** run 3.3 V only (drop `sd-uhs-*`, pin LDO5 at 3.3 V) — no 1.8 V switching on a bring-up board.

> **Board:** **6-layer** (Rockchip's recommendation; DDR on L1/L6 over solid GND). 0.65 mm FCCSP → via-in-pad
> (JLC POFV, **not** HDI). X-ray inspection; bake MSL3 parts. DDR: 2× ×16 single rank, T-topology per the
> Rockchip DDR3 template; run **DDR3-1066** first. Details: [RK3568-DDR3-ROUTING-STUDY.md](../gameboy-v4/pcb/RK3568-DDR3-ROUTING-STUDY.md).
