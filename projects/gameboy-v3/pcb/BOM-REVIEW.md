# gameboy-v3 PCB — Independent BOM / Datasheet Review

**Date:** 2026-08-12
**Reviewer:** independent adversarial datasheet pass (multi-agent finder → verifier; each finding re-derived from the actual datasheets in `cad/docs/` and a second pass tried to refute it).
**Scope:** all of [`BOM.md`](BOM.md) §1–§10 — every strap, divider, cap value, DC-bias derating, pinout, brownout, SI and thermal claim re-computed from the datasheet, not trusted from the BOM.
**Status of fixes:** **report only — none of these are applied to `BOM.md` yet.** This doc is the punch-list for schematic capture.

> The BOM is fundamentally sound. It had already been through its own adversarial passes (§10), and this review *refuted* some plausible-looking concerns (e.g. the 24 MHz load caps are correct). Almost everything below is a strap / value / note fix, not an architecture problem. **If you fix only three things before ordering: the BQ24074 EN1/EN2 straps, the FP6161 RUN net, and get Zettler's RGB bit map.**

## Summary

| Severity | Count | The ones that reach silicon broken |
|---|---:|---|
| 🔴 BLOCKER | 1 | BQ24074 EN1/EN2 unstrapped → charger stuck at 100 mA |
| 🟠 HIGH | 2 | FP6161 core enable/sequencing unspecified; RGB666 bit-map likely inverted |
| 🟡 MEDIUM | 12 | see below |
| 🟢 LOW | 15 | accuracy / stale-doc / provisioning |
| ⚪ Refuted | 1 | 24 MHz load caps are actually correct |
| ✅ Cleared | — | STM6601 low-batt lockout; CH224K resistor values |

**Verification gaps:** several parts have **no datasheet in `cad/docs/`** (Adafruit 2011 cell, USBLC6-2SC6, SP3004-04XTG, PJ-327C-4A jack, FT7311, and the Hirose FPC reference). Findings touching those are marked *uncertain* / listed as questions, not confirmed defects. *(The Vybronics LRA was in this list — now **resolved**, see question 4.)*

---

## 🔴 BLOCKER

### B1 — BQ24074 EN1/EN2 never strapped → charger boots in USB100 (100 mA); ILIM resistor (#56) is inert
**Ref:** §5 #51 / #56 · **Category:** pinout-strap · **Verdict:** CONFIRMED (twice, independently)

Both EN1/EN2 have internal ~285 kΩ pulldowns; the ILIM resistor sets the input limit **only** in the `EN2=1, EN1=0` state. #51's "datasheet-verified pin config" lists CE/TMR/ITERM/ILIM/ISET but **omits EN1/EN2**, and no net in §5 straps them → floating = `0,0` = **USB100 = 100 mA input**, ILIM does nothing. Charge-while-play disabled, battery net-drains while plugged, and dead-cell cold-boot may lack the input current to bring rails up. Kills the §9 finding-#1 premise.

- **Evidence:** bq24074.pdf Table 7-2 (p.9): `0,0→100 mA USB100`; `1,0→"Set by an external resistor from ILIM to VSS"`. Table 7-1 (p.8): "internally pulled down ~285 kΩ. **Do not leave EN1 or EN2 unconnected.**" I_INMAX EC table confirms 90/95/100 mA at `0,0`.
- **Fix:** strap **EN2 → HIGH, EN1 → GND**. Tie EN2 to the **always-on OUT/SYS node** — **NOT to IN.** IN survives to 28 V on a mis-plug, which exceeds the 7 V EN abs-max; OUT is 3.4–4.4 V (VIH is only 1.4 V) and is safe. A 0 Ω from EN2→OUT is fine. Add both as explicit BOM rows/nets.

---

## 🟠 HIGH

### H1 — 0.9 V core buck (FP6161) enable + power sequencing unspecified
**Ref:** §1 #3 / §6 #91 / §9 · **Category:** pinout-strap · **Verdict:** CONFIRMED

