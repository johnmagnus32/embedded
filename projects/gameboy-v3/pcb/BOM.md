# gameboy-v3 PCB — Bill of Materials + Schematic/Routing Notes

Integrated T113-S3 handheld (mainline Linux) on the proven `t113-breakout` core: 5" 800×480 IPS
RGB666 panel + FT7311 touch, haptics, 6-axis IMU, I2S audio (speaker + headphone), 1S LiPo
charge/power-path, onboard Bluetooth. **115 parts, all sourced.** Datasheets in `../../../cad/docs/`.

**Src:** ✅ = confirmed LCSC part # · ⚠️ = candidate. **Fit:** JLC = SMT-placed (needs LCSC #) · Hand = you attach (module/cell/speaker/LRA).

---

## 1. Core — SoC + power + clock + storage

| # | Qty | Role | Part | LCSC # | Interface / net | Src | Fit | Note |
|---|-----|------|------|--------|-----------------|-----|-----|------|
| 1 | 1 | SoC | T113-S3 | C5197687 | system | ✅ | JLC | Dual Cortex-A7, 128 MB in-package DDR3, eLQFP-128. |
| 2 | 1 | USB-C | TYPE-C-31-M-12 | C165948 | 5 V in + USB2 | ✅ | JLC | Receptacle (charge + FEL/data). **Shell + stainless mid-plate + all 4 THT retention posts → GND** (ESD discharge path + mechanical retention; direct short — plastic-shelled handheld, no separate earthed chassis). Optional DNP hedge: 4.7 nF/2 kV ∥ 1 MΩ shell-to-GND (split off only if a first-article EMI/ground-loop issue appears; default = stuff the short). |
| 3 | 1 | 0.9 V core buck | FP6161KR-LF-ADJ | C77234 | **input = SYS** | ✅ | JLC | Sync buck, SOT-23-5, 1 A, 0.6 V FB. **RUN driven from STM6601 EN via RC #9 (≥2 ms after VCC-IO)** — do NOT tie RUN→SYS (no true-off/drain) or →3.3 V (RUN abs-max = VIN). FB set by #6/#7; output cap = 10 µF (see #21). |
| 4 | 1 | Core-buck inductor | SMNR4020-2.2UH | C135262 | FP6161 (#3) | ✅ | JLC | 2.2 µH 3.4 A shielded. |
| 5 | 1 | Amp mono strap R | RS-03K6803FT | C140074 | MAX98357A SD/MODE (#48) | ✅ | JLC | 680 K 1% 0603. |
| 6 | 1 | Core FB R_bot | 0603WAF1503T5E | C22807 | FP6161 FB | ✅ | JLC | 150 K 1%. With #7 (75 K) → 0.6·(1+75/150) = 0.9 V. |
| 7 | 1 | Core FB R_top | 0603WAF7502T5E | C23242 | FP6161 FB | ✅ | JLC | 75 K 1%. |
| 8 | 1 | Core FB feed-forward | CL05C100JB5NNNC | C32949 | across #7 | ✅ | JLC | 10 pF C0G 0402 — REQUIRED for loop stability. |
| 9 | 2 | Core-buck RUN RC | 100 kΩ 0603 (C14675) + 100 nF 0603 (C14663) | C14675 / C14663 | STM6601 EN → R → RUN(pin1); C: RUN→GND | ✅ | JLC | Delays VDD-CORE to ≥2 ms after VCC-IO. ~10 ms (drop C for faster, floor ~2 ms). |
| 10 | 1 | DDR ZQ | 0603WAF2400T5E | C23350 | T113 pin 47 → GND | ✅ | JLC | 240 Ω 1% (JEDEC ZQ-cal nominal). |
| 11 | 1 | SPI-NOR | W25Q128JVSIQ | C97521 | SPI0 boot | ✅ | JLC | 128 Mbit, SOIC-8, QE=1. |
| 12 | 1 | microSD socket | A-MicroTF-1.85A | C22467599 | SDC0 | ✅ | JLC | Push-push, card-detect. |
| 13 | 2 | microSD ESD array | Littelfuse SP3004-04XTG ×2 | C207280 | SDC0 CLK/CMD/DAT0-3 | ✅ | JLC | 2× 4-ch → all 6 SD lines. 0.85 pF ultra-low-cap, SOT-563, unidirectional rail-clamp. |
| 14 | 6 | SD/RESET pull-ups | RC0603JR-0710KL | C99198 | SD CMD+DAT0-3 → 3.3 V | ✅ | JLC | 10 KΩ. **⚠️ RESET pull-up goes to the 1.8 V VCC-RTC rail, NOT 3.3 V** (RESET buffer = VCC-RTC, abs-max 2.16 V). SD-line pulls stay on 3.3 V. |
| 15 | 1 | HOSC crystal | X322524MRB4SI | C70571 | 24 MHz | ✅ | JLC | SMD3225-4P, CL 18 pF (loads #17). |
| 16 | 1 | RTC crystal | SC-20S 32.768kHz | C97607 | RTC | ✅ | JLC | SMD2012-2P (loads #18). |
| 17 | 2 | 24 MHz load caps | 0402CG220J500NT | C1555 | HOSC | ✅ | JLC | 22 pF C0G 0402. |
| 18 | 2 | RTC load caps | 0402CG180J500NT | C1549 | RTC xtal | ✅ | JLC | 18 pF C0G 0402. |
| 19 | ~25 | Decoupling | CC0603KRX7R9BB104 | C14663 | per-power-pin | ✅ | JLC | 100 nF 0603 X7R (per-ball + flash + SD). |
| 20 | ~9 | Bulk | CL21A106KOQNNNE | C1713 | bulk | ✅ | JLC | 10 µF 16 V 0805. **Place one on 5 V VBUS + one on SYS (Rev-A brownout fix); one = core-buck (#3) output.** |
| 21 | ~4 | Bulk | CC0603KRX5R6BB475 | C109456 | general bulk | ✅ | JLC | 4.7 µF 0603 X5R. |
| 22 | 1 | RESET button | TS-1187A-B-A-B | C318884 | RESET | ✅ | JLC | 6×6 mm tactile. |
| 23 | 1 | Power LED | 19-213SYGC | C2986027 | status | ✅ | JLC | Green 0603. |
| 24 | 1 | Power-LED resistor | 0603WAF5100T5E | C23193 | LED limit | ✅ | JLC | 510 Ω. |

**§1 notes:** DDR3 is on a separate **1.5 V VCC-DRAM** rail (balls 48/49), sourced by the internal **LDOB off 3.3 V** (firmware sets LDOB=1.5 V). Include the VBUS + SYS input bulk caps (#20).

---

## 2. Screen — Zettler ATM0500D27-CT (5" 800×480 IPS, RGB666 via TCON-LCD0, 22 pins on the PD bank)

`sun4i_rgb` + `panel-dpi` + `edt-ft5x06` touch. RGB uses PD10-12 → **SPI1 is unavailable.**

| # | Qty | Role | Part | LCSC # | Interface / net | Src | Fit | Note |
|---|-----|------|------|--------|-----------------|-----|-----|------|
| 25 | 1 | 5" IPS panel (+ FT7311 touch on-panel) | Zettler ATM0500D27-CT | DigiKey (module) | RGB666 PD0–21; touch on FFC #27 | ✅ | Hand | 24-bit RGB, IPS, logic 2.7–3.6 V. **RGB bit-map NOT datasheet-resolved — get Zettler's map; default = drive 6 MSBs R7..R2, ground R1/R0.** Touch = FocalTech **FT7311, I2C 0x38**; bind `edt,edt-ft5506` (or add `focaltech,ft7311`) — confirm on panel. |
| 26 | 1 | Display FPC connector | BOOMELE 0.5-40PFGPZ | C9160 | 40P 0.5 mm ZIF, right-angle SMD | ✅ | JLC | **Bottom-contact → mount on the TOP/screen-facing face** (ribbon copper-down). Set pin-1 + orientation; test-fit before locking. |
| 27 | 1 | Touch FFC connector | HDGC 1.0K-1.5-6PWB | C2919568 | 6P 1.0 mm (1=VDD 2=GND 3=SCL 4=SDA 5=INT 6=RST) | ✅ | JLC | Bottom-contact, latching lid. **⚠️ Opposite orientation from #26 → mount on the BOTTOM/back face** (touch ribbon copper-up, wraps around the board edge). Check fold/length. |
| 28 | 1 | Backlight WLED boost | TI TPS61165DBVR | C58756 | WLED CC boost + CTRL | ✅ | JLC | SOT-23-6. **VIN from SYS/5 V, NOT 3.3 V.** 40 mA via #33. **Route CTRL to a PWM-capable GPIO** (EasyScale or `pwm-gpio`; no HW PWM on T113). Open-LED OVP is internal (no OVP pin). |
| 29 | 1 | Boost inductor | SMNR4020-10UH | C135263 | boost | ✅ | JLC | 10 µH 1.6 A shielded. |
| 30 | 1 | Boost diode | MDD SS16 | C2481 | boost | ✅ | JLC | 60 V 1 A Schottky, SMA(DO-214AC). |
| 31 | 1 | Boost output cap (Cout) | YAGEO CC0805KKX7R9BB225 | C125847 | boost | ✅ | JLC | 2.2 µF 50 V X7R 0805 (≥1 µF after 19 V derate; ≥50 V clears OVP). |
| 32 | 1 | Boost input cap (Cin) | CC0603KRX5R6BB475 | C109456 | at VIN (SYS) | ✅ | JLC | 4.7 µF 0603 (reuse #21). |
| 33 | 1 | Boost FB sense R (Rset) | 5.1 Ω 1% 0402 | verify (Basic) | boost FB | ✅ | JLC | Sets 40 mA (VREF 200 mV / 5.1 Ω). Brightness knob; never exceed 40 mA. **Kelvin-route its ground to the TPS61165 GND pin.** |
| 34 | 1 | Boost COMP cap | YAGEO CC0402KRX7R7BB224 | C326590 | COMP (SOT-23 pin 5) → GND | ✅ | JLC | 220 nF 16 V X7R — **mandatory loop-comp; omit → unstable.** Place close to pin 5. |

**§2 notes:**
- **RGB666 bit-map (copper-committing, unverified):** panel is genuine 24-bit, datasheet silent on bit order. **Get Zettler's R/G/B bit-to-pin map before routing.** Convention-safe default: drive the 6 MSBs **R7..R2 / G7..G2 / B7..B2**, tie the 2 LSBs (R1/R0, G1/G0, B1/B0) to GND. Grounding the wrong pair (MSBs) caps each channel at ~25 % → dark/no white.
- **Timing:** DCLK **25 MHz** (§3.2.4; 27 MHz max — pll-video0 ÷12 = 25.00). **SYNC-DE mode; HSYNC/VSYNC active-low, DE active-high.** Do NOT use a 33.3 MHz template.
- **DISP (FPC pin 31):** display-on input, no internal pull → **10 KΩ pull-up + a spare GPIO (PD22)**; floats dark otherwise.
- Wire onto `lcd_rgb666_pins` (PD0–21) + `tcon_lcd0_out` RGB endpoint; enable `&de`.

---

## 3. Motion (IMU) + Haptics

| # | Qty | Role | Part | LCSC # | Interface / net | Src | Fit | Note |
|---|-----|------|------|--------|-----------------|-----|-----|------|
| 35 | 1 | IMU (6-axis) | LSM6DSOXTR | C481766 | **i2c2** 0x6A + INT1/INT2 | ✅ | JLC | `st,lsm6dsox`. CS high = I2C (#38); SA0→GND = 0x6A (#39). **⚠️ INT1 must NOT be pulled to VDD_IO** (selects I3C-only = dead IMU) → 10 KΩ pulldown #40 + pinmux input-no-pull through boot. **Unused aux pins (Mode-1, DS Table 1 / Fig 24): SDx (pin 2) → GND, SCx (pin 3) → GND** — mandatory hard tie (must not float; no pull-R). **OCS_Aux (pin 10) = intentional NC** (do NOT tie — could perturb the aux path); **SDO_Aux (pin 11) = NC** (or VDDIO). INT2 (pin 9) → PE1 GPIO (programmable output, no tie). |
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
| 48 | 1 | Amp SD/MODE strap | RS-03K6803FT | C140074 | SD/MODE → 3.3 V | ✅ | JLC | 680 KΩ (#5) → (L/2+R/2) mono. **Mute-GPIO must be open-drain/hi-Z** (hi-Z = mono/run via this R; drive low = shutdown, for jack-detect). |
| 49 | 1 | Amp GAIN_SLOT strap | net (no part) | — (net) | GAIN_SLOT → VDD | ✅ | JLC | **VDD tie = 6 dB** (0 dBFS ≈ 0.8 W into 8 Ω, no clip). NOT 15 dB. |
| 50 | 1 | Amp VDD bypass | CC0603KRX7R9BB104 | C14663 | VDD | ✅ | JLC | 100 nF. |
| 51 | 1 | Amp VDD bulk | CL21A106KPFNNNE | C17024 | VDD | ✅ | JLC | 10 µF. |
| 52 | 1 | Speaker | Same Sky CES-2704-088L050 | DigiKey 10821313 | amp OUT± | ✅ | Hand | Ø27 mm, 8 Ω, 0.8 W, enclosed. **Leads solder direct to OUT± (no connector). Cap ALSA softvol to protect (0.8 W).** |

---

## 5. Battery — charge + power-path + fuel gauge (1S LiPo)

Topology: battery ↔ **BQ24074 power-path → SYS (~3.0–4.4 V)** → **TPS63021 buck-boost → 3.3 V** (sole 3.3 V source) + **FP6161 → 0.9 V core** (#3). USB-C 5 V (via CH224K) charges/supplements; does not power rails directly.

| # | Qty | Role | Part | LCSC # | Interface / net | Src | Fit | Note |
|---|-----|------|------|--------|-----------------|-----|-----|------|
| 53 | 1 | Power-path charger | BQ24074RGTR | C54313 | autonomous; /CHG,/PGOOD open-drain | ✅ | JLC | QFN-16-EP. System on **OUT = SYS** (OUT regulates ~4.4 V w/o cell). **CE→GND** (must not float). **🔴 EN1→GND, EN2→HIGH (strap to OUT via #60, NOT IN — EN abs-max 7 V) — else charger boots at 100 mA and ILIM is inert.** ISET #57, ILIM #58, TS #59. **EP → VSS with a via array** (heat path). |
| 54 | 1 | Charger IN cap | CL10A475KO8NNNC | C19666 | IN (pin 13) → VSS | ✅ | JLC | 4.7 µF **16 V** X5R (must stay <10 µF for USB-IF inrush; 16 V for OVP window). Place local. |
| 55 | 1 | Charger OUT cap | CL21A106KOQNNNE | C1713 | OUT (pins 10/11) → VSS | ✅ | JLC | 10 µF. **OUT = SYS node** → place close to IC. |
| 56 | 1 | Charger BAT cap | CL21A106KOQNNNE | C1713 | BAT (pins 2/3) → VSS | ✅ | JLC | 10 µF, near the JST-PH (#74). |
| 57 | 1 | ISET (charge current) | 0603WAF2001T5E | C22975 | ISET (pin 16) → VSS | ✅ | JLC | 2.0 kΩ 1% → ~445 mA (0.22C). **Mandatory — open = no charge.** |
| 58 | 1 | ILIM (input limit) | 0603WAF1201T5E | C22765 | ILIM (pin 12) → VSS | ✅ | JLC | 1.2 kΩ → ~1.34 A input. **Never below 1.1 kΩ** (IN abs-max 1.6 A). Active only when EN2=1/EN1=0 (#60). |
| 59 | 1 | TS NTC-defeat | RC0603JR-0710KL | C99198 | TS (pin 1) → VSS | ✅ | JLC | 10 kΩ — mandatory (2-wire cell, no NTC; '74 has no float-disable). |
| 60 | 1 | EN2 strap | 0 Ω 0603 | C21189 | EN2 (pin 5) → OUT/SYS; EN1 (pin 6) → GND | ✅ | JLC | Puts charger in ILIM-resistor mode. EN1→GND is a net. |
| 61 | 1 | USB-PD sink | CH224K | C970725 | CC1/CC2; CFG1 → 5 V | ✅ | JLC | ESSOP-10. Negotiates 5 V. **CC1/CC2 → this IC only — NO external 5.1 KΩ CC pulldowns.** CFG1→VBUS (#64) = 5 V; CFG2/CFG3→GND (nets). PG (pin 10) leave open (or pull-up to 3.3 V for "adapter present"). |
| 62 | 1 | CH224K VDD series R | 0603WAF1001T5E | C21190 | 5 V VBUS → VDD (pin 1) | ✅ | JLC | 1 kΩ (feeds the internal shunt-reg VDD). |
| 63 | 1 | CH224K VDD cap | CL10A105KB8NNNC | C15849 | VDD (pin 1) → GND | ✅ | JLC | 1 µF. |
| 64 | 1 | CH224K CFG1 strap | RC0603JR-0710KL | C99198 | CFG1 (pin 9) → VBUS | ✅ | JLC | 10 kΩ → 5 V request. |
| 65 | 1 | CH224K VBUS-sense R | RC0603JR-0710KL | C99198 | VBUS (pin 8) → 5 V rail | ✅ | JLC | 10 kΩ (hi-Z voltage sense). |
| 66 | 1 | 3.3 V buck-boost | TPS63021DSJR | C202140 | SYS → 3.3 V logic | ✅ | JLC | VSON-14-EP. Sole 3.3 V source. **EN ← STM6601 (#94, must not float); PS/SYNC→GND (#68); FB→VOUT (#67); VINA cap #69.** **L/C stability-critical — use the datasheet Table-1 combo (#71/#72).** EP → PGND pour + thermal vias. |
| 67 | 1 | Buck-boost FB tie | 0603WAF0000T5E | C21189 | FB (pin 3) → VOUT | ✅ | JLC | 0 Ω — fixed-version FB senses rail externally; float = no regulation. |
| 68 | 1 | Buck-boost PS/SYNC strap | 0603WAF0000T5E | C21189 | PS/SYNC (pin 13) → GND | ✅ | JLC | 0 Ω = power-save on (must not float). Placed 0 Ω keeps force-PWM option. |
| 69 | 1 | Buck-boost VINA bypass | CC0603KRX7R9BB104 | C14663 | VINA (pin 1) → GND | ✅ | JLC | 100 nF. **Hard upper limit 0.22 µF — no bulk cap here.** |
| 70 | 2 | Buck-boost input caps | CL21A106KOQNNNE | C1713 | VIN (pins 10/11) → PGND | ✅ | JLC | 2×10 µF 16 V, local to the converter pins. |
| 71 | 3 | Buck-boost output caps | CL21A226MAQNNNE | C45783 | VOUT (pins 4/5) → PGND | ✅ | JLC | 3×22 µF 25 V — stability-critical (Table-1). Min 2×22 µF. Place close. |
| 72 | 1 | Buck-boost inductor | MWSA0503S-1R5MT | C408407 | L1/L2 | ✅ | JLC | 1.5 µH, Isat 9 A (≥4.5 A floor). 5.4×5.2 mm — flag size for layout. |
| 73 | 1 | LiPo cell | Adafruit 2011 (2000 mAh) | — (Adafruit) | JST-PH 2-pin | ✅ | Hand | 3.7 V, integral PCM. **⚠️ Verify continuous ≥2 A + PCM trip >2 A on arrival.** ~60×36×7 mm — confirm enclosure fit. |
| 74 | 1 | Battery connector | S2B-PH-SM4-TB(LF)(SN) | C295747 | mates #73 | ✅ | JLC | JST-PH 2 mm, right-angle SMD. **⚠️ POLARITY: red = + → BQ24074 BAT (reversed = destroyed cell).** |
| 75 | 1 | Fuel divider R_top | 0603WAF1803T5E | C22827 | BAT → GPADC0 | ✅ | JLC | 180 kΩ. Backup voltage sense (4.2 V→1.50 V, under 1.8 V AVCC). |
| 76 | 1 | Fuel divider R_bot | 0603WAF1003T5E | C25803 | GPADC0 → GND | ✅ | JLC | 100 kΩ (ratio 0.357). |
| 77 | 1 | Fuel gauge (SoC %) | MAX17048G+T10 | C2682616 | i2c1 0x36 | ✅ | JLC | `maxim,max17048`, DFN-8-EP. **VDD (pin 3) → battery+ (power + cell-sense); CELL (pin 2) = NO-CONNECT; QSTRT (6) → GND; CTG (1) → GND; ALRT (5) → GPIO (opt, needs pull-up).** EP → GND. |
| 78 | 1 | MAX17048 VDD bypass | CL10A105KB8NNNC | C15849 | VDD (pin 3) → GND | ✅ | JLC | 1 µF at pin 3 (not CELL). |
| 79 | 1 | Charge-status LED | 19-213SYGC/S530-E2/5T | C2986027 | /CHG (open-drain) → LED → 3V3 | ✅ | JLC | Lit while charging (0 GPIO). |
| 80 | 1 | Charge-LED resistor | 0603WAF5100T5E | C23193 | LED limit | ✅ | JLC | 510 Ω. |
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
| 92 | 1 | Soft power button | ALPS SKRTLBE010 | C127481 | STM6601 PB (#94) **AND** native SoC EINT | ✅ | JLC | `KEY_POWER`. **EINT MUST be a native SoC pin, not the expander** (must wake/power-on from off). |
| 93 | 2 | L/R bumper tactiles | ALPS SKRTLBE010 | C127481 | 2× expander pin → GND | ✅ | JLC | `KEY_L1/KEY_R1`. 100k-cycle — upgrade to 500k/1M (ALPS SKHHLNA010/SKHHLQA010) in a later rev if they wear. |
| 94 | 1 | Push-button power controller | STM6601CA2BDM6F | C109022 | PB←#92; EN(pin9)→TPS63021 EN; PSHOLD(pin4)↔GPIO | ✅ | JLC | TDFN-12. **`C`** = active-high EN, long-push deasserts (true off). **VCC → always-on SYS** (NOT the switched 3.3 V). PSHOLD = `gpio-poweroff` handshake. Caps #95/#96. EN push-pull (no pull-up); RST/INT/VCCLO/PBOUT/SR/CSRD leave open. |
| 95 | 1 | STM6601 VCC decoupling | CC0603KRX7R9BB104 | C14663 | VCC (pin 1) → GND | ✅ | JLC | 100 nF, close to device. |
| 96 | 1 | STM6601 VREF cap (CREF) | CL10A105KB8NNNC | C15849 | VREF (pin 3) → GND | ✅ | JLC | 1 µF — mandatory even though VREF unused. Place close. |

**§6 note — expander budget:** 10 gamepad + 2 volume + 2 bumpers = 14/16 pins (power button is a native EINT, not on the expander). 2 spare. All-tactile → HASL fine (no ENIG). Run i2c1 ≥400 kHz, stay interrupt-driven (never poll).

---

## 7. Shared bus / cross-cutting

| # | Qty | Role | Part | LCSC # | Interface / net | Src | Fit | Note |
|---|-----|------|------|--------|-----------------|-----|-----|------|
| 97 | 2 | I2C pull-ups (SDA/SCL) | 0603WAF1501T5E | C22843 | i2c1 SDA/SCL → 3V3 | ✅ | JLC | **1.5 KΩ** (10 K/4.7 K fail the 400 kHz rise-time). Add DNP parallel-R pads to tune; floor ~1 kΩ. Measure bus C on first article if the button board cable is long. |
| 98 | 1 | USB2 D+/D− ESD array | USBLC6-2SC6 | C7519 | USB-C D+/D− + VBUS clamp | ✅ | JLC | SOT-23-6, ~3.5 pF. Optional 5 V VBUS TVS (SMAJ5.0A) as DNP. |

---

## 8. Audio-out + Bluetooth (Tier-2)

| # | Qty | Role | Part | LCSC # | Interface / net | Src | Fit | Note |
|---|-----|------|------|--------|-----------------|-----|-----|------|
| 99 | 1 | Bluetooth module | Ezurio BT830-SA-01 | DigiKey (not JLC) | **UART3 HCI** + VREG_EN_RST# + 3.3 V | ✅ | Hand | CSR8811, integrated antenna, BlueZ (`btattach`). **Fit=Hand — no JLC footprint → draw a custom symbol+footprint** (castellated, pads extend outward). Antenna end overhangs board edge, keep-out ≥20 mm from metal/battery/LCD. See integration note. |
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
- **PCM5102A (#103):** **SCK(12)→GND** (MCLK-less internal PLL; float/high = dead). FMT(16)/FLT(11)/DEMP(10)→GND. **XSMT(17)→a GPIO** (not hard-high; soft-mute the DAC when headphones unplugged). **Constraint: run I2S1 at 64fs (or 32fs), fs ≥ 16 kHz** or the DAC PLL won't lock.
- **TPA6132A2 (#110):** inverting single-ended — DAC → #108 RC → #111 coupling cap → INL−(1)/INR−(4); INL+(2)/INR+(3) → GND. OUTL(16)/OUTR(5) → jack tip/ring direct (no caps); SGND(15) → sleeve. G0(6)=G1(7)→GND (−6 dB). EP → GND + vias (never VDD). Cap ALSA softvol max (amp clips into 16 Ω at full DAC scale).
- **Audio routing (software):** speaker amp #47 + DAC #103 share I2S1 (both receivers) → route by enable/disable. Jack detect (#115) → `gpio-keys SW_HEADPHONE_INSERT`; IN → speaker off (SD low) + HP on (EN high); OUT → reverse. **3 native GPIOs: jack-detect (in), speaker-SD (open-drain out), HP-EN (out).** Analog HP audio has no external ESD array (relies on the amp's ±8 kV); route HP traces away from switchers/backlight/RF, clean SGND return.

---

## Schematic & routing notes

**Power tree:** battery ↔ BQ24074 → **SYS (~3.0–4.4 V)** → TPS63021 buck-boost → **3.3 V** (sole source) + FP6161 → **0.9 V core**. Internal LDOs make 1.8 V + 1.5 V-DDR from 3.3 V. Rails to widen/thermal: 3.3 V (~1.1 A worst) + SYS. Exposed-pad thermal-via arrays on **FP6161 (#3), TPS63021 (#66), BQ24074 (#53), MAX98357A (#47)**. 4-layer minimum.

**Pinmux (mainline `pinctrl-sun20i-d1`; 72 bonded GPIO, PD bank full):**
- RGB666 LCD = **PD0-21**, DISP = **PD22** · SPI0-NOR = **PC2-5** · microSD = **PF0-5** · UART0 console = **PE2/3**
- **BT HCI = UART3 PG0-3** (NOT UART1 — its flow-control PG6-9 clashes i2c1) · I2S1 = **PG12/13/15** · i2c1 (touch/haptics/expander/gauge/INA) = **PG8/9** · IMU **i2c2 = PE4/5**
- USB0 (USB-C) dedicated balls; USB1 unused. **Never use phantom D1 pins** PC0/1, PB0/1/8-12, PE14-17, PG16-18.
- Native single GPIOs needed: PCA9555-INT, power-btn EINT, touch INT+RST, jack-detect, speaker-SD, HP-EN, backlight-CTRL, XSMT, /PGOOD, ALRT, DISP.

**I2C address map (collision-free):**
- **i2c1:** PCA9555 `0x20` · MAX17048 `0x36` · FT7311 touch `0x38` · INA226 `0x40` · DRV2605L `0x5A`
- **i2c2:** LSM6DSOX `0x6A`

**Routing rules:**
- **RGB666:** `drive-strength=<10>`; DNP series-R footprint on **DCLK** (0 Ω stuffed); route the 22-line bus over solid GND; keep the ribbon away from GPADC/touch/I2S. **Confirm the bit-map/MSB order before committing copper.**
- Keep **I2S clocks (PG12/13) away from the slow i2c1 pair (PG8/9)** — ground trace between; unused I2S MCLK (PG11) = static GPIO.
- **Kelvin-route** the INA226 shunt (#82) and the Rset ground (#33).
- Route analog headphone traces away from switchers / backlight / RF.
- Design-for-debug: test points on every rail (5V/3.3V/0.9V/1.8V/1.5V/~19V-BL); 0 Ω in series on boost + amp feeds (lift to meter); DNP bulk pads on 3.3 V.

**Open items — resolve before routing (need vendor data / bench test):**
1. **RGB bit-to-pin map + MSB order** (Zettler, or bench-test the panel) — copper-committing.
2. **FPC contact-side** for #26/#27 (physically confirmed bottom-contact; test-fit before locking footprints).
3. **FT7311** I2C address (0x38 assumed) + report format — confirm on panel via `i2cdetect`.
4. **Vybronics LRA (#46)** coil impedance ≥8 Ω + max drive vs DRV OD_CLAMP.
5. **Adafruit 2011 cell (#73)** continuous ≥2 A + PCM trip >2 A.

**Symbol library** (`../../../cad/symbols/easyeda2kicad.*`): 59/62 parts fetched. Missing = **C2481 (SS16), C23179 (470 Ω), C33353 (2.2 nF)** — their footprints (SMA, R0603, C0603) are already present; re-fetch to match by LCSC#, or reuse the generic symbol. **BT830 (#99) needs a hand-drawn symbol+footprint.**
