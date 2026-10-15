# gameboy-v3 PCB — Bill of Materials + Schematic/Routing Notes

Integrated T113-S3 handheld (mainline Linux) on the proven `t113-breakout` core: 5" 800×480 IPS
RGB666 panel + FT7311 touch, haptics, 6-axis IMU, I2S audio (speaker + headphone), 1S LiPo
charge/power-path, onboard Bluetooth. **Stock checked 2026-10-13 (JLC + Digi-Key): three parts need action — #25 (panel channel), #94 (JLC pre-order or consign), #99 (last-time buy).** Datasheets in `../../../cad/docs/`.

**Src:** ✅ = confirmed LCSC part # · ⚠️ = candidate. **Fit:** JLC = SMT-placed (needs LCSC #) · Hand = you attach (module/cell/speaker/LRA) · — = not assembled / deleted (fab.py skips it).

---

## 1. Core — SoC + power + clock + storage

| # | Qty | Role | Part | LCSC # | Interface / net | Src | Fit | Note |
|---|-----|------|------|--------|-----------------|-----|-----|------|
| 1 | 1 | SoC | T113-S3 | C5197687 | system | ✅ | JLC | Dual Cortex-A7, 128 MB in-package DDR3, eLQFP-128. |
| 2 | 1 | USB-C | TYPE-C-31-M-12 | C165948 | 5 V in + USB2 | ✅ | JLC | Receptacle (charge + FEL/data). **Shell + stainless mid-plate + all 4 THT retention posts → GND** (ESD discharge path + mechanical retention; direct short — plastic-shelled handheld, no separate earthed chassis). Optional DNP hedge: 4.7 nF/2 kV ∥ 1 MΩ shell-to-GND (split off only if a first-article EMI/ground-loop issue appears; default = stuff the short). |
| 3 | 1 | 0.9 V core buck | FP6161KR-LF-ADJ | C77234 | **input = +3V3** (step 2) | ✅ | JLC | Sync buck, SOT-23-5, 1 A, 0.6 V FB. **VIN and the RUN RC #9 are on +3V3** (step 2, was SYS + the STM6601 EN): RUN passes its 1.5 V threshold ~6 ms after +3V3 rises (≥2 ms after VCC-IO), RUN never exceeds VIN, and the buck is unpowered whenever +3V3 is off (true-off kept). +3V3 worst case ~1.4 A vs the TPS63021's 2 A (boost) / 4 A (buck). FB set by #6/#7; CIN = C33, COUT = C34 10 µF (#20) at L1; core bulk C39 10 µF (#20) on F under U1 at core ball 81; FB sensed locally at C34. |
| 4 | 1 | Core-buck inductor | SMNR4020-2.2UH | C135262 | FP6161 (#3) | ✅ | JLC | 2.2 µH 3.4 A shielded. |
| 5 | 1 | Amp mono strap R | RS-03K6803FT | C140074 | MAX98357A SD/MODE (#48) | ✅ | JLC | 680 K 1% 0603. **[AUDIT] DNP — duplicate of #48; populate only ONE (both = 340 k mis-selects SD_MODE).** |
| 6 | 1 | Core FB R_bot | 0603WAF1503T5E | C22807 | FP6161 FB | ✅ | JLC | 150 K 1%. With #7 (75 K) → 0.6·(1+75/150) = 0.9 V. |
| 7 | 1 | Core FB R_top | 0603WAF7502T5E | C23242 | FP6161 FB | ✅ | JLC | 75 K 1%. |
| 8 | 1 | Core FB feed-forward | CL05C100JB5NNNC | C32949 | across #7 | ✅ | JLC | 10 pF C0G 0402 — REQUIRED for loop stability. |
| 9 | 2 | Core-buck RUN RC | 100 kΩ 0603 (C14675) + 100 nF 0603 (C14663) | C14675 / C14663 | +3V3 → R → RUN(pin1); C: RUN→GND | ✅ | JLC | Delays VDD-CORE ~6 ms after +3V3 (τ 10 ms to the 1.5 V RUN threshold; ≥2 ms after VCC-IO). Step 2: fed from +3V3 instead of the STM6601 EN. |
| 10 | 1 | DDR ZQ | 0603WAF2400T5E | C23350 | T113 pin 47 → GND | ✅ | JLC | 240 Ω 1% (JEDEC ZQ-cal nominal). |
| 11 | 1 | SPI-NOR | W25Q128JVSIQ | C97521 | SPI0 boot | ✅ | JLC | 128 Mbit, SOIC-8, QE=1. |
| 12 | 1 | microSD socket | A-MicroTF-1.85A | C22467599 | SDC0 | ✅ | JLC | Push-push. Card-detect (pin 9) is N.O. to the shell (GND) and closes on insert → PG10 (step 3; was PF6) + R48 10 kΩ pull-up (#124): active-low. |
| 13 | 2 | microSD ESD array | Littelfuse SP3004-04XTG ×2 | C207280 | SDC0 CLK/CMD/DAT0-3 | ✅ | JLC | 2× 4-ch → the 6 SD lines + SD_CD (step 3 remap: D1.1 CLK, D1.3 D0, D1.4 D1, D1.6 SD_CD; D2.1 D2, D2.3 D3, D2.6 CMD; D2.4 spare). 0.85 pF ultra-low-cap, SOT-563, unidirectional rail-clamp. |
| 14 | 6 | SD/RESET pull-ups | RC0603JR-0710KL | C99198 | SD CMD+DAT0-3 → 3.3 V | ✅ | JLC | 10 KΩ. **⚠️ RESET pull-up goes to the 1.8 V VCC-RTC rail, NOT 3.3 V** (RESET buffer = VCC-RTC, abs-max 2.16 V). SD-line pulls stay on 3.3 V. **[AUDIT] netlist ties VCC-RTC to the 1.8 V LDOA rail — verify it's the same copper vs the breakout.** |
| 15 | 1 | HOSC crystal | X322524MRB4SI | C70571 | 24 MHz | ✅ | JLC | SMD3225-4P, CL 18 pF (loads #17). |
| 16 | 1 | RTC crystal | SC-20S 32.768kHz | C97607 | RTC | ✅ | JLC | SMD2012-2P (loads #18). |
| 17 | 2 | 24 MHz load caps | 0402CG220J500NT | C1555 | HOSC | ✅ | JLC | 22 pF C0G 0402. |
| 18 | 2 | RTC load caps | 0402CG180J500NT | C1549 | RTC xtal | ✅ | JLC | 18 pF C0G 0402. |
| 19 | ~25 | Decoupling | CC0603KRX7R9BB104 | C14663 | per-power-pin | ✅ | JLC | 100 nF 0603 X7R: per-ball (+3V3: C8–C10/C12/C29/C30; LDOA: C23–C25 + C13 at VCC-TVIN 107) + flash (C7 at U3.8) + SD (C11 at U4.4). |
| 20 | 8 | Bulk | CL21A106KOQNNNE | C1713 | bulk | ✅ | JLC | 10 µF 16 V 0805. SYS bulk is the Rev-A brownout fix; **none on VBUS**: hard-started VBUS stays ≤ 10 µF (C55 only; USB 2.0 §7.2.4.1; C32 deleted 2026-10-13). One = FP6161 CIN at U2.4 VIN (C33); one = FP6161 COUT at L1 (C34); one = core bulk at the SoC (C39); one = LDOB-OUT bulk at U1.30 (C35); one = LDO-IN/VCC-PE bulk (C36); one = microSD VDD bulk at U4.4 (C40). |
| 21 | ~4 | Bulk | CC0603KRX5R6BB475 | C109456 | general bulk | ✅ | JLC | 4.7 µF 0603 X5R. One = LDOA-OUT bulk at U1.28 (C42). |
| 22 | 1 | RESET button | TS-1187A-B-A-B | C318884 | RESET | ✅ | JLC | 6×6 mm tactile. |
| 23 | 1 | Power LED | 19-213SYGC/S454/5T | C2986057 | status | ✅ | JLC | Green 0603. 2026-10-13: C2986027 (S530-E2) ran out at JLC → S454 bin, same EasyEDA footprint and pin map. |
| 24 | 1 | Power-LED resistor | 0603WAF5100T5E | C23193 | LED limit | ✅ | JLC | 510 Ω. |

**§1 notes:** DDR3 is on a separate **1.5 V VCC-DRAM** rail (balls 48/49), sourced by the internal **LDOB off 3.3 V** (firmware sets LDOB=1.5 V). Include the SYS input bulk caps (#20); VBUS keeps only C55 4.7 µF + C93 100 nF (USB: ≤ 10 µF).

---

## 2. Screen — Zettler ATM0500D27-CT (5" 800×480 IPS, RGB666 via TCON-LCD0, 22 pins on the PD bank)

`sun4i_rgb` + `panel-dpi` + `edt-ft5x06` touch. RGB uses PD10-12 → **SPI1 is unavailable.**

| # | Qty | Role | Part | LCSC # | Interface / net | Src | Fit | Note |
|---|-----|------|------|--------|-----------------|-----|-----|------|
| 25 | 1 | 5" IPS panel (+ FT7311 touch on-panel) | Zettler ATM0500D27-CT | DigiKey (module) | RGB666 PD0–21; touch on FFC #27 | ✅ | Hand | 24-bit RGB, IPS, logic 2.7–3.6 V. **RGB bit-map bench-verified 2026-08-23 (BOM-REVIEW H2): drive the 6 MSBs R7..R2, ground R1/R0; RGB order, no swap.** Touch = FocalTech **FT7311, I2C 0x38**; bind `edt,edt-ft5506` (or add `focaltech,ft7311`) — confirm on panel. **Sourcing (2026-10-13): Digi-Key sells it only as non-stock, in multiples of 510 ($66, 22 weeks) — find another channel for 10.** |
| 26 | 1 | Display FPC connector | BOOMELE 0.5-40PFGPZ | C9160 | 40P 0.5 mm ZIF, right-angle SMD | ✅ | JLC | **Bottom-contact → mount on the TOP/screen-facing face** (ribbon copper-down). **Pin 1 = LEFT end seen from the front (confirmed 2026-10-13); the EasyEDA footprint numbered its pads right-to-left and was renumbered to the connector's own circuit order.** Test-fit before locking. |
| 27 | 1 | Touch FFC connector | HDGC 1.0K-1.5-6PWB | C2919568 | 6P 1.0 mm (1=VDD 2=GND 3=SCL 4=SDA 5=INT 6=RST) | ✅ | JLC | Bottom-contact, latching lid. **⚠️ Opposite orientation from #26 → mount on the BOTTOM/back face** (touch ribbon copper-up, wraps around the board edge). Check fold/length. |
| 28 | 1 | Backlight WLED boost | TI TPS61165DBVR | C58756 | WLED CC boost + CTRL | ✅ | JLC | SOT-23-6. **VIN from SYS/5 V, NOT 3.3 V.** 40 mA via #33. **CTRL on PE10 = hardware PWM4** (step 3; was PE13/PWM5) (the T113 has PWM0–7; the mainline D1/T113 PWM driver is still out of tree, v13), with EasyScale or on/off on the same pin meanwhile. PWM at 6.5–100 kHz (DS p.14); EasyScale: never step up from 0 mV (DS p.20). R51 100 kΩ pull-down (#127) holds CTRL low while PE10 is Hi-Z. Open-LED OVP is internal (no OVP pin). |
| 29 | 1 | Boost inductor | SMNR4020-10UH | C135263 | boost | ✅ | JLC | 10 µH 1.6 A shielded. |
| 30 | 1 | Boost diode | MDD SS16 | C2481 | boost | ✅ | JLC | 60 V 1 A Schottky, SMA(DO-214AC). |
| 31 | 1 | Boost output cap (Cout) | YAGEO CC0805KKX7R9BB225 | C125847 | boost | ✅ | JLC | 2.2 µF 50 V X7R 0805 (≥1 µF after 19 V derate; ≥50 V clears OVP). |
| 32 | 1 | Boost input cap (Cin) | CC0603KRX5R6BB475 | C109456 | at VIN (SYS) | ✅ | JLC | 4.7 µF 0603 (reuse #21). |
| 33 | 1 | Boost FB sense R (Rset) | YAGEO RC0402FR-075R1L | C137955 | boost FB | ✅ | JLC | Sets 40 mA (VREF 200 mV / 5.1 Ω). Brightness knob; never exceed 40 mA. **Kelvin-route its ground to the TPS61165 GND pin.** |
| 34 | 1 | Boost COMP cap | YAGEO CC0402KRX7R7BB224 | C326590 | COMP (SOT-23 pin 5) → GND | ✅ | JLC | 220 nF 16 V X7R — **mandatory loop-comp; omit → unstable.** Place close to pin 5. |

**§2 notes:**
- **RGB666 bit-map (bench-verified 2026-08-23, BOM-REVIEW H2):** the panel is genuine 24-bit. Drive the 6 MSBs **R7..R2 / G7..G2 / B7..B2** and tie the 2 LSBs (R1/R0, G1/G0, B1/B0) to GND; the bench test showed RGB order (no swap), a monotonic gray ramp and full-brightness white. (Grounding the wrong pair, the MSBs, would cap each channel at ~25 % → dark, no white.)
- **Timing:** DCLK **25 MHz** (§3.2.4; 27 MHz max — pll-video0 ÷12 = 25.00). **SYNC-DE mode; HSYNC/VSYNC active-low, DE active-high.** Do NOT use a 33.3 MHz template.
- **DISP (FPC pin 31):** display-on input, no internal pull → **10 KΩ pull-up + a spare GPIO (PD22)**; floats dark otherwise.
- Wire onto `lcd_rgb666_pins` (PD0–21) + `tcon_lcd0_out` RGB endpoint; enable `&de`.

---

## 3. Motion (IMU) + Haptics

| # | Qty | Role | Part | LCSC # | Interface / net | Src | Fit | Note |
|---|-----|------|------|--------|-----------------|-----|-----|------|
| 35 | 1 | IMU (6-axis) | LSM6DSOXTR | C481766 | **i2c2** 0x6A + INT1/INT2 | ✅ | JLC | `st,lsm6dsox`. CS high = I2C (#38); SA0→GND = 0x6A (#39). **⚠️ INT1 must NOT be pulled to VDD_IO** (selects I3C-only = dead IMU) → 10 KΩ pulldown #40 + pinmux input-no-pull through boot. **Unused aux pins (Mode-1, DS Table 1 / Fig 24): SDx (pin 2) → GND, SCx (pin 3) → GND** — mandatory hard tie (must not float; no pull-R). **OCS_Aux (pin 10) = intentional NC** (do NOT tie — could perturb the aux path); **SDO_Aux (pin 11) = NC** (or VDDIO). INT2 (pin 9) → PE0 GPIO (programmable output, no tie; step 3, was PE1; INT1 is now on PE1). |
| 36 | 1 | IMU VDD decoupling | CC0603KRX7R9BB104 | C14663 | VDD (pin 8) | ✅ | JLC | 100 nF (datasheet-required). |
| 37 | 1 | IMU VDDIO decoupling | CC0603KRX7R9BB104 | C14663 | VDDIO (pin 5) | ✅ | JLC | 100 nF (datasheet-required). |
| 38 | 1 | IMU CS tie | 0 Ω / short | — (net) | CS (pin 12) → VDD_IO | ✅ | JLC | **CS high selects I2C** — mandatory. |
| 39 | 1 | IMU SA0 strap | net | — (net) | SDO/SA0 (pin 1) → GND | ✅ | JLC | Sets 0x6A (must not float). |
| 40 | 1 | IMU INT1 pulldown | RC0603JR-0710KL | C99198 | INT1 (pin 4) → GND | ✅ | JLC | 10 KΩ — guarantees I2C (not I3C) at POR. |
| 41 | 1 | Haptic driver | DRV2605LDGSR | C527464 | **i2c1** 0x5A, EN→3V3 | ✅ | JLC | `ti,drv2605l`. **VDD on 3.3 V (NOT SYS)** — logic pins would exceed abs-max if SYS sags. Motor → OUT± direct. **VDD/NC (pin 6) → tie to the 3.3 V VDD net** (same as pin 10; DS: optional 2nd supply, "tie to VDD or float" — tie shares the H-bridge supply, #43/#45 caps serve both). |
| 42 | 1 | Haptic REG cap | CL10A105KB8NNNC | C15849 | REG → GND | ✅ | JLC | 1 µF — mandatory for internal-LDO stability; place close. |
| 43 | 1 | Haptic VDD bypass | CC0603KRX7R9BB104 | C14663 | VDD | ✅ | JLC | 100 nF HF bypass. |
| 44 | 1 | Haptic IN/TRIG tie | 0 Ω / net | — (net) | IN/TRIG → GND | ✅ | JLC | Required for I2C-only internal-trigger mode. |
| 45 | 1 | Haptic VDD bulk | CL21A106KOQNNNE | C1713 | VDD | ✅ | JLC | 10 µF — sources motor/overdrive pulse locally. |
| 46 | 1 | Actuator (LRA) | Vybronics VG0832022D | DigiKey 9974288 | DRV OUT± | ✅ | Hand | 8 mm coin LRA, 235 Hz, 1.8 VAC. **Leads solder direct to OUT± pads (no connector).** Record 235 Hz in DRV config. |

---

## 4. Sound — I2S Class-D amp + speaker (on-chip codec has no mainline driver)

| # | Qty | Role | Part | LCSC # | Interface / net | Src | Fit | Note |
|---|-----|------|------|--------|-----------------|-----|-----|------|
| 47 | 1 | Class-D amp | MAX98357AETE+T | C910544 | I2S1 (BCLK/LRCLK/DIN) | ✅ | JLC | `maxim,max98357a`; no MCLK. **VDD on SYS.** EP → GND pour + thermal vias (audio GND return). Output kept filterless. |
| 48 | 1 | Amp SD/MODE series R | RS-03K6803FT | C140074 | PE6 (SPK_EN) → SD/MODE | ✅ | JLC | 680 KΩ in series from PE6, MAX98357A DS Fig 5: PE6 high (3.3 V) → SD_MODE 0.38–0.47 V = (L/2+R/2) mono; PE6 low or Z (reset) → internal 100 k pull-down = shutdown, so the amp **boots off**. **Drive PE6 push-pull** (2026-10-13; was SD/MODE → 3.3 V with an open-drain mute GPIO, which ran the amp from reset; moved off PE13 so the backlight gets PWM5). **[AUDIT] Populate THIS one; DNP #5 (duplicate 680 K).** |
| 49 | 1 | Amp GAIN_SLOT strap | net (no part) | — (net) | GAIN_SLOT → VDD | ✅ | JLC | **VDD tie = 6 dB** (0 dBFS ≈ 0.8 W into 8 Ω, no clip). NOT 15 dB. |
| 50 | 1 | Amp VDD bypass | CC0603KRX7R9BB104 | C14663 | VDD | ✅ | JLC | 100 nF. |
| 51 | 1 | Amp VDD bulk | CL21A106KPFNNNE | C17024 | VDD | ✅ | JLC | 10 µF. |
| 52 | 1 | Speaker | Same Sky CES-2704-088L050 | DigiKey 10821313 | amp OUT± | ✅ | Hand | Ø27 mm, 8 Ω, 0.8 W, enclosed. **Leads solder direct to OUT± (no connector). Cap ALSA softvol to protect (0.8 W).** |

---

## 5. Battery — charge + power-path + fuel gauge (1S LiPo)

Topology: battery ↔ **BQ24073 power-path → SYS (~3.0–4.4 V)** → **TPS63021 buck-boost → 3.3 V** (sole 3.3 V source) + **FP6161 → 0.9 V core** (#3). USB-C 5 V (Type-C sink, 2× 5.1 kΩ Rd #128; 5 V only) charges/supplements; does not power rails directly.

| # | Qty | Role | Part | LCSC # | Interface / net | Src | Fit | Note |
|---|-----|------|------|--------|-----------------|-----|-----|------|
| 53 | 1 | Power-path charger | BQ24073RGTR | C15220 | autonomous; /CHG,/PGOOD open-drain | ✅ | JLC | QFN-16-EP. **2026-10-13: BQ24074 (C54313) is out of stock → BQ24073** (bq24074.pdf p.6: same footprint and VOUT(REG) 4.4 V; OVP 6.6 V vs 10.5 V, fine on 5 V VBUS; gameboy-v2 made the same swap). **Pin 15 is TD on the '073 (ITERM on the '074): TD → GND = termination on, at the fixed 10 % (p.9: do not leave TD unconnected).** System on **OUT = SYS** (OUT regulates ~4.4 V w/o cell). **CE→GND** (must not float). **🔴 EN1→GND, EN2→HIGH (strap to OUT via #60, NOT IN — EN abs-max 7 V) — else charger boots at 100 mA and ILIM is inert.** ISET #57, ILIM #58, TS #59, TMR #123. **EP → VSS with a via array** (heat path). |
| 54 | 1 | Charger IN cap | CL10A475KO8NNNC | C19666 | IN (pin 13) → VSS | ✅ | JLC | 4.7 µF **16 V** X5R (must stay <10 µF for USB-IF inrush; 16 V for OVP window). Place local. |
| 55 | 1 | Charger OUT cap | CL21A106KOQNNNE | C1713 | OUT (pins 10/11) → VSS | ✅ | JLC | 10 µF. **OUT = SYS node** → place close to IC. |
| 56 | 1 | Charger BAT cap | CL21A106KOQNNNE | C1713 | BAT (pins 2/3) → VSS | ✅ | JLC | 10 µF, near the JST-PH (#74). |
| 57 | 1 | ISET (charge current) | 0603WAF2001T5E | C22975 | ISET (pin 16) → VSS | ✅ | JLC | 2.0 kΩ 1% → ~445 mA (0.22C). **Mandatory — open = no charge.** |
| 58 | 1 | ILIM (input limit) | 0603WAF3601T5E | C22980 | ILIM (pin 12) → VSS | ✅ | JLC | 3.6 kΩ → the **safe** input limit 366/424/483 mA (KILIM 1330/1525/1720 AΩ, DS p.12), always in circuit. R54 1.8 kΩ (#132) switched in parallel by Q1 (#133) gives the **fast** 1.2 kΩ → 1.24/1.34/1.45 A (step 1c; was a fixed 1.2 kΩ). **Never below 1.1 kΩ** (IN abs-max 1.6 A). Active only when EN2=1/EN1=0 (#60). |
| 59 | 1 | TS battery-temperature NTC | KNTC0603/10KF3950 | C2892547 | TS (pin 1) → VSS | ✅ | JLC | 10 kΩ B3950 NTC (as gb2): pauses charging outside ≈3–47 °C (worst case 1–5 / 43–52 °C; 75 µA TS bias, VCOLD 2.1 V / VHOT 0.3 V, bq24074.pdf p.13); fail-safe (open = cold, short = hot). Back side at the battery-cutout edge, ≥ 20 mm from U10 (the cell sits in a through-cutout). 2026-10-13: was a fixed 10 kΩ, which never paused. |
| 60 | 1 | EN2 strap | 0 Ω 0603 | C21189 | EN2 (pin 5) → OUT/SYS; EN1 (pin 6) → GND | ✅ | JLC | Puts charger in ILIM-resistor mode. EN1→GND is a net. |
| 61 | 0 | ~~USB-PD sink~~ | ~~CH224K~~ | ~~C970725~~ | ~~CC1/CC2; CFG1 → 5 V~~ | — | — | **Deleted 2026-10-13:** the CH224K is discontinued (0 at JLC and LCSC), and the BQ24073 cuts off above 6.6 V, so PD above 5 V is unusable anyway → a 5 V USB-C sink with 2× 5.1 kΩ Rd (#128), as gb2 and the breakout. Was: an ESSOP-10 PD sink on CC1/CC2 requesting 5 V. |
| 62 | 0 | ~~CH224K VDD series R~~ | ~~0603WAF1001T5E~~ | ~~C21190~~ | ~~5 V VBUS → VDD (pin 1)~~ | — | — | **Deleted 2026-10-13** with #61 (was R19). Was: 1 kΩ (feeds the internal shunt-reg VDD). |
| 63 | 0 | ~~CH224K VDD cap~~ | ~~CL10A105KB8NNNC~~ | ~~C15849~~ | ~~VDD (pin 1) → GND~~ | — | — | **Deleted 2026-10-13** with #61 (was C58). Was: 1 µF. |
| 64 | 0 | ~~CH224K CFG1 strap~~ | ~~RC0603JR-0710KL~~ | ~~C99198~~ | ~~CFG1 (pin 9) → VBUS~~ | — | — | **Deleted 2026-10-13** with #61 (was R20). Was: 10 kΩ → 5 V request. |
| 65 | 0 | ~~CH224K VBUS-sense R~~ | ~~RC0603JR-0710KL~~ | ~~C99198~~ | ~~VBUS (pin 8) → 5 V rail~~ | — | — | **Deleted 2026-10-13** with #61 (was R21). Was: 10 kΩ (hi-Z voltage sense). |
| 66 | 1 | 3.3 V buck-boost | TPS63021DSJR | C202140 | SYS → 3.3 V logic | ✅ | JLC | VSON-14-EP. Sole 3.3 V source. **EN ← STM6601 (#94, must not float); PS/SYNC→GND (#68); FB→VOUT (#67); VINA cap #69.** **L/C stability-critical — use the datasheet Table-1 combo (#71/#72).** EP → PGND pour + thermal vias. |
| 67 | 1 | Buck-boost FB tie | 0603WAF0000T5E | C21189 | FB (pin 3) → VOUT | ✅ | JLC | 0 Ω — fixed-version FB senses rail externally; float = no regulation. |
| 68 | 1 | Buck-boost PS/SYNC strap | 0603WAF0000T5E | C21189 | PS/SYNC (pin 13) → GND | ✅ | JLC | 0 Ω = power-save on (must not float). Placed 0 Ω keeps force-PWM option. |
| 69 | 1 | Buck-boost VINA bypass | CC0603KRX7R9BB104 | C14663 | VINA (pin 1) → GND | ✅ | JLC | 100 nF. **Hard upper limit 0.22 µF — no bulk cap here.** |
| 70 | 2 | Buck-boost input caps | CL21A106KOQNNNE | C1713 | VIN (pins 10/11) → PGND | ✅ | JLC | 2×10 µF 16 V, local to the converter pins. |
| 71 | 3 | Buck-boost output caps | CL21A226MAQNNNE | C45783 | VOUT (pins 4/5) → PGND | ✅ | JLC | 3×22 µF 25 V — stability-critical (Table-1). Min 2×22 µF. Place close. |
| 72 | 1 | Buck-boost inductor | MWSA0503S-1R5MT | C408407 | L1/L2 | ✅ | JLC | 1.5 µH, Isat 9 A (≥4.5 A floor). 5.4×5.2 mm — flag size for layout. |
| 73 | 1 | LiPo cell | **PKCELL LP-803860** (2000 mAh, 1S), bought as **Adafruit 2011** (the same cell + datasheet, pre-terminated with a genuine JST-PH plug) | — (PKCELL, datasheet on file) | JST-PH 2-pin | ✅ | Hand | 3.7 V, integral PCM (R5402N101KD + SME8205). **Form factor datasheet-verified: max 62 × 38.5 × 8.5 mm** (L×W×T); leads 24 AWG ~100 mm exit a short (38.5 mm) end → place CN1 there. **PCB anchored to this footprint** — enclosure pocket ~64 × 40 × 10 mm (incl. ~10 % swell). Current: 1C = 2 A continuous ≈ the ~1.8 A worst-case peak — **OK**: the TPS63021 buck-boost holds 3.3 V through SYS sag and the PCM OCP trips well above 2 A (typical play ~1 A). A 2C `803860` is optional extra margin (same size → no board change). ⚠️ On arrival confirm PCM OCP >2 A + polarity red=+→BAT. |
| 74 | 1 | Battery connector | S2B-PH-SM4-TB(LF)(SN) | C295747 | mates #73 | ✅ | JLC | JST-PH 2 mm, right-angle SMD. **⚠️ POLARITY: red = + → BQ24073 BAT (reversed = destroyed cell).** Pad 1 = JST Circuit No.1 = BAT (checked 2026-10-13 against JST C295747 p.1/p.3 and Adafruit's JSTPH2: not mirrored); '+' / '−' silk at pads 1 / 2: **to do in step 2** (refresh the footprint from the library for the '+', add a '−'; the board copy has only a 0.06 mm dot). **Meter the cell's plug before the first plug-in**; if reversed, swap the crimp contacts. |
| 75 | 0 | ~~Fuel divider R_top~~ | ~~0603WAF1803T5E~~ | ~~C22827~~ | ~~BAT → GPADC0~~ | — | — | **Deleted 2026-10-13** (was R24): the MAX17048 (#77) already reports VCELL + SOC over I²C; GPADC0 (U1.101) is NC. |
| 76 | 0 | ~~Fuel divider R_bot~~ | ~~0603WAF1003T5E~~ | ~~C25803~~ | ~~GPADC0 → GND~~ | — | — | **Deleted 2026-10-13** (was R25), with #75. |
| 77 | 1 | Fuel gauge (SoC %) | MAX17048G+T10 | C2682616 | i2c1 0x36 | ✅ | JLC | `maxim,max17048`, DFN-8-EP. **VDD (pin 3) → battery+ (power + cell-sense); CELL (pin 2) = NO-CONNECT; QSTRT (6) → GND; CTG (1) → GND; ALRT (5) → PB5 + R43 10 k pull-up (#119).** EP → GND. |
| 78 | 1 | MAX17048 VDD bypass | CL10A105KB8NNNC | C15849 | VDD (pin 3) → GND | ✅ | JLC | 1 µF at pin 3 (not CELL). |
| 79 | 1 | Charge-status LED | 19-213SYGC/S454/5T | C2986057 | /CHG (open-drain) → LED → R26 → SYS_SRC (charger OUT) | ✅ | JLC | Lit during the first charge cycle, also with the system off (dark after termination and during top-ups); 2 Hz blink = safety-timer fault; stays lit during a TS pause (0 GPIO). 2026-10-13: fed from SYS_SRC instead of +3V3 (off while the handheld is off); part → S454 bin (C2986027 ran out). |
| 80 | 1 | Charge-LED resistor | 0603WAF1001T5E | C21190 | LED limit | ✅ | JLC | 1 kΩ from SYS_SRC (4.4 V) ≈ 2.5 mA (TI typ app p.1: 1 kΩ to OUT); was 510 Ω from +3V3. |
| 81 | 1 | Power/current monitor | TI INA226AIDGSR | C49851 | i2c1 0x40 + shunt #82 | ✅ | JLC | `ina2xx` hwmon (SYS-rail V/I/P telemetry). **Wire VBUS (pin 8) → SYS rail.** A0/A1 → GND. Optional DNP input filter (2× ≤10 Ω series + 0.1–1 µF). |
| 82 | 1 | INA226 shunt | RLP25FEGMR010 | C393072 | series in SYS path → IN+/IN− | ✅ | JLC | 10 mΩ 1% 3 W 2512, ±50 ppm. **Kelvin/4-terminal.** |
| 83 | 1 | INA226 VS decoupling | CC0603KRX7R9BB104 | C14663 | VS → GND | ✅ | JLC | 100 nF. |
| 84 | 1 | /PGOOD pull-up | RC0603JR-0710KL | C99198 | /PGOOD → 3V3 + GPIO | ✅ | JLC | 10 kΩ (charger-state telemetry). |

---

## 6. Controls — buttons via PCA9555 I2C expander + `gpio-keys`

| # | Qty | Role | Part | LCSC # | Interface / net | Src | Fit | Note |
|---|-----|------|------|--------|-----------------|-----|-----|------|
| 85 | 1 | GPIO expander | PCA9555PWR | C2864778 | i2c1 0x20 + INT | ✅ | JLC | `nxp,pca9555` IRQ gpiochip. **A2/A1/A0 hard-strapped (base 0x20, must not float).** All 16 pins = inputs at POR, internal ~100 kΩ pull-ups → **no per-button pull-ups.** INT open-drain → pull-up #89. Firmware: park cmd pointer ≠00h after reads (INT erratum) + `gpio-keys` poll safety net. |
| 86 | 4 | D-pad tactiles | TS-1187A-B-A-B | C318884 | 4× expander pin → GND | ✅ | JLC | 5.1×5.1 mm, 1.6 N soft, top-actuated. Rocker keycap over the 4 for D-pad roll. |
| 87 | 4 | A/B/X/Y tactiles | TS-1187A-B-A-B | C318884 | 4× expander pin → GND | ✅ | JLC | Same part as #86. |
| 88 | 2 | Start/Select tactiles | TS-1187A-B-A-B | C318884 | 2× expander pin → GND | ✅ | JLC | Same part as #86. |
| 89 | 1 | Expander INT pull-up | RC0603JR-0710KL | C99198 | INT → 3V3 | ✅ | JLC | 10 kΩ. |
| 90 | 1 | PCA9555 VDD decoupling | CC0603KRX7R9BB104 | C14663 | VDD → GND | ✅ | JLC | 100 nF. |
| 91 | 2 | Volume ± tactiles | ALPS SKRTLBE010 | C127481 | 2× expander pin → GND | ✅ | JLC | Side-actuated SMD (4.5×3.4 mm, 1.6 N), footprint `KEY-SMD_SKRTLAE010-1`. `KEY_VOLUMEUP/DOWN`. Shared part w/ #92/#93 (qty 5). |
| 92 | 1 | Soft power button | ALPS SKRTLBE010 | C127481 | STM6601 PB (#94) **AND** native SoC EINT | ✅ | JLC | `KEY_POWER`. **EINT MUST be a native SoC pin, not the expander** (must wake/power-on from off). **[AUDIT] Route the SoC EINT from STM6601 /PB_OUT via #117, not the raw PB node (PB idles ~SYS 4.4 V).** |
| 93 | 2 | L/R bumper tactiles | ALPS SKRTLBE010 | C127481 | 2× expander pin → GND | ✅ | JLC | `KEY_L1/KEY_R1`. 100k-cycle — upgrade to 500k/1M (ALPS SKHHLNA010/SKHHLQA010) in a later rev if they wear. |
| 94 | 1 | Push-button power controller | STM6601CA2BDM6F | C109022 | PB←#92; EN(pin9)→TPS63021 EN; PSHOLD(pin4)↔GPIO | ✅ | JLC | TDFN-12. **`C`** = active-high EN, long-push deasserts (true off). **VCC → always-on SYS** (NOT the switched 3.3 V). PSHOLD = `gpio-poweroff` handshake. Caps #95/#96. EN push-pull (no pull-up); RST/INT/VCCLO/SR/CSRD leave open; **PBOUT → 3.3 V pull-up (#117) → T113 PE13** (step 3; was PE10) [AUDIT: was 'leave open'; levels the button EINT off the ~SYS PB node]. **Stock (2026-10-13): JLC has 49 (36 pre-orderable) — pre-order ~12 into the JLC parts library, or buy at Digi-Key (9,628) and consign.** |
| 95 | 1 | STM6601 VCC decoupling | CC0603KRX7R9BB104 | C14663 | VCC (pin 1) → GND | ✅ | JLC | 100 nF, close to device. |
| 96 | 1 | STM6601 VREF cap (CREF) | CL10A105KB8NNNC | C15849 | VREF (pin 3) → GND | ✅ | JLC | 1 µF — mandatory even though VREF unused. Place close. |

**§6 note — expander budget:** 10 gamepad + 2 volume + 2 bumpers = 14/16 pins (power button is a native EINT, not on the expander). 2 spare. All-tactile → HASL fine (no ENIG). Run i2c1 ≥400 kHz, stay interrupt-driven (never poll).

---

## 7. Shared bus / cross-cutting

| # | Qty | Role | Part | LCSC # | Interface / net | Src | Fit | Note |
|---|-----|------|------|--------|-----------------|-----|-----|------|
| 97 | 2 | I2C pull-ups (SDA/SCL) | 0603WAF1501T5E | C22843 | i2c1 SDA/SCL → 3V3 | ✅ | JLC | **1.5 KΩ** (10 K/4.7 K fail the 400 kHz rise-time). Add DNP parallel-R pads to tune; floor ~1 kΩ. Measure bus C on first article if the button board cable is long. |
| 98 | 1 | USB2 D+/D− ESD array | USBLC6-2SC6 | C7519 | USB-C D+/D− + VBUS clamp | ✅ | JLC | SOT-23-6, ~3.5 pF. VBUS pin (5) decoupled by C93 100 nF (#130, ST Fig 18). Optional 5 V VBUS TVS (SMAJ5.0A) as DNP. |

---

## 8. Audio-out + Bluetooth (Tier-2)

| # | Qty | Role | Part | LCSC # | Interface / net | Src | Fit | Note |
|---|-----|------|------|--------|-----------------|-----|-----|------|
| 99 | 1 | Bluetooth module | Ezurio BT830-SA-01 | DigiKey (not JLC) | **UART3 HCI** + VREG_EN_RST# + 3.3 V | ✅ | Hand | CSR8811, integrated antenna, BlueZ (`btattach`). **Fit=Hand — no JLC footprint → draw a custom symbol+footprint** (castellated, pads extend outward). Antenna end overhangs board edge, keep-out ≥20 mm from metal/battery/LCD. See integration note. **End of life: Ezurio last-time-buy notice 4 Sep 2026 (factory LTB by 3 Dec 2026); Digi-Key had 536 at $18.81 on 2026-10-13 — buy gb3 + gb2 + spares now.** |
| 100 | 1 | BT830 VREG_OUT_HV cap | CC0603KRX5R6BB475 | C109456 | pin 10 → GND | ✅ | JLC | 4.7 µF low-ESR — **mandatory ≥1.5 µF** (internal HV-LDO stability), even though 1.8 V unused. |
| 101 | 2 | BT830 supply decoupling | CC0603KRX7R9BB104 | C14663 | VREG_IN_HV (9) + VDD_PADS (1) → GND | ✅ | JLC | 100 nF ×2. |
| 102 | 1 | BT830 3.3 V bulk | CC0603KRX5R6BB475 | C109456 | near pin 9 → GND | ✅ | JLC | 4.7 µF RF-burst reservoir. |
| 103 | 1 | Stereo I2S DAC (HP out) | TI PCM5102APWR | C107671 | **I2S1 (shared w/ #47)** + 4 straps | ✅ | JLC | `ti,pcm5102a`, TSSOP-20. Line-level → HP amp #110. Config straps + PLL constraint: see integration note. |
| 104 | 3 | I2S1 series-R | 0603WAF0000T5E | C21189 | I2S1 BCLK/LRCK/DOUT (driver end) | ✅ | JLC | 0 Ω footprint (multi-drop). Stuff 0 Ω; →33 Ω if edges ring. |
| 105 | 4 | PCM5102A supply decoupling | CC0603KRX7R9BB104 | C14663 | AVDD/CPVDD/DVDD/LDOO → GND | ✅ | JLC | 100 nF ×4 (LDOO cap mandatory; DVDD strapped 3.3 V; do NOT drive LDOO). |
| 106 | 3 | PCM5102A supply bulk | CL21A106KOQNNNE | C1713 | AVDD/CPVDD/DVDD → GND | ✅ | JLC | 10 µF ×3. |
| 107 | 2 | PCM5102A charge-pump caps | CC0805KKX7R9BB225 | C125847 | CAPP(2)↔CAPM(4) + VNEG(5)→GND | ✅ | JLC | 2.2 µF ×2 — mandatory (flying cap + VNEG reservoir; else no −3.3 V rail → dead output). |
| 108 | 2 | PCM5102A anti-imaging R | UNI-ROYAL 0603WAF4700T5E | C23179 | OUTL(6)/OUTR(7) series → #111 | ✅ | JLC | 470 Ω 1% (Fig 33 RC; fc ≈ 154 kHz; isolates the coupling-cap load). |
| 109 | 2 | PCM5102A anti-imaging C | Samsung CL10C222JB8NNNC | C33353 | OUTL/OUTR → AGND | ✅ | JLC | 2.2 nF **C0G** (low-distortion; not X7R). |
| 110 | 1 | Stereo headphone amp | TI TPA6132A2RTER | C69901 | DAC (via #108+#111) → INL−/INR−; OUT → jack #115 | ✅ | JLC | DirectPath capless, WQFN-16, pin-strapped gain, no I2C. **VDD 3.3 V.** G0/G1→GND = −6 dB; EN #114. **HPVDD(12): 2.2 µF cap ONLY — never to a supply.** See integration note. |
| 111 | 4 | TPA6132A2 1 µF caps | CL10A105KB8NNNC | C15849 | CFLYING + CHPVSS + 2× input coupling | ✅ | JLC | 1 µF X5R ×4 (CHPVSS ≥ CFLYING; input caps DC-block DAC into inverting inputs). |
| 112 | 2 | TPA6132A2 2.2 µF caps | CC0805KKX7R9BB225 | C125847 | CHPVDD(12) + CVDD(14) → GND | ✅ | JLC | 2.2 µF ×2 (CHPVDD cap-only, never to supply). |
| 113 | 1 | TPA6132A2 VDD HF cap | CC0603KRX7R9BB104 | C14663 | VDD (14) → GND | ✅ | JLC | 100 nF close-in. |
| 114 | 1 | TPA6132A2 EN pull-down | RC0603FR-07100KL | C14675 | EN (13) → GND | ✅ | JLC | 100 kΩ (boots OFF; GPIO drives EN high after DAC settles). |
| 115 | 1 | 3.5 mm headphone jack | PJ-327C-4A | C145813 | HP amp OUT → tip/ring; SGND → sleeve; detect → GPIO | ✅ | JLC | TRS + detect switch. **Read the pad map (tip/ring/sleeve/switch) from the maker drawing** (varies). Detect GPIO → mute speaker amp (#47 SD). |

**Integration notes (multi-pin modules):**
- **BT830 (#99):** UART3 with 4-wire HW flow control (RTS/CTS); `VREG_EN_RST#` (pin 8) driven from a GPIO (boot low, hold >5 ms, then high — do NOT strap through 10 kΩ; ≤4.7 kΩ if strapped). `VREG_IN_HV`(9)=3.3 V, `VDD_PADS`(1)=3.3 V. `VREG_OUT_HV`(10) needs #100. `SPI_PCM#_SEL`(28) → GND (or leave; internal pulldown).
- **PCM5102A (#103):** **SCK(12)→GND** (MCLK-less internal PLL; float/high = dead). FMT(16)/FLT(11)/DEMP(10)→GND. **XSMT(17)→PB3 + R44 10 k pull-down (#120)** (boots soft-muted; not hard-high; soft-mute the DAC when headphones unplugged). **Constraint: run I2S1 at 64fs (or 32fs), fs ≥ 16 kHz** or the DAC PLL won't lock.
- **TPA6132A2 (#110):** inverting single-ended — DAC → #108 RC → #111 coupling cap → INL−(1)/INR−(4); INL+(2)/INR+(3) → GND. OUTR(5) → jack tip (L), OUTL(16) → ring (R), direct (no caps): step 3 swapped the channels, so the amp's R channel carries LEFT (DAC OUTL → R36/C86 → INR−(4)) and its L channel RIGHT (R35/C85 → INL−(1)); the two channels are identical, with shared EN and gain; SGND(15) → sleeve. G0(6)=G1(7)→GND (−6 dB). EP → GND + vias (never VDD). Cap ALSA softvol max (amp clips into 16 Ω at full DAC scale).
- **Audio routing (software):** speaker amp #47 + DAC #103 share I2S1 (both receivers) → route by enable/disable. Jack detect (#115) → `simple-audio-card,hp-det-gpios` on PG14 (step 3; was PE12) (reports SW_HEADPHONE_INSERT; don't also bind gpio-keys to PG14; speaker-off on insert needs `pin-switches = "Speaker"` + a rule, HP_EN follows the Headphones pin); IN → speaker off (SPK_EN low) + HP on (EN high); OUT → reverse. **3 native GPIOs: jack-detect (PG14 in, via R49 10 kΩ, bias-pull-up, high = inserted), speaker-EN (PE6, push-pull), HP-EN (out).** Analog HP audio has no external ESD array (relies on the amp's ±8 kV); route HP traces away from switchers/backlight/RF, clean SGND return.

---

## Schematic & routing notes

**Power tree:** battery ↔ BQ24073 → **SYS (~3.0–4.4 V)** → TPS63021 buck-boost → **3.3 V** (sole source) → FP6161 → **0.9 V core** (the core buck runs from +3V3 since step 2). Internal LDOs make 1.8 V + 1.5 V-DDR from 3.3 V. Rails to widen/thermal: 3.3 V (~1.4 A worst incl. the core buck; TPS63021 2 A boost / 4 A buck) + SYS. Exposed-pad thermal-via arrays on **FP6161 (#3), TPS63021 (#66), BQ24073 (#53), MAX98357A (#47)**. 4-layer minimum.

**Pinmux (mainline `pinctrl-sun20i-d1`; 72 bonded GPIO, PD bank full):**
- RGB666 LCD = **PD0-21**, DISP = **PD22** · SPI0-NOR = **PC2-5** · microSD = **PF0-5** + card-detect **PG10** (step 3; was PF6) · UART0 console = **PE2/3**
- **BT HCI = UART3 PG0-3** (NOT UART1 — its flow-control PG6-9 clashes i2c1) · I2S1 = **PG12/13/15** · i2c1 (touch/haptics/expander/gauge/INA) = **PG8/9** · IMU **i2c2 = PE4/5**
- USB0 (USB-C) dedicated balls; USB1 unused. **Never use phantom D1 pins** PC0/1, PB0/1/8-12, PE14-17, PG16-18.
- Native single GPIOs needed: PCA9555-INT, power-btn EINT, touch INT+RST, jack-detect, speaker-EN, SD-CD (PG10), HP-EN, backlight-CTRL, XSMT, /PGOOD, ALRT, DISP.

**I2C address map (collision-free):**
- **i2c1:** PCA9555 `0x20` · MAX17048 `0x36` · FT7311 touch `0x38` · INA226 `0x40` · DRV2605L `0x5A`
- **i2c2:** LSM6DSOX `0x6A`

**Routing rules:**
- **RGB666:** data/DE/HSYNC/VSYNC may use `drive-strength=<10>` (optional); **PD18 DCLK** stays at the reset Level 1 (`<20>`) or `<30>`, never `<10>` (needs its own DT pin group); **R50 33 Ω DCLK source R at U1.75 (#126)**; route the 22-line bus over solid GND; keep the ribbon away from GPADC/touch/I2S. The bit-map/MSB order is bench-verified (BOM-REVIEW H2).
- Keep **I2S clocks (PG12/13) away from the slow i2c1 pair (PG8/9)** — ground trace between; unused I2S MCLK (PG11) = static GPIO.
- **Kelvin-route** the INA226 shunt (#82) and the Rset ground (#33).
- Route analog headphone traces away from switchers / backlight / RF.
- Design-for-debug: bare back-side test pads on every rail (#131: 5V/BAT/SYS/3.3V/0.9V/1.8V/1.5V/~19V-BL/GND) + the console pads (#122). Total SYS current comes from the INA226 (#81), so no 0 Ω meter links or DNP bulk pads (dropped 2026-10-13).

**Open items — resolve before routing (need vendor data / bench test):**
1. **RGB bit-to-pin map + MSB order** (Zettler, or bench-test the panel) — copper-committing.
2. **FPC contact-side** for #26/#27 (physically confirmed bottom-contact; test-fit before locking footprints).
3. **FT7311** I2C address (0x38 assumed) + report format — confirm on panel via `i2cdetect`.
4. **Vybronics LRA (#46)** coil impedance ≥8 Ω + max drive vs DRV OD_CLAMP.
5. **Battery cell (#73)** — chosen: **PKCELL LP-803860** 2000 mAh 1S (datasheet on file); form factor 62 × 38.5 × 8.5 mm. 1C=2 A ≈ ~1.8 A peak but OK (buck-boost holds rails, OCP >2 A); a 2C `803860` is optional margin at the same size. On arrival confirm OCP >2 A + polarity red=+→BAT.

**Symbol library** (`../../../cad/symbols/easyeda2kicad.*`): 59/62 parts fetched. Missing = **C2481 (SS16), C23179 (470 Ω), C33353 (2.2 nF)** — their footprints (SMA, R0603, C0603) are already present; re-fetch to match by LCSC#, or reuse the generic symbol. **BT830 (#99) needs a hand-drawn symbol+footprint.**


## Audit-recommended additions (2026-09-18 — user-approved, sourced via part-# carry-over)

A per-subsystem netlist-correctness audit (vs. component datasheets) found two parts the design **notes** call for but the numbered list omitted / a topology fix. Added here and mirrored in `gb3/gb3-bom.csv` + `gb3/gb3-nets.csv` (refdes R39/R40):

| # | Qty | Role | Part | LCSC | Net | Src | Fit | Notes |
|---|----:|------|------|------|-----|:---:|-----|-------|
| 116 | 1 | DISP pull-up | RC0603JR-0710KL | C99198 | PANEL_DISP → 3.3 V | ✅ | JLC | 10 KΩ. Line 64 requires it ("DISP … 10 KΩ pull-up") but no part was listed. refdes **R39**. Part # confirmed via C99198 carry-over (#14/#84/#89). |
| 117 | 1 | STM6601 /PB_OUT pull-up | RC0603JR-0710KL | C99198 | /PB_OUT → 3.3 V → PE13 | ✅ | JLC | 10 KΩ. Levels the power-button EINT to 3.3 V; PE13 (step 3; was PE10) reads /PB_OUT instead of the ~SYS PB node. refdes **R40**. See #92/#94. Part # = C99198 carry-over. |
| 118 | 2 | i2c2 pull-ups (IMU bus) | 0603WAF1501T5E | C22843 | I2C2_SCL/SDA → 3.3 V | ✅ | JLC | 1.5 KΩ ×2 (reuses #97 part; 2.2–4.7 K also fine). **🔴 BLOCKER: i2c2 had no pull-ups → IMU never ACKs.** refdes R41/R42. Part # = C22843 carry-over (#97). |

**Also applied to `gb3-nets.csv` (net-only, NO new parts):** VCC-RTC→LDOA merge (blocker: was a dead island), SW1 pad-pairing (later corrected to `{1,2}=RESET/{3,4}=GND` per datasheet — see round 3), R21 wired as a real series `VBUS_SENSE`, C19–C22 decoupling re-pointed to AVCC/HPVCC/VRA1/VRA2, and R38 = wiring the **existing #33** Rset (FB→GND, backlight blocker).

~~**NOT added (judged unnecessary):** MAX17048 ALRT pull-up — #77 marks ALRT optional (polled over I²C).~~ Reversed 2026-10-13: ALRT latches low until the host clears it, so it needs the pull-up → #119 (R43).

**Still needs a copper/footprint decision (not changed here):** FPC1 touch contact-side. (CN1 polarity, `SD_CD` and C32 were closed 2026-10-13: step 1b.)


### Independent-review round 2 (applied)

- **#118 i2c2 pull-ups (R41/R42)** — 🔴 blocker: the IMU I²C bus had no pull-ups; added 2×1.5 K to +3V3.
- **R1 DNP** — the R1/R14 duplicate 680 K mono-strap: R1 (#5) set place=no, R14 (#48) populated.
- **OCS_Aux** pin-name fix (was OSC_AUX) on the IMU aux pin.

### Datasheet-driven resolution round 3 (applied 2026-09-18)

Verified against maker datasheets (C318884 TS-1187A, C127481 ALPS SKRTLBE010, C145813 PJ-327C-4A + CUI SJ-3524 as the labeled reference):
- **TS-1187A tactiles (SW1–11)** — 🔴 the `{1,3}/{2,4}` assumption was **wrong**: A–B (pins 1,2) and C–D (pins 3,4) are the internally-common pairs, so signal was shorted to GND (RESET→GND = no boot). **Fixed to `{1,2}=signal / {3,4}=GND`.** **Footprint cross-checked** (`SW-SMD_4P-L5.1-W5.1…`): pads 1,2 on the top row, 3,4 on the bottom row = the two common pairs → matches the fix (closes the "footprint pairing" review item).
- **ALPS SKRT (SW12–16)** — confirmed `{1,3}` common / `{2}` = switched pole; netlist (signal on pin 2, GND on 1/3 + lugs) is correct. No change.
- **PJ-327C-4A jack (Audio1)** — confirmed stereo w/ tip switch: `1=sleeve, 2=tip(L), 3=ring(R), 4=tip-switch` (one NC pair {2,4}); netlist matches. Final continuity-meter check at assembly.

Still open for the reviewer (not netlist-fixable): battery current margin + PCM OCP, DRV2605L LRA clamp firmware, symbol pin counts (P1=42/FPC1=6+2, FPC1=8), FT7311 VDD headroom, and doc nits (PCA9555 pull-up ≈33 k). (Closed 2026-10-13: AVCC bulk #129, `SD_CD` pull-up #124, `GAUGE_ALRT` #119, the pin-107 census, and the **JACK_DET** caveat: the TPA6132A2's OUTL is 20 Ω to GND in shutdown (tpa6132a2.pdf p.5), so unplugged reads low; R49 #125 limits PE12's current while the amp is on.)

## Design-data closure, step 1 (2026-10-13 — user-approved)

Six open schematic decisions + four fab blockers, each verified against the datasheets by an independent check. Mirrored in `gb3/gb3-bom.csv` + `gb3/gb3-nets.csv`; board updated by `gb3/sync_netlist.py`.

| # | Qty | Role | Part | LCSC | Net | Src | Fit | Notes |
|---|----:|------|------|------|-----|:---:|-----|-------|
| 119 | 1 | MAX17048 ALRT pull-up | RC0603JR-0710KL | C99198 | GAUGE_ALRT → 3.3 V | ✅ | JLC | 10 KΩ. ALRT is open-drain and latches low until the host clears CONFIG.ALRT (DS p14). Pulled to +3V3 (PB5's VCC-IO rail), not BAT. refdes **R43**. |
| 120 | 1 | PCM5102A XSMT pull-down | RC0603JR-0710KL | C99198 | DAC_XSMT → GND | ✅ | JLC | 10 KΩ. The DAC boots soft-muted (PB3 is Z at reset) and un-mutes only when PB3 drives high. No cap on XSMT. refdes **R44**. |
| 121 | 2 | UART0 console series R | 0603WAF5100T5E | C23193 | UART0_TX/RX → TP1/TP2 | ✅ | JLC | 510 Ω. Back-power protection (t113-breakout REV-B Fix 2: PE3's ESD diode back-powered a Rev-A board from the USB-UART adapter). refdes **R45/R46**. |
| 122 | 3 | Console probe pads | bare pad (`manual:TestPoint`) | — | UART0_TX_PAD / UART0_RX_PAD / GND | — | — | 1.5 mm bare pads on the back: TP1 = TX, TP2 = RX, TP3 = GND. Fit — : fab.py leaves them out of the JLC BOM and CPL. |

**Net-only changes (no new parts):**
- **Speaker amp SD/MODE → DS Fig 5** (#48): PE13 (`SPK_EN`) → R14 680 K → SD_MODE (`SPK_SD`); the amp boots in shutdown. Firmware: PE13 push-pull output, high = on. (Step 1b moved SPK_EN to PE6 so PE13's PWM5 can drive the backlight; step 3 then moved the backlight to PE10/PWM4 and the power-button EINT to PE13.)
- **`USB_DP`/`USB_DM` → `USB_D+`/`USB_D-`**, so KiCad pairs them: netclass patterns and the "USB pair" rule in `gb3.kicad_dru` (gap, uncoupled length, in-pair skew) updated.
- **BAT_SENSE divider deleted** (#75/#76, R24/R25); U1.101 GPADC0 = NC.
- **Internal LDO bulk:** C35 (#20, 10 µF) → LDOB at U1.30, C42 (#21, 4.7 µF) → LDOA at U1.28. Both were spare +3V3 bulk.
- **FP6161 input cap:** C33 (#20) → CIN at U2.4 VIN, moved from the soc sheet to core-buck.
- **Charger** BQ24074 → BQ24073 (#53), pin 15 TD → GND.
- **Kept as is:** U12 (TPS63021) PG stays NC. It is a current-limit flag, not power-good (DS §7.3.4), and NC is allowed. T113 RGB666 R/B channel identity was re-checked and is correct.

**Devicetree / firmware:** PB5 = MAX17048 IRQ (`IRQ_TYPE_LEVEL_LOW`, `wakeup-source`); PB3 = XSMT, push-pull, default low; PE6 = speaker enable (PE13 until step 1b), push-pull, default low; GPADC0 unused.

## Design-data closure, step 1b (2026-10-13 — user-approved)

The 17 "should-fix" items from the step-1 sweep, each re-derived from the datasheets by one reviewer and re-checked by an independent skeptic (none refuted), plus the user's choices: USB-C via 2× Rd, a board NTC. Mirrored in `gb3/gb3-bom.csv` + `gb3/gb3-nets.csv`; board updated by `gb3/sync_netlist.py`.

| # | Qty | Role | Part | LCSC | Net | Src | Fit | Notes |
|---|----:|------|------|------|-----|:---:|-----|-------|
| 123 | 1 | Charge safety-timer R (TMR) | 0603WAF6802T5E | C23231 | TMR (U10.14) → VSS | ✅ | JLC | 68 kΩ 1% (as gb2 R21): tMAXCHG 6.8/9.1/11.3 h, tPRECHG 41/54/68 min (bq24074.pdf p.13; eq. 6/7 p.27). The floating default (4/5/6 h) can expire before a from-empty charge at 445 mA finishes. Never tie TMR to VSS (no backstop). refdes **R47**. |
| 124 | 1 | SD card-detect pull-up | RC0603JR-0710KL | C99198 | SD_CD → 3.3 V (PG10) | ✅ | JLC | 10 kΩ (MangoPi R48). The socket's CD switch closes to the shell on insert → active-low on PG10 (step 3; was PF6), so the DT can use `cd-gpios` instead of `broken-cd` polling. refdes **R48**. |
| 125 | 1 | Jack-detect series R | RC0603JR-0710KL | C99198 | JACK_SW → JACK_DET (PG14) | ✅ | JLC | 10 kΩ at the jack: the tip switch rests on the ground-centred TPA6132 output that drives the tip (OUTR since the step-3 channel swap; ±1.5 V when on); caps PG14's clamp current at ≤ 0.1 mA and is its only ESD current limit. Unplugged + amp off ≤ 0.5 V worst case (VIL max 0.99 V). refdes **R49**. |
| 126 | 1 | RGB DCLK series R | 0603WAF330JT5E | C23140 | U1.75 PD18 (LCD0_CLK) → RGB_DCLK → P1.30 | ✅ | JLC | 33 Ω source termination at the T113 pin (≤ 8 mm, target ≤ 5): ~48 mm of PCB + the panel's 53 mm FPC; removes the drive-Level-2/3 overshoot and cuts harmonics up to ~6 dB (MangoPi R59 33R, gb2 #79). Kept in subcircuit soc so RGB_DCLK keeps its DRU rules. refdes **R50**. |
| 127 | 1 | Backlight CTRL pull-down | RC0603FR-07100KL | C14675 | BL_CTRL → GND | ✅ | JLC | 100 kΩ at U6.2: holds CTRL below 0.4 V while PE10 is Hi-Z (reset; a disabled PWM channel in suspend) for pad leakage up to ~4 µA (the internal pull-down alone: ~0.25 µA; typical leakage is nA). 33 kΩ would also cover the T113's 10 µA worst case. refdes **R51**. |
| 128 | 2 | USB-C CC Rd | RC0603FR-075K1L | C105580 | CC1 / CC2 → GND | ✅ | JLC | 5.1 kΩ ×2: Type-C sink, 5 V only (as gb2 R15/R16 and the breakout R1/R2). Replaces the discontinued CH224K (#61–#65). refdes **R52/R53**. |
| 129 | 1 | AVCC (ALDO) output cap | CC0805KKX7R9BB225 | C125847 | U1.89 AVCC → GND | ✅ | JLC | 2.2 µF X7R 0805 beside C19: the internal ALDO is on at reset (POWER_REG ALDO_EN = 1, 1.80 V); MangoPi C11 / DevTerm C60 2.2 µF. refdes **C92**. |
| 130 | 1 | VBUS HF cap | CC0603KRX7R9BB104 | C14663 | +5V → GND at D4.5 | ✅ | JLC | 100 nF at the USBLC6 VBUS pin (ST Fig 18); not bulk. refdes **C93**. |
| 131 | 9 | Rail test points | bare pad (`manual:TestPoint`) | — | +5V · BAT · SYS · +3V3 · +0V9 · LDOA · LDOB · BL_LED_P · GND | — | — | Ø1.5 mm bare pads on the back, nothing assembled: TP4 +5V, TP5 BAT, TP6 SYS, TP7 +3V3, TP8 +0V9, TP9 LDOA, TP10 LDOB/VCC-DRAM (the BOM-REVIEW M2/Q13 point, at U1.48/49), TP11 BL_LED_P (~19 V; 37–39 V with the LED string open), TP12 GND. Fit — : not in the JLC BOM/CPL. |

**Changes to existing parts:**
- **Charge LED** (#79/#80): R26 is 1 kΩ from SYS_SRC (charger OUT), so it lights while charging with the system off.
- **VBUS:** C32 (10 µF) deleted. With it, 14.7 µF was hard-started at attach, over USB 2.0's 10 µF; every regulator already runs from SYS behind the charger. C93 100 nF (#130) added.
- **TS** (#59): R17 is now a 10 kΩ B3950 NTC; it moves to the battery-cutout edge in the placement step.
- **SoC decoupling:**
  - C13 → LDOA at VCC-TVIN 107.
  - C39 → 10 µF core bulk at the core balls; C34 = the FP6161 COUT (core-buck sheet); C36 = LDO-IN/VCC-PE bulk.
  - C7 (U3), C11 and C40 (U4) relabelled to the sheets they physically serve; R11 → soc (it sits at U1.27).
  - The AVCC (89) / HPVCC (97) notes now state the decision: their own internal-LDO nodes, never merged with LDOA. VCC-TVIN (107) stays on LDOA, as MangoPi's R71/C70 does, and never on +3V3 (2.16 V abs-max).
- **SD card-detect:** U4.9 → PF6 (SD_CD) + R48 (#124), with ESD on D1.3. SD_CMD moved to D2.4, beside its pad. (Step 3: CD → PG10, and the SP3004 channels remapped: CD on D1.6, CMD on D2.6.)
- **Backlight PWM:** BL_CTRL → PE13 (PWM5) and SPK_EN → PE6 (#28, #48), plus R51 (#127). (Step 3: BL_CTRL → PE10/PWM4.)
- **Jack detect:** R49 (#125) in series; the tip-switch net is now JACK_SW.
- **DCLK:** R50 33 Ω (#126); the U1 side is the local net /soc/LCD0_CLK.
- **USB-C:** CH224K + R19/R20/R21/C58 (#61–#65) deleted → R52/R53 Rd (#128).
- **LEDs:** U5/U14 → C2986057 (#23/#79).
- **CN1:** notes only (pad 1 = JST circuit 1 = BAT; not mirrored); the '+'/'−' silk marks are a step-2 task.

**Fab outputs:**
- `gb3/sync_netlist.py` now stamps every footprint's `LCSC Part` field from `gb3-bom.csv`. Before this, 131 of them were library leftovers: every R0603 said C23219 (a 620 kΩ), every C0603 C33353, and D4 an MT3608 boost IC. The KiCad Fabrication Toolkit reads that field, so it would have ordered those parts. This run stamped 137, including the new parts and the U5/U14 LED swap.
- Fab outputs will come from a port of gb2's `fab/fab.py`: part numbers from `gb3-bom.csv`, Fit from this file, bottom-side CPL rotation 180 − θ. Never use the Fabrication Toolkit button; its `AUTO TRANSLATE` is now off.
- Check every polarized part in JLC's placement preview before ordering.

**Devicetree / firmware:**
- **SD:** override the stock `cd-gpios` (PF6) with `cd-gpios = <&pio 6 10 GPIO_ACTIVE_LOW>` (PG10, step 3). Drop the `&mmc0 { broken-cd; }` block from gb3 builds by moving it into a breakout-Rev-A-only overlay.
- **WiFi conflict:** the inherited MangoPi DT enables `&mmc1` (WiFi) on PG0–PG5 and `wifi_pwrseq` on PG12. gb3 uses PG0–PG3 for UART3/BT and PG12 for I2S1_LRCK, so gb3's overlays must disable both. Disabling `&mmc1` also releases PG10 (PINOUT.md), now the SD card-detect.
- **Backlight:**
  - Now: `gpio-backlight` on PE10 (step 3; was PE13), or EasyScale.
  - Later: the out-of-tree pwm-sun8i (v13) + `pwm-backlight` at ~25 kHz (`pwms = <&pwm 4 40000 0>`, PWM4 on PE10). Give it no `enable-gpios` or `power-supply`, so brightness 0 holds duty 0.
- **Speaker:** `sdmode-gpios = <&pio 4 6 GPIO_ACTIVE_HIGH>` (PE6).
- **Jack:** PG14 `gpio_in` + bias-pull-up, `simple-audio-card,hp-det-gpios = <&pio 6 14 GPIO_ACTIVE_HIGH>` (step 3; was PE12); gb3's I2S1 pin group must not claim PG14 (DIN unused); HP_EN (PB2) via `simple-audio-amplifier` on the Headphones path.
- **Bring-up:** TP10 first shows whether boot code raised LDOB from its 1.35 V default to 1.5 V.

**Purchases:** BT830 (#99) last-time buy now; the panel (#25) needs a channel other than Digi-Key; STM6601 (#94) pre-order at JLC or consign.

**Open item, closed in step 1c:** in this EN1/EN2 mode the charger drew up to 1.34 A (ILIM) from any source, with no VIN-DPM, so a 500 mA USB port was overloaded → the firmware-raised limit below.

## Design-data closure, step 1c (2026-10-13 — user-approved): firmware-raised charger input limit

The charger drew up to 1.34 A from any source while the handheld ran (no VIN-DPM in its ILIM mode), overloading 500 mA USB ports. With the handheld off it draws only the ~445 mA charge current, so a firmware-controlled limit is enough. Designed by one reviewer and checked by two independent skeptics (electrical and system).

| # | Qty | Role | Part | LCSC | Net | Src | Fit | Notes |
|---|----:|------|------|------|-----|:---:|-----|-------|
| 132 | 1 | ILIM fast resistor | 0603WAF1801T5E | C4177 | ILIM → Q1 drain (CHG_ILIM_SW) | ✅ | JLC | 1.8 kΩ in parallel with R16 (#58) while Q1 conducts: 3.6k ∥ 1.8k = 1.2 kΩ → 1.24/1.34/1.45 A (KILIM 1500/1610/1720). Keep the ILIM copper (U10.12–R16–R54) ≤ 8 mm, one layer (DRU rule "Charger ILIM node"). refdes **R54**. |
| 133 | 1 | ILIM fast switch | CJ 2N7002 | C8545 | gate CHG_FAST (PB7) · source GND · drain → R54 | ✅ | JLC | N-MOSFET, SOT-23 (pin 1 G, 2 S, 3 D): Vth 1.0–2.5 V, RDS(on) ≤ 7 Ω at 5 V, IDSS ≤ 80 nA (CJ DS p.2). Check its orientation in JLC's placement preview. refdes **Q1**. |
| 134 | 1 | ILIM switch gate pull-down | RC0603JR-0710KL | C99198 | CHG_FAST → GND | ✅ | JLC | 10 kΩ at the gate: fast stays off while PB7 is Z (reset, boot, handheld off) — ≤ 0.10 V at the T113's 10 µA pad leakage, ≤ 0.40 V even with PB7's internal 80–120 kΩ pull-up on (a stock MangoPi U-Boot enables it) vs Vth ≥ 1.0 V. refdes **R55**. |
| 135 | 2 | USB-C CC sense resistors | 0603WAF2003T5E | C25811 | USBC_CC1 / USBC_CC2 → USBC_CC_SENSE | ✅ | JLC | 200 kΩ from each CC into one node → GPADC0 (U1.101); the idle CC sits at 0 V on its Rd, so the node ≈ 0.506 × the active CC. The sense path keeps the effective Rd at 4.96–5.11 kΩ (Type-C: 5.1 kΩ ±10%). refdes **R56/R57**. |
| 136 | 1 | CC sense filter cap | CC0603KRX7R9BB104 | C14663 | USBC_CC_SENSE → GND at U1.101 | ✅ | JLC | 100 nF, τ ≈ 10 ms (PD BMC bursts, noise) + charge reservoir for the ADC sample. refdes **C94**. |

**How it works:**
- **Safe** (PB7 low or Z: handheld off, reset, boot, a crashed policy): R16 alone → 366–483 mA, inside any USB port's 500 mA. With the handheld off the cell still charges at ≈ 0.42 A; the charger slows its safety timer in proportion when input-limited (SLUS810N p.25).
- **Fast** (PB7 high): 1.24–1.45 A, only while VBUS is present (PGOOD) and the USB-C source advertises ≥ 1.5 A on CC.
- **Node voltages on GPADC0:** Default sources (incl. legacy A-to-C cables) 0.15–0.29 V; 1.5 A 0.40–0.56 V; 3 A 0.72–0.99 V.
- **Policy thresholds (raw counts, 12-bit, 0–1.8 V):** enter fast at ≥ 0.360 V (raw 819); back to safe below 0.335 V (raw 762); above 1.10 V (raw 2503) = invalid → safe.
- **Measured margins:** 21–47 mV, including a powered cable's Ra on the idle CC. The ±10 mV ADC error and the 100 nA leakage are assumptions — check at bring-up.
- **Blind spots:** a debug accessory or Y-cable pulling both CCs up reads one class high. A non-compliant A-to-C cable with a 10 kΩ pull-up reads as 3 A.

**Firmware (to do when the image is brought up):**
- **Kernel:** a config fragment with `CONFIG_SUN20I_GPADC=y` and `CONFIG_CHARGER_GPIO=y` (`sunxi_defconfig` has neither), plus a DT overlay in the gb3 image layer:
  - `gpio-charger` with `gpios = <&pio 1 6 GPIO_ACTIVE_LOW>` (PB6 /PGOOD; step 3, was PB4), `charge-current-limit-gpios = <&pio 1 7 GPIO_ACTIVE_HIGH>` (PB7) and `charge-current-limit-mapping = <1340000 0x1>, <420000 0x0>`;
  - `&gpadc { status = "okay"; channel@0 { reg = <0>; }; }`.
  - Find the IIO device by name (`sun20i-gpadc`), not by index (the IMU registers IIO too).
- **Policy, in powerd (`src/services/powerd.c`), driven by NETLINK uevents:**
  - On daemon start, any charger change event, any read error or invalid reading, and before suspend, write the safe value first.
  - With VBUS present: wait 150 ms, take the median of 8 readings 40 ms apart, enter fast at raw ≥ 819.
  - While fast, sample every 10–20 ms and go safe after 2–3 consecutive readings below raw 762. Type-C requires a sink to follow a lowered Rp within tSinkAdj = 60 ms (R2.5 §4.6.2.1, Table 4-32); a laptop lowers its port when its adapter is removed.
  - Collapse latch: if PGOOD drops within 5 s of going fast, stay safe until an unplug of ≥ 2 s.
  - Optional user override, "fast on USB-A chargers", default off.
  - Never set `bias-pull-up` on PB7.
- **Hangs:** no watchdog is bound today (the sunxi wdt node is `reserved`; `sunxi_wdt` stops it at probe). A kernel panic reboots via `panic=10`; a hung powerd keeps its last state until a long press powers off. Feed `/dev/watchdog` from powerd if that matters.
- **Custom kernel:** the `KERNEL=custom` path has no gpio-charger or GPADC driver, so it simply stays at the safe limit.

**Bring-up:**
- **Fit the cell:** with no cell (or a tripped PCM), the safe limit can't carry the ~1 A system load, so the board brown-out loops until the policy runs. Bring up with the cell fitted, or 3.7–4.0 V on CN1.
- **Read GPADC0 with no source:** raw ≈ 25 means ~0.1 µA of leakage. Above ~150 nA, switch R56/R57 to 2× 100 kΩ.
- **Compare against a DMM:** check the GPADC0 reading against a meter at the node with a 1.5 A and a 3 A source.
- **Step PB7 under full load,** with and without a cell, and scope VBUS and OUT. TI documents no prohibition on switching RILIM while running, and both values are inside its validated 1.1–8 kΩ range.

## Placement, step 2 (2026-10-14 — user-approved)

Placement after the original `place.py` run is data now: `gb3/placement/step2.csv` (112 moves, each with its reason) applied by `gb3/place_moves.py`. That script refuses the parts the enclosure fixes: P1, FPC1, USBC1, Audio1, U4, CN1, U18, LRA1, SPK1 and SW2–SW16. The moves were surveyed per area (power, SoC, peripherals, board), merged and tested by a planner, checked by an EE-layout and a manufacturing skeptic, then finalised and re-checked.

**What changed:**
- **Core buck** (U2/L1/C33/C34 + FB/RUN parts) moved next to U1's core balls: C34 is 8.6 mm from U1.116 (was 65 mm), CIN 0.34 mm from VIN, and the CIN/COUT grounds are on one island (FP6161 p.10–11). It now runs from +3V3 (BOM #3/#9).
- **U1 decoupling** is on F, directly under U1 (as the t113-breakout does), so the B-side ring stays clear for U1's ~72-signal fan-out. Every decap's rail pad is ≤ 2.2 mm from its ball (was up to 18 mm), and different rails on F are ≥ 0.45 mm apart. The 29-via plan routes; it is in `~/.cache/gb3-step2p/final/iter/final/viafit2*`.
- **Crystals:** X1 is moved/rotated out of the SD fan-out; X2 is on F under U1.24/25, so the RTC nets may take 1 via (DRU).
- **USB-C cluster:** D4 is at the connector with flow-through, C93 at D4.5, and the Rd/CC-sense resistors are out of the pair's path. The USB pair may use 3 vias (DRU).
- **Other moves:**
  - DCLK R50 is 0.5 mm from U1.75 (on F).
  - The console R45/R46 + TP1–TP3 are at U1's north-east corner.
  - Backlight boost: C45's return is now 3.2 mm to U6.4.
  - The charger's ILIM node is 3.6 mm.
  - The NTC R17 is at the cell's right wall.
  - The test-pad strip reads 3V3 | GND | 5V | SYS | BAT, so a slipped probe can't push VBUS into the cell.
- **Status LEDs:** both on F, in separate windows — U5 (power) at (125.4, 34.6), U14 (charge) at (160.5, 34.6).
- **Checks:** DRC errors 5 → 5 (all pre-existing). 0 courtyard overlaps; pad-level spacing is clean (F ≥ 0.40 mm, B ≥ 0.30 mm between different nets). Board vs schematic 0 mismatches; all 26 reserved routing corridors are clear.

**For the enclosure:**
- **Panel film:** an insulating film on the panel back, and no foam over x 57.0–73.4, y 66.6–86.2 (the F decap island, ≤ 1.45 mm tall under a modelled 2.0 mm gap).
- **D-pad rib:** the back-shell rib must go around the core buck behind SW3 (x 39.7–49.3, y 66.8–80.6; L1 is 2.0 mm tall).
- **Windows and access:** two LED windows (above). A reset pinhole at SW1 (82.0, 35.7) on the back, if reset should be reachable.
- **Optional:** a gap filler between the NTC and the cell.
- **Mounting holes:** none placed yet. Free spots: (101.5, 36.0) and (180.5, 35.5).

**Left for the routing plan (step 3):**
- **Zones:** an F.Cu no-copper rule area under X2 (SC-20S); stitched In2 GND islands under the U2, U6 and U12 switch nodes (B.Cu's neighbour plane is +3V3).
- **Crystal and island vias:** move the RESET via to about (69.4, 67.9), clear of X2; add GND vias for C9/C10/C17 inside U1's pin ring.
- **GPIO / pin swaps, decided with a routing trial:** BL_CTRL↔PWR_BTN (PE13↔PE10, both PWM-capable) to keep the PWM lane off X1's load cap; SD_CD, JACK_DET, BT_EN_RST; the USBLC6, PCA9555 and TPA6132A2 channel orders.
- **Antenna keep-out:** trim it to clear U18's own pad 14 (the remaining DRC error).
- **Pours:** thermal reliefs on small SMD pads (0402 tombstoning).
- **Silk pass:** 851 silk warnings vs 321 before. Move F-island refs to F.Fab; keep polarity marks printable; add CN1 '+'/'−', J2 '+' and test-pad labels.
- **Bring-up check:** before the first power-up, meter +0V9, LDOA, LDOB and AVCC against +3V3 on the first boards.

## Routing plan, step 3 (2026-10-14 — user-approved)

The plan is `gb3/route/PLAN.md`, with the reserved channels in `route/channels.py` and U1's per-pin escape in `route/mcu_escape.py`. Every decision in it was measured on one scratch board with all the drafted groups routed together. Applied now: pin swaps, placement, two new caps, keep-outs, jumper pad groups and the DRU. There is no copper on the board yet: route groups 05, 10 and 40 are drafted, and step 4 builds them.

**Pin swaps** (each removes crossings or vias measured in the trial):

| # | Change | Gain |
|---|---|---|
| A1 | PE13 ↔ PE10: power-button EINT on PE13, backlight CTRL on PE10 (PWM4) | the PWM lane no longer passes 0.56 mm from X1's HOSC_IN load cap, and needs no via pair across PSHOLD |
| A2 | SD card-detect PF6 → PG10 | CD no longer crosses the six SD lines (−2 vias) |
| A3 | Jack detect PE12 → PG14 (I2S1-DIN0, unused) | the display bus needs PE12's only F path |
| A4 | USBLC6 D4: I/O1 (1/6) = D−, I/O2 (3/4) = D+ | the pair runs straight from the receptacle through D4 to U1 |
| A5 | PCA9555 U16: port 0 = right grip, port 1 = left grip (keymap below) | 416 → 356 mm, 20 → 14 vias |
| A6 | TPA6132A2 channels swapped: amp channel R carries LEFT (OUTR → tip), channel L carries RIGHT | the two DAC chains nest with 0 vias; the channels are identical, with shared EN and gain |
| A7 | PE0 ↔ PE1: IMU INT2 on PE0, INT1 on PE1 | the IMU ribbon is planar (0 vias) |
| A8 | SP3004 channels: D1.3 SD_D0, D1.4 SD_D1, D1.6 SD_CD, D2.6 SD_CMD (D2.4 spare) | every ESD tap is a stub off its own lane |
| O1 | PB4 ↔ PB6: charger /PGOOD on PB6, BT VREG_EN_RST# on PB4 | saves BT_EN_RST's via pair |

**Placement** (`gb3/placement/step3.csv`, 18 rows):
- R39 (the DISP pull-up) is out of the backlight feed's lane.
- The IMU cluster (U7 + 5 passives) sits east of the RGB via field.
- The SD support row (D1/D2, the pull-ups and two caps) lets the seven SD lanes pass under the ESD parts.
- **New: C95/C96**, 100 nF +3V3/GND (BOM #19, C14663), beside the RGB via field. The B.Cu display traces reference the +3V3 plane, so their return current needs a cap where it changes planes. The nearest decap to the bus vias was 5.1–17.2 mm away; it is now 4.0–8.4 mm.

**Rules and areas:**
- **DRU:**
  - DCLK stays on F with 0 vias, and its 33 Ω source stub is ≤ 4.5 mm.
  - USB pair: ≤ 4 vias, skew ≤ 4 mm (≈ 25 ps against the 2.08 ns UI), uncoupled ≤ 16 mm, plus a gap exception inside the USBC1/D4 pin fields.
  - NOR skew ≤ 12 mm.
  - TP11 (the 37–39 V backlight OVP node) is ≥ 0.6 mm from everything (IPC-2221B B2).
  - Thermal reliefs on small GND pads.
  - The HighCurrent width rules also cover pin tips outside the EasyEDA courtyards.
- **Keep-outs** (`route/footprint_keepouts.py`):
  - no copper under X2 (F) and no tracks over X1 (F);
  - U4's prohibition bars (B) and the SW12–16 lug gaps (F);
  - the antenna keep-out, trimmed to clear U18's pad 14 (DRC errors 5 → 4; the 4 left are all at USBC1: its pads against its own NPTH holes and the board edge).
- **Jumper pad groups** (`gb3/jumper_groups.py`): the TS-1187A switches' internally common pads (1/2, 3/4) and D4's pass-through pins (1/6, 3/4) need no copper tie.

**Mounting holes:**
- **H2** (M2, at 180.5, 35.0) is placed, with a 2.75 mm keep-out.
- **H1 is dropped:** its M2 head would sit ~0.9 mm under the display module (y 37.0).
- Scheme, as gb2: screws clamp, shell fingers locate. The other grip-corner holes will be picked from free board area before the grip routing (group 85), and shown on a render first.

**Battery connector (CN1):** the JST PH is rated 2 A per contact, and the cell's 1C rate is also 2 A. The all-at-once worst case is 2.7 A (U12 1.87 + U6 0.33 + U9 0.5 A). The BAT copper is sized for 2.7 A, but 2 A is the continuous limit. **Decision: firmware load cap.** On battery, powerd caps backlight and volume when the INA226 (U15) reads > 1.8 A.

**Fab:** ask JLC for plugged/capped vias in U1's exposed pad (16 open 0.3 mm holes under the 5.6 mm pad).

**Devicetree / firmware** (the DT lines above are updated to match):
- **SD:** `cd-gpios = <&pio 6 10 GPIO_ACTIVE_LOW>` (PG10) overrides the stock MangoPi value (PF6). Disabling the inherited `&mmc1`, already required, frees PG10.
- **Jack:** `hp-det-gpios = <&pio 6 14 GPIO_ACTIVE_HIGH>` (PG14). gb3's I2S1 pin group must not claim PG14.
- **Backlight:** `gpio-backlight` on PE10 now; later PWM4, `pwms = <&pwm 4 40000 0>`.
- **Power-button EINT:** PE13.
- **IMU:** INT1 = PE1, INT2 = PE0.
- **Charger:** `gpio-charger` `gpios = <&pio 1 6 GPIO_ACTIVE_LOW>` (PB6).
- **BT:** VREG_EN_RST# = PB4.
- **uart3:** BT uses the PG0–3 group only, never the MangoPi default `uart3_pb_pins`. PB6 is now the charger's open-drain /PGOOD, which a TX idling high would fight, and PB7 is CHG_FAST.
- **Buttons** (`gpio-keys` on the PCA9555):
  - P00 R1, P01 VOL+, P02 VOL−, P03 B, P04 X, P05 SELECT, P06 Y, P07 A;
  - P10 UP, P11 DOWN, P12 LEFT, P13 RIGHT, P14 START, P15 L1.

**Checks:**
- ERC: 0 errors.
- Board vs schematic: 0 net or part mismatches. KiCad's parity check lists 387 warnings, all library-name/field cosmetics, the same as before step 3.
- DRC: 4 errors, all at USBC1 (pre-existing); 388 unconnected (no copper yet).
- Every step-3 tool reruns with no change.
- Groups 05/10/40 built on a scratch copy: DRC errors 4 → 4, unconnected 388 → 312.

**Next (step 4):**
- The route groups in order (05, 10, 20, 25, 30, 40, 50, 60, 70, 75, 80, 85), with the second review's items (end of `route/PLAN.md`).
- Then fanout, pours, checks, silk and the fab outputs.