#3 states only "Input = SYS", #91 gates only the TPS63021 EN, and §9's tree shows no core enable. T113 §5.12.1 requires **VCC-IO up ≥2 ms before VDD-CORE**. The trap: "RUN must not float" → someone ties RUN→SYS → the core rail is alive whenever a cell is present (no true off + battery drain), and gating RUN off the *same* STM6601 EN as 3.3 V raises both together, still violating the 2 ms order. **RUN abs-max = VIN(SYS)**, so RUN also can't tie to the regulated 3.3 V (3.3 V > a 3.0 V SYS sag). New to v3 — the breakout had no soft-power/battery split, so "silicon-proven" does not cover it.

- **Evidence:** T113 §5.12.1 (VCC-IO before VDD-CORE, ≥2 ms). FP6161 DS: pin1=RUN, VRUN 0.3/1/1.5 V, abs-max "RUN −0.3 to VIN".
- **Fix:** drive RUN from the STM6601 EN (swings to the SYS level, keeping RUN ≤ VIN) through an RC giving ≥2 ms, so the core enables only after VCC-IO is valid. *(Correction to a sub-claim: T113 §5.12.2 says no ordering restriction on power-*off*, so the off-state issue is battery-drain / no-true-off, not a shutdown-sequencing violation.)*

### H2 — RGB666 bit map (§10 V4) rests on a misread datasheet note and is likely inverted
**Ref:** §2 #24 (lines 117–121) / §10 V4 (line 550) · **Category:** pinout-strap · **Verdict:** CONFIRMED

The panel is a genuine **24-bit** part (pin table lists full R0–R7/G0–G7/B0–B7; §1: 16.7 M colors). The BOM's "R6/R7 unused, R0=MSB" rests on §3.1.2 **Note 2**, which is actually the VIH/VIL *applicability* list — and it references a "RESET" signal that **does not exist** in the 40-pin pinout (boilerplate tell). The datasheet is **silent on bit order**. Under the conventional ascending bus (R7=MSB), V4's plan (drive R0–R5, ground R6/R7) grounds the **two MSBs of every channel** → each channel capped at 0b00111111 = 63/255 = **24.7% of full scale** → a dark, no-true-white image. The convention-safe wiring is the opposite (drive R7..R2, ground R1/R0).

- **Evidence:** ATM0500D27-CT §2 Pin Assignment (p.4): pins 5–12 `R0~R7` (full bus, none marked unused). §1: 16.7 M colors / 24-bit. §3.1.2 Note 2 contains `RESET` but the pin table has DISP (pin 31), no RESET.
- **Fix:** delete the Note-2 justification; treat neither the 6-of-8 selection nor MSB order as datasheet-resolved. Get Zettler's explicit R/G/B bit-to-pin map before routing. Absent that, default to driving the 6 MSBs.

---

## 🟡 MEDIUM

### M1 — RESET pull-up (#13) is a VCC-RTC (1.8 V) pin, lumped with the 3.3 V SD pull-ups
**Ref:** §1 #13 · voltage-rating · CONFIRMED
RESET (ball 27, I/OD) buffer power = VCC-RTC = 1.8 V (abs-max 2.16 V). A 3.3 V pull-up over-stresses the pin and injects ~80 µA into the RTC rail (clamps to ~2.5 V). Board still boots, so silicon-proof doesn't clear it. **Fix:** pull RESET to the 1.8 V VCC-RTC net; verify the breakout's actual net (BOM lists only the resistor, not the rail).

### M2 — 3.3 V budget under-counts DDR term + ignores in-package LDOB heat
**Ref:** §1 note / §9 Rail A · thermal / value-error · CONFIRMED
VCC-DRAM is capped at **400 mA** (Table 5-3), not 300, and "no datasheet ceiling" is wrong — there is one. If VCC-DRAM is internal-LDOB-sourced off 3.3 V, LDOB dissipates up to **(3.3−1.5)×0.4 ≈ 0.72 W inside the package** on top of ~0.72 W core, against Tj 110 °C; the datasheet's ambient ratings are explicitly conditioned on *external* VCC-DRAM power. **Fix:** budget 400 mA (→ 3.3 V worst ≈ 1.1 A) + the in-package heat; if Tj is tight, add an external 1.5 V DDR buck (removes both the reflection and the heat). Rail current still fits the buck-boost — thermal/accuracy, not brownout. (The genuinely-open item is LDOB's *linear* current capability — keep §10 V23 scope.)

