# gameboy-v3 PCB — Independent BOM / Datasheet Review

**Date:** 2026-08-12
**Reviewer:** independent adversarial datasheet pass (multi-agent finder → verifier; each finding re-derived from the actual datasheets in `cad/docs/` and a second pass tried to refute it).
**Scope:** all of [`BOM.md`](BOM.md) §1–§10 — every strap, divider, cap value, DC-bias derating, pinout, brownout, SI and thermal claim re-computed from the datasheet, not trusted from the BOM.
**Status of fixes:** **report only — none of these are applied to `BOM.md` yet.** This doc is the punch-list for schematic capture.

> The BOM is fundamentally sound. It had already been through its own adversarial passes (§10), and this review *refuted* some plausible-looking concerns (e.g. the 24 MHz load caps are correct). Almost everything below is a strap / value / note fix, not an architecture problem. **If you fix only three things before ordering: the BQ24074 EN1/EN2 straps, the FP6161 RUN net, and get Zettler's RGB bit map** (the last is now ✅ done — RGB666 map bench-confirmed on silicon 2026-08-23).

## Summary

| Severity | Count | The ones that reach silicon broken |
|---|---:|---|
| 🔴 BLOCKER | 1 | BQ24074 EN1/EN2 unstrapped → charger stuck at 100 mA |
| 🟠 HIGH | 1 | FP6161 core enable/sequencing unspecified (H2 RGB666 bit-map ✅ resolved on silicon 2026-08-23) |
| 🟡 MEDIUM | 12 | see below |
| 🟢 LOW | 15 | accuracy / stale-doc / provisioning |
| ⚪ Refuted | 1 | 24 MHz load caps are actually correct |
| ✅ Cleared | — | STM6601 low-batt lockout; CH224K resistor values |

**Verification gaps:** several parts have **no datasheet in `cad/docs/`** (Adafruit 2011 cell, USBLC6-2SC6, SP3004-04XTG, PJ-327C-4A jack, FT7311, and the Hirose FPC reference). Findings touching those are marked *uncertain* / listed as questions, not confirmed defects. *(Closed: Q2 (FPC contact-side — both bottom-contact, opposite faces), Q3 (software bring-up), Q4 (resolved), Q6–Q10 (schematic/routing notes), Q11 (single-board), Q12 (softvol/M5), Q13 (internal-LDOB confirmed). **Still open: Q5 speaker Le — verify-on-arrival** (Q1 RGB bit-map ✅ closed 2026-08-23 — bench-tested on silicon; H2 resolved).)*

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

### H2 — [✅ RESOLVED] RGB666 bit map = R7=MSB (drive R7..R2, ground R1/R0) — datasheet-confirmed + bench-tested on silicon 2026-08-23
**Ref:** §2 #24 (lines 117–121) / §10 V4 (line 550) · **Category:** pinout-strap · **Verdict:** CONFIRMED

The panel is a genuine **24-bit** part (pin table lists full R0–R7/G0–G7/B0–B7; §1: 16.7 M colors). The BOM's original "R6/R7 unused, R0=MSB" rested on §3.1.2 **Note 2**, which is actually the VIH/VIL *applicability* list — and it cites a "RESET" signal that **does not exist** in the 40-pin pinout (pin 31 is DISP) → **boilerplate, disregard.** **Firsthand re-read (2026-08-13): the datasheet is NOT silent — it leans to R7=MSB.** The §3.2 timing diagrams label the data bus **`DR[7:0]`/`DG[7:0]`/`DB[7:0]`** (bracket notation → bit 7 = MSB) and §3.2.4 is titled "Parallel 24-bit RGB" — so the authoritative content points to a full 8-bit bus with **R7=MSB → RGB666 = drive R7..R2, ground R1/R0** (the convention-safe default). The original plan (drive R0–R5, ground R6/R7) would ground the **two MSBs** → each channel capped at 63/255 = **24.7 % of full scale** → dark, no true white. **Caveat:** `[7:0]` is a near-universal *convention*, not an explicit "R7 is the MSB" sentence, and this is a no-name panel on a copper-committing net — so it's strong evidence, not a guarantee.

