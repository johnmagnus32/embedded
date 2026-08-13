# gameboy-v3 PCB — Bill of Materials (WORKING DRAFT)

**Status:** Early draft / planning. This is the integrated **gameboy-v3 handheld** board:
a T113-S3 running mainline Linux, adding a proper feature set on top of the proven
`t113-breakout` core.

**Target feature set (this revision):**

- ✅ **Better screen** — 5" 800×480 IPS, RGB666 parallel (not the 2.4" ILI9341 SPI panel)
- ✅ **Haptics** — vibration feedback (LRA/ERM via a haptic driver IC)
- ✅ **Motion** — 6-axis IMU (accel + gyro) for tilt controls
- ✅ **Sound** — I2S Class-D amp + speaker
- ✅ **Battery** — 1S LiPo, USB-C charge + power-path, on-screen %
- ✅ **Bluetooth** (onboard) — **Ezurio BT830-SA** (CSR8811, UART HCI, integrated antenna, BlueZ); dual-mode → headphones + controllers; see §8 #99. **WiFi: NOT on the board** — the USB1-host dongle port was **dropped** (decided: no internal USB1 port), so wireless is **Bluetooth-only**. *(No JLC/DigiKey WiFi module fit the T113's SDIO+mainline+onboard-antenna need; rather than add an internal USB1 port for a dongle, WiFi is left out. It can still be added later via a **USB-C OTG** dongle on USB0 — no board change.)*
- (carried over) NOR boot, microSD, USB-C, up to 10 gamepad buttons

**Sourcing legend (Src column) — do we have a real, confirmed part number?**
- ✅ **Have a part #** — a concrete part is chosen and its LCSC # is confirmed sourceable
  (real PN, in stock, JLC tier known). Ready to drop into the order.
- ⚠️ **Candidate only** — a plausible pick from design analysis, but the exact LCSC # /
  stock / JLC tier is **not yet confirmed**.
- 🔲 **TODO** — no part chosen yet; we pick one together.

> **Note:** this column is *only* about part-number confidence. Whether a part is also
> **silicon-proven** (actually booted on `t113-breakout` Rev-A / `gameboy`-v1) is a separate
> property called out in the row's **Note** — a `✅` part may still be new to this board.

**Fit legend (Fit column) — who physically installs it:**
- **JLC** — surface-mount part JLCPCB reflows onto the board (in the BOM+CPL you upload). Needs an LCSC #.
- **Hand** — external module / accessory you attach yourself (panel, LiPo cell, speaker, vibration motor,
  silicone button pads, thumbstick / WiFi modules). Sourced from DigiKey/AliExpress/etc., **not** JLC-placed.
- **PCB** — an etched-copper feature, not a placed component (e.g. an antenna keep-out or copper pattern). Zero parts. *(Currently unused — the carbon-contact button combs that needed this were dropped for all-tactile buttons.)*