### M3 — 0.9 V core set below VDD-CORE typ + uses the FP6161 minimum output cap
**Ref:** §9 Rail B / #6 #7 #20 · value-error · CONFIRMED
Divider is correctly 0.900 V, but VDD-CORE typ is **0.95 V**; worst-case VFB gives ~0.867 V — only ~17 mV over the 0.85 V floor before IR-drop/load-step droop at 800 mA. #20 uses 4.7 µF (smallest in the FP6161 table) vs the 10 µF typ-app. **Fix:** use 10 µF on the core output; if the breakout wasn't validated at 1.2 GHz, consider ~0.93 V (82 k/150 k); keep §10 V23.

### M4 — DRV2605L logic pins exceed abs-max at low battery if VDD moved to SYS (§10 V8)
**Ref:** §3 #39 / #94 / §10 V8 · voltage-rating · CONFIRMED
With VDD on SYS (3.0–4.4 V) but EN hard-tied to 3.3 V and SDA/SCL pulled to 3.3 V, the pins hit VDD+0.3 when SYS sags to ~3.0 V under a motor pulse (abs-max = VDD+0.3). Forward-biases the ESD clamps into VDD / back-powers i2c1. **Fix (cleanest):** revert V8 for the DRV only — keep VDD on 3.3 V (local 10 µF bulk #43 buffers the pulse; 3.3 V fully drives a 1.8 Vrms LRA). The amp #45 may stay on SYS (its logic pins are +6 V-rated / sit near 0.4 V). *(See also Q: why does V8 move it at all?)*

### M5 — MAX98357A at 15 dB gain hard-clips above −7.6 dBFS on the SYS rail
**Ref:** §4 #47 / #50 · value-error · CONFIRMED
DAC FS 1.27 Vrms × 15 dB = 7.16 Vrms vs a 2.97 Vrms rail limit at 4.2 V → clips above −7.6 dBFS. **6 dB** (GAIN_SLOT→VDD) maps 0 dBFS to 2.54 Vrms ≈ 0.807 W — almost exactly the CES-2704 0.8 W rating, no clipping. **Fix:** change #47 from 100 kΩ→GND to a direct VDD tie (6 dB); keep the softvol cap as the continuous-power backstop. *(§4's "1–2 W into 8 Ω" overstates — max is ~1.2 W on ≤4.4 V.)*

### M6 — LSM6DSOX interface latched at power-up by INT1 → a DT bias-pull-up or driven-high pin selects I3C-only
**Ref:** §3 #34 · pinout-strap · CONFIRMED
T113 has no I3C → I3C-only = dead IMU. Default (internal pulldown + hi-Z GPIO) lands on I2C, but a pinctrl `bias-pull-up` or a pin driven high during power-up overrides it. **Fix:** populate a ~10 kΩ external pulldown at INT1 and set its pinmux to input-no-pull through boot; verify the chosen pin's reset-default. *(LSM6DSOX §5.3.)*

### M7 — PCA9555 §8.4.1.1 interrupt erratum is structurally triggered by this shared bus
**Ref:** §6 #82 / #86 / §10 V7 · erratum-unaddressed · CONFIRMED
INT can be improperly de-asserted when the last command byte was 00h (which the mainline `pca953x` bulk-read leaves) *and* another i2c1 slave (MAX17048/INA226/FT7311, all polled) is addressed for read → a button edge in that window can be dropped. Not fatal (resyncs on next edge), fixable in your own kernel. **Fix:** park the command pointer at a non-00h register after each input read; add a low-rate `gpio-keys` poll safety net; verify with `evtest`. *(Note: §10 V7 correctly moves the IMU to i2c2 — the trigger is the gauge/INA/touch reads, not the IMU.)*

### M8 — PCM5102A output anti-imaging RC (2×470 Ω + 2×2.2 nF) referenced in the design but has no BOM rows
**Ref:** §8 #100–#110 · bom-completeness · CONFIRMED
The integration note routes "DAC → 470 Ω/2.2 nF RC → coupling cap", and Fig 33 + the EC footnote require it, but rows #100–#110 contain no 470 Ω / 2.2 nF part → on a no-rework JLC build they won't be placed and the DAC runs outside its characterized filter (out-of-band noise to ~3 MHz into the HP amp; the 470 Ω also isolates the line driver from the 1 µF coupling-cap load). fc = 1/(2π·470·2.2 nF) ≈ 154 kHz. **Fix:** add 2×470 Ω (1%, 0402/0603) series on OUTL(6)/OUTR(7) + 2×2.2 nF C0G shunt to AGND, as explicit rows ahead of the #106 coupling caps.

### M9 — Hardwired XSMT=high drives the shut-down TPA6132A2 inputs past their (collapsed-rail) abs-max during speaker-only playback
**Ref:** §8 #100 vs #105 / #109 · voltage-rating · CONFIRMED
The routing truth table makes "headphones out → HP amp in shutdown" the default; in shutdown the charge-pump rails collapse so the input abs-max window shrinks to ≈±0.3 V while the DAC drives ~2.8 Vpk through the coupling caps → sustained clamp-diode current that can back-pump HPVDD past its 1.9 V abs-max. Softvol only attenuates (still >0.3 V at −12 dB). **Fix:** route XSMT(17) to a spare GPIO and soft-mute the DAC when headphones are unplugged (built-in 104-sample ramp avoids pops), or keep the HP amp EN asserted whenever the DAC streams (~2.1 mA). If the hard-tie is kept, bench-verify the shutdown clamp current + HPVDD/HPVSS under full-scale before fab.

### M10 — Shipping cell (Adafruit 2011, 2000 mAh) has no datasheet on file → "1C = 2 A RESOLVED" is asserted, not verified
**Ref:** §5 #70 · sourcing · CONFIRMED
Only the superseded 1200 mAh 503562 doc is present (rated "1 C5A max continuous"). If the 2011 mirrors that, the ~1.8–2 A combined-load peak sits at ~0.9–1C with near-zero margin → an aligned CPU+backlight-inrush+audio+haptic burst could trip the PCM (board reset). **Fix:** downgrade the claim to "verify-on-arrival"; confirm continuous ≥2 A + PCM trip well above 2 A + 60×36×7 mm fit + JST-PH polarity. *(ISET at 445 mA/0.22C is safe regardless.)*

### M11 — BQ24074 linear power-path dissipation → charge-current foldback at warm ambient
**Ref:** §5 #51 · thermal · CONFIRMED (informational)
Requested calc (charge-only): P=(5−3.0)·0.445 = 0.89 W → TJ ≈ 65 °C (θJA 44.5 °C/W). Fine. But at the full ~1.45 A input limit while *playing on a low cell*, the input FET drops 5 V→~3.7 V → ~1.9 W, total ~2 W → TJ ≈ 114 °C at 25 °C ambient → **thermal-regulation foldback at warm ambient.** Self-protecting (throttles at 125 °C); exposed-pad→VSS + via array is mandatory (already in BOM) and the θJA depends on it. **Fix:** none required; document that simultaneous heavy play+charge will reduce charge rate. *(Also: #51's "OUT clamps to 3.4 V on a dead cell" is '72 behavior — the '74 regulates OUT to VO(REG)=4.4 V. Doesn't change the EN2-strap safety.)*

### M12 — [UNCERTAIN] Display/touch FPC connector contact-side can't be confirmed from files present
**Ref:** §2 #25 / #26 · pinout-strap · UNCERTAIN
The datasheet's reference part is **Hirose FH33J-40S-0.5SH(10)**; the BOOMELE C9160 substitute's "bottom-contact CONFIRMED" comes purely from the BOM's own read of a §7 side-view whose fine labels aren't resolvable, and the Hirose datasheet isn't in the folder. A contact-side mismatch = FPC gold fingers face away = **dead panel, no rework.** **Fix:** pull the FH33J drawing, confirm its contact side, then confirm C9160 (and the 6P/1.0 mm touch #26, HDGC C2919568) place contacts on the same face as the panel FPC — *before* routing, not "on arrival."

---

## 🟢 LOW — accuracy, stale text, provisioning

| # | Ref | Issue | Fix |
|---|---|---|---|
| L1 | §2 TODO (line ~649) | Stale "use Ampire 33.3 MHz template" contradicts the correct §3.2.4 25 MHz / 27 MHz-max stated twice elsewhere (§2 gate, V22). 33.3 MHz is 23% over max. | Replace with §3.2.4 values: DCLK typ 25 MHz (pll-video0 ÷12 = 25.000), ~816×496 @ 25 MHz ≈ 61.8 Hz. |
| L2 | §2 / §10 V22 | HSYNC/VSYNC/DE polarity not captured. Datasheet waveforms: HSYNC/VSYNC **active-low**, DE **active-high**, SYNC-DE valid. | Set `hsync-active=<0>`, `vsync-active=<0>`, `de-active=<1>` in panel-dpi. |
| L3 | §2 #27 | "3 parallel LED strings" is an assumption — datasheet only guarantees ≥2 (40 mA total > 25 mA/LED abs-max). | Qualify to "≥2 strings"; design fine (Rset regulates total 40 mA). Per-LED margin thinner (20/25 mA) if only 2. |
| L4 | §2 #31 (+ crosscut) | Cin note says "on the 3.3 V rail", but VIN moved to SYS (#27/V3/V12). It's a VIN-pin cap; belongs on SYS. 10 V rating OK for SYS ≤4.5 V (DC-bias → ~2.5–3 µF, still in the 1–4.7 µF window). | Reword to "at VIN on SYS"; keep the 10 V part. Doc/placement only. |
| L5 | §2 #27 / note 6 | `pwm-gpio` dimming below ~6.5 kHz trips EasyScale/shutdown (CTRL-low >260 µs → ES, >2.5 ms → off). The datasheet's low-freq path needs an FB-pin RC network the BOM lacks. | Prefer EasyScale (primary, zero extra parts). If PWM needed, add the Fig-16 FB RC (~100 k+80 k+0.1 µF); never low-freq PWM direct on CTRL. |
| L6 | §10 V14 | Rationale wrong — CTRL has an internal 400–1600 kΩ pulldown, so backlight already defaults OFF at boot. | Keep the 100 kΩ (harmless belt-and-suspenders); fix the note. |
| L7 | §2 #29 / §10 V16 | SS14 (40 V) sits ~1 V under the 39 V max open-LED OVP. | Promote V16 into #29: SS16 (60 V), free 1:1 SMA swap. |
| L8 | §3 #44 | DRV2605L RATED_VOLTAGE≈0x46 / OD_CLAMP≤0x78 must be programmed for the 1.8 VAC LRA before auto-cal (POR defaults over-drive it). ✅ LRA datasheet now confirms 22.5 Ω / 1.8 Vrms (1.85 max) / 235 Hz (see Q4), so OD_CLAMP=0x78 is **mandatory** — on 3.3 V the bridge could push ~2.33 Vrms > the 1.85 Vrms max. | Firmware/bring-up; verify the mainline `ti,drv2605` driver actually writes these. |
| L9 | §3 header | Stale: §3 intro says IMU shares i2c1; V7/§9.5/§6 correctly put it on i2c2. | Update §3 intro. |
| L10 | §5 #63 / §9 | TPS63021 margin overstated: "~2.5×, 1.9–2.4 A at 3.0 V" uses the retired 745 mA load + wrong Fig-2 point. Real ≈ **1.6–1.9×** vs ~1 A (min-spec part reads ~1.6 A at 3.0 V). | Restate the number; V23 scope stays mandatory. Still clears 1×. |
| L11 | §6 #82 / #86 / §9 | PCA9555 pull-up overstated ~3×: it's a ~100 kΩ resistor (≈33 µA at 3.3 V), not a "100 µA source". | Reword to "~100 kΩ (≈33 µA, weak)". No-external-pull-up decision still correct; keep button traces local to the expander. |
| L12 | §7 #94 / §10 V6 | Rise-time inconsistency: "10 kΩ → 680 ns" uses ~80 pF; "1.5 kΩ → 190 ns" uses 150 pF. At 150 pF, 10 kΩ = **1.27 µs**. Also DNP-tuning **lower bound ~1 kΩ** (below that, paralleled pull-ups over-sink the 3 mA I_OL of the '9555 SDA/INT). 1.5 kΩ meets 300 ns only to ~236 pF. | Correct the figure; annotate the tuning floor; measure bus C on first article if the daughterboard cable is long. |
| L13 | §7 #95 | USBLC6-2SC6 cap self-contradictory in-row (0.35 pF vs 3.5 pF). ~3.5 pF is correct (fine for USB2 HS). | Single correct figure; reserve "ultra-low-cap 0.85 pF" for the SP3004 SD array (#12). |
| L14 | §8 #102 / #103 | PCM5102A LDOO (pin 18) missing the 10 µF bulk shown in Fig 33 (only 0.1 µF placed). Table 12 requires only 0.1 µF. | Optional: add a 4th 10 µF on LDOO for transient response. |
| L15 | §9 SYS table / finding #1 | Charge/input numbers don't match the parts: SYS table "740 mA (0.5C)" vs #55's 2.0 kΩ = **445 mA (0.22C, ~4.5 h)**; finding #1 "1.46 A (1.1 kΩ)" vs #56 = 1.2 kΩ = **1.34 A**. Both built values correct. | Reconcile §9 narrative + user-facing charge-time. |

---

## ✅ Cleared — potential blockers that checked out

- **STM6601 low-battery lockout — CLEARED.** The `A` suffix gives V_TH+ = 2.40/2.50/**2.60 V max** over full temp/tolerance (Table 5). VCC = SYS = the cell (3.0 V PCM cutoff) → worst case 2.60 V < 3.0 V → **0.40 V margin, no lockout in any corner.** The design dodged a real trap: the next option up (`M` = 3.10 V, min 3.00 V) would have locked out a usable cell. Keep `A` locked. Full `STM6601CA2BDM6F` decode confirmed field-by-field; EN confirmed push-pull active-high (drives TPS63021 EN directly, no pull); PB internal 100 kΩ pull-up; PSHOLD handshake + startup pulldown failsafe; VCC-on-SYS placement correct; unused open-drain outputs + SR/CSRD safe left open.
- **CH224K VDD series-R = 1 kΩ (#59) and VBUS-sense R = 10 kΩ (#62)** — both confirmed correct per the §6.2 reference schematic. **Resolves the BOM's two flagged uncertainties.** *(Minor: CFG1 uses 10 kΩ→5 V, which works; the datasheet-canonical no-MCU value is 100 kΩ→VBUS.)*
- **BQ24074 ISET 445 mA, ILIM 1.34 A** (safely under the 1.6 A IN abs-max even at the 1% worst corner 1.45 A), **TS 10 kΩ** (mandatory — the '74 has no float-disable unlike '72/'73), **OVP 10.5 V / IN abs-max 28 V** — all confirmed OK.

## ⚪ Refuted

- **24 MHz load caps (22 pF) are correct.** The "22 pF is below textbook" concern only holds under the model that excludes crystal C0. The T113 datasheet (Table 5-10) folds C0 into Cshunt: CL = (C1·C2)/(C1+C2) + Cshunt → 11 pF + ~6.5 pF ≈ 18 pF = the crystal's stated CL. No change needed. *(The ±ppm concern is also moot — no REFCLK-OUT/WiFi on this board.)*

---

## Verification gaps / open questions (close before / during bring-up)

Highest-value first. Several exist only because the part's datasheet is not in `cad/docs/`.

1. **Zettler RGB bit-to-pin map + MSB order** — drives H2. Datasheet doesn't state it.
2. **FPC contact side** — pull the Hirose FH33J-40S-0.5SH(10) drawing; confirm C9160 + the touch connector place contacts on the same face as the panel FPC gold fingers.
3. **FT7311 touch** — address (assumed 0x38) + report format; no FT7311 datasheet on file → confirm on the physical panel via `i2cdetect`. Bind `edt,edt-ft5506` or add a `focaltech,ft7311` compatible.
4. **Vybronics VG0832022D — ✅ RESOLVED (2026-08-13).** Recovered from `cad/docs/Vybronics-VG0932022D-datasheet.md` (filename typo — the content is the VG0832022D spec; every page header says so). Coil **resistance = 22.5 Ω ±10%** (§4-3) → far above the DRV2605L ~4 Ω `ZL(th)` trip and the ≥8 Ω target, squarely in the 8–35 Ω LRA range. Rated **1.8 Vrms**, operating max **1.85 Vrms** (§2-1/2-2) → OD_CLAMP = 0x78 (2.546 Vpeak = 1.8 Vrms) sits safely under the 2.616 Vpeak ceiling; resonance **235 ±7 Hz** confirmed. Good match; see L8.
5. **CES-2704 voice-coil inductance** — MAX98357A filterless needs >10 µH; unspecified in its datasheet.
6. **INA226** — confirm VS source (SYS keeps telemetry alive at cold-start vs 3.3 V), A0/A1 both to GND for 0x40, and that the shunt is in the **SYS-load** path (won't capture battery/charge current — fine if board-draw is the intent).
7. **SP3004-04XTG (#12)** — no datasheet; confirm it's a self-referenced unidirectional array (no rail pin that must tie to SD 3.3 V).
8. **PJ-327C-4A jack** — no datasheet; read tip/ring/sleeve/detect pad map + detect-switch polarity (NC vs NO) from the maker drawing → feeds the `SW_HEADPHONE_INSERT` logic + speaker-mute truth table. Confirm the detect GPIO's pull.
9. **Why does §10 V8 move the DRV2605L VDD to SYS at all?** A 1.8 Vrms LRA needs ~2.55 Vpeak, which 3.3 V supplies with headroom, and SYS creates the M4 over-voltage. If nothing else needs it, keep the DRV on 3.3 V.
10. **PCA9555 A2/A1/A0 straps** — confirm all three hard-strapped (device at exactly 0x20; must not float).
11. **Daughterboard cable length / final bus C** — the 150 pF estimate assumes a short cable; 1.5 kΩ meets the 300 ns budget only to ~236 pF. Measure on first article before committing the pull-up.
12. **ALSA softvol cap** — confirm it's applied in the kernel machine driver / UCM so it's active from first boot (it's the only speaker protection *and* the only thing keeping the TPA6132A2 out of clip). A raw full-scale `aplay` at bring-up otherwise over-drives the 0.8 W speaker.
13. **VCC-DRAM source** — which rail actually sources 1.5 V on this SiP (internal LDOB off 3.3 V, or other)? Sets M2's 400 mA reflection. BOM flags it unconfirmed vs the t113-breakout schematic.

---

## Fix-first shortlist

1. **BQ24074 EN2→OUT/SYS high, EN1→GND** (B1). One net each; without it the charger is a 100 mA device.
2. **FP6161 RUN net + ≥2 ms sequencing** (H1). RC from STM6601 EN; don't tie to SYS or 3.3 V.
3. **Zettler RGB bit map** (H2). Blocking for *routing*, not schematic.

Everything else is strap/value/note-level and can be batched into the schematic-capture pass.