- **Evidence:** §2 Pin Assignment (p.4): pins 5–12 `R0~R7` (full 8-bit bus). §1: 16.7 M colors / 24-bit. **§3.2.1–3.2.3 timing diagrams label the bus `DR[7:0]/DG[7:0]/DB[7:0]` (→ R7=MSB, conventional); §3.2.4 = "Parallel 24-bit RGB Input Timing."** §3.1.2 Note 2's `R0~R5` list is boilerplate (cites a nonexistent RESET) → disregard.
- **Fix:** disregard Note 2; the `DR[7:0]` bus notation + pin table point to **R7=MSB → drive R7..R2, ground R1/R0** (convention-safe default, now datasheet-supported). Because `[7:0]` is convention (not an explicit statement) on a copper net, **confirm before routing** — a one-line Zettler bit-to-pin map, or bench-test the panel on the breakout (in hand). Not "silent" — it leans clearly, just short of ironclad.
- **✅ Resolved (2026-08-23):** bench-tested on the physical panel over the T113 RGB666 DPI path (15 MHz). `fbtest bars` = RGB order, no swap; `fbtest gray` gradient = smooth/neutral/monotonic ⇒ correct bit order; solid white = full brightness ⇒ MSBs not grounded. **R7=MSB, drive R7..R2, ground R1/R0 confirmed correct on silicon → cleared to route the RGB copper.**

---

## 🟡 MEDIUM

### M1 — RESET pull-up (#13) is a VCC-RTC (1.8 V) pin, lumped with the 3.3 V SD pull-ups
**Ref:** §1 #13 · voltage-rating · CONFIRMED
RESET (ball 27, I/OD) buffer power = VCC-RTC = 1.8 V (abs-max 2.16 V). A 3.3 V pull-up over-stresses the pin and injects ~80 µA into the RTC rail (clamps to ~2.5 V). Board still boots, so silicon-proof doesn't clear it. **Fix:** pull RESET to the 1.8 V VCC-RTC net; verify the breakout's actual net (BOM lists only the resistor, not the rail).

### M2 — 3.3 V budget under-counts DDR term + ignores in-package LDOB heat
**Ref:** §1 note / §9 Rail A · thermal / value-error · CONFIRMED
VCC-DRAM is capped at **400 mA** (Table 5-3), not 300, and "no datasheet ceiling" is wrong — there is one. If VCC-DRAM is internal-LDOB-sourced off 3.3 V, LDOB dissipates up to **(3.3−1.5)×0.4 ≈ 0.72 W inside the package** on top of ~0.72 W core, against Tj 110 °C; the datasheet's ambient ratings are explicitly conditioned on *external* VCC-DRAM power. **Fix:** budget 400 mA (→ 3.3 V worst ≈ 1.1 A) + the in-package heat; if Tj is tight, add an external 1.5 V DDR buck (removes both the reflection and the heat). Rail current still fits the buck-boost — thermal/accuracy, not brownout. (The genuinely-open item is LDOB's *linear* current capability — keep §10 V23 scope.)
> **DECISION (2026-08-13, see Q13):** confirmed VCC-DRAM = **internal LDOB off 3.3 V** (t113-breakout, silicon-proven). **Keeping internal LDOB — no external buck, no DNP hedge.** Datasheet ballpark (θJA 20.36 °C/W, P_D ≈ 2.5 W worst) → Tj ≈ 58 °C typ, thin only at worst-case-in-hot-shell = accepted first-spin risk. **Diagnosable on silicon** (THS die-temp + 1.5 V LDOB-OUT/VCC-DRAM test point + memtester-under-load); if inadequate, the fix is an external buck on **Rev-B** (respin — no footprints provisioned). Budget-accuracy note (400 mA / 1.1 A) still applies to §9.

### M3 — 0.9 V core set below VDD-CORE typ + uses the FP6161 minimum output cap
**Ref:** §9 Rail B / #6 #7 #20 · value-error · CONFIRMED
Divider is correctly 0.900 V, but VDD-CORE typ is **0.95 V**; worst-case VFB gives ~0.867 V — only ~17 mV over the 0.85 V floor before IR-drop/load-step droop at 800 mA. #20 uses 4.7 µF (smallest in the FP6161 table) vs the 10 µF typ-app. **Fix:** use 10 µF on the core output; if the breakout wasn't validated at 1.2 GHz, consider ~0.93 V (82 k/150 k); keep §10 V23.