> **Feature parts are mostly ⚠️/🔲 — they still need a live LCSC check.** The design rationale
> (interface choice, pin budget, driver support) is settled; the exact part numbers are not.
> See [DESIGN-NOTES](#design-notes--open-decisions) for why each interface was chosen and the
> pin budget it fits into.

---

## 1. Core SoC + Power + Clock + Storage (PROVEN — from `t113-breakout`)

This whole section booted mainline Linux to an interactive shell on the Rev-A breakout.
Reuse verbatim unless a feature below forces a change.

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 1 | 1 | SoC | T113-S3 | C5197687 | system | ✅ | JLC | Dual Cortex-A7, 128 MB in-package DDR3, eLQFP-128 |
| 2 | 1 | Power | TYPE-C-31-M-12 | C165948 | 5 V in | ✅ | JLC | USB-C receptacle (5 V in + USB2 data) |
| 3 | 1 | Power | FP6161KR-LF-ADJ | C77234 | **0.9 V core** buck, **input = SYS** | ✅ | JLC | Sync buck, SOT-23-5, 1 A, 0.6 V FB. **Silicon-proven** (t113-breakout). ⚠️ **Input = SYS** (BQ24074 OUT / battery power-path, ~3.0–4.4 V — NOT 5 V, which vanishes on battery). Core rail ~800 mA worst → ~80 % of 1 A, OK (scope-verify §10 V23). **🟠 H1 — RUN/enable net (NEW to v3; the breakout had no battery/soft-power split, so silicon-proof doesn't cover it):** do NOT tie RUN→SYS (core always-on = no true off + battery drain) and do NOT tie RUN→3.3 V (RUN abs-max = VIN(SYS), which sags to ~3.0 V < 3.3 V). **Drive RUN from the STM6601 EN (#94) through an RC (≥2 ms)** so the core enables only AFTER VCC-IO is valid — T113 §5.12.1: VCC-IO up ≥2 ms before VDD-CORE. RC parts = #9. |
| 4 | 1 | Power | SMNR4020-2.2UH | C135262 | 0.9 V core buck | ✅ | JLC | 2.2 µH 3.4 A shielded. Proven part; for the FP6161 core buck (#3). |
| 5 | 1 | Power | RS-03K6803FT | C140074 | amp mono strap | ✅ | JLC | 680 K 1% 0603 — **now only the MAX98357A SD/MODE mono-mode strap (#48)**. (Qty 2→1: the 3.3 V FB-divider R_top was removed — the 3.3 V rail is now the **TPS63021 buck-boost (#66)**, which is **fixed 3.3 V** with FB tied to VOUT via #67, so no external divider.) |
| 6 | 1 | Power | 0603WAF1503T5E | C22807 | FB divider (0.9 V core) | ✅ | JLC | 150 K 1% 0603 — R_bot of the **0.9 V core** FB divider (pairs with #7 = 75 K R_top → 0.6·(1+75/150) = 0.9 V; FP6161 0.6 V-FB proven). Qty 2→1 (the 3.3 V-rail R_bot was removed with the SY8089). |
| 7 | 1 | Power | 0603WAF7502T5E | C23242 | FB divider | ✅ | JLC | 75 K 1% 0603 — R_top, 0.9 V core |
| 8 | 1 | Power | CL05C100JB5NNNC | C32949 | FB comp | ✅ | JLC | 10 pF C0G 0402 feed-forward across the 0.9 V core R_top (#7) — REQUIRED for loop stability. Qty 2→1 (the 3.3 V-rail feed-forward was removed with the SY8089). |
| 9 | 2 | FP6161 RUN RC (H1) | 100 kΩ 0603 (C14675) + 100 nF 0603 (C14663) | C14675 / C14663 | STM6601 EN → R → RUN(pin1); C: RUN→GND | ✅ | JLC | **H1 fix.** RC delays core-buck enable so VDD-CORE comes up **≥2 ms after VCC-IO** (T113 §5.12.1). 100 kΩ×100 nF ≈ 10 ms (≫2 ms; drop C for a faster wake, floor ~2 ms). RUN driven from EN swings to the SYS level → RUN ≤ VIN always. Both reuse confirmed reels; **verify the delay on the RUN threshold (V_RUN ~1 V).** |
| 10 | 1 | Power | 0603WAF2400T5E | C23350 | DDR ZQ | ✅ | JLC | 240 Ω 1% 0603 — ZQ-calibration (T113 pin 47 → GND) |
| 11 | 1 | Storage | W25Q128JVSIQ | C97521 | SPI0 boot | ✅ | JLC | 128 Mbit SPI-NOR, SOIC-8, QE=1 |
| 12 | 1 | Storage | A-MicroTF-1.85A | C22467599 | SDC0 | ✅ | JLC | microSD push-push socket w/ card-detect |
| 13 | 2 | microSD ESD array | **Littelfuse SP3004-04XTG ×2** | C207280 | SDC0 CLK/CMD/DAT0-3 (#12) → clamp | ✅ | JLC | **2× 4-channel arrays → covers all 6 SD lines** (CLK, CMD, DAT0-3), 2 spare. **0.85 pF ultra-low-cap** — the key spec (fine for the 25–50 MHz SD clock; a 50 pF part would round the edges + hurt SI). **Unidirectional rail-clamp = correct for unipolar SD** (unlike the bipolar headphone lines). SOT-563, IEC 61000-4-2, 15 V clamp. C207280 confirmed on JLC page. *(One-part 6-ch SP3004-06 is NOT stocked on JLC → use 2× the 4-ch. User-insertable card slot = a real ESD entry point.)* |
| 14 | 6 | Storage | RC0603JR-0710KL | C99198 | SD/RESET | ✅ | JLC | 10 KΩ 0603 SD pull-ups (CMD+DAT0-3). **⚠️ M1: pull RESET to the 1.8 V VCC-RTC rail, NOT 3.3 V** — RESET (ball 27, I/OD) buffer power = VCC-RTC = 1.8 V (abs-max 2.16 V); a 3.3 V pull-up over-stresses the pin + injects ~80 µA into the RTC rail (clamps ~2.5 V). Tie the RESET pull-up to the 1.8 V VCC-RTC net (confirm the breakout's actual net). SD-line pulls stay on 3.3 V. |
| 15 | 1 | Clock | X322524MRB4SI | C70571 | HOSC | ✅ | JLC | 24 MHz crystal, SMD3225-4P, CL=18 pF |
| 16 | 1 | Clock | SC-20S 32.768kHz | C97607 | RTC | ✅ | JLC | 32.768 kHz RTC crystal, SMD2012-2P |
| 17 | 2 | Clock | 0402CG220J500NT | C1555 | xtal load | ✅ | JLC | 22 pF C0G 0402 (24 MHz load caps) |
| 18 | 2 | Clock | 0402CG180J500NT | C1549 | xtal load | ✅ | JLC | 18 pF C0G 0402 (32.768 kHz load caps) |
| 19 | ~25 | Decoupling | CC0603KRX7R9BB104 | C14663 | decoupling | ✅ | JLC | 100 nF 0603 X7R (per-power-pin + flash + SD + RESET) |
| 20 | ~9 | Decoupling | CL21A106KOQNNNE | C1713 | bulk | ✅ | JLC | 10 µF 16 V 0805 (bulk + SD VDD; **incl. VBUS input bulk cap** — see note) |
| 21 | ~4 | Decoupling | CC0603KRX5R6BB475 | C109456 | bulk | ✅ | JLC | 4.7 µF 0603 X5R — general bulk. **⚠️ M3: use 10 µF (0805, the #20/C1713 reel) at the 0.9 V core buck (#3) OUTPUT, not this 4.7 µF** (which is the FP6161 table minimum). VDD-CORE typ is 0.95 V and worst-case VFB (~0.867 V) is only ~17 mV over the 0.85 V floor before load-step droop, so don't skimp the core output cap. *(The old SY8089 3.3 V-output-cap warning is removed: the 3.3 V rail is now the TPS63021 buck-boost, whose stability-critical output cap is #71 = 3×22 µF.)* |
| 22 | 1 | Control | TS-1187A-B-A-B | C318884 | RESET btn | ✅ | JLC | 6×6 mm tactile — RESET button |
| 23 | 1 | Status | 19-213SYGC | C2986027 | status | ✅ | JLC | Green LED 0603 — power indicator |
| 24 | 1 | Status | 0603WAF5100T5E | C23193 | LED limit | ✅ | JLC | 510 Ω 0603 — LED current limit |

> **Note on input bulk cap (Rev-A lesson):** the Rev-A breakout browned out under sustained
> current because it omitted the input bulk cap. **This board MUST include ~10 µF bulk on the 5 V VBUS
> (charger IN) AND on the SYS node** feeding the buck-boost (#66 has its own input caps #70) + the 0.9 V
> core buck (#3). Covered by #20 if placed at those inputs — see `t113-breakout/REV-B-FIXES.md`.
>
> **⚠️ T113 datasheet corrections (verified against T113-S3-datasheet.md):**
> - **DDR3 is on a separate 1.5 V rail (VCC-DRAM, balls 48/49), NOT the 3.3 V rail.** The earlier
>   brownout wording ("3.3 V feeds DDR3") is imprecise — DDR current only reflects onto 3.3 V *if* the
>   internal LDOB is wired to VCC-DRAM. **Confirm the t113-breakout schematic path** (it's already
>   silicon-proven, so it works — just document which rail sources VCC-DRAM). Swings the 3.3 V budget ±400 mA.
> - **LDOB default = 1.35 V (DDR3L), but the SiP die is DDR3 (1.5 V)** — the DRAM-init firmware MUST
>   reprogram LDOB to 1.5 V (boot0 already does this on the working breakout; note it so it isn't lost).
> - **Internal LDO current limits are UNSPECIFIED** in the datasheet — the 3.3 V "1:1 reflection" brownout
>   math has no datasheet ceiling; treat those percentages as estimates, verify on silicon (scope the rail).
> - §1 support parts (ZQ #10, RESET pull-up, 24 MHz + load caps, per-ball decoupling) are inherited from
>   the **silicon-proven** t113-breakout — values are validated by that board, not re-derived from the datasheet.
>   The **240 Ω ZQ (#10) is the JEDEC nominal**, not stated in the datasheet — matches the proven board.

---

## 2. Screen — Zettler ATM0500D27-CT, 5" 800×480 IPS, RGB666 parallel  ✅ (panel LOCKED)

**Panel chosen: Zettler ATM0500D27-CT** (5" 800×480 IPS, 24-bit RGB parallel, FT7311 cap touch) —
datasheet-confirmed against all 4 gates (see below). **Interface: RGB666 DPI via TCON-LCD0**
(22 pins, entire PD bank). Not DSI (needs 4-layer controlled-impedance), not SPI (bandwidth-starved).
Mainline `sun4i_rgb` + `panel-simple` (`panel-dpi`) + `edt-ft5x06` touch — DT-only except one
`tcon_lcd0_out` RGB endpoint edit. **This kills SPI1** (RGB uses PD10-12 = SPI1's only pins).

| # | Qty | Role | Part (candidate) | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 25 | 1 | 5" IPS panel (+ FT7311 touch on-panel) | ✅ **Zettler ATM0500D27-CT** (AZ Displays / DigiKey) | DigiKey (module) | RGB666, PD0–21 | ✅ | Hand | **LOCKED — datasheet-confirmed.** Genuine **IPS** (80/80/80/80° verified), 24-bit RGB parallel (RGB666-wireable: drive 6 of 8 bits/color — **bit-to-pin map NOT datasheet-resolved, get Zettler's map; convention-safe default = 6 MSBs R7..R2, ground R1/R0**, see gate 3 below), "LCD controller N.A." (raw RGB, correct kind). Module 120.7×75.8×4.25 mm, active 108×64.8 mm. Logic 2.7–3.6 V (3.3 V ✓). ~$77, reputable vendor + real datasheet (`ATM0500D27-CT.pdf` in this dir). **Touch: FocalTech FT7311 capacitive is bonded on-panel** (not a separate part) — I2C, 2.8–3.3 V, on the 6-pin FFC (#27). Driver `edt-ft5x06` (`CONFIG_TOUCHSCREEN_EDT_FT5X06`): **FT7311 has no dedicated compatible** (verified against the of_match — edt-ft5206/5306/5406/5506 + focaltech-ft5426/5452/6236/8201/8719; **there is NO `GENERIC_FT` fallback** — earlier claim corrected). Bind as **`edt,edt-ft5506`** (a listed 10-point variant, per §10 V17) or add a 1-line `focaltech,ft7311` compatible (we build our own kernel) — works **if** the FT7311 uses the standard FocalTech report format, **confirm on the actual panel**. **NOT a BOM risk:** touch is bonded on-panel (can't swap without swapping the whole module) and is optional/deferrable for a gamepad — a software/bring-up item only. |
| 26 | 1 | Display FPC connector | **BOOMELE 0.5-40PFGPZ** — 40P 0.5 mm, bottom-contact, hinged-lid (ZIF), right-angle SMD | **C9160** ✓ | mechanical | ✅ | JLC | **JLC-stocked (Extended, Economic+Standard PCBA, EasyEDA footprint). Contact side CONFIRMED from datasheet §7 mechanical drawing:** FPC side-view shows Conductor(gold fingers) on the panel REAR face + Stiffener on the front → tail folds behind the module → contacts face the board = **bottom-contact** ✓ (matches this connector). Sanity-check the physical part on arrival (gold-finger side faces board). Confirm live stock qty. |
| 27 | 1 | Touch FFC connector | **HDGC 1.0K-1.5-6PWB** — 6P 1.0 mm FFC, **hinged-lid (flip-flap)**, bottom-contact, right-angle SMD | **C2919568** ✓ | mechanical | ✅ | JLC | **JLC-stocked (Extended, Economic+Standard PCBA, EasyEDA footprint).** For the SEPARATE 6-pin touch FFC (1=VDD 2=GND 3=SCL 4=SDA 5=INT 6=RST). **Same mechanism + contact side as the 40-pin display connector (C9160)** → both ribbons seat identically (flip-lid, bottom-contact). Latching (lid clamps) = good handheld retention. Contact side = bottom (matches datasheet §7: conductor on rear face, folds behind module); sanity-check gold-finger side on the physical ribbon. |
| 28 | 1 | **Backlight driver — WLED boost** | ✅ **TI TPS61165DBVR** — SOT-23-6, 3–18 V in → 3–38 V CC boost, 1.2 A sw, PWM+EasyScale, OVP+OTP | **C58756** ✓ | WLED CC boost + EN | ✅ | JLC | **JLC-stocked (Extended, Economic+Standard, EasyEDA footprint).** Single-string driver → drives the panel's 2-terminal LED+/LED− (3 parallel strings matched inside the panel). **⚠️ Feed VIN from SYS/5V-VBUS, NOT the 3.3 V rail** — verification found the boost's ~290 mA on the single-1A 3.3V buck pushes it to ~65% typ / ~1A worst-case (brownout class). From SYS/5V the buck drops to ~41% and boost duty improves (0.83→0.74). Set 40 mA via sense R. **Dimming** = EasyScale 1-wire or `pwm-gpio` (T113 has no mainline HW PWM → no smooth `pwm_bl`); **no dedicated I2C dimmer chip** — checked LM3630A/KTZ8866/MP3309C/AW99706, all JLC 0-stock, and the TPS61165 does its own dimming anyway. **Route the BL control pin to a PWM-capable GPIO** to keep software dimming open. **See §10 Verification Fixes.** |
| 29 | 1 | BL boost L | **SMNR4020-10UH** — 10 µH, 1.6 A, shielded, 4×4 mm | **C135263** ✓ | boost stage | ✅ | JLC | JLC-stocked; **same family as the t113-breakout core inductor (SMNR4020-2.2UH / C135262)** — one footprint family. 1.6 A ≫ ~0.6 A peak; 215 mΩ DCR fine at 40 mA (~18 mW loss). Confirm 10 µH vs TPS61165 app circuit. |
| 30 | 1 | BL boost diode | ✅ **MDD SS16** — 60 V, 1 A Schottky, SMA(DO-214AC) *(was SS14/40 V)* | **C2481** ✓ | boost stage | ✅ | JLC | **L7/V16: SS16 (60 V), not SS14 (40 V)** — SS14's 40 V sat only ~1 V above the ~39 V open-LED OVP clamp (too tight); 60 V clears it comfortably. **1:1 SMA(DO-214AC) swap — same footprint as SS14** (C2481 = MDD's SS16, sibling of the SS14 C2480). 1 A ≫ ~0.6 A peak. C2481 confirmed on JLC page. ⚠️ minor: **Extended tier** (SS14/C2480 was Basic → small per-reel feeder fee) + VF 700 mV@1A (vs SS14 550 mV — a few mW more diode loss, negligible). |
| 31 | 1 | BL boost **output cap (Cout)** | **YAGEO CC0805KKX7R9BB225** — 2.2 µF 50 V X7R 0805 | **C125847** ✓ | boost stage | ✅ | JLC | Fixes the derating flaw (0603 1µF dropped to ~0.5µF < TI's 1µF min): 2.2µF/50V/**0805 X7R** keeps well above 1µF at 19V bias. ≥50 V clears the ~38–40 V OVP clamp. (Extended tier — fine.) |
| 32 | 1 | BL boost **input cap (Cin)** | **reuse #21 — CC0603KRX5R6BB475, 4.7 µF 0603** | **C109456** ✓ | boost stage | ✅ | JLC | **No new part** — reuses the existing 4.7 µF 0603 already in §1. On the 3.3 V rail (needs only ≥10 V; this is fine). Just add 1 to the qty. |
| 33 | 1 | BL boost **FB sense R (Rset)** | **5.1 Ω, 1%, 0402** — sets the 40 mA LED current | verify (Basic) | boost stage | ✅ | JLC | **Datasheet-confirmed:** TPS61165 VREF = **200 mV** (196/200/204). `Rset = 0.200 V / 0.040 A = 5.0 Ω → E96 5.1 Ω`. **JLC search:** `5.1 ohm 0402` → filter 1% / Basic. ~8 mW. **This resistor is the brightness knob** — bigger R = dimmer; never exceed the panel's 40 mA max (LED lifetime). |
| 34 | 1 | BL boost **COMP cap** | ✅ **YAGEO CC0402KRX7R7BB224** — 220 nF X7R 16 V ±10% 0402 | **C326590** ✓ | TPS61165 COMP (**SOT-23 pin 5**) → GND | ✅ | JLC | **DATASHEET-MANDATORY loop-compensation cap** — **confirmed against the TPS61165 datasheet** (typical-app circuit shows 220 nF on COMP→GND; Pin Functions: COMP = SOT-23 **pin 5**, "connect an external capacitor to compensate the converter"). Sets the transconductance-error-amp compensation; **omit → the boost loop is unstable.** 16 V ≫ COMP abs-max (3 V). **Distinct value — NOT the 100 nF #19 reel.** Place close to pin 5. C326590 confirmed on JLC page (Extended tier; ±10% is fine for a compensation cap). |

> **The 4 gates — ALL CLEARED for the ATM0500D27-CT** (from its datasheet):
> 1. **IPS** — ✅ datasheet states IPS, viewing angle **80/80/80/80°** (genuine, not TN).
> 2. **Interface** — ✅ 24-bit RGB parallel, 40-pin FPC, "LCD controller N.A." (raw RGB — correct kind).
> 3. **18-bit RGB666** — ✅ 6 bits/color; keeps data on PD, off PB2-7/I2S audio. **🟠 WHICH 6 pins — NOT
>    datasheet-resolved (do not route on assumption).** The panel is a genuine **24-bit** part (pin table
>    lists the FULL `R0~R7/G0~G7/B0~B7`, §1 = 16.7 M colors) and the datasheet is **silent on bit order**.
>    An earlier note here leaned on **§3.1.2 Note 2** as an "active-input" list — that was a **misread**: Note 2
>    is the VIH/VIL *applicability* list and even names a `RESET` signal that **doesn't exist** in the 40-pin
>    pinout (pin 31 is DISP) → boilerplate, not authoritative. **Convention-safe default: drive the 6 MSBs
>    (`R7..R2`), ground the 2 LSBs (`R1/R0`)** — grounding the *low* pins caps nothing; grounding the wrong
>    pair (R7/R6, the MSBs) would cap every channel at 63/255 ≈ 25% (dark, no true white). **⚠️ GET ZETTLER'S
>    explicit R/G/B bit-to-pin map before routing** — treat neither the 6-of-8 selection nor MSB order as
>    settled. See §10 V4.
> 4. **Touch chip** — ⚠️ **FocalTech FT7311** (not GT911), **I2C addr 0x38** (FocalTech-family default —
>    the panel datasheet does NOT state an address; confirm against the FT7311 controller datasheet):
>    `edt-ft5x06` has **no** FT7311 in of_match and **no `GENERIC_FT` fallback** — bind as `edt,edt-ft5506`
>    (a listed 10-pt variant) or add a 1-line `focaltech,ft7311` compatible (we build our own kernel).
>    Software-only, non-blocking. Need 2 GPIOs for INT+RST. *(Gates kept here as the checklist for any future/alternate panel.)*
>
> **DT scaffold + timings:** the datasheet §3.2.4 DOES give a numeric timing table — **DCLK 23/25/27 MHz
> (typ 25), 27 MHz MAX** — so author `panel-dpi` from THOSE values, **NOT** the Ampire 33.3 MHz template
> (33.3 MHz would exceed the panel's 27 MHz max). pll-video0 300 MHz ÷12 = **25.00 MHz** exactly. Drive in
> **SYNC-DE mode** (datasheet confirms). Wire onto `lcd_rgb666_pins` (PD0–21) + the `tcon_lcd0_out` RGB
> endpoint. **⚠️ The DT/kernel bring-up is bigger than "one edit" — see §10 V5** (enable `&de`, pinctrl,
> configs, verify T113 parallel-RGB TCON support). Interface path present in-tree; parallel-RGB support is
> the open risk.

---

## 3. Motion (IMU) + Haptics  ⚠️  (share the sensor I2C bus)

Both are small I2C peripherals on the **one shared sensor I2C bus** (mainline function
name is `"i2c1"`, *not* `"twi1"`). No address clash with touch.

| # | Qty | Role | Part (candidate) | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 35 | 1 | IMU (6-axis) | ✅ **LSM6DSOXTR** | **C481766** ✓ | I2C 0x6A + INT1/INT2 | ✅ | JLC | **Datasheet-verified.** Mainline `st_lsm6dsx`, DT compat `st,lsm6dsox`. Adafruit breakout for bring-up. ⚠️ **INT1 (pin4) mode-select trap: do NOT pull INT1 to VDD_IO** (that selects I3C-only, disables I2C) — route to a GPIO or leave (internal pull-down = I2C). **M6: add an explicit ~10 kΩ pulldown at INT1 (#40) + set its pinmux input-no-pull through boot** — a stray `bias-pull-up` or a pin driven high at power-up latches the part I3C-only (= dead IMU; T113 has no I3C). No 5V tolerance; allow 35 ms boot. |
| 36 | 1 | IMU VDD decoupling | 100 nF 0603 | **C14663** ✓ | LSM6DSOX VDD (pin 8) | ✅ | JLC | **Datasheet-required** (§7.1 Fig 24). MISSING before. Reuse #19 reel. |
| 37 | 1 | IMU VDDIO decoupling | 100 nF 0603 | **C14663** ✓ | LSM6DSOX VDDIO (pin 5) | ✅ | JLC | **Datasheet-required** (§7.1). MISSING before. Reuse #19 reel. |
| 38 | 1 | IMU CS pin tie | 0 Ω / short to VDD_IO | — (net/0Ω) | LSM6DSOX CS (pin 12) → high | ✅ | JLC | **MANDATORY: CS high selects I2C** (§5.1) — without it the part boots SPI and won't answer on I2C. |
| 39 | 1 | IMU SA0 address strap | tie SDO/SA0 (pin 1) → GND | — (net) | sets I2C addr 0x6A | ✅ | JLC | **MANDATORY: pin 1 floats by default** (undefined addr). GND = **0x6A** (matches our config). VDD_IO would = 0x6B. |
| 40 | 1 | LSM6DSOX INT1 pulldown (M6) | 10 kΩ 0603 (reuse C99198) | C99198 | INT1(pin4)→GND | ✅ | JLC | **M6 fix.** External pulldown + pinmux input-no-pull through boot → guarantees I2C (a stray bias-pull-up / driven-high pin at POR would latch I3C-only = dead IMU). Reuses the 10 kΩ reel. |
| 41 | 1 | Haptic driver | ✅ **DRV2605LDGSR** | **C527464** ✓ | I2C 0x5A, EN→3V3 | ✅ | JLC | **Confirmed in JLC stock (Extended, VSSOP-10, Economic+Standard PCBA, EasyEDA symbol avail).** Mainline `ti,drv2605l`, FF_RUMBLE. Motor wires straight to OUT± (no FET/flyback). Adafruit #2305 breakout for breadboard bring-up. |
| 42 | 1 | Haptic REG cap (DRV2605L) | 1 µF X5R 0603 | **C15849** ✓ | REG→GND | ✅ | JLC | **Datasheet-MANDATORY** for internal-LDO stability (without it the part may not enumerate on I2C). Place <few mm from REG pin. Reuse the C15849 reel (Samsung CL10A105KB8NNNC, 1 µF/50 V/X5R). |
| 43 | 1 | Haptic VDD bypass (DRV2605L) | 100 nF 0603 | **C14663** ✓ | VDD | ✅ | JLC | HF bypass (§9.2.2.2). Reuse #19 reel. **Note: datasheet mandates C(VDD) ≥1µF (Rev C→D change) — satisfied by the 10µF bulk row below;** this 100nF is the close-in HF cap, not the required C(VDD). |
| 44 | 1 | Haptic IN/TRIG tie (DRV2605L) | tie IN/TRIG → GND (0 Ω) | — (net/0Ω) | IN/TRIG pin | ✅ | JLC | **REQUIRED for I2C-only internal-trigger mode** (§9.2.2.3) — floating is out of spec. MISSING before. |
| 45 | 1 | Haptic VDD bulk (DRV2605L) | 10 µF 0805 | **C1713** ✓ | VDD | ✅ | JLC | Local bulk — sources the pulsed motor/overdrive current so it isn't pulled through the shared rail. Reuse the #20 reel. |
| 46 | 1 | Actuator (LRA) | ✅ **Vybronics VG0832022D** — 8 mm coin LRA, 235 Hz, 1.8 VAC, wire leads | [DigiKey 9974288](https://www.digikey.com/en/products/detail/vybronics-inc/VG0832022D/9974288) | via DRV2605L OUT± | ✅ | Hand | **Chosen final-board LRA** (spec-sheet-confirmed; Hz-rated = true LRA). **Record 235 Hz → DRV2605L LRA config.** Hand-added mechanical part (like speaker/cell); leads **solder direct to the OUT± pads** — same as the speaker (#52), **no connector** (DECIDED: direct-solder, not a JST). Just two through-hole/SMD solder lands on OUT± in the layout. Bench-test with a cheap ERM coin (Adafruit #1201) — 2 wires on OUT±. |

> **Cheaper haptics alternative (no IC):** `gpio-vibra` / `pwm-vibra` drive an ERM motor via a
> transistor + flyback diode straight off a GPIO/PWM. Lower quality than DRV2605L closed-loop
> LRA, but fewer parts. Decision TODO.

---

## 4. Sound — I2S Class-D amp + speaker  ⚠️

On-chip T113 codec has **no mainline driver** (and only ~8 mW) → use I2S + external amp,
same proven approach as gameboy-v1. On `i2s1` (or `i2s2` fallback on the free PB bank).

| # | Qty | Role | Part (candidate) | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 47 | 1 | Class-D amp | ✅ **MAX98357AETE+T** — I2S mono Class-D, TQFN-16 | **C910544** ✓ | I2S1 (BCLK/LRCLK/DIN) | ✅ | JLC | **Silicon-proven on gameboy-v1.** Mainline `maxim,max98357a`; no MCLK needed. |
| 48 | 1 | Amp SD/MODE resistor | RS-03K6803FT | C140074 | MAX98357A SD/MODE → **3.3 V** | ✅ | JLC | Sets **(L/2 + R/2) mono** mode. **Pull rail decided = 3.3 V; resistor = 680 KΩ (same part as #5 — reused, not a new line item).** Datasheet-verified (Tables 5/6 + trip points): SD_MODE has an internal **100 KΩ pulldown** (RPD 92–108 K) → 680 K/3.3 V gives pin ≈ **0.42 V**, centered in the (L/2+R/2) window **0.24–0.65 V** (>130 mV margin on both edges, all tolerances). Datasheet-exact RLARGE@3.3 V = 634 K (C65831, Extended) — 680 K is well within margin and reuses a proven part. **Boot-safe:** if 3.3 V lags at power-up, RPD holds SD_MODE low → amp stays in shutdown until logic is up. |
| 49 | 1 | Amp GAIN_SLOT strap | **GAIN_SLOT → VDD (direct tie, no resistor) = 6 dB** | — (net) | MAX98357A GAIN_SLOT→VDD | ✅ | JLC | **CHANGED to 6 dB (M5) — was 100 KΩ→GND = 15 dB, which HARD-CLIPS.** At 15 dB the DAC FS (1.27 Vrms) × gain ≈ 7.16 Vrms ≫ the ~2.97 Vrms rail limit at 4.2 V → clips above −7.6 dBFS. **GAIN_SLOT→VDD (6 dB)** maps 0 dBFS to ~2.54 Vrms ≈ 0.8 W into 8 Ω = the CES-2704 rating, no clipping. Net tie, no part (the C14675 100 KΩ instance is dropped here). Keep the ALSA softvol cap as the continuous-power backstop. |
| 50 | 1 | Amp VDD bypass | CC0603KRX7R9BB104 — 100 nF 0603 | **C14663** ✓ | MAX98357A VDD | ✅ | JLC | HF decoupling at VDD. Reuses the #19 reel (bump qty). |
| 51 | 1 | Amp VDD bulk | CL21A106KPFNNNE — 10 µF 0805 | **C17024** ✓ | MAX98357A VDD | ✅ | JLC | VDD bulk (sources audio transients) — datasheet-recommended (0.1µF + 10µF on VDD). v1 part. **Note: MAX98357A has a single VDD pin, no "PVDD"** (label corrected). |
| 52 | 1 | Speaker | ✅ **Same Sky CES-2704-088L050** — Ø27×4.9 mm, 8 Ω, 0.8 W, 90 dB, enclosed, bare wire leads | [DigiKey 10821313](https://www.digikey.com/en/products/detail/same-sky-formerly-cui-devices/CES-2704-088L050/10821313) | amp OUT± (solder leads) | ✅ | Hand | Enclosure-included (sealed cavity), thin. Datasheet: freq **500 Hz–20 kHz** (NOT the "50 Hz" the DigiKey listing shows — listing error), Fo 850 Hz. Leads solder direct to OUT± pads (no connector — same as motor). ⚠️ **0.8 W rated / 1 W max — the MAX98357A can push ~1–2 W into 8 Ω, so cap ALSA softvol** to avoid over-driving. Louder alt if needed: PUI ASE02008MS (97 dB). |

> **Software volume is not automatic.** The MAX98357A (#47) path exposes no ALSA volume kcontrol → add an ALSA
> **softvol** plugin + a small userspace daemon that turns the KEY_VOLUME events into mixer changes. Trivial,
> but it's userspace policy you write, not a freebie. (Also the softvol cap that protects the 0.8 W speaker #52.)

---

## 5. Battery + Charging + Power-Path  ⚠️  (see the separate battery design)

1S LiPo, USB-C charge + run (power-path), buck-boost 3.3 V rail, MAX17048 fuel gauge (GPADC divider backup).

> **§5 status (after the BQ24074 + CH224K datasheet passes):** charger (#53) + CH224K (#61) + their
> support parts are **datasheet-verified** and have confirmed LCSC #s; still **not silicon-proven**.
> **Architecture = "Option 1" (simplified):** the input current limit is a **single fixed 1.1 kΩ ILIM
> resistor** (#58); the old 2N7002 FET + CH224K-flag "boost" was **DELETED** — the CH224K is a *voltage*
> negotiator (no usable ≥1.5 A current flag), and the charger's own DPPM + battery-supplement handle weak
> sources. **Charge-while-playing** works on any real charger; a laptop SDP port stays 500 mA and the
> battery supplements. **§5 datasheet passes DONE:** BQ24074 charger + CH224K PD sink + **TPS63021 buck-boost**
> (#66 + support #67–e/#72) all verified — the buck-boost sources ~1.9–2.4 A at the 3.0 V dead-battery floor
> (~2.5× the 745 mA worst case). **§5 FULLY SOURCED** — every row #53–73 has a JLCPCB-confirmed part #; ISET
> sized to the 2000 mAh cell (#73, Adafruit 2011), fuel gauge = **MAX17048 (#77, ~±1 %)** with the GPADC divider (#75/#76) as backup. **Two residual non-sourcing
> caveats to carry to layout:** (1) **battery connector polarity** (#74) — + pad must match the Adafruit cable
> (red=+) → BQ24074 BAT, reversed = destroyed cell; (2) **CH224K VBUS-R value** (#65, 10 kΩ) is from my
> datasheet-schematic read, worth a glance at a full CH224K datasheet (low risk — a 1 kΩ also works).

| # | Qty | Role | Part (candidate) | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 53 | 1 | Power-path charger | BQ24074RGTR | C54313 | autonomous; /CHG,/PGOOD open-drain | ✅ | JLC | QFN-16-EP 3×3, JLC _Extended_. True DPPM power-path — system on **OUT/SYS, not BAT**; runs with battery absent (**OUT regulates to VO(REG) ≈ 4.4 V** with no cell — the "clamps to 3.4 V" was '72 behavior, not the '74; M11). **Datasheet-verified pin config:** **CE→GND** (tie low = charging enabled; internal 285 kΩ, must not float), **🔴 EN1→GND + EN2→HIGH (strap to OUT/SYS, NOT IN) — REQUIRED (B1):** EN1/EN2 both have internal 285 kΩ pulldowns; left floating = `0,0` = **USB100 = 100 mA input** and the ILIM resistor (#58) is **INERT** (ILIM only acts in EN2=1/EN1=0). Kills charge-while-play + dead-cell instant-on. EN abs-max = 7 V → strap EN2 to the always-on **OUT** node (3.4–4.4 V), **never IN** (IN tolerates 28 V on a mis-plug). Strap = #60. **TMR floats** = 5 h default timer (fine), **ITERM floats** = 10 % default (fine). Input limit ← ILIM (#58); charge current ← ISET (#57). **Thermal:** 1.5 A linear charge into a dead cell ≈ 2.4 W → TJ > 125 °C → chip throttles, so **ISET is set well below 1.5 A** (the 1.5 A is the INPUT budget for run+charge, not a fast-charge target). **MANDATORY: exposed pad → VSS with a via array** (primary heat path). OVP 10.5 V (never trips at 5 V; IN survives 26 V so a mis-plugged 9 V won't destroy it). PN + all support datasheet-verified; **not yet silicon-proven** on this board. |
| 54 | 1 | Charger IN cap | CL10A475KO8NNNC | C19666 | BQ24074 IN(pin13)→VSS | ✅ | JLC | Datasheet-required IN bypass, 1–10 µF, **must stay <10 µF** (USB-IF inrush). 4.7 µF **16 V** X5R 0603 (Samsung), JLC **Basic**. **NOT the #21 reel** — that part (C109456) is only 10 V, too low for the IN node which can see the OVP window; this is a dedicated 16 V part. Place local to pin 13. |
| 55 | 1 | Charger OUT cap | CL21A106KOQNNNE | C1713 | BQ24074 OUT(pins10/11)→VSS | ✅ | JLC | Datasheet-required OUT bypass, 4.7–47 µF. 10 µF/16 V 0805, reuse #20. **OUT = the SYS node** feeding the buck-boost (#66) + whole board → architecture-critical; place close to the IC. |
| 56 | 1 | Charger BAT cap | CL21A106KOQNNNE | C1713 | BQ24074 BAT(pins2/3)→VSS | ✅ | JLC | Datasheet-required BAT bypass, 4.7–47 µF. 10 µF/16 V 0805, reuse #20. Place near the JST-PH (#74). |
| 57 | 1 | Charge-current resistor (ISET) | 0603WAF2001T5E | C22975 | BQ24074 ISET(pin16)→VSS | ✅ | JLC | **Mandatory — open ISET = the part does NOT charge at all.** ICHG = KISET/RISET, KISET = 797/890/975. **2.0 kΩ 1% 0603 (Basic) → ~445 mA typ (≤488 mA at max-K, ~399 min).** Sized conservatively for the LiPo (#73, now Adafruit 2011 2000 mAh): ~445 mA ≈ 0.22C — safe/gentle for the linear charger. (Could raise toward ~1 A/0.5C for faster charge on the 2000 mAh, at the cost of more charger heat — optional.) 1 % required (RISET short-test). C22975 confirmed on JLCPCB page (2 kΩ / 1% / 0603 / Basic). |
| 58 | 1 | Input-limit resistor (ILIM) | 0603WAF1201T5E | C22765 | BQ24074 ILIM(pin12)→VSS | ✅ | JLC | Input current limit, IIN = KILIM/RILIM (KILIM ≈1610 typ / 1550 TI-example). **1.2 kΩ (Basic) → ~1.34 A typ input** — chosen over the datasheet-floor 1.1 kΩ (C22764, ~1.46 A but _Extended_) to stay Basic; the difference is negligible for charge-while-play (~6.7 W input ≫ 2 W system + charge). **Never go below 1.1 kΩ** (IN abs-max 1.6 A) — 1.2 kΩ is safely above. **Single fixed resistor** — Option 1: FET-switched boost DELETED (CH224K has no usable ≥1.5 A flag; charger DPPM + battery-supplement handle weak sources — see §9). Tradeoff: VIN-DPM back-off inactive at fixed ILIM; compliant ports self-limit + battery supplements. C22765 confirmed on JLCPCB page (1.2 kΩ / 1% / 0603 / Basic). |
| 59 | 1 | TS NTC-defeat resistor | RC0603JR-0710KL | C99198 | BQ24074 TS(pin1)→VSS | ✅ | JLC | **Mandatory — our 2-wire cell (#73) has no NTC, so TS floats → reads "too cold" → charging suspended.** The '74 has NO float-disable (unlike '72/'73), so this is required, not optional. 10 kΩ 0603 (75 µA × 10 k = 0.75 V, mid-window between VHOT 0.3 V / VCOLD 2.1 V). Reuse #14. |
| 60 | 1 | BQ24074 EN2 strap (B1) | 0 Ω 0603 (reuse C21189) | C21189 | EN2(pin5)→OUT/SYS; EN1(pin6)→GND (net) | ✅ | JLC | **B1 fix.** EN2→OUT puts the charger in ILIM-resistor mode (EN2=1/EN1=0) so #58 actually works; EN1→GND is a net (no part). Tie EN2 to **OUT** (always-on 3.4–4.4 V < 7 V EN abs-max), never IN. Reuses the confirmed 0 Ω reel (#67/#68). |
| 61 | 1 | **USB-PD sink** (charge-while-playing) | CH224K | **C970725** | CC1/CC2 from receptacle; **CFG1=high → 5 V** | ✅ | JLC | WCH CH224K, ESSOP-10, JLC _Extended_. USB-PD/QC/BC1.2 **voltage** negotiator (**not** a current-flag source — that premise was wrong). **Requests 5 V** via CFG1=high (#64); **CFG2/CFG3→GND** (X for 5 V; 'K has no internal pulldown → must tie). Autonomous, no T113 firmware, works from a dead battery. **⚠️ CC1/CC2 → this IC ONLY — no external 5.1 KΩ CC pulldowns** (has its own Rd PHY; the t113-breakout HAD them — don't copy). **PG(pin10)** open-drain active-low: **not used to gate current** (its trip semantics are undocumented in this brief manual, and a 500 mA port also presents 5 V) → leave open, or pull-up to 3.3 V only if the T113 reads "adapter present." ⚠️ this is WCH "manual 1" (overview); CFG/PG detail unverified vs a full datasheet (Option 1 accepted that). Charge-while-playing works on any real charger; a laptop SDP port stays 500 mA (battery supplements). |
| 62 | 1 | CH224K VDD series R | 0603WAF1001T5E | C21190 | 5 V VBUS → CH224K VDD(pin1) | ✅ | JLC | VDD is a 3.0–3.6 V internal-shunt-reg pin; CH224K ref-schematic §6.2 (and R1 in the §7.2.1 replacement guide) feed it from 5 V VBUS through a **1 kΩ series R** — value is datasheet-backed, not a guess. (Tolerance irrelevant for a shunt-reg feed; 1% is just the stock Basic part.) C21190 confirmed on JLCPCB page (1 kΩ / 1% / 0603 / Basic). |
| 63 | 1 | CH224K VDD cap | CL10A105KB8NNNC | C15849 | CH224K VDD(pin1)→GND | ✅ | JLC | Datasheet-required 1 µF decoupling on VDD (§6.2). 1 µF / 50 V / X5R / 0603, **reuse the #42 reel** (bump qty). VDD is only the ~3.0–3.6 V shunt-reg node so any rating ≥6.3 V is plenty — the old "≥16 V" note was over-spec. C15849 confirmed on JLCPCB page (1 µF / 50 V / X5R / 0603 / Basic). |
| 64 | 1 | CH224K CFG1 5 V-strap R | RC0603JR-0710KL | C99198 | CH224K CFG1(pin9)→VBUS | ✅ | JLC | Pulls CFG1 **high to request 5 V** (CFG1=high ⇒ 5 V per I/O table). 10 kΩ → 5 V VBUS (CFG1 abs-max 8 V, safe). Reuse #14. (CFG2/CFG3→GND are nets, no part.) |
| 65 | 1 | CH224K VBUS-sense series R | RC0603JR-0710KL | C99198 | CH224K VBUS(pin8) → 5 V rail | ✅ | JLC | VBUS (pin 8) = high-impedance voltage-sense input. **Value = 10 kΩ** — the CH224K ref schematic (§6.2 / §7.2.1) shows R1=1 kΩ→VDD (=#62) and **R2=10 kΩ→VBUS(pin8)**; the earlier "~1 kΩ" here was the VDD value mis-copied. Reuses the existing 10 kΩ reel (#14/#59/#64); C99198 confirmed on JLCPCB page (10 kΩ / ±5% / 0603 / Extended — tolerance fine for a sense R). ⚠️ **VALUE caveat (not part-#):** the 10 kΩ is from my datasheet-schematic read (couldn't re-verify live — PDF locked); glance at the CH224K ref schematic to double-check R2=10 kΩ. Functionally a 1 kΩ would also work (both just feed a hi-Z sense node), so low risk. |
| 66 | 1 | 3.3 V buck-boost | TPS63021DSJR | C202140 | battery → 3.3 V logic | ✅ | JLC | 14-pin VSON-14-EP(3×4), JLC _Extended_. **C202140 confirmed on JLCPCB page = TPS63021DSJR, "1.8–5.5 V in, 2.4 MHz, 3.3 V Buck-Boost Fixed, VSON-14-EP(3×4)"** — correct variant (21=fixed 3.3 V, DSJR exposed-pad) + package, NOT the wrong C64595. 1S cell (3.0–4.2 V) straddles 3.3 V → needs buck-**boost** (confirmed: OUT/SYS swings 4.4 V→~3.0 V). **Datasheet-verified:** delivers **~1.9–2.4 A at the 3.0 V dead-battery floor** vs our 745 mA worst case → **~2.5× margin** (dynamic current-limit fold-back only below 2.3 V, never entered). Internal fixed compensation ⇒ **inductor + output-cap MUST be a datasheet Table-1 combo** (not free-choice). Pin config (all datasheet-verified): **EN** ← STM6601 (#94, must-not-float — powers rail on); **PS/SYNC→GND** (#68, power-save on, must-not-float); **FB→VOUT** external 0 Ω (#67 — fixed part senses rail externally, NOT internal); **VINA** = control supply (its own 100 nF #69) fed from VIN. **MANDATORY: exposed pad → PGND pour + thermal via array** (electrical PGND return + only heat path). ⚠️ verify LCSC C202140, NOT C64595. Not silicon-proven on this board. |
| 67 | 1 | Buck-boost FB tie | 0603WAF0000T5E | C21189 | TPS63021 FB(pin3) → VOUT | ✅ | JLC | **Fixed-version FB MUST be externally tied to VOUT** (0 Ω or direct net) — the internal trimmed divider senses the rail through this connection. Float = no regulation. **No R1/R2 adjustable divider** (that's the TPS63020). 0 Ω 0603 jumper, Basic; **same part as #68** (bump qty). C21189 confirmed on JLCPCB page (0 Ω / 0603 / Basic). |
| 68 | 1 | Buck-boost PS/SYNC strap | 0603WAF0000T5E | C21189 | TPS63021 PS/SYNC(pin13) → GND | ✅ | JLC | **Must not float** (datasheet). Tie **LOW = power-save enabled** → ~25 µA Iq, best light-load efficiency/runtime for an always-on battery rail (ripple stays within ~3 % of nominal, fine for logic). 0 Ω to GND (or direct net); a placed 0 Ω keeps the option to force-PWM later. **Same part as #67** (0 Ω 0603 jumper, Basic). C21189 confirmed on JLCPCB page (0 Ω / 0603 / Basic). |
| 69 | 1 | Buck-boost VINA bypass | CC0603KRX7R9BB104 | C14663 | TPS63021 VINA(pin1)→GND | ✅ | JLC | Control-stage supply bypass, **0.1 µF (datasheet §8.2.2.5)**. ⚠️ **HARD UPPER LIMIT 0.22 µF — do NOT substitute a bulk cap here.** 100 nF 0603, reuse #19 reel. VINA also routes to the VIN/SYS input node. |
| 70 | 2 | Buck-boost input cap | CL21A106KOQNNNE | C1713 | TPS63021 VIN(pins10/11)→PGND | ✅ | JLC | Datasheet input bypass, **2×10 µF**, X5R/X7R, local to VIN/PGND (NOT covered by the charger OUT cap #55 — datasheet wants it at the converter's own pins). 10 µF/16 V 0805 reuse #20 — **16 V is needed** (SYS reaches ~4.4 V; DC-bias derates ~−50 %). |
| 71 | 3 | Buck-boost output cap | CL21A226MAQNNNE | C45783 | TPS63021 VOUT(pins4/5)→PGND | ✅ | JLC | **Datasheet nominal = 3×22 µF; stability-critical** (must land in the Table-1 L/C matrix — internal compensation). Minimum 2×22 µF. 22 µF / **25 V** / X5R / 0805 (Samsung), Basic — **25 V is a bonus** (less DC-bias derating at the 3.3 V node → keeps effective cap in-band; the ~33 µF floor is easily cleared). Place close to VOUT/PGND. C45783 confirmed on JLCPCB page (22 µF / 25 V / X5R / 0805 / Basic). qty 3. |
| 72 | 1 | Buck-boost inductor | MWSA0503S-1R5MT | C408407 | TPS63021 L1/L2 | ✅ | JLC | 1.5 µH shielded (Sunlord), 5.4×5.2 mm, JLC _Extended_. **Isat 9 A / Irms 8.2 A / DCR 25 mΩ** (confirmed on JLCPCB page). Value 1.5 µH = datasheet-confirmed (≥1 µH for stability). **Isat 9 A ≫ the ≥4.5 A floor** — clears the ~4 A switch current limit (3.5/4.0/4.5 A) with 2× margin so it won't saturate before protection trips on startup/fault (the failure a 2 A part would cause). DCR 25 mΩ at target. Size 5.4×5.2 mm (larger than the ~4×4 target — the trade for 9 A Isat; fine here — flag for layout). |
| 73 | 1 | LiPo cell | **Adafruit 2011 — 1S LiPo, 2000 mAh, JST-PH** | — (Adafruit) | JST-PH 2-pin | ✅ | Hand | **CHOSEN (upsized from the 1200 mAh per the pre-layout review).** 3.7 V nom (4.2 max / 3.0 cut-off), 2000 mAh ±2 %, **integral PCM** (over-charge / over-discharge-3.0 V / short). **JST-PH 2-pin — same family as the old 1200 mAh, so the #74 footprint + polarity are UNCHANGED.** ⚠️ **C-rate = VERIFY-ON-ARRIVAL (M10 — no datasheet on file for the 2011).** 1C = 2 A *should* cover the ~1.8–2 A worst-case peak (steady ~1.3 A ≈ 0.65C), but only the superseded 1200 mAh 503562 doc is on file (rated "1 C5A max continuous"). If the 2011 mirrors that, the peak sits at ~0.9–1C with near-zero margin → an aligned CPU+backlight-inrush+audio+haptic burst could trip the PCM (board reset). **Confirm continuous ≥2 A + PCM trip well above 2 A on arrival.** ~2× runtime vs the 1200 mAh. NO thermistor → ISET (#57) = 2.0 kΩ (~445 mA ≈ 0.22C — safe/gentle for the linear charger; could raise for faster charge). ⚠️ **Size = 60 × 36 × 7 mm (≈10 mm longer + 0.8 mm thicker than the 1200 mAh) → confirm enclosure fit.** Adafruit page lists only "std 0.2C discharge" (no explicit max — normal Adafruit listing; the cell handles 1C continuous). Hand-added post-PCBA; verify JST-PH polarity vs the #74 land (BQ24074 convention). |
| 74 | 1 | Battery connector | S2B-PH-SM4-TB(LF)(SN) | C295747 | mates Adafruit #73 cell | ✅ | JLC | JST-**PH** 2.0 mm, **2-pin**, SMD **side/right-angle** (low profile — cable exits parallel to board), 2 A, JLC _Extended_. Confirmed on JLCPCB page (PH / 2 mm / 1x2P / Surface Mount Right Angle). (Old C160404 was wrong — JST _SH_ 1 mm 4-pin; C295747 is the correct PH part.) **⚠️ POLARITY: match + pad to the Adafruit cell cable (red = +) → BQ24074 BAT; reversed = destroyed cell.** Distinct from the speaker connector. |
| 75 | 1 | Fuel-gauge divider R_top | 0603WAF1803T5E | C22827 | BAT→GPADC0 | ✅ | JLC | **180 kΩ 1% 0603** (UNI-ROYAL), JLC _Extended_. Paired with #76 = 100 kΩ. Datasheet-verified: GPADC is **12-bit, full-scale 0–AVCC, AVCC = 1.8 V** (T113 DS Table 5-7) → **4.2 V maps to 1.50 V** (83 % FS, ~17 % headroom, no clipping), 3.0 V → 1.07 V. **Divider is MANDATORY for protection too** — raw 4.2 V on GPADC0 exceeds AVCC and could damage the input. Source Z ≈ 64 kΩ (OK with the §10 V13 100 nF filter); drain ≈15 µA (negligible). C22827 confirmed on JLCPCB page (180 kΩ / 1% / 0603). **Role now = BACKUP** — the MAX17048 gauge (#77) is the primary battery % (~±1 %); this divider is kept as a cheap independent voltage sanity-check + early-bring-up reading (the single GPADC channel is otherwise unused). |
| 76 | 1 | Fuel-gauge divider R_bot | 0603WAF1003T5E | C25803 | GPADC0→GND | ✅ | JLC | **100 kΩ 1% 0603** (UNI-ROYAL), JLC _Basic_. Paired with R_top #75 = 180 kΩ → ratio 100/(180+100)=0.357 → 4.2 V→1.50 V, under the 1.8 V AVCC full-scale. C25803 confirmed on JLCPCB page (100 kΩ / 1% / 0603 / Basic). |
| 77 | 1 | Battery fuel gauge (SoC %) | **MAX17048G+T10** | C2682616 | I2C (i2c1, addr 0x36); **VDD(pin3)→battery+**; QSTRT(6)→GND; CTG(1)→GND; ALRT(5)→GPIO opt | ✅ | JLC | **Dedicated ModelGauge fuel gauge → accurate battery %** (~±1 %, smooth). 1S, **2.5–4.5 V**, **3 µA** quiescent. Mainline **`maxim,max17048`** (max17040 driver). DFN-8-EP(2×2), **exposed pad → GND**. ⚠️ **WIRING (audit-corrected — the MAX17048 is NOT wired like the '49):** power + cell-sense are BOTH on **VDD (pin 3) → battery+** (datasheet: VCELL measured VDD↔GND); **CELL (pin 2) = NO-CONNECT** on the '48 (leave open). Bypass cap #78 goes on **VDD**, not CELL. ⚠️ **QSTRT (pin 6) MUST tie to GND** (quick-start input, must-not-float — audit MISSING-find, net strap). **CTG (pin 1) → GND.** **ALRT (pin 5)** open-drain → optional GPIO low-batt IRQ; if used, add a pull-up (rely on the T113 GPIO internal pull-up, or DNP a 10 kΩ). C2682616 confirmed on JLC page. |
| 78 | 1 | MAX17048 VDD bypass | CL10A105KB8NNNC | C15849 | MAX17048 **VDD(pin3)** → GND | ✅ | JLC | 0.1–1 µF bypass at **VDD** (datasheet: "only one external component" = this bypass; 1 µF is a fine conservative substitute for the 0.1 µF spec). **Place at pin 3, not CELL** (audit fix — CELL is NC on the '48). No RC filter needed (the gauge averages 4 conversions internally). Reuse the C15849 1 µF reel. |
| 79 | 1 | Charge status LED | 19-213SYGC/S530-E2/5T | C2986027 | bq24074 **/CHG** (open-drain) → LED → 3V3/OUT | ✅ | JLC | 0 GPIO — driven straight by the charger. **Wired to /CHG** (lit while charging, dark when full) — one LED can't show both /CHG and /PGOOD, /CHG chosen. **Green reads as "green=charging, dark=done"** (unconventional but free — a red-while-charging LED would be a new part). Reuse the #23 reel (C2986027). Confirmed on JLCPCB page. |
| 80 | 1 | Charge LED resistor | 0603WAF5100T5E | C23193 | LED current limit | ✅ | JLC | 510 Ω 0603, LED series resistor for the charge LED (#79). Reuse the #24 reel (C23193). Confirmed on JLCPCB page. |
| 81 | 1 | Power/current monitor (telemetry) | **TI INA226AIDGSR** | C49851 | I2C (i2c1; A0/A1-strapped addr) + shunt #82 across SYS | ✅ | JLC | **On-board power telemetry** — reads **bus voltage + current + power** on the SYS rail (total board draw) over I2C. Mainline **`ina2xx`** hwmon → values in `/sys/class/hwmon/` (log/graph from Linux; **satisfies the §10 V23 "measure the rail current" TODO in-system**, no bench scope needed). 16-bit, ±81.92 mV shunt FS; A0/A1 tie to set the I2C address (net, no parts). C49851 confirmed on JLC page (VSSOP-10, 0–36 V CM bus, 2.7–5.5 V VS, ±81.92 mV shunt FS). **Support-parts pass DONE (audit):** VS decouple #83 ✅, shunt #82 ✅, A0/A1 straps ✅. ⚠️ **At schematic capture: wire VBUS (pin 8) → the SYS rail** (else current reads but bus-voltage + power do NOT). **Optional DNP input filter** — 2× ≤10 Ω series R on IN+/IN− (stuff 0 Ω) + a 0.1–1 µF cap across them (DNP); populate only if the SYS-rail 2.4 MHz buck-boost noise disturbs the reading (audit: recommended provision, not mandatory). |
| 82 | 1 | INA226 sense shunt | **RLP25FEGMR010** — 10 mΩ 1% 3W 2512 current-sense (alt Vishay WSL2010R0100, C844898, for 2010) | C393072 | in series in the SYS current path → INA226 IN+/IN− (Kelvin) | ✅ | JLC | **10 mΩ → ±8 A INA226 range** (~250 µA/bit = 2.5 µV shunt-LSB ÷ 10 mΩ), ~0.04 W at 2 A (~20 mV drop — negligible). Dedicated current-sense, **±50 ppm/°C** low-TCR (accurate), ~210k stock, ~$0.05. Kelvin/4-terminal preferred; 2-terminal 1% fine for debug-grade. C393072 confirmed on JLC page (TA-I Tech, 10 mΩ / 1% / 3 W / ±50 ppm / 2512). |
| 83 | 1 | INA226 VS decoupling | CC0603KRX7R9BB104 | C14663 | INA226 VS → GND | ✅ | JLC | 100 nF 0603 at the INA226 supply pin. Reuse the #19 reel. |
| 84 | 1 | Charger /PGOOD pull-up | RC0603JR-0710KL | C99198 | bq24074 /PGOOD (open-drain) → 3V3 + a GPIO | ✅ | JLC | 10 kΩ so the T113 can read **power-good** on a native GPIO (charger-state telemetry). */CHG* is already readable via the LED node (#79). Route both /CHG + /PGOOD to spare GPIOs (pinmux audit found ~14 free). Reuse the #14 reel. |

> **PMIC alternative (not chosen):** the **AXP717** folds charging + all rails + a native fuel
> gauge into one mainline-supported chip. Rejected for sourcing/board-simplicity (fine-pitch QFN,
> spotty stock), not driver support. Revisit if the discrete approach gets unwieldy.

---

## 6. Controls — up to 10 gamepad buttons via I2C expander  ⚠️/🔲

**Decided: I2C GPIO expander (PCA9555) on the shared sensor bus + `gpio-keys`.** A resistor-ladder
ADC is **disqualified** — it can't report simultaneous presses (diagonals + A/B combos). The
expander reports every pin independently and returns all 16 in one read, so chords work cleanly.

**Why expander over 10 direct GPIO** (pin budget fits either way — this is a layout/wiring choice):
- Costs **2 shared-I2C pins + 1 INT**, vs. 10 dedicated PE-bank GPIO → frees ~8 GPIO and greatly
  simplifies routing.
- Enables a **button daughterboard**: put the expander + switches on the D-pad/button board and run
  a thin 4-wire cable (SDA/SCL/INT/3V3+GND) to the mainboard. Ideal for a handheld's split layout.
- 16 IO on the PCA9555 covers all 10 buttons + 6 spare (could absorb the on/off sense, charger
  status LEDs, etc.).

**Latency:** the added cost is one I2C read (~0.1 ms @ 400 kHz) — imperceptible, and dwarfed by the
5–10 ms `gpio-keys` debounce that runs regardless. **Two rules to keep it that way:**
1. **Wire the INT pin → a spare SoC EINT** and stay interrupt-driven. The mainline `pca953x` driver
   exposes the expander as an IRQ-capable `gpiochip`, so `gpio-keys` binds its pins like native GPIO.
   Do **not** poll — polling reintroduces the poll-interval as worst-case latency.
2. **Run the shared bus ≥400 kHz.** If the IMU ends up polled fast for tilt, bus contention adds
   *jitter*; if that bites, move the IMU to its own TWI controller (4 available) to decouple it.

**Button FEEL — ALL buttons are tactiles (decision):** originally the D-pad + face buttons were going to
use carbon-contact silicone pads (the true Game-Boy/Switch rubber-dome mechanism). **Dropped in favor of
tactiles** because tactiles are **bench-testable on the bare board** (no enclosure/retention-plate needed),
need **no custom comb footprint**, and let the board stay **HASL** (no forced ENIG). Tradeoff: a soft
low-force tactile is *close to* but not true rubber-dome feel — accepted for testability. Gamepad D-pad +
A/B/X/Y (#86/#87) = **soft low-force top-actuated** tactiles + keycaps; Start/Select (#88) = 6×6 top
tactile; Volume/Power/Bumpers (#91/#92/#93) = side-actuated tactiles. Electrically every button is one
expander pin → GND (bumpers/volume too), so the pin budget is unchanged. Diagonals work because the
expander reads all pins independently; a shell **rocker keycap** over the 4 D-pad tactiles restores roll.

| # | Qty | Role | Part (candidate) | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 85 | 1 | GPIO expander | PCA9555PWR | C2864778 | I2C (shared bus) 0x20–0x27 + INT | ✅ | JLC | TI, TSSOP-24, JLC _Extended_. Mainline `nxp,pca9555` (`pca953x` driver) as IRQ-capable gpiochip → `gpio-keys` binds its pins. **Datasheet-verified:** slave addr = **0 1 0 0 A2 A1 A0 → base 0x20** (non-A part, so 0x20–0x27 plan holds; A0/A1/A2 tie to VCC/GND — "connect directly," must not float). **Config regs default 0x06/0x07 = 0xFF → all 16 pins are INPUTS at POR** (safe for buttons). **Inputs have an INTERNAL ~100 kΩ pull-up** (≈33 µA typ at 3.3 V; I_IL spec'd −100 µA max — **L11** corrects the earlier "100 µA source" wording) → **so external per-button pull-ups are NOT needed** (see #91, now removed). ⚠️ **M7 — §8.4.1.1 INT erratum (structural on this bus):** INT can be improperly de-asserted when the last command byte was 00h (the mainline `pca953x` bulk-read leaves it there) AND another polled i2c1 slave (MAX17048/INA226/FT7311) is addressed for read → a button edge in that window can be dropped (not fatal — resyncs on next edge). In our own kernel: **park the command pointer at a non-00h register after each input read** + add a low-rate `gpio-keys` poll safety net; verify with `evtest`. INT is open-drain → needs pull-up (#89). C2864778 confirmed on JLCPCB page (TSSOP-24 / TI). |
| 86 | 4 | D-pad tactiles | TS-1187A-B-A-B | C318884 | 4× expander pin → GND | ✅ | JLC | **Converted from silicone-carbon pads → tactiles** (bench-testable on bare board; no custom combs; no ENIG needed). **Reuse the confirmed C318884** (XKB, 5.1×5.1 mm, top-actuated, **1.6 N soft**, 1.5 mm travel, 4-pin, 100k cycles, JLC _Basic_) — same part as RESET #22 / Start-Select #91. 4 discrete buttons (U/D/L/R); diagonals work (expander reads all pins independently); a shell **rocker keycap** on top restores D-pad roll. Chosen over the smaller 2-pin C49234124 (same 1.6 N feel, but this is bigger-base + 4-pin + Basic + already proven). Qty 4. |
| 87 | 4 | Face A/B/X/Y tactiles | TS-1187A-B-A-B | C318884 | 4× expander pin → GND | ✅ | JLC | Same part as #86 (C318884). A/B/X/Y press straight down → top-actuated. **1.6 N soft feel** — closest to Switch among tactiles; true rubber-dome feel would need silicone pads (rejected: needs ENIG + retention plate + not bench-testable). Qty 4. |
| 88 | **2** | Start / Select tactiles | TS-1187A-B-A-B (top-actuated 6×6) | C318884 | 2× expander pin → GND | ✅ | JLC | **Top-actuated** is fine here (Start/Select press straight down). **Reuse the RESET tactile C318884 (#22, page-confirmed)** — same part, qty 2. |
| 89 | 1 | Expander INT pull-up | RC0603JR-0710KL | C99198 | INT (open-drain) | ✅ | JLC | 10 kΩ 0603 → 3V3. PCA9555 INT is open-drain — pull-up required (DS line 1112). Reuse the C99198 reel (#14/#59/#64, already page-confirmed). **No per-button pull-ups needed** — the PCA9555's inputs have an internal ~100 µA pull-up (DS §8.1); every tactile just ties pin→GND (a solid 0 Ω close, well under VIL = 0.3·VCC). |
| 90 | 1 | PCA9555 VDD decoupling | CC0603KRX7R9BB104 | C14663 | PCA9555 VDD (#85) → GND | ✅ | JLC | 100 nF 0603 HF decoupling at the expander VDD (pre-layout-review finding — was not an explicit row). Reuse the #19 reel (bump qty). DS §11.1 also shows a bulk cap — the general §1 bulk (#20) covers it. |

> **Board finish:** with all-tactile buttons (carbon pads dropped) there is **no ENIG requirement** — plain
> **HASL is fine** (the ENIG-flatness constraint only existed to seat a flat carbon pill). Cheaper default finish.
> **Keycaps:** every gamepad/system button is an SMD tactile pressed by a **shell keycap/rocker** — the feel
> and travel are set by the keycap + shell, designed *around* the chosen tactile (buttons are bench-testable
> on the bare board first). *(If truest Switch rubber-dome feel is ever wanted, the carbon-pad path can return
> as a later rev — it would re-impose ENIG + a retention plate; deliberately deferred for testability.)*

**Volume + system buttons — reuse the PCA9555's ~6 spare pins (no new part type):**

The Switch uses **volume +/− BUTTONS, not a slider**, and the MAX98357A is **fixed-gain** (no volume
register) → **volume is a software/ALSA setting** on this board. So volume = 2 momentary buttons mapped
to `KEY_VOLUMEUP`/`KEY_VOLUMEDOWN`; software adjusts the ALSA mixer. A physical slider/pot is rejected:
it needs a scarce ADC to read and still can't do true analog volume without a codec gain register.

| # | Qty | Role | Part (candidate) | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 91 | 2 | Volume +/− tactiles | HX-3x5-CA-1.6N right angle | C49234144 | 2× PCA9555 spare pin → GND | ✅ | JLC | `gpio-keys` KEY_VOLUMEUP/DOWN → software ALSA volume. **No slider, no ADC.** **Side-actuated SMD tactile** (hanxia, 4.5×3.3 mm, right-angle, with mounting bracket, 1.6 N, 100k cycles), JLC _Extended_. Confirmed on JLCPCB page. **Shared side-push part across volume/power/bumpers (#92/#93)** — total qty 5. Bench-testable on the bare board (press → `gpio-keys` registers); shell designed around it later. |
| 92 | 1 | Soft power button | HX-3x5-CA-1.6N right angle | C49234144 | PB(#94) **AND** native SoC EINT (`wakeup-source`) | ✅ | JLC | `gpio-keys` KEY_POWER. **On/full-off work at bring-up** (via #94 + `gpio-poweroff`); short-press sleep/wake is later software (Tier-3), no HW change. **EINT MUST be a native SoC pin, NOT the expander** (sleeping/unpowered expander can't wake/power-on the SoC). **Same side-push part as #91** (shared C49234144). |
| 93 | 2 | L / R bumper tactiles | HX-3x5-CA-1.6N right angle | C49234144 | 2× PCA9555 spare pin → GND | ✅ | JLC | `gpio-keys` KEY_L1 / KEY_R1 (shoulder buttons). **Shared side-push part w/ #91/#92** (C49234144). ⚠️ **only 100k cycles** — fine for spin 1, but a bumper gets hammered vs occasional volume taps → **upgrade L/R to a higher-cycle-life tactile in a later rev if they wear/feel mushy.** **Adds 2 buttons beyond the original "10"** → uses 2 of the 4 spare expander pins (now 14/16). Bench-testable now. Qty 2. |

**On/off — soft power button ONLY (Switch-style), via a push-button power-latch controller:**

Decided: **no mechanical slide switch** — match the Switch (single soft power button). BUT with no PMIC,
a bare button can't turn the board **on from fully-off** (nothing is alive to see the press). So the
slide switch is **replaced** (not just removed) by a **push-button power controller** that keeps a tiny
always-on domain watching the button. This gives the exact target UX on ONE button + ONE circuit:

- **Long press (from off) → on:** controller sees the press → enables the buck-boost → rails up → SoC cold-boots.
- **Long press (running) → full off:** SoC (via its EINT) does a clean Linux shutdown, then `gpio-poweroff`
  releases the controller's HOLD line → **rails actually cut** (a true off, not a CPU halt).
- **Short press → sleep/wake (LATER, software only):** rails stay up in suspend-to-RAM; the button's
  wakeup EINT resumes. **No hardware change** to add this — same button, same circuit.
- **Charge-while-off** preserved: the bq24074 charger sits **upstream** of the latch.
- **Hardware backstop — NOT present as wired (datasheet-verified):** the STM6601's hang-proof force-off needs **PB _and_ SR pressed together** (DS §operation) — and we leave SR unused (no second button). So **power-off is software-only** (SoC clean-shutdown → PSHOLD low → EN deasserts). To get the hardware force-off, add a 2nd momentary button on **SR→GND** (SR must be a momentary switch, never grounded permanently). Optional; the SoC path covers normal use.

| # | Qty | Role | Part (candidate) | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 94 | 1 | Push-button power controller | STM6601CA2BDM6F | C109022 | PB←button; EN(pin9)→TPS63021 EN; PSHOLD(pin4)↔SoC GPIO (`gpio-poweroff`) | ✅ | JLC | STMicro, TDFN-12 (2×3), JLC _Extended_. C109022 confirmed on JLCPCB page. **Variant decoded from DS Table 9/11 & verified against the design:** **C** = active-**high** EN + long-push **deasserts EN** (true power-cut — matches TPS63021's active-high EN; NOT the "A/B assert-RST" options); **A** = V_TH+ ≈ 2.5 V (selector lists 2.60/2.40 for this SKU) — **must be < 3.0 V empty-cell or it locks out a low battery**; **2** = 200 mV hyst; **B** = tON_BLANK 1.4 s power-on hold. **Handshake (DS pin table + §pin-desc, verified):** PB (pin6, internal 100 kΩ pull-up, switch→GND) long-press-from-off → EN asserts *if VCC>V_TH+*; SoC drives **PSHOLD high** to confirm boot (holds EN); SoC drives **PSHOLD low** → EN deasserts → rails cut = clean `gpio-poweroff`; if PSHOLD not high within tON_BLANK → power-up aborts (hung-SoC failsafe). Button #92 wires to BOTH this (PB) AND a native SoC wakeup EINT. **VCC → always-on SYS node** (BQ24074 OUT, upstream of the buck-boost — NOT the switched 3.3 V rail it enables, else power-on-from-off deadlocks). Support: VCC cap #95, CREF #96. Datasheet-verified: EN is **push-pull** (no pull-up needed); RST/INT/VCCLO/PBOUT open-drain outputs **unused → leave open** (no pull-ups); SR & CSRD **leave open** (see notes). **Cheaper alt (not chosen):** a discrete P-FET soft-latch (AO3401A, C15127) can replace the STM6601 if you don't want the IC — smaller BOM cost, but you build the on/off timing + power-on-from-dead logic yourself. |
| 95 | 1 | STM6601 VCC decoupling | CC0603KRX7R9BB104 | C14663 | STM6601 VCC(pin1)→GND | ✅ | JLC | 100 nF 0603 X7R — datasheet directs "0.1 µF as close to the device as possible" (DS §2 VCC). Reuse #19 reel. |
| 96 | 1 | STM6601 VREF cap (CREF) | CL10A105KB8NNNC | C15849 | STM6601 VREF(pin3)→GND | ✅ | JLC | 1 µF 0603 — **DS-MANDATORY even though we don't use the 1.5 V reference** (Fig 1 note 2 / §2 VREF: "Capacitor CREF is mandatory on VREF output even if VREF is not used"). Omit → reference/startup unstable. Reuse the C15849 reel (#42/#63). Place close to pin 3. |

> **Expander I2C address:** PCA9555 base 0x20 + A2/A1/A0 straps. Confirm it doesn't collide with the
> other i2c1 devices (**FT7311 touch 0x38, MAX17048 0x36, INA226 0x40, DRV2605L 0x5A**) — 0x20 (base, range 0x20–0x27) is clear of all. *(IMU LSM6DSOX 0x6A is on i2c2, separate bus.)* **Full i2c1 map verified collision-free** (audit): 0x20 / 0x36 / 0x38 / 0x40 / 0x5A all distinct.
> **Expander pin budget:** 10 gamepad + 2 volume + 2 L/R bumpers = **14 of 16 pins** (power button is a native
> EINT, #92, not on the expander). **2 expander pins spare** — free for future use (e.g. an extra button, or
> stick-clicks if analog sticks are ever added back — sticks were dropped for spin 1).
> **Daughterboard option:** if the buttons live on their own board, the 4-wire cable carries the whole
> button subsystem; keep the INT pull-up on the mainboard side.

---

## 7. Shared bus / cross-cutting glue  ⚠️

Parts that serve the **shared I2C bus** (spans §2 touch + §3 IMU/haptics + §6 expander) rather than any one
subsystem — so they live here, not under a single device.

| # | Qty | Role | Part (candidate) | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 97 | 2 | I2C pull-ups (SDA + SCL) | 0603WAF1501T5E | C22843 | shared I2C SDA/SCL → 3V3 | ✅ | JLC | **1.5 KΩ 1% 0603 (Basic)** (one each on SDA, SCL). ⚠️ **Value verified: 10K FAILS the 400 kHz rise-time budget (~680 ns vs 300 ns limit); 4.7K also disqualified.** 1.5 KΩ → tr≈190 ns @150pF, sink 1.9 mA < 3 mA cap. Add **DNP parallel-R pads** to tune on first article (bus C is an estimate — final trace/device count sets it). ⚠️ **L12:** the earlier "10 KΩ → 680 ns" used ~80 pF; at 150 pF, 10 KΩ = **~1.27 µs**. DNP-tuning **floor ≈ 1 KΩ** (below that, paralleled pull-ups over-sink the '9555's 3 mA I_OL on SDA/INT). 1.5 KΩ meets the 300 ns budget only to ~236 pF → **measure bus C on the first article if the button daughterboard cable is long.** Bus = touch + haptics + expander (IMU moves to its own TWI — §10). C22843 confirmed on JLCPCB page (1.5 kΩ / 1% / 0603 / Basic). |
| 98 | 1 | USB2 D+/D− ESD array | **USBLC6-2SC6** | C7519 | USB-C (#2) D+/D− + VBUS clamp | ✅ | JLC | **ESD protection for the USB-C data port** (#2) — the charge + FEL-flash port, most exposed to plug/unplug ESD. Low-cap (**~3.5 pF**, fine for USB2 HS — **L13** corrects the in-row "0.35 pF" typo; the 0.85 pF figure belongs to the SP3004 SD array #13) SOT-23-6 array on D+/D− with a VBUS clamp pin; protects the **T113 USB PHY** (a blown PHY = no software recovery). Near-universal on USB2 ports. **Promoted from §10 V11.** C7519 confirmed on JLC page (SOT-23-6L, IEC 61000-4-2, ~3.5 pF — fine for USB2 HS). Optional extra: a **5 V VBUS TVS** (e.g. SMAJ5.0A) as a DNP pad. *(No USB1 port on this board — the WiFi-dongle path was dropped — so this one USB-C port is the only USB ESD needed.)* |

---

## 8. Switch-class extras — Tier 2 (optional, "feels like a Switch")  ⚠️/🔲

Higher effort/cost but still mainline. Add per appetite.

| # | Qty | Role | Part (candidate) | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 99 | 1 | Bluetooth module (onboard) | ⚠️ **Ezurio BT830-SA-01** (CSR8811) | DigiKey (not JLC) | **UART HCI** (TX/RX/RTS/CTS) → a T113 UART + VREG_EN_RST# strap + 3.3 V | ✅ | Hand | **CHOSEN onboard BT** (Bluetooth-only — CSR8811 has no WiFi; see the BT830 integration note below §8). **Datasheet-verified:** native **UART HCI** module (NOT smartBASIC/SPP — that's the BT900), *"native support for … Linux Bluetooth software stacks"* → **BlueZ** via `btattach`/`hciattach` (H4/H5). **Dual-mode** = Classic (A2DP headphones) + BLE (controllers). **Integrated ceramic antenna** → RF is solved, **no antenna trace to design** (just a keep-out, see integration note below §8). 8.5×13 mm, **castellated edge pads → hand-solderable** with an iron. VDD_PADS sets I/O level (use 3.3 V). ~$17, 539 stock. ⚠️ **"Not For New Designs"** — fine for a hobby one-off (in stock now), don't design a product line on it. **Fit = Hand:** JLC leaves the footprint bare; you solder the DigiKey module. |
| 100 | 1 | BT830 regulator stability cap ⚠️REQUIRED | CC0603KRX5R6BB475 | C109456 | BT830 VREG_OUT_HV (pin 10) → GND | ✅ | JLC | **DATASHEET-MANDATED** (BT830 §9 Method #1: *"a minimum 1.5 µF capacitor MUST be connected to Pin-10 (VREG_OUT_HV)… low-ESR MLCC"*). Stabilizes the internal HV-LDO that powers the radio core — **required even though the 1.8 V is not used externally** (the pin-table "N/C if unused" only forbids external 1.8 V loads, NOT this cap). 4.7 µF low-ESR X5R, reuse #21 reel. Omit → LDO can oscillate / module may not enumerate. Verified by the support-parts pass (both skeptics). |
| 101 | 2 | BT830 supply decoupling | CC0603KRX7R9BB104 | C14663 | BT830 VREG_IN_HV (pin 9) + VDD_PADS (pin 1) → GND | ✅ | JLC | 100 nF ×2 local HF decoupling, one at each supply pin (matches this BOM's per-IC convention). Best-practice (datasheet is preliminary, gives no pin-9/1 value; CSR8811 carries the fine decoupling internally), but cheap insurance for a 2.4 GHz radio's analogue supply. Reuse #19 reel. |
| 102 | 1 | BT830 3.3 V bulk / RF-burst reservoir | CC0603KRX5R6BB475 | C109456 | BT830 3.3 V feed (near pin 9) → GND | ✅ | JLC | 4.7 µF bulk near the module's 3.3 V input. Table 7 gives **average** current only (≤11.5 mA); a Class-1 (+7 dBm) module's TX-slot bursts draw higher unspecified instantaneous current — this reservoir supplies them locally so the module's own supply stays clean. **NOT a rail-brownout fix** (that risk is negligible — see power budget); it's local supply/RF quality. Reuse #21 reel. |
| 103 | 1 | Stereo I2S DAC (headphone out) | **TI PCM5102APWR** | C107671 | **I2S1 — SHARED with speaker amp #47** (BCLK/LRCLK/DIN; no MCLK, internal PLL) + 4 config straps | ✅ | JLC | **CHOSEN headphone source.** T113 on-chip codec has **NO mainline driver** (verified in the kernel tree — only `dmic`+`i2s` DAIs, no analog-codec node/driver; §4). PCM5102A = stereo I2S DAC, mainline **`ti,pcm5102a`** (hardware-mode, no I2C). **Shares I2S1** with the MAX98357A (both are I2S *receivers* on the same 3 lines) → **0 extra I2S pins, PB bank stays free**. No MCLK needed (internal PLL from BCLK, like the amp). Ground-centered (VCOM/charge-pump) **line-level** output → **HP amp #110 → jack #115** — ⚠️ **PCM5102A is a LINE driver (1 kΩ min load); it will NOT drive 16–32 Ω headphones directly** (~24 dB loss through the datasheet 470 Ω) → a HP amp follows (support-parts pass caught this; my earlier "fine for earbuds" was wrong). **Support parts (datasheet-pass-verified):** decoupling #105, bulk #106, charge-pump caps #107. **no HW volume** → ALSA softvol (same as the speaker amp); **no HW jack-detect** → jack-detect GPIO (#115). **Config straps + shared-I2S1 PLL constraint — see the PCM5102A integration note below §8** (SCK=GND for MCLK-less internal PLL; run I2S1 at 64fs). |
| 104 | 3 | I2S1 series-R (SI, multi-drop) | 0603WAF0000T5E | C21189 | T113 I2S1 BCLK/LRCK/DOUT driver end → series | ✅ | JLC | **0 Ω series-R footprint** on the three I2S1 lines at the T113 driver end — I2S1 is now **multi-drop** (MAX98357A #47 + PCM5102A #103 both receivers). **Stuff 0 Ω by default**; swap to ~33 Ω on first article if the multi-drop edges ring (pre-layout-review SI finding). Reuse the #67/#68 0 Ω reel. |
| 105 | 4 | PCM5102A supply decoupling | CC0603KRX7R9BB104 | C14663 | AVDD(8)/CPVDD(1)/DVDD(20)/LDOO(18) → GND | ✅ | JLC | 100 nF ×4, one per supply pin. **LDOO 0.1 µF is datasheet-REQUIRED** (Table 12 — internal-LDO stability); AVDD/CPVDD/DVDD per Fig 33. **DVDD strapped to 3.3 V** (matches T113 LVCMOS; the on-chip LDO makes the 1.8 V core — do NOT drive LDOO externally). Reuse #19 reel. Support-parts-pass verified (both skeptics). |
| 106 | 3 | PCM5102A supply bulk | CL21A106KOQNNNE | C1713 | AVDD/CPVDD/DVDD → GND | ✅ | JLC | 10 µF ×3 bulk (Fig 33 shows 0.1 µF + 10 µF on each of AVDD/CPVDD/DVDD). Reuse #20 reel. |
| 107 | 2 | PCM5102A charge-pump caps | CC0805KKX7R9BB225 | C125847 | CAPP(2)↔CAPM(4) flying cap + VNEG(5)→GND | ✅ | JLC | **2.2 µF ×2 — MANDATORY.** No flying cap (CAPP-CAPM) or VNEG reservoir → no −3.3 V rail → **dead ground-centered output**. Reuse the #31 reel (2.2 µF 50 V X7R 0805; ≫ the 6.3 V needed). Support-parts-pass verified. |
| 108 | 2 | PCM5102A anti-imaging R (M8) | ✅ **UNI-ROYAL 0603WAF4700T5E** — 470 Ω 1% 0603 | **C23179** ✓ | OUTL(6)/OUTR(7) series → coupling cap #111 | ✅ | JLC | **M8 fix — was MISSING.** Datasheet Fig 33 anti-imaging RC: 470 Ω series on each DAC output ahead of the 1 µF coupling caps (fc = 1/(2π·470·2.2 nF) ≈ 154 kHz); also isolates the line driver from the coupling-cap load. Same UNI-ROYAL 0603 family as the §1 resistors, JLC **Basic**. C23179 confirmed on JLC page. |
| 109 | 2 | PCM5102A anti-imaging C (M8) | ✅ **Samsung CL10C222JB8NNNC** — 2.2 nF 50 V C0G 0603 | **C33353** ✓ | OUTL/OUTR → AGND (shunt) | ✅ | JLC | **M8 fix — was MISSING.** 2.2 nF **C0G** shunt to AGND, pairs with #108 (470 Ω) = the datasheet-characterized anti-imaging filter (fc ≈ 154 kHz). C0G dielectric = low-distortion (correct for the analog audio path, not X7R). C33353 confirmed on JLC page (Extended tier). |
| 110 | 1 | Stereo headphone amp | **TI TPA6132A2RTER** | C69901 | DAC OUTL/R (via RC + 1 µF coupling #111) → INL−/INR−; OUTL(16)/OUTR(5) → jack #115; G0/G1/EN straps | ✅ | JLC | **DirectPath capless** stereo HP amp — **analog, pin-strapped gain (G0/G1), NO I2C, NO driver** (the JLC "digitally programmable gain" was misleading; datasheet = 4 fixed gains −6/0/+3/+6 dB). **VDD from 3.3 V** — output is **supply-independent** (internal HPVDD + charge pump), so 5 V buys nothing. **Gain = −6 dB (G0=G1→GND)** + **cap the ALSA softvol max** (amp clips into 16/32 Ω at full DAC scale — max ~0.63 Vrms/16 Ω vs DAC 2.1 Vrms — so limit digital max, like the speaker). 25 mW×2/16 Ω, 0.01 % THD, WQFN-16-EP (JLC reflows). Support: #111–#114. ⚠️ **HPVDD(12) = 2.2 µF cap ONLY — NEVER tie to VDD/any supply (damage).** Wiring/straps: see the TPA6132A2 integration note below §8. Support-parts pass verified (both skeptics). |
| 111 | 4 | TPA6132A2 1 µF caps | CL10A105KB8NNNC | C15849 | CFLYING(CPP11↔CPN9) + CHPVSS(8→GND) + CINPUT ×2 (series → INL−1/INR−4) | ✅ | JLC | 1 µF X5R ×4, all datasheet-required: **CFLYING** (charge-pump flying), **CHPVSS** (neg-rail reservoir, must be ≥ CFLYING), **2× input coupling** (DC-block the single-ended DAC into the inverting inputs; fc ≈ 6 Hz with the 26.4 kΩ RIN). Reuse #42 reel. |
| 112 | 2 | TPA6132A2 2.2 µF caps | CC0805KKX7R9BB225 | C125847 | CHPVDD(12→GND) + CVDD(14→GND) | ✅ | JLC | 2.2 µF X7R ×2: **CHPVDD** = internal-bias decouple (⚠️ **cap ONLY — NEVER to a supply**), **CVDD** = VDD decouple (place within 5 mm). Reuse #31 reel. |
| 113 | 1 | TPA6132A2 VDD HF cap | CC0603KRX7R9BB104 | C14663 | VDD(14) close-in → GND | ✅ | JLC | 100 nF close-in HF decoupling at VDD (pairs with the 2.2 µF #112) — preserves the 100 dB PSRR. Reuse #19 reel. |
| 114 | 1 | TPA6132A2 EN pull-down | RC0603FR-07100KL | C14675 | EN(13) → GND | ✅ | JLC | 100 kΩ pull-down so the amp **boots OFF**; a T113 GPIO drives EN high **after the DAC settles** (jack-detect can gate it too). Reuse #49 reel. |
| 115 | 1 | 3.5 mm headphone jack (TRS) | **PJ-327C-4A** | C145813 | HP amp OUTL(16)/OUTR(5) → tip/ring; SGND(15)→sleeve; detect switch → GPIO | ✅ | JLC | Stereo **TRS** jack, SMD, with a **detect switch** — **58k stock, the commodity standard** (Korean Hroparts). Part # + SMD confirmed on JLC page. Stereo out only (no mic). Fed by the **HP amp #110** (OUTL/OUTR direct — no caps; amp **SGND → jack sleeve/GND**). ⚠️ **At schematic capture, read the pad map (tip/ring/sleeve/switch) from the drawing/EasyEDA symbol** (PJ-327 pin arrangements vary by maker) — not shown on the parametric listing. **Detect switch → a T113 GPIO** to mute the speaker amp (#47) on insert — needs GPIO control of the MAX98357A SD pin (small change to #48's strap: add a jack-detect GPIO that can pull SD_MODE low = shutdown). |

> **BT830 integration note (wiring/layout for #99 + support caps #100/#101/#102):**
> **(1) UART** — UART_TX/RX/RTS/CTS → a free T113 UART **with 4-wire HW flow control** (CTS floats HIGH =
> "not clear to send" → module won't TX if omitted; 4-wire needs no external pulls, all 4 pins have internal
> weak pull-ups); bring up via `btattach -P h4` (or h5) → `hci0`.
> **(2) Enable (pin 8, VREG_EN_RST#)** — **drive from a T113 GPIO** (push-pull to 3.3 V; boot low, hold >5 ms,
> then high → also gives SW reset). ⚠️ **Do NOT tie high with the 10 kΩ reel (C99198)** — the pin's *strong*
> internal pull-down (≤150 µA) leaves 10 kΩ at only ~1.8 V < VIH 2.31 V → module stuck in reset. If strapped
> instead of a GPIO, use **≤4.7 kΩ**. Never exceed VDD_PADS (3.3 V).
> **(3) Power** — VREG_IN_HV(pin9)=3.3 V (module self-generates 1.8 V internally); VDD_PADS(pin1)=3.3 V (I/O
> level). ⚠️ **VREG_OUT_HV(pin10) is NOT "leave open"** — it needs the mandatory ≥1.5 µF cap (#100) even though
> the 1.8 V is unused externally.
> **(4) SPI_PCM#_SEL(pin28)** — optionally tie GND (internal weak pull-down already defaults it → PCM/UART-HCI
> boot; no strap required).
> **(5) Layout** — footprint pads **extend OUTWARD past the module edge** (castellations iron-solderable by
> hand); **integrated-antenna end overhangs the board edge / no-copper keep-out** on all layers; keep metal
> (battery/speaker/LCD shield) **≥20 mm, ideally 30–40 mm**.
> **(6) WiFi** — **NOT on the board** (CSR8811 is BT-only, and the USB1 dongle port was dropped). WiFi only via a USB-C OTG dongle (USB0) if ever needed — no board provision.
>
> **PCM5102A integration note (config straps + shared-bus, for #103 — datasheet-pass-verified):**
> **Config straps (hardware-mode DAC — these are net ties, "silent if wrong"):**
> **SCK(12) → hard-tie GND** (0 Ω/copper, NOT floating/pulled) — forces the internal PLL to derive the clock
> from BCK, since the shared I2S1 is **MCLK-less**; float or pull-high = no clock = **dead output**.
> **FMT(16) → GND** (standard I2S, matches the bus/#47); **FLT(11) → GND** (normal filter);
> **DEMP(10) → GND** (de-emphasis off); **XSMT(17) → a spare T113 GPIO (M9), NOT hard-tied high** (high = un-mute, low = soft-mute). Hard-tying high means that during **speaker-only** playback the shut-down HP amp's collapsed-rail inputs get driven ~2.8 Vpk through the coupling caps → clamp-diode current can back-pump HPVDD past its 1.9 V abs-max. Put XSMT on a GPIO and **soft-mute the DAC (built-in 104-sample ramp, pop-free) whenever headphones are unplugged** — or keep the HP-amp EN asserted whenever the DAC streams. If the hard-tie is kept, bench-verify the shutdown clamp current + HPVDD/HPVSS at full scale before fab.
> **Shared-I2S1 with the speaker amp #47 is valid** (both are I2S receivers, negligible load, both standard
> I2S + MCLK-less). ⚠️ **ONE constraint:** the DAC's PLL only locks at **BCK = 32fs or 64fs, fs ≥ 16 kHz**
> (Table 11) → **configure T113 I2S1 at 64fs @ 44.1/48 kHz** (a 48fs ratio or 8 kHz stream leaves the DAC
> silent while the speaker still plays). Can't route different audio to speaker vs headphones (one shared DIN)
> — gate outputs instead (jack-detect GPIO → MAX98357A SD to mute the speaker on insert).
>
> **TPA6132A2 integration note (#110 HP amp — datasheet-pass-verified):**
> **Single-ended wiring (datasheet Fig 27):** each DAC channel → its 470 Ω/2.2 nF anti-imaging RC → **1 µF
> coupling cap (#111)** → the **inverting** input INL−(1)/INR−(4); tie **INL+(2)/INR+(3) directly to GND**
> (the amp is inverting in this mode — matched both channels, inaudible for headphones). **OUTL(16)/OUTR(5) →
> jack tip/ring directly (NO output caps — DirectPath 0 V-centered); SGND(15) → jack sleeve/GND.**
> **Straps:** **G0(6)=G1(7)→GND = −6 dB** (max headroom). **EN(13)** = 3.3 V GPIO + 100 kΩ pull-down (#114) →
> boots OFF; assert EN **after** the DAC output settles (built-in pop suppression, 5 ms start-up).
> ⚠️ **HPVDD(12): connect ONLY the 2.2 µF cap (#112) — NEVER to VDD or any supply (device damage).**
> **Clip control:** amp max ≈ 0.63 Vrms/16 Ω vs DAC 2.1 Vrms FS → **−6 dB gain + cap the ALSA softvol max**
> (e.g. ~−12 dB, same idea as the speaker-protection cap). Optional analog input divider (~−6 dB, **DNP pads**)
> as a boot/fail-safe. **VDD from 3.3 V (5 V gives no more output — supply-independent).** **EP → GND + thermal
> vias, never VDD.**
>
> **Audio output routing + headphone-detect + SI/ESD (ties speaker #47 ↔ headphones #103/#110/#115):**
> **Auto-switch is SOFTWARE, not automatic** — discrete amps, not a single auto-routing codec (the tradeoff vs
> the ES8316 we dropped). Both the speaker amp (#47) and the HP DAC (#103) sit on the **same I2S1** stream (both
> receivers) → you can't send different audio to each; "routing" = enabling/disabling the two amps.
> **Detect:** jack switch (#115) → a native T113 GPIO; mainline **`gpio-keys` → SW_HEADPHONE_INSERT**.
> **Truth table:** headphones **IN** → speaker OFF (MAX98357A SD low) + HP amp ON (TPA6132A2 EN high);
> headphones **OUT** → speaker ON (SD released) + HP amp OFF (EN low). A small userspace daemon (or an ASoC
> machine-driver GPIO jack / ALSA UCM) applies this on the detect event.
> ⚠️ **Speaker SD is multi-function (mode + shutdown)** → the mute GPIO must be **open-drain / hi-Z**: hi-Z = the
> 680 kΩ (#48) sets mono + run; drive low = shutdown. (Small pop on toggle — MAX98357A pop suppression is modest.)
> **GPIO budget: 3 NATIVE T113 GPIOs** — jack-detect (in), speaker-SD (open-drain out), HP-EN (out). **Native,
> NOT on the PCA9555 expander** (must be direct/fast).
> **SI / ESD / brownout:**
> • **Brownout: no risk** — the DAC + amp are low-power (~tens of mA peak from 3.3 V; charge-pump inrush is tiny,
>   small caps); local decoupling (#105/#106/#107, #111–#113) covers audio transients. Negligible on the 2 A buck (#4).
> • **I2S1 is now multi-drop** (amp #47 + DAC #103 on the same BCLK/LRCK/DIN) → keep the branch **stubs short**;
>   optional **series source-terminator on BCLK** if running high sample rates (BCLK ≤ ~12 MHz @ 192 k/64fs).
> • **The jack is a user-accessible analog port** → **audio-line ESD relies on the TPA6132A2's ±8 kV HBM
>   (IEC 61000-4-2)** — no external array. *(Decided: a common ground-referenced array like SRV05-4 / common-anode
>   PESD3V3S2UT clips the bipolar audio at ~−0.7 V; a true bidirectional array can't be trusted from JLC's clone
>   labels — not worth it over the amp's built-in ±8 kV. Optional future add = a genuine-vendor bidirectional array
>   on tip/ring + a unipolar diode on the detect line.)* + optional ferrite/RC EMI
>   filter at the connector. **Route the analog HP traces away from switchers / backlight PWM / the RF path**, and
>   give SGND a clean, quiet ground return.
>
> **Skipped (Tier 3):** potentiometric sticks (drift), physical volume slider/pot (needs ADC, no benefit
> over buttons), TAS5805M stereo amp (overkill), WS2812 via LEDC (no D1/T113 dtsi node — hand-author).
> **Prove-on-silicon-first:** suspend-to-RAM sleep/wake — T113 mainline deep-sleep is immature in 6.12;
> ship clean shutdown + backlight-off + low cpufreq as the practical low-power state first.

---

## 9. Power budget — whole-board current table

Summed across ALL subsystems. **Topology (battery handheld — reconciled with §5, supersedes the old USB-only
breakout tree):** battery ↔ **BQ24074 power-path → SYS node (~3.0–4.4 V, NOT 5 V)**; **SYS → TPS63021
buck-boost (#66) → 3.3 V logic**; **SYS → FP6161 buck (#3) → 0.9 V core**. The T113's internal *linear*
LDOA/LDOB make 1.8 V + 1.5 V-DDR3 FROM the 3.3 V rail → that current reflects onto the **buck-boost**. When
plugged, USB-C 5 V (CH224K PD) feeds the charger IN and charges/supplements; SYS stays at ~battery voltage
(the 5 V does not power the rails directly). Legend: numbers are mA; "firm" = datasheet, "est" = bounded
estimate (verify on silicon). ⚠️ **The old SY8089 3.3 V buck (#4) + its inductor (#6) were REMOVED** — a buck
can't make 3.3 V from a 1S cell below ~3.6 V, so the buck-boost is the sole 3.3 V source.

### Rail A — 3.3 V logic (**TPS63021 buck-boost #66, ~1.9–2.4 A** at the 3.0 V floor) — *boost/audio/haptics live on SYS, not here (see §10 V3/V8)*
> ⚠️ **This table under-counts the 3.3 V rail** (pre-layout-review finding): it omits the T113 **VCC-PD/PE/PG/PLL** domain supplies (RGB screen bank + I2S/I2C + PLL ≈ +320 mA worst) and the §8 audio (PCM5102A #103 + TPA6132A2 #110) + BT830 (#99). **True 3.3 V worst ≈ 1 A, not 745 mA.** Still well within the buck-boost's ~1.9–2.4 A, but the margin claims below are optimistic → **§10 V23 silicon scope is mandatory.**
> ⚠️ **M2 — DDR term + in-package LDOB heat:** VCC-DRAM is capped at **400 mA** (Table 5-3), not 300, and "no datasheet ceiling" was wrong. If VCC-DRAM is internal-LDOB-sourced off 3.3 V, LDOB dissipates up to (3.3−1.5)×0.4 ≈ **0.72 W inside the package** on top of ~0.72 W core (against Tj 110 °C) — the datasheet's ambient ratings assume *external* VCC-DRAM power. Budget 400 mA (→ 3.3 V worst ≈ **1.1 A**) + the in-package heat; if Tj is tight, add an **external 1.5 V DDR buck** (removes both the reflection and the heat). Confirm which rail sources VCC-DRAM on this SiP.
| Load | Typ | Worst | Basis |
|------|----:|------:|-------|
| T113 VCC-IO | 60 | 150 | firm (max 150 mA, Table 5-3) |
| LDO-IN → LDOA (1.8 V IO/analog/PLL), 1:1 | 60 | 120 | est |
| LDO-IN → LDOB (1.5 V DDR3 die), 1:1 | 150 | 300 | **est — largest & least-certain term; LDO limit UNSPEC'd in datasheet** |
| VDD18-DRAM (1.8 V ctrl) | 5 | 10 | firm (10 mA max) |
| NOR (SPI0) | 5 | 15 | est |
| microSD write burst | 30 | 100 | est |
| logic / pull-ups / misc | 20 | 50 | est |
| **3.3 V TOTAL** | **~330** | **~745** | **≈ 75 % of 1 A worst → within a bare 1 A but ABOVE the 700–800 mA derate** |

### Rail B — 0.9 V core (separate FP6161, 1 A max)
| Load | Worst | Basis |
|------|------:|-------|
| VDD-CORE0/1 + VDD-SYS0/1/2 (dual-A7 @1.2 GHz) | ~800 | firm (Table 5-3 sum) → ~80 % of 1 A, OK but **scope-verify** (§10 V23) |

### SYS node (~3.0–4.4 V power-path, **NOT 5 V**) — buck-boost + core-buck inputs, moved analog loads + charging
| Load | Worst | Basis |
|------|------:|-------|
| 3.3 V buck-boost INPUT (≈ 3.3 V × ~1 A / 0.9 eff / **3.7 V SYS**) | ~990 | derived (÷SYS ≈3.7 V, **not ÷5 V** → higher than the old estimate; peaks toward the 3.0 V floor) |
| 0.9 V core buck INPUT (≈ 0.9 V × 800 mA / 0.85 eff / 3.7 V) | ~230 | derived (core buck now off SYS too) |
| Backlight boost input (from SYS) | ~180 | firm-ish (19 V×40 mA/eff) |
| Audio amp PVDD (peak; avg ≪ w/ crest factor + softvol) | ~300 pk | firm |
| Haptics VDD (pulsed, tens of ms) | ~150–250 pk | firm |
| Bluetooth BT830 | ~12 | firm (datasheet Table 7: 5 mA idle / 7.1 mA TX / 11.5 mA RX-worst; TX burst ~30-40 mA but off the module's INTERNAL 1.8 V LDO, buffered by the pin-10 cap — 3.3 V rail sees only smoothed low-tens-of-mA) — negligible on the ~2 A buck-boost (#66). (No WiFi on the board — USB1 dongle port dropped — so nothing else here.) |
| Battery charge current (BQ24074) | ~740 (ISET) | set by ISET #57 (~0.5C), thermally capped; input limit ~1.46 A (ILIM #58) shared run+charge |

### ⚠️ Two findings the SUM reveals (piecewise analysis could not):

1. **USB-500 mA input budget is BLOWN when plugged + busy → ADDRESSED by the PD sink (§5 #61) + fixed 1.5 A ILIM (#58).** The
   3.3 V buck input alone (~550 mA) exceeds the 500 mA a bare-CC USB-C port allows, so at full load the
   battery would drain while plugged. **Fix: CH224K negotiates a real 5 V/high-current contract + the charger's
   input limit is set to ~1.46 A (fixed 1.1 kΩ ILIM, #58)** → up to ~7 W input, enough to run the 2–3 W load AND
   charge simultaneously → charge-while-playing. ⚠️ **Residual limits (unchanged):** only helps PD/QC/BC1.2
   wall/power-bank sources; a laptop **SDP** port still caps at 500 mA → the charger's DPPM prioritizes the
   system load and the **battery supplements** (net slow drain under bursts), and the board still **can't run
   screen-on from a fully-dead battery on a 500 mA-only source**. (Option 1: no FET-gated adaptive limit —
   the charger self-manages weak sources; see §5.)
2. **The 3.3 V rail is heavily loaded (true worst ≈ 1 A)** even AFTER offloading boost/audio/haptics, and the
   **DDR3 term (150–300 mA) is an estimate with no datasheet ceiling.** **→ RESOLVED BY TOPOLOGY:** the 3.3 V
   rail is the **TPS63021 buck-boost (#66)**, which is *mandatory* for a 1S battery (a plain buck can't make
   3.3 V once the cell drops below ~3.6 V) and inherently sources **~1.9–2.4 A** even at the 3.0 V floor — so
   the rail has ~2× margin with no "upsize a buck" decision needed. *(This supersedes the old plan of upsizing
   a 3.3 V buck; the SY8089 + its inductor were removed.)* **Still measure the actual 3.3 V current on silicon**
   (§10 V23) — needed for battery runtime + thermal regardless.

### Does it force component changes?
- **Battery capacity:** **≥2000 mAh required** — not only for runtime (~4 h screen-on) but because the full-load
  peak (~1.8–2 A) exceeded the old 1200 mAh cell's 1 C limit → **RESOLVED: upsized to Adafruit 2011 (2000 mAh, 1C = 2 A)**, same JST-PH family (#73). See §5.
- **3.3 V rail:** the **TPS63021 buck-boost (#66, ~1.9–2.4 A)** is the sole 3.3 V source — mandatory for a 1S cell.
  The old SY8089 buck (#4) + its inductor (#6) were **removed** (a buck can't boost below ~3.6 V). 0.9 V core keeps the proven 1 A FP6161 (#3), now fed from SYS.
- **Charger:** BQ24074 input limit set to a fixed ~1.46 A (1.1 kΩ ILIM, #58) so a PD source can run+charge; charge *current* (ISET, #57) set well below that (~0.5C) because linear charge into a low cell is thermally capped. See finding #1.
- Everything else: tracing/thermal only (wide traces on the 3.3 V + SYS rails, buck thermal).

> **Biggest uncertainty is the SoC itself, not the peripherals** — the T113 CPU+DDR+internal-LDO current
> dominates the 3.3 V budget and the datasheet does NOT spec the internal-LDO limit. No part-picking fixes
> this; **only a silicon measurement** (dual-A7 @1.2 GHz + screen-on, scope the 3.3 V rail) resolves it.
> Wireless is **Bluetooth-only (BT830, ~7 mA on 3.3 V)** — negligible, no rail concern. No WiFi on the board (USB1 dongle port dropped), so nothing else loads the rails here.

---

## 9.5 Pinmux audit (pre-layout — from the mainline `pinctrl-sun20i-d1` mux table)

**Verdict: PIN-FEASIBLE, no BOM change.** T113-S3 eLQFP-128 has **72 bonded GPIO** (PB2-7, PC2-7, PD0-22,
PE0-13, PF0-6, PG0-15); **~58 used, ~14 spare**. Only the **PD bank is full** (RGB666 LCD). Two verifiers
independently re-derived the map from the pinctrl driver + D1/T113 reference DTS and **fully agreed**.

**Pin map:** LCD RGB666 = **PD0-21** (+ DISP PD22) · SPI0-NOR = **PC2-5** · microSD SDC0 = **PF0-5** ·
UART0 console = **PE2/3** · **BT-HCI UART = UART3 PG0-3** · I2S1 = **PG12/13/15** (LRCK/BCLK/DOUT, shared amp+DAC) ·
i2c1 sensors (touch/haptics/expander) = **PG8/9** · IMU i2c2 = **PE4/5** · GPADC + USB0 (USB-C) = dedicated balls (USB1 unused — no dongle port) ·
~24 free pins cover all single GPIO/EINT/PWM needs (PCA9555-INT, power-btn, touch INT, jack-detect, spkr-SD, HP-EN, backlight, etc.).

**DT-authoring rules (layout/software, NOT parts):**
- ⚠️ **Put BT on UART3 (PG0-3), NOT UART1** — UART1's flow-control group PG6-9 collides with the i2c1 sensor bus (PG8/9); UART3/PG0-3 is free (ex-SDIO, since WiFi is USB). *(Alt: keep UART1, move i2c1 to PE0/1.)*
- ⚠️ **Never allocate the non-bonded "phantom" D1 pins** PC0/1, PB0/1/8-12, PE14-17, PG16-18 — put IMU i2c2 on **PE4/5**.
- **USB0** = forced-role/device (no ID/VBUS-detect GPIO — those D1 pins PD20/21 are now LCD), or move detect to a spare PE pin.

**Design choices this audit VALIDATED (why they were right):** (1) all sensors on **I2C** — RGB666 on PD kills SPI1 (SPI1 only exists on PD10-15); (2) WiFi is a **USB dongle** — SDIO (mmc1) would collide with both I2S1 and BT UART3; (3) **RGB666 not RGB888** — keeps the PB bank free.

---

## 10. Verification fixes (screen + motion + sound reviews)

Parts/changes surfaced by the adversarial verification of §2/§3/§4. **Root cause: the 3.3 V logic rail is
heavily loaded** (screen-on). Two fixes: **(a)** the 3.3 V rail is the **TPS63021 buck-boost (#66, ~2 A)** —
not a 1 A buck (the old SY8089 was removed; see §9); **(b)** move the hungry/noisy analog loads (backlight
boost, audio amp, haptic driver) to **SYS/power-path** (works plugged *and* on battery — NOT "VBUS-5V", which
vanishes on battery). Plus the missing support glue below.

### BLOCKERS — board won't work without these
| # | Fix | Part | Why |
|---|-----|------|-----|
| V1 | **Panel DISP (FPC pin 31) net** | 10 KΩ to VDD (C99198) **+** route to a spare GPIO (PD22 free) | DISP is a display-on *input* with no internal pull → float = **screen stays dark** even with perfect RGB. Not previously in BOM. |
| V2 | **DRV2605L REG cap** | **1 µF X7R 0603** (C15849), <few mm from REG pin | Datasheet-mandatory for internal-LDO stability; without it the part may oscillate / not enumerate on I2C. §3 had zero haptic passives. |
| V3 | **Move backlight boost VIN → SYS/5V** | routing (no part) + local input cap on the 5V feed | Off-loads ~290 mA from the 1 A buck (→ ~41%). Keeps VBUS bulk cap #20. Confirm USB-C source ≥1.5 A. |
| V4 | **RGB666 bit map is NOT datasheet-resolved — get Zettler's explicit R/G/B bit-to-pin map before routing.** Panel is genuine 24-bit (full R0–R7 bus); the old "§3.1.2 Note 2 ⇒ active inputs R0–R5" reading was a **misread** (Note 2 is the VIH/VIL list and cites a nonexistent RESET pin). **Convention-safe default: drive the 6 MSBs (R7..R2), tie the 2 LSBs (R1/R0, G1/G0, B1/B0) to GND.** Grounding the *wrong* pair (the MSBs R7/R6) caps each channel at 63/255 ≈ 25% → dark, no true white. | routing (no part) | Floating CMOS color inputs → noise; wrong 6-of-8 or MSB/LSB order → dark or scrambled color. |
| V5 | **DT/kernel bring-up** (not a part) | — | Enable `&de` (DRM master — whole stack is off without it); add `tcon_lcd0_out` RGB endpoint@0 + `panel-dpi` node + `pinctrl-0=<&lcd_rgb666_pins>`; remove ILI9341 overlay (PD10-12 clash); enable `DRM_FBDEV_EMULATION`/`FB_DEVICE`/`TOUCHSCREEN_EDT_FT5X06`. **Verify T113 TCON actually emits parallel RGB** (primarily targets DSI). |

### RISKS — fix before fab (cheap now, respin later)
| # | Fix | Part |
|---|-----|------|
| V6 | **I2C pull-ups → 1.5 KΩ** (see #97) + DNP parallel pads | 2× 1.5 KΩ 0603 |
| V7 | **Route IMU (#35) to its OWN TWI** (T113 has 4) + FIFO/watermark-INT | routing (no part) — polled tilt eats ~54% of shared bus |
| V8 | **Amp (#47) VDD → SYS/power-path; DRV2605L (#41) VDD → KEEP ON 3.3 V (M4 — reverted).** The MAX98357A logic pins are +6 V-rated, so SYS (3.0–4.4 V) is fine + gives headroom. But the DRV2605L's EN/SDA/SCL sit at 3.3 V while its input abs-max = VDD+0.3; if its VDD were on SYS and SYS sagged to ~3.0 V under a motor pulse, the 3.3 V logic pins would exceed abs-max → forward-bias the ESD clamps / back-power i2c1. So keep DRV VDD on **3.3 V** (its local 10 µF #45 buffers the motor pulse; 3.3 V fully drives the 1.8 Vrms LRA). | routing — amp on SYS, DRV on 3.3 V |
| V9 | **DRV2605L VDD decoupling + local bulk** | 0.1 µF (C14663) + 10 µF (C1713) at VDD — sources pulsed motor/overdrive locally |
| V10 | **Panel VDD local decoupling** at FPC pin 4 | 100 nF (C14663) + 1–10 µF (C15849/C1713); bump #19/#20 qty |
| V11 | **USB2 D+/D- ESD array + VBUS TVS** — ✅ **promoted to BOM row #98** (USBLC6-2SC6; optional VBUS TVS = DNP). No longer a loose note. |
| V12 | **TPS61165 VIN HF cap** | 100 nF (C14663) at VIN + bulk on the 5V feed |
| V13 | **GPADC0 battery-sense filter** | ~100 nF to AGND — Class-D 300 kHz noise aliases into the fuel-gauge SAR otherwise |
| V14 | **Backlight EN/CTRL pull-down** | 100 KΩ 0402 to GND — floats during boot → undefined backlight |

### MINOR — provision / document
| # | Fix |
|---|-----|
| V15 | SI: set `drive-strength=<10>` on `lcd_rgb666_pins` (free, tunable); DNP series-R footprint on **DCLK only** (0 Ω stuffed); route the 22-line bus over solid GND; keep the ribbon away from GPADC/touch/I2S/WiFi. (**22-33 Ω terminators on all lines = NOT needed** at 25 MHz — refuted.) |
| V16 | SS14 → **SS16 (60 V)** Schottky for open-LED OVP-clamp margin (1:1 SMA swap). |
| V17 | Touch DT compat = **`edt,edt-ft5506`** (a *listed* string) — `focaltech,ft7311` won't bind. INT 10 KΩ pull-up + FFC VDD 100 nF. Deferrable. |
| V18 | DRV2605L IN/TRIG (pin 4) → 10 KΩ to GND; EN keep tie + DNP GPIO-stuff option. |
| V19 | MAX98357A TQFN exposed pad → ground pour + thermal vias (it's the audio GND return). Class-D output = **keep filterless** (v1-proven; DNP ferrite+1nF footprint as EMC insurance only). |
| V20 | **Mechanical:** LRA (235 Hz) + speaker (850 Hz) shake the IMU → place IMU near rotational center, define DT mount-matrix, blank tilt integration during haptic events. |
| V21 | Kelvin-route Rset (#33) ground to TPS61165 GND pin. |
| V22 | **Pixel clock: use ~25 MHz (datasheet §3.2.4, 27 MHz MAX) with the panel's OWN porches — NOT the Ampire 33.3 MHz template** (would exceed the panel max). pll-video0 ÷12 = 25.00 MHz exactly. Also set DT DCLK sample edge/phase (`bus_flags`/`pixelclk-active`) — wrong edge = half-UI (~20 ns) error. |
| V23 | **Scope-check Buck B (0.9 V core) too** — dual-A7 @ 1.2 GHz can be 400–600 mA on the other 1 A buck; within limit but verify on silicon (same falling-edge method as the 3.3 V rail). |
| V24 | ~~TPS61165 OVP-pin handling~~ **RESOLVED — no external OVP pin exists.** The TPS61165 (SOT-23-6) has only CTRL/COMP/FB/GND/SW/VIN; open-LED protection (38 V typ) is **internal**, sensed via the **SW** pin (datasheet Pin Functions). **Nothing to route, no part to add.** |
| V25 | **Layout: keep I2S clocks (PG12/PG13, 1.5–3 MHz) off the slow open-drain i2c1 pair (PG8/PG9)** on the shared PG bank — ground trace between them; leave unused I2S MCLK (PG11) a static GPIO. |
| V26 | **Kernel config also needs `CONFIG_BACKLIGHT_GPIO`** (in addition to the FBDEV/FB/EDT-FT5X06 configs in V5) for the gpio-backlight on/off path. |

> **Software (no board impact):** ALSA **softvol cap** (bounds amp rail current + protects the 0.8 W speaker),
> fade-before-stop + clean I2S teardown (pop suppression), and record 235 Hz → DRV2605L LRA config.
> **Design-for-debug hooks to add now:** test points on every rail (5V/3.3V/0.9V/1.8V/1.5V/~19V-BL); a 0 Ω
> in series with the boost + amp feeds (lift to meter current); DNP bulk-cap pads on 3.3V; and re-verify
> rails with the Rev-B falling-edge scope method under screen+audio+haptic+SD load.

---

## 11. Independent datasheet review (2026-08-12) — fixes applied + punch-list

A second, independent adversarial datasheet pass (`BOM-REVIEW.md`, multi-agent finder → verifier) re-derived
every strap/value/derating from the datasheets in `cad/docs/`. It found **1 BLOCKER + 2 HIGH + 12 MEDIUM +
15 LOW**, refuted one concern (22 pF crystal caps are correct), and cleared two (STM6601 low-batt lockout,
CH224K resistor values). The two highest-stakes findings (B1, H2) were **re-verified against the datasheets here**
and confirmed. Value/strap fixes are applied inline in §1–§10; **new parts are folded into their functional sections** (§1/§3/§5/§8 — inserting them renumbered the rows after each insertion point); the full disposition is the punch-list table below.

**Full punch-list (disposition):** ✅ = applied inline · ➕ = new row (folded into its section) · 📐 = capture/routing/DT/firmware · 🔬 = needs vendor data

| ID | Sev | Finding | Row(s) | Disposition |
|----|-----|---------|--------|-------------|
| B1 | 🔴 | BQ24074 EN1/EN2 unstrapped → 100 mA charger, ILIM inert | #53 | ✅ #53 note **+ ➕ #60** (EN2→OUT, EN1→GND) |
| H1 | 🟠 | FP6161 core RUN/enable + ≥2 ms sequencing unspecified | #3 | ✅ #3 note **+ ➕ #9** (RC from STM6601 EN) |
| H2 | 🟠 | RGB666 bit map misread (Note 2 boilerplate) → likely inverted | #25, gate 3, V4 | ✅ reverted to 6-MSB default (R7..R2, gnd R1/R0) **+ 🔬 get Zettler's map** |
| M1 | 🟡 | RESET pull-up belongs on 1.8 V VCC-RTC, not 3.3 V | #14 | ✅ #14 note |
| M2 | 🟡 | DDR term = 400 mA (not 300) + in-package LDOB heat (~0.72 W) | §9 | ✅ §9 note (budget 400 mA / 1.1 A; ext DDR buck if Tj tight) |
| M3 | 🟡 | Core output cap = 10 µF (not 4.7 µF min); VDD-CORE typ 0.95 V | #21, #6/#7 | ✅ #21 note (10 µF core out); 📐 optional 0.93 V (82 k/150 k) |
| M4 | 🟡 | DRV2605L logic > abs-max if VDD on SYS at low battery | V8, #41 | ✅ V8 reverted — DRV stays on 3.3 V (amp stays on SYS) |
| M5 | 🟡 | MAX98357A 15 dB hard-clips above −7.6 dBFS | #49 | ✅ changed to 6 dB (GAIN_SLOT→VDD) |
| M6 | 🟡 | LSM6DSOX INT1 can latch I3C-only at POR | #35 | ✅ #35 note **+ ➕ #40** (10 kΩ pulldown) |
| M7 | 🟡 | PCA9555 §8.4.1.1 INT erratum on shared i2c1 | #85 | ✅ #85 note (park cmd ptr ≠00h) + 📐 poll safety net |
| M8 | 🟡 | PCM5102A anti-imaging RC has no BOM rows | §8 | ➕ #108/#109 (2×470 Ω + 2×2.2 nF) |
| M9 | 🟡 | XSMT hard-high back-pumps shut-down HP amp HPVDD | PCM note | ✅ XSMT→GPIO + DAC soft-mute when unplugged |
| M10 | 🟡 | Adafruit 2011 C-rate asserted, no datasheet on file | #73 | ✅ downgraded to verify-on-arrival |
| M11 | 🟡 | BQ24074 ~2 W foldback at warm ambient; OUT regulates 4.4 V | #53 | ✅ #53 note (OUT=4.4 V) + 📐 doc: play+charge throttles rate |
| M12 | 🟡 | Display/touch FPC contact-side unconfirmed | #26/#27 | 🔬 pull Hirose FH33J drawing, confirm same face **before routing** |
| L1 | 🟢 | Stale "33.3 MHz Ampire template" | §2 TODO | ✅ corrected to §3.2.4 25 MHz |
| L2 | 🟢 | HSYNC/VSYNC/DE polarity not captured | V22 | ✅ noted (HS/VS active-low, DE active-high) |
| L3 | 🟢 | "3 parallel LED strings" is an assumption | #28 | 📐 qualify "≥2 strings" (design unaffected) |
| L4 | 🟢 | Cin note "3.3 V rail" but VIN moved to SYS | #32 | 📐 reword to "at VIN on SYS" (10 V part OK) |
| L5 | 🟢 | pwm-gpio <6.5 kHz trips EasyScale/shutdown | #28 | 📐 prefer EasyScale; add Fig-16 FB RC only if PWM |
| L6 | 🟢 | V14 rationale wrong (CTRL has internal pulldown) | V14 | 📐 keep 100 k, fix the note |
| L7 | 🟢 | SS14 (40 V) ~1 V under 39 V OVP | #30 | ✅ → SS16 (60 V) |
| L8 | 🟢 | DRV2605L RATED_VOLTAGE/OD_CLAMP for 1.8 VAC LRA | #46 | 📐 firmware: program before auto-cal |
| L9 | 🟢 | §3 intro says IMU shares i2c1 (it's i2c2) | §3 | 📐 update intro |
| L10 | 🟢 | TPS63021 margin overstated (~1.6–1.9×, not 2.5×) | #66/§9 | 📐 restate (still clears 1×; V23 stays) |
| L11 | 🟢 | PCA9555 pull-up ~100 kΩ (33 µA), not "100 µA source" | #85 | ✅ corrected |
| L12 | 🟢 | I2C rise-time figures + ~1 kΩ tuning floor | #97 | ✅ corrected |
| L13 | 🟢 | USBLC6 cap in-row contradiction (3.5 pF correct) | #98 | ✅ corrected |
| L14 | 🟢 | PCM5102A LDOO missing the Fig-33 10 µF (0.1 µF suffices) | #105/#106 | 📐 optional add |
| L15 | 🟢 | §9 charge numbers stale vs built parts | §9 | 📐 reconcile (ISET 445 mA/0.22C; ILIM 1.34 A) |

**Cleared / refuted (no action):** STM6601 low-batt lockout — keep the **`A`** suffix (V_TH+ ≤2.60 V < 3.0 V cutoff; the `M`=3.10 V option would lock out a usable cell). CH224K VDD-R **1 kΩ** (#62) + VBUS-sense **10 kΩ** (#65) confirmed. 24 MHz **22 pF** load caps correct (T113 folds crystal C0 into Cshunt).

**Vendor-confirm gaps (🔬 — cannot close from files):** (1) Zettler RGB bit-to-pin map + MSB order; (2) Hirose FH33J FPC contact side vs C9160/C2919568; (3) FT7311 I2C addr/report format; (4) Vybronics VG0832022D coil impedance (≥8 Ω, its PDF wouldn't parse) + max drive vs OD_CLAMP; (5) Adafruit 2011 continuous-discharge + PCM trip. Full detail: `BOM-REVIEW.md`.

---

## Design notes / open decisions

**Pin budget (from feasibility analysis): FITS with lots of room — ~45 of 72 usable GPIO, ~27 spare**
(the expander frees the 10 button GPIO the earlier direct-GPIO plan used). The binding constraint is
**per-bank**, not total: the RGB screen fills the **entire PD bank** (PD0–21). Everything else lands
on other banks so they don't fight.

| Subsystem | Bus / peripheral | Pins | Bank |
|-----------|------------------|------|------|
| Screen (RGB666) | TCON-LCD0 | 22 | PD (full) |
| Backlight enable | GPIO | 1 | PD/PG |
| NOR (exists) | SPI0 | 4 | PC |
| SD (exists) | SDC0 | 6 | PF |
| Console (exists) | UART0 | 2 | PE |
| Buttons ×10 | **PCA9555 on shared I2C** (not GPIO) | **0** (+1 INT) | via I2C |
| Audio | I2S1 | 3 | PG |
| Shared sensor I2C | i2c1 (touch + IMU + haptics + **expander**) | 2 | PG |
| INTs (IMU/touch/charger/**expander**) | EINT | ~6 | PG/PE |
| Haptics | I2C only | 0 | — |
| Battery fuel gauge | MAX17048 on I2C (+ GPADC backup) | 0 | shares i2c1, addr 0x36 |

> **Buttons moved to a PCA9555 I2C expander** (see §6): frees ~8–10 PE-bank GPIO vs. direct wiring,
> at the cost of 1 INT pin + sharing the I2C bus. Enables a button daughterboard on a 4-wire cable.
> Full N-key rollover preserved (one read returns all 16 pins). Latency delta ~0.1 ms — imperceptible.

**Key constraints to respect at layout:**

1. **Only 2 SPI controllers, both spoken for** — SPI0 = NOR, SPI1 = *killed by RGB* (RGB uses
   PD10-12). **No SPI left** for any peripheral → IMU/haptics/touch must be I2C. No future SPI.
2. **RGB666 not RGB888** — RGB888 needs PB2-7, which carry I2S audio. Staying 18-bit keeps PB free.
3. **One display tech on PD** — RGB, LVDS, DSI, and old SPI1-ILI9341 all mux onto PD0-21. Pick one (RGB).
4. **DT function name is `"i2c1"`**, not `"twi1"` (datasheet vendor name), or the bus won't come up.
   The button expander binds via `nxp,pca9555` as an IRQ-capable gpiochip; `gpio-keys` then references
   the expander's gpiochip — **wire its INT and stay interrupt-driven (never poll)**; run the bus ≥400 kHz.
5. **GPADC full-scale ≈ 1.8 V**, not 3.3 V — size the battery divider accordingly.
6. **No mainline HW PWM** on T113 (6.12) → backlight brightness is `gpio-backlight` (on/off) or jittery
   `pwm-gpio`. No JLC-stocked I2C dimmer was found (LM3630A/KTZ8866/MP3309C/AW99706 all 0-stock), so #28 is
   the **TPS61165 boost** (on/off + EasyScale/`pwm-gpio`). Route the BL control pin to a PWM-capable GPIO.
7. **4-layer board minimum** (128-pin 0.5 mm QFP + in-package DDR3 + USB2 + 22 fast RGB lines).
8. **Include the VBUS input bulk cap** the Rev-A breakout was missing (brown-out fix).
9. **Soft-power-button-only (no slide switch) REQUIRES a power-latch controller (#94), not just a GPIO.**
   With no PMIC, a bare button can't power the board **on from fully-off** — the STM6601-class controller's
   always-on domain does that. The button (#92) wires to BOTH the controller AND a native SoC EINT
   (`wakeup-source`, NOT the expander). Full-off = SoC clean shutdown → `gpio-poweroff` releases the
   controller's HOLD → real rail cut. On/off work at bring-up; sleep/wake is a later software-only add.
10. **Bus headroom:** 4 TWI total — `i2c1` = shared sensor bus (touch/IMU/haptics/expander); a spare TWI is
    free (would host an ADS1015 on its own bus if analog sticks are ever added back — dropped for spin 1).
    **Bluetooth = onboard BT830 on a UART** (HCI); **WiFi = none** (no USB1 port; SDC1 free — no SDIO WiFi); **headphone DAC (PCM5102A #103) shares I2S1 with the speaker amp** — no extra I2S pins, **PB bank stays free** (I2S2 not needed; on-chip codec has no mainline driver).
11. **All buttons are tactiles (carbon pads dropped) → no ENIG requirement; HASL is fine.** The ENIG-flatness
    constraint only existed to seat a flat carbon pill. **(No per-button pull-ups needed either — PCA9555
    inputs have an internal ~100 µA pull-up; a tactile just ties pin→GND. Datasheet-confirmed, see #89.)**
12. **Buttons are bench-testable on the bare board** — press each tactile, confirm `gpio-keys` registers,
    *before* any enclosure exists. Keycaps/rockers + shell are designed around the chosen tactiles afterward.

**Immediate TODOs to turn ⚠️/🔲 into concrete parts (let's do these together):**

- [x] **5" panel LOCKED: Zettler ATM0500D27-CT** (#25) — datasheet-confirmed IPS + RGB parallel + RGB666.
      Connectors spec'd: **40P/0.5mm ZIF bottom-contact** display (#26) + **6P/1.0mm ZIF** touch (#27).
      Backlight = 18 V/40 mA (#28–29). Touch = FT7311 (#25). **Timings (L1 — corrected):** the datasheet §3.2.4
      DOES give a table — **DCLK 23/25/27 MHz (typ 25, 27 MAX)**, SYNC-DE mode, **HSYNC/VSYNC active-low + DE
      active-high** (L2). pll-video0 ÷12 = 25.00 MHz exactly. **Do NOT use the 33.3 MHz Ampire template** (23% over max). See V22.
      Remaining: confirm live LCSC stock for both connectors; enable `CONFIG_TOUCHSCREEN_EDT_FT5X06` (not GOODIX);
      (optional) draft the `panel-dpi` + FT7311 DT overlay.
- [ ] Confirm **BQ24074 / TPS63021 / JST-PH** LCSC #s + JLC tier live (#53, #66, #74).
- [x] **IMU chosen: LSM6DSOX (C481766, confirmed in JLC stock).** DT compat `st,lsm6dsox`, driver
      `st_lsm6dsx`. Adafruit LSM6DSOX breakout for breadboard bring-up.
- [x] **Haptics chosen: DRV2605L (C527464) + Vybronics VG0832022D LRA (235 Hz, DigiKey 9974288).**
      `ti,drv2605l`, Adafruit #2305 + ERM coin for bench bring-up. Record 235 Hz → driver config.
      Motor connector: **DECIDED — direct-solder the LRA leads to OUT± (no connector),** same as the speaker.
- [ ] Pick **speaker** + enclosure approach (#52) and its **connector** (#48).
- [ ] Confirm **PCA9555** LCSC # + tier (#85), set its I2C address strap, and decide mainboard vs
      button-daughterboard layout.
- [ ] **Buttons:** all tactiles now (carbon pads dropped → HASL ok, no ENIG). Pick the **soft low-force
      top-actuated tactile** for D-pad + ABXY (#86/#87, shared C#); Start/Select = C318884 (#88 ✅);
      Volume/Power/Bumpers = C49234144 side-actuated (#91/#92/#93 ✅). (No per-button pull-ups — PCA9555
      internal, #89.) Keycaps/rockers designed around the chosen tactiles later.
- [ ] Confirm the **push-button power controller** (#94, STM6601 vs discrete P-FET latch) + verify LCSC/tier;
      wire the power button (#92) to both it and a native SoC EINT. Volume +/− (#91) on PCA9555 spare pins.
- [x] **Fuel gauge = MAX17048 (#77, I2C, ~±1 %)** for accurate on-screen %; GPADC divider (#75/#76) kept as backup. *(Reverses the earlier "divider-only" call — a dedicated gauge was added for a smooth %.)*
- [ ] Confirm the **shared-I2C-bus SDA/SCL pull-up value** (per §10 V-items; the PCA9555 DS just says "pull-up to VCC"). This is the *bus* pull-up, distinct from the (removed) per-button idea.

**Tier-1 extras to lock (cheap, recommended now):**

- [x] **Stereo:** DROPPED for spin 1 — mono (single MAX98357A #47 + one speaker #52) kept. Handheld speaker spacing makes true stereo barely perceptible; revisit only if a later shell separates two speakers.
- [ ] **Charge LED** off bq24074 — pick LED/resistor (reuse #23/#24 parts).
- [x] **Backlight driver LOCKED: TI TPS61165DBVR (C58756)** — 3.3 V→~19 V CC boost, 40 mA via ~5 Ω sense R.
      (I2C dimmers LM3630A/KTZ8866 were 0-stock, so no smooth `pwm_bl`; use EasyScale/`pwm-gpio`.)
      Remaining: size the boost inductor/diode (#29/#30) + **50 V** output cap (#31) per the TPS61165 datasheet.

**Tier-2 extras to decide (optional, per appetite):**

- [x] **Analog sticks: DROPPED for spin 1.** JLC-buildable YTL sticks exist but are springy RC/panel-style (not console thumb-feel) + mostly no click; a true thumbstick is a hand-mounted v4-class module. D-pad + buttons for now; 2 spare expander pins + a spare TWI left free if ever added back.
- [ ] **Bluetooth = onboard Ezurio BT830-SA-01** (#99, DigiKey, CSR8811 UART-HCI, integrated antenna, BlueZ). **TODO before layout:** (1) footprint w/ **outward-extended pads** (hand-solderable castellations) + **antenna keep-out / board-edge overhang**; (2) UART (w/ RTS/CTS) → free T113 UART, `btattach` bring-up; (3) VREG_EN_RST# strap + 3.3 V VDD/VDD_PADS + decoupling. **Fit=Hand** (JLC leaves footprint bare; you solder the module). ⚠️ NFND part — fine for a one-off.
- [x] **WiFi = DROPPED** (decided: no internal USB1 port). Wireless is Bluetooth-only (BT830). No SDIO WiFi module fit the T113 (all candidates PCIe/PIO/bare-chip — see §8); adding an internal USB1 dongle port was declined. If WiFi is ever needed: a USB-C OTG dongle on USB0 (no board change).
- [ ] **Headphone out = PCM5102A stereo DAC on shared I2S1** (#103) + PJ-327C-4A TRS jack (#115). On-chip codec has no mainline driver → external DAC. **TODO:** (1) verify PCM5102A on JLC + datasheet support-parts pass (decoupling, charge-pump cap, config straps, output RC); (2) verify jack pinout vs drawing; (3) jack-detect GPIO → mute speaker amp (#47 SD). Stereo out only (no mic); SW volume (softvol).
- [x] **Ambient-light auto-brightness: DROPPED** — manual brightness (backlight #28) is enough; no sensor/daemon/enclosure window. (Full-off latch is already handled by #94.)

**Phased bring-up order (once fabbed):** power + NOR + SD + UART console (proven) → screen
(riskiest new path — RGB endpoint DT edit) → buttons (`evtest` diagonals) → audio (`aplay`) →
sensor I2C (IMU + haptics) → touch → battery telemetry (GPADC).

**Related docs:**
- `../board/t113-gameboy/` — current DT overlays (ILI9341, NOR, UART0 console) to evolve.
- `../../t113-breakout/BOM.md` — the proven core this board builds on.
- `../../t113-breakout/REV-B-FIXES.md` — silicon-learned fixes (input bulk cap, SD card-detect, USB HS).
