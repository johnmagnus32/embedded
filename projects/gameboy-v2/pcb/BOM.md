# gameboy-v2 PCB — Bill of Materials

**Status:** Parts-only, **75 rows: 75 ✅ — FULLY SOURCED.** All parts confirmed live on JLCPCB / DigiKey — incl. the §8 **LCD 5 V boost** (MT3608 + supports, for the 2090's VIN = 5 V) and the §9 **HSE crystal + 15 pF loads** (Bluetooth clock accuracy); row #20 SD_MODE resistor = 620 kΩ / C23219. **Ready for schematic
capture.** The integrated **gameboy-v2 handheld**: STM32F411RE (game logic, input, audio, FPGA config) +
iCE40UP5K FPGA (PPU) driving an ILI9341 over an 8-bit parallel 8080 bus, with battery, Bluetooth, haptics,
motion, and console-feel buttons.

**Feature set:**
- **STM32F411RE** (Cortex-M4, bare-metal RTOS) + **iCE40UP5K FPGA** (PPU → ILI9341 over 8080; screen hangs off the FPGA)
- **Screen:** Adafruit 2090 (2.8" ILI9341), 8-bit 8080, on removable 2.54 mm headers
- **Audio:** MAX98357A I2S Class-D amp + speaker (6 dB gain)
- **Haptics:** DRV2605L + Vybronics 235 Hz LRA · **Motion:** LSM6DSOX 6-axis IMU
- **NOR flash:** W25Q128 (on SPI3, shared with the FPGA-config bus)
- **Bluetooth:** Ezurio BT830-SA-01 (CSR8811, UART-HCI on USART1 4-wire, BLE-only host)
- **Battery:** 1S LiPo (Adafruit 2011, 2000 mAh), USB-C charge + power-path, buck-boost 3.3 V rail; % via a divider → STM32 ADC
- **Buttons:** up to 13 all-tactile via a PCA9555 I2C expander (0 STM32 GPIO), + L/R bumpers
- **Power on/off:** single slide switch in the SYS system-power path (true off, charge-while-off preserved)
- **Programming:** ST-Link V2 (SWD) header; **UART header** on USART2 (PA2/PA3) for debug

**Src legend:** ✅ = exact part confirmed good on JLCPCB (or proven on gameboy-v1 / ice40-breakout / breadboard).
⚠️ = candidate not yet confirmed (currently none — all rows ✅). **Fit:** JLC = SMT, JLCPCB reflows it ·
Hand = external module / THT you attach yourself.

> **Straps and nets are not separate rows** — mandatory ties (iCE40 config straps, IMU CS/SA0, DRV IN/TRIG,
> GAIN_SLOT→VDD, TPS63021 VINA→VIN, BT enable, direct-solder speaker/LRA pads) live in the owning part's Note.

---

## 1. Core — MCU + FPGA + power + clock + debug headers

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 1 | 1 | MCU / game logic / FPGA-config master | STM32F411RET6 | C94355 | SPI1 PPU, SPI3 cfg+NOR, I2S2, I2C1, USART1/2, ADC1_IN0 | ✅ | JLC | Cortex-M4, LQFP-64, 512 KB flash / 128 KB RAM, ~49 usable GPIO. VDD 1.7–3.6 V from the 3.3 V rail; integrated LDO makes the 1.2 V core (single **VCAP_1** cap). VDDA filtered from VDD via a ferrite (#5). HSI 16 MHz, no HSE crystal. Bare-metal RTOS — board.dts is a pin map, not a runtime binding. **VBAT (pin 1) → tie to VDD** (no coin cell — must not float) + local 100 nF. |
| 2 | 1 | FPGA / PPU (renders to ILI9341 8080) | iCE40UP5K-SG48I | C2678152 | SPI-slave config + runtime SPI1 cmd + 8-bit 8080 LCD out | ✅ | JLC | QFN-48 7×7 mm, 5.3K LUT. **Core VCC 1.14–1.26 V (abs-max 1.42 V) — MUST come from the 1.2 V LDO (#4), never 3.3 V.** VPP_2V5 ≥2.30 V (SB_HFOSC in use) → tie it + all three VCCIO banks to 3.3 V (matches STM32 I/O). Internal SB_HFOSC ~24 MHz. Exposed pad = ONLY ground → thermal via array. STM32 configures it as SPI-slave at boot (bitstream in STM32 flash). **PPU bitstream must constrain the config MISO pin SPI_SO (IOB_32a, the shared PC11 net) to input/high-Z in user mode** so it can't contend with the W25Q128 SO on SPI3 reads (#3). **Assign LCD_RST + LCD_LITE (backlight) to Bank-0 spare GPIOs in a v2-specific `fpga/gameboy-v2.pcf`** (e.g. RST=pin 23 IOT_37a, LITE=pin 35 IOT_46b_G0 — push-pull; NOT the open-drain RGB pins 39/40/41). Both are required by §2 (#15/#17) but are absent from every current .pcf (v2 reuses ice40-breakout's). Keep the 6 config pins package-fixed (SPI_SCK=15/SI=17/SO=14/SS_B=16, CRESET=8, CDONE=7); Bank 2 is fully spare. Order early — non-swappable, Extended tier. |
| 3 | 1 | NOR flash (audio/asset store) | W25Q128JVSIQ | C97521 | SPI3 shared bus (PC10/11/12), /CS = PB0 | ✅ | JLC | 128 Mbit SOIC-8. **Multi-drops SPI3 with the iCE40 config path** (SCK PC10, MOSI PC12, MISO PC11); separate CS (NOR /CS = PB0, FPGA SS_B = PB6), shared PC11 MISO net. "IQ" = QE=1 → hard-tie IO2 (/WP, pin 3) / IO3 (/HOLD, pin 7) to VCC (defined level). 100 nF at VCC. **Firmware: hold PB0 HIGH during FPGA config and PB6 HIGH during NOR access** — CS mutual-exclusion. **HW backstop added: 10 kΩ pull-UP on /CS (PB0) → NOR defaults deselected through POR/reset/fault (#10)**; pair with the FPGA SPI_SO high-Z constraint (#2). |
| 4 | 1 | iCE40 1.2 V core LDO (VCC + VCCPLL) | AP2127K-1.2TRG1 | C151376 | 3.3 V → 1.2 V; feeds iCE40 VCC×2 + VCCPLL | ✅ | JLC | SOT-23-5, 300 mA fixed 1.2 V. Fed from the 3.3 V rail (~2.1 V headroom). 4.7 µF in + 4.7 µF out. **⚠️ CRITICAL: EN/SHUTDOWN (pin 3) MUST tie to VIN/3.3 V** — active-high enable with an internal 3 MΩ pull-DOWN, so floating = LDO OFF → no 1.2 V core → **dead FPGA/screen**. Direct tie or 10–100 kΩ pull-up to 3V3. Pin 4 = NC (fixed version). |
| 5 | 2 | Ferrite bead (VCCPLL + VDDA filter) | GZ1608D601TF | C1002 | iCE40 1.2 V→VCCPLL; STM32 VDD→VDDA | ✅ | JLC | 600 Ω @ 100 MHz 0603. Two instances: iCE40 1.2 V→VCCPLL (+100 nF + 4.7 µF) and STM32 VDD→VDDA. |
| 6 | ~13 | Per-pin decoupling | CC0603KRX7R9BB104 (100 nF 0603 X7R) | C14663 | one per supply pin | ✅ | JLC | iCE40 ×7 (VCC×2, VCCIO_0/1/2, VPP_2V5, VCCPLL), STM32 VDD ×4 (one per VDD pin) + VBAT + NRST filter, W25Q128 VCC. Shared reel across the board. |
| 7 | ~6 | Bulk (VCAP_1 + iCE40 rails) | CC0603KRX5R6BB475 (4.7 µF 0603 X5R) | C109456 | STM32 VCAP_1 + VDD bulk; iCE40 LDO in/out + 1.2 V bulk + VCCPLL | ✅ | JLC | 4.7 µF. Shared reel (BT830 §7 reuses it). |
| 8 | 1 | VDDA HF bypass | CC0603KRX7R9BB103 (10 nF 0603) | C100042 | STM32 VDDA HF | ✅ | JLC | HF bypass on VDDA, downstream of the VDD→VDDA ferrite. VDDA is the ADC reference (battery %) — keep it clean. |
| 9 | 1 | VDDA bulk | CC0603KRX5R7BB105 (1 µF 0603) | C106215 | STM32 VDDA bulk | ✅ | JLC | 1 µF, pairs with #8's 10 nF through the ferrite. ADC-reference stability. |
| 10 | ~7 | 10 kΩ pulls (BOOT0/CRESET/CDONE/SS_B/NOR-CS + others) | RC0603JR-0710KL | C99198 | see note | ✅ | JLC | Shared 10 kΩ reel. Core uses 5: **BOOT0 pull-DOWN** (boot from main flash), **CRESET_B pull-UP** (no internal pull-up), **CDONE pull-UP** (open-drain + LED), **SS_B pull-DOWN** (slave-boot strap — SS_B idles HIGH = master boot; STM32 drives PB6 low at CRESET release; **do NOT add a pull-up**), **NOR /CS pull-UP** (PB0 → 3V3 — HW backstop so the W25Q128 defaults deselected on the shared SPI3 bus through POR/reset/fault; #3). NRST needs only the 100 nF filter (internal pull-up). Reel also feeds the IMU INT1 pulldown, charger TS-defeat, expander INT pull-up. |
| 11 | 2 | LED current-limit | 0603WAF5100T5E (510 Ω 0603) | C23193 | CDONE LED + status LED | ✅ | JLC | ~2.5–3 mA. CDONE "config OK" LED + power/status LED. |
| 12 | 2 | Green LED | 19-213SYGC/S530-E2/5T | C2986027 | CDONE indicator + power/status | ✅ | JLC | 0603 green. CDONE lights at FPGA user mode. |
| 13 | 1 | SWD programming/debug header | Pin Header 1x5 2.54 mm | C358687 | 3V3 / SWDIO(PA13) / SWCLK(PA14) / GND / NRST | ✅ | Hand | ST-Link V2 pinout. Sole flashing/debug path. THT → hand-solder. |
| 14 | 1 | UART debug/console header | Pin Header 1x3 2.54 mm | C49257 | TX / RX / GND on **USART2 (PA2/PA3)** | ✅ | Hand | Debug + flash-upload. **Console stays on USART2** (USART1 is Bluetooth) — do NOT revert to USART1. USART2 is the ST-Link VCP line. |

> **iCE40 config bus (from ice40-breakout):** SPI3 SCK/MOSI/MISO (PC10/11/12) + CRESET_B (PB1), CDONE (PB2),
> SS_B (PB6); CRESET 10 K pull-up, SS_B 10 K pull-**down** (slave-boot strap), CDONE 10 K pull-up + LED, **NOR /CS
> (PB0) 10 K pull-up** (HW backstop → NOR defaults deselected). STM32 streams the bitstream at boot. The W25Q128
> shares SPI3 — the driver must arbitrate CS (#3), and the PPU bitstream must leave SPI_SO high-Z in user mode (#2).

---

## 2. Screen — Adafruit 2090 ILI9341, 8-bit parallel 8080 (FPGA-driven)

The panel hangs off the iCE40 PPU over an **8-bit 8080 bus** (LCD_D0–D7 + WR + DC + CS, RD tied high, RST
from an FPGA GPIO). **The screen uses zero STM32 pins** — the STM32 only feeds sprite data to the FPGA over
SPI1. Module is the Adafruit 2090 (breadboard-confirmed in 8080 mode), mounted removable on 2.54 mm headers.

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 15 | 1 | LCD module (parallel-capable) | **Adafruit 2090** — 2.8" ILI9341 TFT breakout v2 | — (Adafruit) | 8-bit 8080 ← iCE40 (D0–7 + WR + DC + CS + RD + RST + LITE) | ✅ | Hand | Breadboard-confirmed in 8080 mode. Onboard 3.3 V LDO + level shifters + backlight driver (so the bare-die VCI/IM/EXTC rules don't apply). **Feed VIN from 5 V** (breadboard-proven at VIN = 5 V — the onboard AMS1117 makes the 3.3 V logic and the backlight runs off VIN via the LITE transistor; VIN < ~4.5 V starves the AMS1117 and the LEDs). **⚠️ v2 has NO 5 V rail → a SYS→5 V boost must be ADDED for the LCD (see the backlight note below + Open items).** Backlight via the **LITE** pin (enable/PWM from a spare iCE40 GPIO). Removable via the header mount (#16). Pinout: Adafruit breakout-v2 learn page + ice40-breakout §3c. |
| 16 | 1–2 | LCD connector (removable mount) | HX PM2.54-1x20P ZC (1x20 female, square-hole) | C41417332 | carries D0–7 / WR / DC / CS / RD / RST / LITE / **VIN(5V)** / GND | ✅ | Hand | Female sockets on the mainboard, male pins on the 2090 → the screen unplugs. THT → hand-solder. **Supply net = VIN (5 V from the §8 boost)**; the 2090's 3Vo pin is its AMS1117 *output* → leave NC (do not drive from the 3.3 V rail). Match the footprint (row count/order) to the 2090 header before locking. |
| 17 | ~2 | RD / RST straps | 10 kΩ 0603 (RD→3V3) + RST→spare FPGA GPIO | C99198 | static straps at the module | ✅ | JLC | **RD (RDX) tied HIGH to 3.3 V** — the FPGA only writes. **RST (RESX) from a spare iCE40 GPIO** so the PPU can issue a proper reset pulse (≥10 µs low; wait ≤120 ms after reset before ILI9341 init). CS is FPGA-driven. |
| 18 | ~2 | Local decoupling at module | 100 nF (C14663) + 10 µF (C1713) | C14663 / C1713 | VIN / logic bypass at the connector | ✅ | JLC | 100 nF at the module supply pins + a 10 µF bulk near the connector to hold the panel's charge-pump rails steady during frame writes. Shared reels. |

> **Backlight — RESOLVED (breadboard confirms VIN = 5 V):** the 2090 runs from **VIN = 5 V** — its onboard
> AMS1117 makes the 3.3 V logic and the panel's 4 parallel white LEDs (Vf ≤3.5 V, ~**240 mA total**) run off VIN
> via the LITE transistor. **⚠️ v2 has NO 5 V rail** (battery→SYS→3.3 V buck-boost; 5 V exists only at USB-C VBUS
> when plugged). **So v2 MUST add a small SYS→5 V boost (~300 mA) feeding the LCD VIN** — a NEW part (boost IC +
> inductor + caps), on the SYS domain, downstream of the slide switch (#55) so it dies at true-off. Do NOT feed
> VIN from bare SYS (3.0–4.4 V starves the AMS1117 at a low cell + dims the LEDs below Vf) or from the 3.3 V rail.
> *(Alternative, more efficient but 2 nets: bypass the AMS1117 — feed the 3.3 V logic pin from the switched 3.3 V
> rail + drive the backlight with a dedicated constant-current boost from SYS. The single 5 V boost matches the
> proven breadboard exactly.)* Panel interface confirmed: ILI9341, 8-bit 8080 (IM[3:0]=0000 on-board), IOVCC/VCI 3.3 V max.

---

## 3. Audio — I2S Class-D amp + speaker

MAX98357A mono Class-D on I2S2, VDD from SYS (battery domain), driving a Same Sky CES-2704 enclosed speaker.

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 19 | 1 | Class-D amp (mono) | MAX98357AETE+T | C910544 | I2S2 PB12(WS)/PB13(SCK)/PB15(SD) | ✅ | JLC | TQFN-16, no MCLK (recovers from BCLK) → 3-wire I2S2. VDD on SYS (2.5–5.5 V; VDD/I2S pins +6 V-rated). UVLO 1.8/2.3 V = clean mute on dips. **GAIN_SLOT → VDD (net) = 6 dB** (15 dB clips on a 1S rail). **Exposed pad → GND pour + thermal vias.** Firmware: start LRCLK within ½ BCLK period; use 8/16/32/44.1/48 kHz; never stop LRCLK while BCLK runs; feed 16-bit PCM. |
| 20 | 1 | SD/MODE strap (mono L/2+R/2) | 620 kΩ 1% 0603 (0603WAF6203T5E) | C23219 | SD_MODE → **3.3 V logic rail** (NOT SYS) | ✅ | JLC | SD_MODE → 3.3 V via **620 kΩ → pin ≈ 0.46 V, centered in the (L/2+R/2) mono window 0.24–0.65 V** (datasheet R_LARGE = 222.2·V_DDIO − 100 = 634 kΩ; any 600–650 kΩ lands mid-window). ~170–210 mV margin to the 0.24 V shutdown (B0) trip, vs only ~30 mV worst-case with the old 1 MΩ (RPD 92 kΩ min + tol + rail dip; trips specified at 25 °C only). Reference the **stable 3.3 V rail, not SYS**. Boot-safe: internal RPD holds it low until 3.3 V is up. UNI-ROYAL, **Extended tier** (one-time loading fee); confirmed live on JLC. |
| 21 | 1 | Amp VDD HF bypass | CC0603KRX7R9BB104 (100 nF) | C14663 | MAX98357A VDD → GND | ✅ | JLC | HF decoupling at VDD; also filters Class-D switching current off SYS. |
| 22 | 1 | Amp VDD bulk | CL21A106KPFNNNE (10 µF 0805) | C17024 | MAX98357A VDD → GND | ✅ | JLC | VDD bulk — sources pulsed Class-D output current. Single VDD pin group (no separate "PVDD"). 10 V rating (lowest-rated SYS part, OK). |
| 23 | 1 | Speaker (8 Ω enclosed) | Same Sky CES-2704-088L050 | DigiKey 10821313 | amp OUTP/OUTN (BTL, solder leads) | ✅ | Hand | Ø27×4.9 mm, 8 Ω, 0.8 W / 1 W max, enclosed, wire leads. Matches the 6 dB gain (2.53 Vrms). **Cap RTOS master volume ≤0.8 W continuous.** **Direct-solder the leads to two OUT± lands (no connector).** |

> **Software:** the MAX98357A has no HW volume → RTOS softvol + the ≤0.8 W cap. Fade-before-stop + clean I2S
> teardown for pops. Class-D output stays **filterless** (optional DNP ferrite + 1 nF EMC pads only).

---

## 4. Motion (IMU) + Haptics + shared I2C bus

LSM6DSOX IMU (0x6A), DRV2605L haptic driver (0x5A), and the PCA9555 expander (0x20, §6) share **STM32 I2C1
(PB8/PB9)**.

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 24 | 1 | IMU (6-axis) | LSM6DSOXTR | C481766 | I2C1 0x6A (PB8/PB9) + INT1→PB10 | ✅ | JLC | LGA-14. VDD/VDDIO to 3.3 V (no 5 V tolerance). Use INT-driven data-ready (INT1→PB10) + FIFO watermark, not fast polling. RTOS: WHO_AM_I(0x0F)=0x6C, CTRL1_XL/CTRL2_G, OUTX/Y/Z. **Mandatory strap nets:** CS(pin12)→3V3 selects I2C (else boots SPI, never answers); SA0(pin1)→GND sets addr 0x6A; **SDx(pin2) + SCx(pin3) → GND** (Mode-1 I2C-slave: the unused master/aux pins must not float). INT2(pin9): leave NC (defaults disabled) or route to a spare EXTI; OCS_Aux/SDO_Aux: soldered-NC. |
| 25 | 1 | IMU INT1 anti-I3C-latch pulldown | 10 kΩ 0603 | C99198 | INT1(pin4) → GND | ✅ | JLC | **10 kΩ pulldown at INT1 + PB10 GPIO input-no-pull through boot** — a pull-up/driven-high INT1 at POR latches the part I3C-only = permanently dead IMU (F411 has no I3C). |
| 26 | 1 | IMU VDD decoupling | 100 nF 0603 | C14663 | VDD(pin8) → GND | ✅ | JLC | Datasheet-required decoupling. |
| 27 | 1 | IMU VDDIO decoupling | 100 nF 0603 | C14663 | VDDIO(pin5) → GND | ✅ | JLC | Same 3.3 V net as VDD. |
| 28 | 1 | Haptic driver | DRV2605LDGSR | C527464 | I2C1 0x5A (PB8/PB9), EN→3V3 | ✅ | JLC | VSSOP-10, closed-loop LRA driver, motor straight to OUT± (no FET/flyback). **VDD on 3.3 V, not SYS** (its 3.3 V logic pins would exceed VDD+0.3 if VDD sagged under a motor pulse). RTOS: EN high → wait 0x5A ACK → load the LRA resonant config → GO. **Mandatory strap net:** IN/TRIG→GND = I2C-only mode (floating → spurious playback). EN→3V3 or a GPIO to power-gate. **VDD/NC (pin 6) → tie to the VDD net** (datasheet-preferred; the optional 2nd supply pin). |
| 29 | 1 | Haptic REG cap (LDO stability) | 1 µF 0603 X5R (CL10A105KB8NNNC) | C15849 | REG → GND | ✅ | JLC | **MANDATORY** — the internal LDO on REG needs ~1 µF low-ESR within a few mm or it oscillates and the part never enumerates on I2C. |
| 30 | 1 | Haptic VDD HF bypass | 100 nF 0603 | C14663 | VDD → GND | ✅ | JLC | HF bypass at VDD. |
| 31 | 1 | Haptic VDD local bulk | 10 µF 0805 | C1713 | VDD → GND | ✅ | JLC | Local bulk — sources the pulsed motor/overdrive current. |
| 32 | 1 | Actuator (LRA) | Vybronics VG0832022D — Ø8 mm coin LRA | DigiKey 9974288 | DRV2605L OUT± | ✅ | Hand | 235 Hz resonant, 1.8 Vrms, 22.5 Ω, 90 mArms max, wire leads (RED=+, BLUE=−). **Record 235 Hz → the DRV2605L LRA config** (resonant-freq register / drive-time). 3.3 V develops the 1.8 Vrms across OUT±. **Direct-solder the leads to OUT± (no connector).** |
| 33 | 2 | Shared I2C pull-ups (SDA + SCL) | 1.5 kΩ 1% 0603 (0603WAF1501T5E) | C22843 | I2C1 SDA(PB9)/SCL(PB8) → 3V3 | ✅ | JLC | 1.5 kΩ meets the 400 kHz rise-time budget (10 kΩ fails; 2.2 kΩ also OK on v2's short bus). Add DNP parallel pads to tune. |

> **I2C address map (clash-free):** LSM6DSOX 0x6A · DRV2605L 0x5A · PCA9555 0x20. **Escape bus:** I2C3
> (PA8/PB4) is free if BT-EN stays off PA8; I2C2's only SCL pin (PB10) is taken by the IMU INT.

---

## 5. Battery + Charging + Power-Path + on/off

**BQ24074 power-path → SYS → TPS63021 buck-boost → 3.3 V; 3.3 V → AP2127K → iCE40 1.2 V core.** LCD module VIN
+ MAX98357A VDD hang off SYS. No USB-PD sink (plain CC, ~500 mA input); battery % via a resistor divider →
STM32 ADC (no fuel-gauge IC).

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 34 | 1 | USB-C receptacle (5 V in) | TYPE-C-31-M-12 | C165948 | VBUS/GND (power-only) | ✅ | JLC | 16-pin, 5 V in, **power-only** (D+/D− not routed — STM32 flashes via SWD/UART). VBUS → IN cap → BQ24074 IN. **VBUS ESD/surge** covered by the 28 V-tolerant / 10.5 V-OVP charger IN + the 4.7 µF IN cap; optional **DNP** 5 V VBUS TVS (SMAJ5.0A) pad only if the exposed port shows ESD flakiness. Full 16-pin USB-2 connector (rated 5 A/20 V; D+/D−/SBU present but unrouted); SMD contacts + THT shield/retention posts. **Bond the shell/mid-plate + both retention posts to GND** (unbonded shell hurts ESD immunity); net all 4 VBUS + all 4 GND contacts. |
| 35 | 2 | USB-C CC pulldowns | 5.1 kΩ 1% 0603 | C105580 | CC1/CC2 → GND | ✅ | JLC | Rd = 5.1 kΩ each → advertises a ≤500 mA (SDP) / up-to-1.5 A Type-C sink. This is v2's whole CC network (no PD sink). |
| 36 | 1 | Power-path charger | BQ24074RGTR | C54313 | autonomous DPPM; /CHG,/PGOOD open-drain | ✅ | JLC | QFN-16-EP. System runs from **OUT/SYS** (works with the cell absent — OUT ≈ 4.4 V). CE→GND. TMR/ITERM float (5 h / 10 %). **Exposed pad → VSS via array** (heat path). RTOS reads /CHG + /PGOOD as GPIO for a charge indicator; charging is autonomous. **/CHG (pin 9) + /PGOOD (pin 7) are open-drain → each needs a 10 kΩ pull-up to 3.3 V** (reuse the 10 kΩ reel #10), or enable the STM32 internal pull-up on those GPIOs. |
| 37 | 1 | EN1/EN2 strap (mandatory) | 0 Ω 0603 (EN2→OUT) + EN1→GND net | C21189 | EN2(pin5)→OUT/SYS; EN1(pin6)→GND | ✅ | JLC | **MANDATORY: EN1→GND + EN2→OUT** = ILIM-resistor mode. Both float to (0,0) = 100 mA input with the ILIM resistor inert otherwise. Strap EN2 to the always-on **OUT** node (3.4–4.4 V < 7 V abs-max), **never IN**. |
| 38 | 1 | Charger IN cap | CL10A475KO8NNNC (4.7 µF/16 V) | C19666 | IN(pin13) → VSS | ✅ | JLC | IN bypass 1–10 µF, **<10 µF** (USB-IF inrush). 16 V (IN sees the OVP window). |
| 39 | 1 | Charger OUT cap (SYS) | CL21A106KOQNNNE (10 µF/16 V) | C1713 | OUT(pins10/11) → VSS | ✅ | JLC | OUT = the SYS node (feeds the buck-boost + LCD VIN + amp) — place close. 16 V. Also the input-bulk brownout guard. |
| 40 | 1 | Charger BAT cap | CL21A106KOQNNNE (10 µF/16 V) | C1713 | BAT(pins2/3) → VSS | ✅ | JLC | BAT bypass. Place near the JST-PH. |
| 41 | 1 | Charge-current resistor (ISET) | 0603WAF2001T5E (2.0 kΩ 1%) | C22975 | ISET(pin16) → VSS | ✅ | JLC | **MANDATORY** (open ISET = no charging). ICHG = KISET/RISET → ~445 mA (~0.22C). DPPM tapers below this on a weak input. 1% required. Cell datasheet confirms 0.2C standard / **1C (2 A) max** charge → 0.22C is safe; keep ≤0.5C anyway since TS is defeated (no over-temp cutoff). |
| 42 | 1 | Input-limit resistor (ILIM) | 0603WAF3301T5E (3.3 kΩ 1%) | C22978 | ILIM(pin12) → VSS | ✅ | JLC | IIN = KILIM/RILIM → ~462–488 mA (at/under the 500 mA SDP ceiling). **Populate it — open ILIM disables all charging.** Stay ≥1.1 kΩ. Sets only the input *ceiling*; charge-while-play is automatic (DPPM). For faster charge-while-play on a known ≥1.5 A source, ~1.1–1.2 kΩ (no VIN-DPM back-off in resistor mode → can brown a weak brick). |
| 43 | 1 | TS NTC-defeat resistor | RC0603JR-0710KL (10 kΩ) | C99198 | TS(pin1) → VSS | ✅ | JLC | **MANDATORY** — the 2-wire cell has no NTC, so TS floats → "too cold" → charging suspended. 10 kΩ → 0.75 V, mid-window. |
| 44 | 1 | 3.3 V buck-boost | TPS63021DSJR (fixed 3.3 V) | C202140 | SYS → 3.3 V logic | ✅ | JLC | VSON-14-EP, fixed 3.3 V (**C202140**). 1S cell straddles 3.3 V → buck-boost mandatory. ~1.9–2.4 A at the 3.0 V floor. **EN = the on/off control point (slide switch #55).** Inductor + output caps MUST be the datasheet Table-1 combo. **Exposed pad → PGND + thermal vias.** |
| 45 | 1 | Buck-boost FB tie | 0603WAF0000T5E (0 Ω) | C21189 | FB(pin3) → VOUT | ✅ | JLC | Fixed-version FB MUST tie to VOUT (0 Ω/net) — the internal divider senses through it. Float = no regulation. |
| 46 | 1 | Buck-boost PS/SYNC strap | 0603WAF0000T5E (0 Ω) | C21189 | PS/SYNC(pin13) → GND | ✅ | JLC | Must not float. LOW = power-save (~25 µA Iq). Placed 0 Ω keeps the force-PWM option. |
| 47 | 1 | Buck-boost VINA bypass + **VIN tie** | CC0603KRX7R9BB104 (100 nF) | C14663 | VINA(pin1) → GND (cap) **AND VINA → VIN/SYS (net)** | ✅ | JLC | **VINA (pin 1) = the UVLO-sensed control-stage supply — MUST be tied to VIN/SYS** (same input rail as pins 10/11), or the converter sits in permanent UVLO → **dead 3.3 V rail**. The 100 nF is only a bypass (**hard limit 0.22 µF**) — it cannot power VINA. ⚠️ TI's datasheet Fig 7/28 *dropped the drawn VINA→VIN wire* (Rev G) — misleading; VINA must still connect to VIN. Direct copper net (or 0 Ω link). |
| 48 | 2 | Buck-boost input cap | CL21A106KOQNNNE (10 µF/16 V) | C1713 | VIN(pins10/11) → PGND | ✅ | JLC | 2×10 µF local to VIN/PGND (at the converter pins). **16 V required** (SYS 4.2 V + DC-bias derate). |
| 49 | 3 | Buck-boost output cap | CL21A226MAQNNNE (22 µF/25 V) | C45783 | VOUT(pins4/5) → PGND | ✅ | JLC | 3×22 µF (min 2), **stability-critical** — must land in the datasheet Table-1 L/C matrix. Place close to VOUT/PGND. |
| 50 | 1 | Buck-boost inductor | MWSA0503S-1R5MT (1.5 µH) | C408407 | L1/L2 | ✅ | JLC | 1.5 µH shielded, Isat **7.2 A guaranteed / 9 A typ**, DCR 25 mΩ (≫ the ~4 A switch limit). 5.2×5.4×2.8 mm — flag footprint. Matches the TPS63021 Table-1 "typical application" combo (1.5 µH + 3×22 µF). |
| 51 | 2 | Battery-sense divider (fuel gauge) | 0603WAF1003T5E (100 kΩ 1%) | C25803 | **BAT node** → PA0 tap → GND | ✅ | JLC | Two 100 kΩ 1% from the **cell/BAT node** (NOT SYS/OUT — DPPM-regulated ~4.4 V reads the charger, not SoC) to GND; tap → PA0 (ADC1_IN0). VREF 3.3 V: 4.2 V→2.10 V, 3.0 V→1.50 V. Firmware: raw×6600/4095 → mV → OCV LUT + IIR. **Reads high while charging** (I_charge×ESR) → firmware gates the % on /CHG or /PGOOD. 1% (ratio = SoC accuracy). |
| 52 | 1 | Battery-ADC filter/reservoir | CC0603KRX7R9BB104 (100 nF) | C14663 | PA0 → GND | ✅ | JLC | Filters divider noise + charges the ADC S/H cap (50 kΩ source Z). Firmware: long ADC sample time (≥480 cycles). |
| 53 | 1 | LiPo cell | Adafruit 2011 — 1S 2000 mAh, JST-PH, integral PCM | — (Adafruit) | JST-PH → BQ24074 BAT | ✅ | Hand | ~5.5–6.5 h screen-on. 3.7 V nom (4.2/3.0), integral PCM. PKCELL LP-803860: 0.2C std / **1C (2 A) max charge**, 2 A continuous discharge (> board peak). **~60 × 38.5 × 8.5 mm max → confirm enclosure fit.** PCM trip thresholds unpublished (R5402N101KD IC). **Verify polarity** (red = + → BAT). |
| 54 | 1 | Battery connector | S2B-PH-SM4-TB(LF)(SN) — JST-PH 2.0 mm 2-pin SMD R/A | C295747 | mates the cell → BQ24074 BAT | ✅ | JLC | 2 A. **Polarity: + pad = Adafruit red; reversed = destroyed cell.** Distinct pitch from speaker/LRA. |

**On/off — single slide switch in the SYS system-power node:**

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 55 | 1 | Power switch (SYS system-power) | XKB SS-12D10L5 — SPDT THT slide, 3 A / 125 V, 13×6.8 mm | C319012 | in series: charger OUT/SYS → **system node** (buck-boost VIN + LCD 5 V-boost in + amp VDD) | ✅ | Hand | **In the SYS→system-power path** so one mechanical switch cuts every switched load → a true "off". The **charger sits upstream** → charge-while-off preserved. **Buck-boost EN ties to the post-switch node** (EN follows VIN → on when powered; never floats). 3 A rating vs a real SYS peak ≈ **1.3–1.8 A** (LCD 5 V-boost input ~0.6 A + buck-boost input ~0.4 A at a 3.0 V cell + MAX98357A ~0.6 A audio peak; ~1 A sustained backlight-on) → ~1.7× margin, not 6×. **Size the SYS copper (charger OUT→switch→boost/buck-boost/amp) for ≥1.8 A.** **Mount at the top/side edge (Game-Boy-style)** — lever through a shell slot; hand-solder or JLC wave-solder. **Confirm the 13×6.8 mm edge fit + lever-travel direction vs the shell slot** — L-knob 5 mm / 2.2 mm travel, 3 THT pins @ 4.7 mm pitch, non-shorting, 10 k cycles; the 3 A rating is AC-spec (fine at 4.4 V DC). |

---

## 6. Controls — up to 13 all-tactile buttons via PCA9555 I2C expander

All buttons hang off the **PCA9555** on the shared I2C1 bus → **buttons cost 0 STM32 GPIO**. 12 buttons (13
with a soft-power option) fit the 16 pins with spares. All-tactile → HASL is fine; internal expander pull-ups
→ no per-button resistors.

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 56 | 1 | GPIO expander (all buttons) | PCA9555PWR | C2864778 | I2C1 (PB8/PB9) base 0x20 + INT | ✅ | JLC | TI TSSOP-24. **Strap A0/A1/A2 → GND for 0x20** (must not float). All 16 pins INPUTS at POR. Internal ~100 kΩ pull-up per input → **no external per-button pull-ups** (a tactile just ties pin→GND). VCC 3.3 V. **INT erratum (§8.4.1.1):** after each input read, re-point the command byte to a non-00h register before another slave read; add a low-rate poll safety net. |
| 57 | 4 | D-pad tactiles (U/D/L/R) | TS-1187A-B-A-B | C318884 | 4× expander pin → GND | ✅ | JLC | XKB 6×6 mm top-actuated, 1.6 N, 100k cycles. Diagonals work (expander reads all pins); a shell rocker keycap restores roll. Software debounce. |
| 58 | 4 | Face tactiles (A/B/X/Y) | TS-1187A-B-A-B | C318884 | 4× expander pin → GND | ✅ | JLC | Same part as the D-pad. Top-actuated. |
| 59 | 2 | Start / Select tactiles | TS-1187A-B-A-B | C318884 | 2× expander pin → GND | ✅ | JLC | Same part, on the expander. |
| 60 | 2 | Volume +/− tactiles | ALPS SKRTLBE010 (side-actuated SMD) | C127481 | 2× expander pin → GND | ✅ | JLC | 4.5×3.4 mm, 1.6 N. RTOS maps the edges to software volume. |
| 61 | 2 | L / R bumper tactiles | ALPS SKRTLBE010 | C127481 | 2× expander pin → GND | ✅ | JLC | Same side-actuated part, edge-mounted. Only 100k cycles — upgrade to a higher-life part (ALPS SKHHLNA010/C125031 = 500k) in a later rev if they wear. |
| 62 | 1 | Expander INT pull-up | RC0603JR-0710KL (10 kΩ) | C99198 | PCA9555 INT (open-drain) → 3V3 | ✅ | JLC | 10 kΩ → 3V3. INT → a native STM32 EINT so the button task is interrupt-driven, not polled. |
| 63 | 1 | PCA9555 VDD decoupling | CC0603KRX7R9BB104 (100 nF) | C14663 | VDD(pin24) → GND | ✅ | JLC | HF decoupling at the expander VDD. |

> **Expander pin budget:** D-pad 4 + face 4 + Start/Select 2 + Volume 2 + L/R 2 = **12 of 16** (4 spare). No
> power button (the slide switch is a hard power switch, not a GPIO). STM32-side cost = PB8/PB9 (I2C) + 1 EINT
> for INT. **Daughterboard option:** put the expander + switches on the button board on a 4-wire cable
> (SDA/SCL/INT/3V3+GND); keep the INT pull-up mainboard-side.

---

## 7. Bluetooth — Ezurio BT830-SA-01 (CSR8811, UART-HCI)

BT830 on **USART1 4-wire HCI** — the only STM32F411 USART with hardware CTS/RTS whose pins are free.

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 64 | 1 | Bluetooth module (onboard) | Ezurio BT830-SA-01 (CSR8811, integrated antenna) | DigiKey | UART-HCI 4-wire → USART1 (PA9/PA10/PA11/PA12) + EN + 3.3 V | ✅ | Hand | Dual-mode, Class-1 +7 dBm, integrated ceramic antenna (no trace, just a keep-out). Castellated pads → hand-solder. **VDD_PADS(pin1) + VREG_IN_HV(pin9) on the regulated 3.3 V rail, NOT SYS** (recommended 3.0–3.6 V). **Enable:** VREG_EN_RST#(pin8) ← STM32 GPIO **PC1** push-pull (boot low, then high >5 ms); NOT a 10 kΩ strap (internal pull-down holds it < VIH). Keep BT-EN off PA8 to preserve I2C3. |
| 65 | 1 | VREG_OUT_HV stability cap (mandatory) | CC0603KRX5R6BB475 (4.7 µF X5R) | C109456 | VREG_OUT_HV(pin10) → GND | ✅ | JLC | **MANDATORY ≥1.5 µF** low-ESR at the internal HV-LDO output — without it the LDO oscillates and the module won't enumerate. **Cap-to-GND only** (abs-max 2.0 V). |
| 66 | 2 | VREG_IN_HV + VDD_PADS decoupling | CC0603KRX7R9BB104 (100 nF) | C14663 | pin 9 + pin 1 → GND | ✅ | JLC | 100 nF ×2, one at each supply pin. |
| 67 | 1 | 3.3 V bulk / RF-burst reservoir | CC0603KRX5R6BB475 (4.7 µF X5R) | C109456 | 3.3 V feed near pin 9 → GND | ✅ | JLC | 4.7 µF near the module's 3.3 V input — supplies Class-1 TX-slot bursts locally. |

> **BT830 integration:** (1) **UART** RX/TX/CTS/RTS → USART1 4-wire; cross-wire STM32_RTS→module_CTS,
> module_RTS→STM32_CTS (module pins have weak internal pull-ups → idles "not-clear-to-send" safely).
> (2) **Layout:** module at a board edge, antenna end overhanging, **no-copper keep-out on all layers**,
> metal/battery ≥20 mm clear. (3) **Firmware:** the CSR8811 is controller-only → the RTOS runs its own
> **BLE-only host** (NimBLE/BTstack) over H4 (flow control present). No Linux/BlueZ; no A2DP (audio stays
> speaker-only).

---

## 8. LCD 5 V boost — NEW (the 2090 needs VIN = 5 V; logically part of §5 power)

Breadboard-proven at **VIN = 5 V**, but v2 has no 5 V rail → a small **SYS→5 V boost** feeds the LCD VIN.
Load ≈ 300 mA (AMS1117 + 3.3 V logic + ~240 mA backlight). On the SYS domain, **downstream of the slide
switch (#55)** for true-off. **MT3608 boost — all 6 rows (68–73) confirmed live on JLC.**

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 68 | 1 | 5 V boost regulator | **MT3608** (Aerosemi, SOT-23-6) | **C84817** | SYS → 5 V, ~300 mA | ✅ | JLC | Async boost, 1.2 MHz, 2 A internal switch, 0.6 V FB ref. SYS (3.0–4.2 V) → 5 V (SYS < 5 V always, so it always boosts). **Needs an external Schottky (#73)** (SW→diode→VOUT); the FB divider (#72) sets 5 V. **EN → VIN** (tie high for always-on; no internal pull) — since the whole boost is post-switch, it powers up with the board. Confirmed live on JLC (Extended tier). |
| 69 | 1 | Boost inductor | Sunlord SWPA4030S4R7MT (4.7 µH, 4×4 mm) | C57269 | MT3608 SW/L | ✅ | JLC | 4.7 µH, **2 A Irms / 3.2 A Isat**, 78 mΩ DCR, 4×4 mm. MT3608 typical value @ 1.2 MHz; worst-case peak I_L ≈ 0.75 A (3.0 V cell) → Isat 3.2 A = ~4× steady-state margin. *(The MT3608's "2 A" is its switch/output-current spec, not the current-limit trip — the real trip [needs-datasheet-confirmation]; brief startup saturation is non-destructive with cycle-by-cycle limiting.)* |
| 70 | 1 | Boost input cap | CL21A106KOQNNNE (10 µF/16 V) | C1713 | boost VIN → GND | ✅ | JLC | Reuse the 10 µF reel, local to the boost VIN (on SYS). |
| 71 | 1 | Boost output cap | CL21A226MAQNNNE (22 µF/25 V) | C45783 | boost 5 V → GND | ✅ | JLC | Reuse the buck-boost 22 µF reel (#49). MT3608 typical Cout = 22 µF; X5R, ≥10 V. Place close to VOUT. |
| 72 | 2 | Boost FB divider | R1 73.2 kΩ 1% (0603WAF7322T5E) + R2 10 kΩ 1% | R1 = C14890 / R2 = C99198 | FB → 5 V / GND | ✅ | JLC | Sets 5 V from the MT3608's 0.6 V FB ref: VOUT = 0.6·(1+R1/R2); **73.2 k / 10 k → 4.99 V** ✓ (~60 µA divider current). R2 reuses the 10 kΩ reel (#10, C99198); R1 = 73.2 kΩ 1% (75 V, ample). |
| 73 | 1 | Boost rectifier (Schottky) | SS34 (MDD, 3 A/40 V, SMA/DO-214AC) | C8678 | MT3608 SW → 5 V out | ✅ | JLC | **Required by the async MT3608** (SW→diode→VOUT). 3 A/40 V ≫ our ~0.6 A and V_R = 5 V; Vf 550 mV@3 A (lower at our current). **JLC Basic tier** (no placement fee). |

> **Alternative (no 5 V rail):** bypass the 2090's AMS1117 — feed its 3.3 V logic pin from the switched 3.3 V
> rail + drive the backlight with a dedicated constant-current boost from SYS (2 nets). The single 5 V boost
> above matches the proven breadboard exactly.

---

## 9. HSE crystal — STM32 clock accuracy (for reliable Bluetooth)

Replaces the ±4 %-over-temp HSI as the PLL source. **0 GPIO cost** — OSC_IN/OSC_OUT are the dedicated PH0/PH1
pins. Added because the BT830 HCI UART framing can't tolerate the HSI's ±4 % over temp (audio pitch also
stabilizes). **Firmware clock-init contract (record in the RTOS init):** switch the PLL source HSI→HSE;
e.g. M=8, N=200, P=2 → 100 MHz SYSCLK (AHB /1, APB1 /2, APB2 /1). Enable **PLLI2S** for the I2S BCLK (the
MAX98357A needs no MCLK) — **48 kHz is exact from an 8 MHz HSE** (set the PLLI2S divisors per ST's clock
config; verified achievable). `board.dts` already carries the `hse` node. Reserve a **0 Ω DNP series pad on
OSC_OUT** (AN2867 gain-limit option) at capture.

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 74 | 1 | HSE crystal | KDS 1C208000BC0R (8 MHz, CL = 12 pF, ±20 ppm, SMD3225-4P) | C57131 | PH0/PH1 (OSC_IN/OSC_OUT) | ✅ | JLC | 8 MHz → PLL /M=8 → 1 MHz PLL input (in ST's 1–2 MHz range). Fundamental, ±20 ppm. **CL = 12 pF → 15 pF load caps (#75).** No external feedback R (F411 internal); optional **0 Ω series Rext on OSC_OUT** per AN2867. |
| 75 | 2 | Crystal load caps | YAGEO CC0402JRNPO9BN150 (15 pF, NP0, ±5%, 50 V, 0402) | C106997 | OSC_IN/OSC_OUT → GND | ✅ | JLC | C_load ≈ 2·(CL − C_stray) = 2·(12 − ~5) ≈ **15 pF** for the CL = 12 pF crystal (#74). NP0/C0G; ±5 % is fine for crystal loads. **Treat as tunable** (F411 pin+board stray can reach ~10 pF combined → 10–15 pF range) and **verify 8 MHz on bring-up.** |

---

## Pin budget (F411 LQFP64, ~49 usable GPIO)

**~28 of ~49 GPIO used, ~20 spare.** Tight on *function* (every hardware-flow-control USART and every spare
I2C bus is consumed), not on count. Buttons cost 0 GPIO (expander). Verified against the F411 AF table.

| Subsystem | Pins | Which |
|-----------|------|-------|
| iCE40 PPU (SPI1) | 4 | PA4 (CS) / PA5 (SCK) / PA6 (MISO, write-only → reclaimable) / PA7 (MOSI) |
| iCE40 config + NOR (SPI3, shared) | 7 | PC10/11/12 + PB0 (NOR /CS) + PB6 (SS_B) + PB1 (CRESET) + PB2 (CDONE) |
| Audio (I2S2) | 3 | PB12 (WS) / PB13 (CK) / PB15 (SD) |
| Shared sensor + button I2C (I2C1) | 2 | PB8 (SCL) / PB9 (SDA) — IMU + haptics + PCA9555 |
| IMU INT1 | 1 | PB10 (EXTI) — forecloses I2C2_SCL |
| PCA9555 INT | 1 | PC0 (EXTI) — must differ from the IMU EXTI line |
| Bluetooth (USART1 4-wire) | 4 | PA9 (TX) / PA10 (RX) / PA11 (CTS) / PA12 (RTS) |
| Bluetooth EN | 1 | PC1 (keeps PA8/I2C3 free) |
| Battery ADC | 1 | PA0 (ADC1_IN0) |
| Console/debug UART (USART2) | 2 | PA2 (TX) / PA3 (RX) |
| SWD | 2 | PA13 / PA14 |
| Buttons (12–13) | 0 | all on the PCA9555 expander |
| Power on/off | 0 | slide switch in the SYS power path (#55) — not a GPIO |
| **Total** | **~28** | of ~49 (BOOT0/NRST are dedicated, non-GPIO) |

**Layout/firmware rules:**
1. **No spare SPI** — SPI1 = PPU, I2S2 = audio, SPI3 = FPGA-config + NOR (shared). New peripherals go on I2C / UART / GPIO only.
2. **USART1 = Bluetooth** (only clean HW-flow-control UART; USART2_CTS is on PA0/ADC, USART6 has none). **Console stays on USART2 (PA2/PA3)** — do not revert per the stale board.dts TODO.
3. **Buttons on the PCA9555 expander** → 0 GPIO.
4. **Kill the 5 V rail** — LCD + amp run from SYS; buck-boost makes 3.3 V; AP2127K makes the iCE40 1.2 V core. The slide switch (#55) in the SYS power path gives a true "off"; the amp self-gates (SD_MODE on the switched rail).
5. **Battery divider taps the BAT/cell node** (not SYS/OUT), VREF = 3.3 V.
6. **All-tactile buttons → HASL** (no ENIG); no per-button pull-ups.
7. **SPI3 CS mutual-exclusion** — never assert NOR /CS + FPGA SS_B at once (firmware); now backed in HW by a 10 kΩ pull-up on NOR /CS (PB0, #3/#10) + the PPU bitstream leaving SPI_SO high-Z in user mode (#2).
8. **Keep PA8 (I2C3_SCL) free** (BT-EN on PC1) → preserves the one hardware I2C escape bus.

---

## Power tree

- **Battery ↔ BQ24074 power-path → SYS (~3.0–4.4 V, not 5 V).**
- **SYS → TPS63021 buck-boost → 3.3 V logic** (STM32, iCE40 I/O, NOR, sensors, expander, BT830, LCD logic).
- **3.3 V → AP2127K LDO → 1.2 V iCE40 core.**
- **On SYS directly:** the **LCD 5 V boost** (feeding the 2090 VIN + its ~240 mA backlight), MAX98357A VDD, charge current — all downstream of the slide switch (#55) except the charger, which is upstream (charge-while-off).
- When plugged, USB-C 5 V feeds the charger IN (≤500 mA plain CC) and charges/supplements; SYS stays at battery voltage.

**3.3 V rail load ≈ 200–300 mA worst (estimate — MEASURE the SYS rail on silicon);** the buck-boost's
~1.9–2.4 A gives large margin (no DDR / RGB panel). All SYS-domain caps ≥10 V (SYS reaches 4.4 V).

---

## Open items + bring-up

- **LCD 5 V boost — RESOLVED** (§8, rows 68–73): the 2090 is breadboard-proven at **VIN = 5 V** and v2 has no
  5 V rail, so a **SYS→5 V MT3608 boost** (gated by the slide switch #55) feeds the LCD VIN. Fully sourced.
- **HSE crystal — DECIDED: add it** (§9, #74/#75): an 8 MHz crystal + 2 load caps on PH0/PH1 (0 GPIO cost)
  replaces the ±4 %-over-temp HSI as the PLL source, so the BT830 HCI framing is reliable over temperature.
  **Follow-up (not BOM):** add an `hse` node to `board.dts` + switch the RTOS PLL source HSI→HSE.
- **BT830 antenna keep-out** (layout): datasheet §12.1.1 = 40 mm top/bottom, 30 mm L/R — the 60×36 mm LiPo will
  violate it. Corner-mount the module, keep the battery/shield out of that quadrant, validate range in-enclosure.
- **`fpga/gameboy-v2.pcf`** (FPGA tooling): assign LCD_RST + LCD_LITE to Bank-0 spares (v2 reuses
  ice40-breakout.pcf, which lacks them); keep config pins package-fixed.
- **Verify-on-arrival:** Adafruit 2011 charge C-rate; enclosure fit for the 2011 (~60 × 38.5 × 8.5 mm — see row 53) + the slide switch (13×6.8 mm edge).
- **Layout-only (intentionally not BOM rows):** mounting holes; test points (SYS, 3V3, 1V2, VBAT, SWD, GND); DNP pads for the OSC_OUT series R (#74) + an optional USB VBUS TVS. Reserve these footprints at capture.
- **Deep-discharge behavior:** on a fully depleted cell, USB-only power won't light the LCD until the cell charges past the BQ24074 DPPM/supplement region — expected, not a fault.
- **Firmware items surfaced by the hardware:** SPI3 CS arbitration (#3); PCA9555 INT-erratum workaround (#56);
  battery-% gating during charge (#51); long ADC sample time (#52); BLE host stack (§7); RTOS softvol +
  ≤0.8 W cap (§3); IMU I3C-avoid pin config (#25).
- **Bring-up order:** power + iCE40 config (CDONE LED) + NOR + SWD/UART console → screen (8080 solid
  background) → buttons (`evtest`-style diagonals) → audio (verify no clipping at 6 dB) → sensor I2C (IMU +
  haptics) → battery telemetry (ADC divider) → Bluetooth (BLE HCI on USART1) → measure true-off current.

## Related docs
- `../board.dts` — v2 pin map (console stays on USART2).
- `../../gameboy/gameboy-pcb/BOM.md` — v1 (proven parts reused here).
- `../../ice40-breakout/BOM.md` + `README.md` — iCE40 core/power + 8080 LCD reference.
- `../../gameboy-v3/pcb/BOM.md` — sibling board; source of the shared feature parts (charger, buck-boost, IMU, haptics, LRA, PCA9555, BT830, battery, speaker).
- Datasheets: `../../../cad/docs/` (STM32F411RE, iCE40UP5K, ILI9341, MAX98357A, LSM6DSOX, DRV2605L, Vybronics, BQ24074, TPS63021, PCA9555, BT830, W25Q128, CES-2704).