### M4 — DRV2605L logic pins exceed abs-max at low battery if VDD moved to SYS (§10 V8)
**Ref:** §3 #39 / #94 / §10 V8 · voltage-rating · CONFIRMED
With VDD on SYS (3.0–4.4 V) but EN hard-tied to 3.3 V and SDA/SCL pulled to 3.3 V, the pins hit VDD+0.3 when SYS sags to ~3.0 V under a motor pulse (abs-max = VDD+0.3). Forward-biases the ESD clamps into VDD / back-powers i2c1. **Fix (cleanest):** revert V8 for the DRV only — keep VDD on 3.3 V (local 10 µF bulk #43 buffers the pulse; 3.3 V fully drives a 1.8 Vrms LRA). The amp #45 may stay on SYS (its logic pins are +6 V-rated / sit near 0.4 V). *(See also Q: why does V8 move it at all?)*

### M5 — MAX98357A at 15 dB gain hard-clips above −7.6 dBFS on the SYS rail
**Ref:** §4 #47 / #50 · value-error · CONFIRMED · **✅ ACCEPTED + APPLIED (2026-08-13)** — BOM.md **#49** (was #47; renumbered by the §11→section reorg) now straps **GAIN_SLOT→VDD = 6 dB**, the 100 KΩ dropped. Rationale confirmed: at 6 dB full digital scale = 0.8 W = the CES-2704 rating, so max digital = max *clean* output (no clipping across the whole range), it keeps the full DAC range (vs capping softvol ~−9 dB at 15 dB), and it's hardware-intrinsic protection — even a raw `aplay` to `hw:0` at bring-up can't overdrive the speaker. Not 3 dB (would cap at ~0.4 W = lose max loudness). *(Applied in the local working tree — uncommitted as of this note.)*
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

### M12 — [✅ RESOLVED] Display/touch FPC contact-side — physically inspected: both bottom-contact, mounted on opposite faces
**Ref:** §2 #26 / #27 · pinout-strap · RESOLVED (physical inspection, 2026-08-13)
The datasheet's reference part is **Hirose FH33J-40S-0.5SH(10)**; the BOOMELE C9160 substitute's "bottom-contact CONFIRMED" comes from the BOM's read of the §7 side-view. **Firsthand re-read (2026-08-13):** the §7 mechanical **does label the FPC "Conductor" and "Stiffener" faces** (so the info *is* in the datasheet, and it reads as **bottom-contact** — conductor on the rear face, folding so contacts face the board, matching C9160) — but the rendered drawing isn't crisp enough to commit copper with 100 % confidence. A contact-side mismatch = FPC gold fingers face away = **dead panel, no rework. Fix:** since the panel is **in hand**, inspect the physical ribbon's gold-finger face (definitive) and/or **bench-test the panel on the FPC breakout**; cross-check the Hirose FH33J drawing. Confirm the display (C9160) **and** the 6P/1.0 mm touch connector (HDGC C2919568) both match — *before* routing.
> **DECISION (2026-08-13):** physically inspected the panel in hand — display 40-pin FPC copper faces **down**, touch 6-pin copper faces **up**; both chosen connectors are **bottom-contact** (C9160 flip-lid; C2919568 confirmed "Bottom Contact" on its LCSC page, along with its two HDGC siblings). **Keeping both bottom-contact, mounted on OPPOSITE PCB faces** — display (C9160) on the **top/screen-facing** face (copper-down → bottom contacts), touch (C2919568) on the **bottom/back** face (copper-up → bottom contacts). **No part change.** *(Considered "both on top" via a top-contact 6-pin swap — rejected: the HDGC 1.0mm/6P family is bottom-contact only, so it needs a new maker + new footprint + re-source; not worth it for spin 1.)* Residual (non-gating): the touch ribbon wraps to the underside — check fold/length in layout; set each footprint's orientation + pin-1 to the observed ribbon; **physical test-fit the ribbons into the connectors before locking footprints.**

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

> Items marked **✅ CLOSED** are captured here as **schematic/routing notes (no BOM change)** — decisions or wiring to apply at schematic capture, nothing to fab differently. The rest stay open because they need a bench measurement, the physical part, a vendor answer, or software bring-up.

1. **Zettler RGB bit-to-pin map + MSB order — ✅ CLOSED (datasheet + bench-test on silicon, 2026-08-23):** both confirmation paths the item asked for are done. **Datasheet:** the ATM0500D27-CT pin table is R0=LSB @ pin 5 … R7=MSB @ pin 12 → **drive R7..R2, ground R1/R0** confirmed (the `DR[7:0]` notation was right; Note 2 was boilerplate). **Bench test** (physical panel driven from the T113 over the FFC breakout, RGB666 DPI @ 15 MHz): `fbtest bars` → channel order **RGB, no swap**; `fbtest gray` gradient → **smooth, neutral, monotonic** ⇒ bit/MSB order correct; solid white at **full brightness** ⇒ MSBs are *not* grounded (rules out the inverted "ground R6/R7" failure that would cap each channel at ~25 %). **RGB666 copper mapping validated end-to-end → cleared to route the RGB traces.** See H2.
2. **FPC contact side (M12) — ✅ CLOSED (physically inspected + decision, 2026-08-13):** panel in hand → display 40-pin copper faces **down**, touch 6-pin copper faces **up**; both connectors are **bottom-contact** (C9160 flip-lid; C2919568 confirmed "Bottom Contact" on LCSC). **DECISION: both bottom-contact, mounted on OPPOSITE PCB faces** — display (C9160) on the **top/screen-facing** face, touch (C2919568) on the **bottom/back** face. **No part change.** Residual (non-gating): touch ribbon wraps to the underside (check the fold in layout); set each footprint's orientation + pin-1; **physical test-fit the ribbons before locking footprints.** See M12.
3. **FT7311 touch — ✅ CLOSED (software bring-up; no hardware/BOM/routing impact):** touch is bonded on-panel (no part choice), and the 6-pin connector + INT/RST GPIOs + pull-ups are provisioned regardless — so it **can't affect the board**. Residual is a deferrable software task (a gamepad works without touch): on the physical panel, `i2cdetect -y -r` → confirm **0x38**, read a report block while touching → confirm the standard FocalTech format (0x02 count, 0x03–0x06 X/Y), then bind **`edt,edt-ft5506`** or add a one-line **`focaltech,ft7311`** to the driver's `of_match` (own kernel). 0x38 + standard format is the overwhelmingly likely answer and is verifiable anytime, before or after fab. Tracked in §2 gate 4 / §10 V17 + the phased bring-up order.
4. **Vybronics VG0832022D — ✅ RESOLVED (2026-08-13).** Recovered from `cad/docs/Vybronics-VG0932022D-datasheet.md` (filename typo — the content is the VG0832022D spec; every page header says so). Coil **resistance = 22.5 Ω ±10%** (§4-3) → far above the DRV2605L ~4 Ω `ZL(th)` trip and the ≥8 Ω target, squarely in the 8–35 Ω LRA range. Rated **1.8 Vrms**, operating max **1.85 Vrms** (§2-1/2-2) → OD_CLAMP = 0x78 (2.546 Vpeak = 1.8 Vrms) sits safely under the 2.616 Vpeak ceiling; resonance **235 ±7 Hz** confirmed. Good match; see L8.
5. **CES-2704 voice-coil inductance** — MAX98357A filterless needs >10 µH; unspecified in its datasheet.
6. **INA226 — ✅ CLOSED (schematic/routing note, no BOM change):** VS → **3.3 V** (adequate — the T113 is the only reader; SYS only adds ~330 µA always-on). **A0→GND, A1→GND** (direct ties, no resistor) = addr **0x40**; must not float. **VBUS(pin 8) → SYS**. Shunt **#79** in series in the **SYS → (buck-boost + core-buck) load path**, **IN+/IN− Kelvin** at the shunt pads → reads **total board draw, not battery/charge current** (separate branch; MAX17048 covers battery state). Optional DNP input RC already provisioned in #78. *Functional check post-fab: known load vs INA226 reading should match.*
7. **SP3004-04XTG (#12) — ✅ CLOSED (schematic-capture check, no BOM change):** when placing the symbol, read its pinout — if self-referenced (TVS to GND, no rail pin), wire the 4 SD lines through it as-is; if it exposes a VCC/rail pin, tie that to the SD 3.3 V rail. Resolved at capture from the symbol/datasheet; no part change either way.
8. **PJ-327C-4A jack — ✅ CLOSED (schematic-capture check, no BOM change):** when placing the symbol, read tip/ring/sleeve/detect from the maker drawing → OUTL/OUTR → tip/ring, SGND → sleeve, detect → a T113 GPIO with a defined pull (internal bias-pull ok). Note the detect polarity (NC vs NO) — that sets the `SW_HEADPHONE_INSERT` sense + speaker-mute truth table in software (derived once known). No part change.
9. **DRV2605L VDD rail — ✅ CLOSED (schematic/routing note, no BOM change), per M4:** keep DRV2605L VDD on the regulated **3.3 V** rail; do **not** route it to SYS. SYS creates the M4 pin-overvoltage and buys nothing — 3.3 V fully drives the 1.8 Vrms LRA and local bulk #43 buffers the motor pulse. (Overrides §10 V8 for the DRV; the amp #45 may still sit on SYS.) Net decision only.
10. **PCA9555 A2/A1/A0 straps — ✅ CLOSED (schematic/routing note, no BOM change):** hard-strap all three to GND/VCC (direct, no resistor) to place the expander at **0x20**; none may float. Copper only — same pattern as the INA226 address straps.
11. **I2C pull-up / bus capacitance — ✅ CLOSED (single-board decision, 2026-08-13):** buttons stay on the **single mainboard** — no button daughterboard, no cable. So the shared i2c1 bus is just the 5 I2C chips + short clustered traces ≈ **40–80 pF**, far under the **236 pF** ceiling for 1.5 kΩ (rise ~100 ns ≪ 300 ns budget → ~3× margin). The 1.5 kΩ pull-up (#94) stands — **no change**. The edge-button long traces are on the expander's slow/DC GPIO side, not the fast I2C bus. **Hygiene, not risk:** keep the DNP parallel-R tuning pads + do a one-time SDA/SCL rise-time scope check at bring-up; keep the I2C trunk compact. *If a button daughterboard is ever added later: latching JST-GH 1.25 mm, short cable, pull-ups on the mainboard side, 100 kHz fallback.*
12. **ALSA softvol ceiling — ✅ CLOSED (software bring-up; hardware baseline now in place via M5):** the finding's original premise ("softvol is the *only* speaker protection") is **superseded** — #49 now straps **6 dB (M5)**, so full digital scale ≈ 0.8 W = the CES-2704 rating and a raw full-scale `aplay` **can no longer over-drive the speaker in hardware**. Softvol drops to **comfort volume + margin** for the speaker. It's still the guard that keeps the **TPA6132A2 out of clip** (lowest amp gain −6 dB is already strapped, so no hardware setting avoids clip at full DAC scale) — but that's a quality/ear-safety matter, not damage or a fab gate. **No hardware / BOM / routing action.** Residual = a routine software task at audio bring-up: apply the softvol ceiling in the *default* ALSA route (`asound.conf` / UCM / machine driver) so even a raw `aplay` passes through it — tracked in §4 + the phased bring-up order, not an open review item.
13. **VCC-DRAM source — ✅ CLOSED (electrical confirmed + decision made, 2026-08-13).** Traced the t113-breakout (silicon-proven): its BOM states *"internal LDOA/LDOB make the 1.8 V and 1.5 V (DDR3) rails from the 3.3 V LDO-IN"*, the schematic has `LDOB-OUT → VCC-DRAM0/1` nets, and there is **no external 1.5 V regulator** — so **VCC-DRAM = internal LDOB off 3.3 V**, confirmed. The 400 mA reflection onto 3.3 V + ~0.72 W in-package LDOB heat are real (see M2). **DECISION: keep internal LDOB — no external buck, no DNP hedge.** Datasheet θJA = 20.36 °C/W, worst P_D ≈ 2.5 W → Tj ≈ 58 °C typical, thin only at simultaneous worst-case in a hot sealed shell → **accepted as first-spin risk because it's fully diagnosable on silicon:** (1) T113 **THS** die-temp (enable the kernel thermal driver); (2) a **test point on the 1.5 V LDOB-OUT/VCC-DRAM net** — catches LDOB current-limit → rail sag; **confirm it lands in layout** (already in the §10 design-for-debug list); (3) **memtester under CPU load** while logging THS + scoping the 1.5 V rail. If inadequate → fix = external 1.5 V buck on **Rev-B** (no footprints provisioned → it's a respin, accepted). No further paper analysis needed.

---

## Fix-first shortlist

1. **BQ24074 EN2→OUT/SYS high, EN1→GND** (B1). One net each; without it the charger is a 100 mA device.
2. **FP6161 RUN net + ≥2 ms sequencing** (H1). RC from STM6601 EN; don't tie to SYS or 3.3 V.
3. ~~**Zettler RGB bit map** (H2)~~ — ✅ **RESOLVED on silicon 2026-08-23** (RGB channel order + R7=MSB bench-confirmed); RGB copper cleared to route.

Everything else is strap/value/note-level and can be batched into the schematic-capture pass.
