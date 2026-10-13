# gameboy-v2 PCB — Bill of Materials

**Status:** Parts-only, **81 rows: 81 ✅ — fully sourced** (#76 TMR 68 kΩ = C23231, confirmed live 2026-09-25; #78 reuses the confirmed 3.3 kΩ C22978; #43 NTC = C2892547 and #79 33 Ω = C23140 confirmed live 2026-09-28; #55 MSK12C02-HB = C431541 + #80 AO3401A = C15127 confirmed live 2026-09-29). 2026-09-28 review fixes applied: SS_B 3.3 kΩ (#78), LCD RD → +5V_LCD, TPS63021 PS/SYNC under firmware control (#46, PC3 — was PC7), footprint fixes (#34/#54/#64), TS battery-temperature NTC (#43), LCD_WR series termination (#79). 2026-09-29: GBA-style side-edge power switch driving a P-FET (#55/#80/#81); face buttons A/B only. Incl. the §8 **LCD 5 V boost** (MT3608 + supports, for the 2090's VIN = 5 V) and the §9 **HSE crystal + 15 pF loads + 0 Ω
series R** (Bluetooth clock accuracy); row #20 SD_MODE resistor = 620 kΩ / C23219. **Capture-readiness audit (2026-09-25) fixes
applied:** LITE → STM32 PC6, RD tied direct to 3V3, exact quantities, 2090 module facts from Adafruit's Eagle files, TMR timer
resistor. **Schematic-capture CSVs written** (`gb2/gb2-bom.csv` + `gb2/gb2-nets.csv`, 2026-09-26: 121 parts, 568 pin rows, adversarially reviewed); next = run `cad/generate_schematic.py` + ERC on a machine with `kicad-cli`. The integrated **gameboy-v2 handheld**: STM32F411RE (game logic, input, audio, FPGA config) +
iCE40UP5K FPGA (PPU) driving an ILI9341 over an 8-bit parallel 8080 bus, with battery, Bluetooth, haptics,
motion, and console-feel buttons.

**Feature set:**
- **STM32F411RE** (Cortex-M4, bare-metal RTOS) + **iCE40UP5K FPGA** (PPU → ILI9341 over 8080; screen hangs off the FPGA)
- **Screen:** Adafruit 2090 (2.8" ILI9341), 8-bit 8080, on removable 2.54 mm headers
- **Audio:** MAX98357A I2S Class-D amp + speaker (6 dB gain)
- **Haptics:** DRV2605L + Vybronics 235 Hz LRA · **Motion:** LSM6DSOX 6-axis IMU
- **NOR flash:** W25Q128 (on SPI3, its own bus — the FPGA moved to SPI1 2026-10-12)
- **Bluetooth:** Ezurio BT830-SA-01 (CSR8811, UART-HCI on USART2 4-wire, BLE-only host)
- **Battery:** 1S LiPo (Adafruit 2011, 2000 mAh), USB-C charge + power-path, buck-boost 3.3 V rail; % via a divider → STM32 ADC
- **Buttons:** 12 all-tactile (D-pad, A/B, Start/Select, Vol±, L/R bumpers) via a PCA9555 I2C expander (0 STM32 GPIO)
- **Power on/off:** GBA-style side-edge slide switch driving a high-side P-MOSFET in the SYS system-power path (true off, charge-while-off preserved)
- **Programming:** ST-Link V2 (SWD) header on the top edge east of the USB-C; the USART1 (PA9/PA10) console on back-side test pads under the MCU

**Src legend:** ✅ = exact part confirmed good on JLCPCB (or proven on gameboy-v1 / ice40-breakout / breadboard).
⚠️ = candidate not yet confirmed (currently none). **Fit:** JLC = SMT, JLCPCB reflows it ·
Hand = external module / THT you attach yourself.

> **Straps and nets are not separate rows** — mandatory ties (iCE40 config straps, IMU CS/SA0, DRV IN/TRIG,
> GAIN_SLOT→VDD, TPS63021 VINA→VIN, BT enable, direct-solder speaker/LRA pads) live in the owning part's Note.
>
> **No DNP:** the schematic flow (`cad/generate_schematic.py`) places every part, so there are no do-not-populate pads —
> optional parts are either fitted or dropped.

---

## 1. Core — MCU + FPGA + power + clock + debug headers

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 1 | 1 | MCU / game logic / FPGA-config master | STM32F411RET6 | C94355 | SPI1 FPGA config + PPU, SPI3 NOR, I2S2, I2C1, USART1/2, ADC1_IN10 | ✅ | JLC | Cortex-M4, LQFP-64, 512 KB flash / 128 KB RAM, 50 GPIO on LQFP64 (48 after the HSE takes PH0/PH1). VDD 1.7–3.6 V from the 3.3 V rail; integrated LDO makes the 1.2 V core (single **VCAP_1** cap). VDDA filtered from VDD via a ferrite (#5). Clock: **8 MHz HSE crystal on PH0/PH1 (§9)** is the PLL source (HSI 16 MHz is only the reset default). Bare-metal RTOS — board.dts is a pin map, not a runtime binding. **VBAT (pin 1) → tie to VDD** (no coin cell — must not float) + local 100 nF. |
| 2 | 1 | FPGA / PPU (renders to ILI9341 8080) | iCE40UP5K-SG48I | C2678152 | SPI-slave config + runtime cmd on the same SPI1 pins + 8-bit 8080 LCD out | ✅ | JLC | QFN-48 7×7 mm, 5.3K LUT. **Core VCC 1.14–1.26 V (abs-max 1.42 V) — MUST come from the 1.2 V LDO (#4), never 3.3 V.** VPP_2V5 ≥2.30 V (SB_HFOSC in use) → tie it + all three VCCIO banks to 3.3 V (matches STM32 I/O). Clock: SB_HFOSC 48 MHz (undivided) → SB_PLL40_CORE → 20 MHz system clock (the PLL is in use, so the VCCPLL filter #5 matters). Exposed pad = ONLY ground → thermal via array. STM32 configures it as SPI-slave at boot (bitstream in STM32 flash). **One SPI1 bus for configuration and runtime (2026-10-12):** the STM32's SPI1 (PA5 SCK / PA7 MOSI, PA4 select) drives the config pins SPI_SCK 15 / SPI_SI 17 / SPI_SS 16; after configuration those pins are general I/O (datasheet §5.1.3) and the PPU gateware uses them as its command bus. **The runtime select is ACTIVE-HIGH** (it shares SPI_SS's 3.3 kΩ pull-down strap #78, so it idles low at no cost). SPI_SO 14 is NC. **v2 FPGA pin map (freeze in a new `fpga/gameboy-v2.pcf`, from ice40-breakout.pcf = the as-built truth):** command bus on the config pins SCK=15 / MOSI=17 / select=16 (active-high; was CS=12/SCK=11/MOSI=10 until 2026-10-12, now spare — spi_cmd.v still samples SCK with the system clock, so no GBIN needed); LCD D0–D7 = 35/34/32/31/28/27/26/25 (**reversed vs ice40-breakout** so the bus runs from U2's J3-facing side to J3 with zero crossings, 2026-10-03; D0 36→35 2026-10-10), WR=37, DC=42, CS=43; **LCD_RST = pin 36 (IOT_48b), driven open-drain** (37→36 and WR 38→37 on 2026-10-10 so RST leaves from the J3 side and only WR has to turn the board's inner corner below U2) (moved from pin 23 so it leaves beside WR/DC/CS instead of crossing the data lines) (drive 0 to reset, high-Z to release — the 2090 has its own APX803 reset supervisor + 10 kΩ pull-up on RST; add SWRESET 0x01 + a 120 ms wait to the PPU init). *(ice40-breakout README §3c's 43/38/34/31 LCD table is stale.)* **LITE is NOT an FPGA pin** — the 2090 holds it at ~4.47 V, above the iCE40's 3.6 V abs-max → it's on STM32 PC6 (#15). All other user I/O → NC. Keep the 6 config pins package-fixed (SPI_SCK=15/SI=17/SO=14/SS_B=16, CRESET=8, CDONE=7); Bank 2 is fully spare. Order early — non-swappable, Extended tier. |
| 3 | 1 | NOR flash (audio/asset store) | W25Q128JVSIQ | C97521 | SPI3 (PC10/11/12), /CS = PD2 | ✅ | JLC | 128 Mbit SOIC-8. **Alone on SPI3** (SCK PC10, MOSI PC12, MISO PC11; /CS PD2) since the FPGA moved to SPI1 (2026-10-12). "IQ" = QE=1 → hard-tie IO2 (/WP, pin 3) / IO3 (/HOLD, pin 7) to VCC (defined level). 100 nF at VCC. **10 kΩ pull-UP on /CS (PD2) → NOR defaults deselected through POR/reset/fault (#10).** |
| 4 | 1 | iCE40 1.2 V core LDO (VCC + VCCPLL) | AP2127K-1.2TRG1 | C151376 | 3.3 V → 1.2 V; feeds iCE40 VCC×2 + VCCPLL | ✅ | JLC | SOT-23-5, 300 mA fixed 1.2 V. Fed from the 3.3 V rail (~2.1 V headroom). 4.7 µF in + 4.7 µF out. **⚠️ CRITICAL: EN/SHUTDOWN (pin 3) MUST tie to VIN/3.3 V** — active-high enable with an internal 3 MΩ pull-DOWN, so floating = LDO OFF → no 1.2 V core → **dead FPGA/screen**. Direct tie or 10–100 kΩ pull-up to 3V3. Pin 4 = NC (fixed version). |
| 5 | 2 | Ferrite bead (VCCPLL + VDDA filter) | GZ1608D601TF | C1002 | iCE40 1.2 V→VCCPLL; STM32 VDD→VDDA | ✅ | JLC | 600 Ω @ 100 MHz 0603. Two instances: iCE40 1.2 V→VCCPLL (+100 nF + 4.7 µF) and STM32 VDD→VDDA. |
| 6 | 14 | Per-pin decoupling | CC0603KRX7R9BB104 (100 nF 0603 X7R) | C14663 | one per supply pin | ✅ | JLC | iCE40 ×7 (VCC×2, VCCIO_0/1/2, VPP_2V5, VCCPLL), STM32 VDD ×4 (one per VDD pin) + VBAT + NRST filter, W25Q128 VCC = **14**. Shared reel: board total C14663 = 25 (the other 100 nF parts have their own rows; +1 = the Q1 soft-start cap C51, #82). |
| 7 | 6 | Bulk (VCAP_1 + iCE40 rails) | CC0603KRX5R6BB475 (4.7 µF 0603 X5R) | C109456 | STM32 VCAP_1 + VDD bulk; iCE40 LDO in/out + 1.2 V bulk + VCCPLL | ✅ | JLC | 4.7 µF ×6: STM32 VCAP_1 + VDD bulk, iCE40 LDO in + out, 1.2 V bulk, VCCPLL. Shared reel (BT830 #65/#67 reuse it → board total 8). |
| 8 | 1 | VDDA HF bypass | CC0603KRX7R9BB103 (10 nF 0603) | C100042 | STM32 VDDA HF | ✅ | JLC | HF bypass on VDDA, downstream of the VDD→VDDA ferrite. VDDA is the ADC reference (battery %) — keep it clean. |
| 9 | 1 | VDDA bulk | CC0603KRX5R7BB105 (1 µF 0603) | C106215 | STM32 VDDA bulk | ✅ | JLC | 1 µF, pairs with #8's 10 nF through the ferrite. ADC-reference stability. |
| 10 | 6 | 10 kΩ pulls (BOOT0/CRESET/CDONE/NOR-CS + /CHG + /PGOOD) | RC0603JR-0710KL | C99198 | see note | ✅ | JLC | Shared 10 kΩ reel. Core uses 4: **BOOT0 pull-DOWN** (boot from main flash), **CRESET_B pull-UP** (no internal pull-up), **CDONE pull-UP** (open-drain + LED), **NOR /CS pull-UP** (PD2 → 3V3 — HW backstop so the W25Q128 defaults deselected through POR/reset/fault; #3). NRST needs only the 100 nF filter (internal pull-up). **+ 2 charger-status pull-ups:** BQ24073 /CHG → PC1 and /PGOOD → PC2, each 10 kΩ → 3V3 (#36) = **6**. (The SS_B pull-down is now 3.3 kΩ — #78.) The reel also feeds #25 (IMU INT1), #72 (boost FB R2) and #82 (Q1 soft-start R31), each in its own row → board total C99198 = 9 (#62, the expander INT pull-up, was removed 2026-10-10). |
| 78 | 1 | SS_B pull-down (slave-boot strap) | 0603WAF3301T5E (3.3 kΩ 1%) | C22978 | iCE40 SPI_SS (pin 16) / PA4 → GND | ✅ | JLC | **SS_B idles HIGH = master boot, so it must read low when CRESET_B releases at power-up** (CRESET_B has a pull-up, so the FPGA self-starts before firmware runs). The iCE40 weak pull-up is up to 128 µA at VCCIO 3.3 V: 10 kΩ would sit at up to 1.28 V (above LVCMOS33 VIL 0.8 V); **3.3 kΩ → ≤ 0.42 V**, guaranteed low. Firmware still drives PA4 low at every configure (`ice40.c`). In user mode PA4 is the PPU command-bus select, active-high, so it idles LOW with this pull-down (no standing current; an active-low select would burn ~1 mA) (2026-10-12). **Do NOT add a pull-up.** Same part as #42 (board total C22978 = 2). *(Numbered 78 to keep existing row numbers stable.)* |
| 11 | 2 | LED current-limit | 0603WAF5100T5E (510 Ω 0603) | C23193 | CDONE LED + power LED | ✅ | JLC | ~2.5–3 mA. CDONE status LED (+3V3 → 510 Ω → LED anode, cathode on CDONE, as ice40-breakout: **lit = FPGA not configured**, goes out in user mode) + a **power LED** (+3V3 → 510 Ω → LED → GND, as v1). Lit only when switched on — no charge indicator while off (3.3 V is post-switch; accepted). |
| 12 | 2 | Green LED | 19-213SYGC/S530-E2/5T | C2986027 | CDONE indicator + power LED | ✅ | JLC | 0603 green. The CDONE LED is lit while CDONE is low (FPGA unconfigured) and goes out when CDONE releases high (user mode); the power LED is lit whenever +3V3 is up. |
| 13 | 1 | SWD programming/debug header | Pin Header 1x5 2.54 mm | C358687 | 3V3 / SWDIO(PA13) / SWCLK(PA14) / GND / NRST | ✅ | JLC | ST-Link V2 pinout (the existing Dupont cable fits). Sole flashing/debug path (USB is power-only). **Top edge, east of the USB-C** (moved 2026-10-12 from the right grip), outside the 2090's outline: plug in with the front shell off. Pin 1 (3V3, square pad) at the west end; silk labels each pin. THT, soldered by JLC (through-hole assembly, Standard PCBA; 2026-10-13). |
| 83 | 12 | Bring-up + console test points | bare Ø1.5 mm copper pads (manual:TestPoint) | — | TP1 VBUS · TP2 BAT · TP3 SYS · TP4/TP10 GND · TP5 +3V3 · TP6 +5V_LCD · TP7 +1V2 · TP8 BOOT0 · TP9 MCO2 (PC9) · TP11 USART1_TX (PA9) · TP12 USART1_RX (PA10) | ✅ | — | **Added 2026-10-10**, all on the **back** (probe with the back shell off; bare copper, nothing assembled, so no double-sided-assembly cost). **Console row (2026-10-12, replaced the console header J2):** along the bottom edge under the MCU, 2.54 mm pitch, west → east MCO · TX · RX · GND — tack-solder Dupont leads, or press a pogo strip / a held 0.1" header on it. **TP8 BOOT0 → bridge to TP5 (+3V3) at power-up** = the STM32 ROM UART bootloader on TP11/TP12 (USART1 PA9/PA10) — a recovery path if SWD is ever locked out. TP9 = MCO2 (PC9): route HSE or SYSCLK out to check the crystal. |

> **iCE40 config bus:** SPI1 SCK/MOSI (PA5/PA7) + select PA4 → SPI_SCK/SI/SS (pins 15/17/16), CRESET_B (PB1), CDONE (PB2) — the same
> pins carry the PPU command bus after configuration (2026-10-12; ice40-breakout used SPI3 + PB6 — re-jumper it to match); CRESET 10 K pull-up, SS_B **3.3 K** pull-**down** (slave-boot strap, #78), CDONE 10 K pull-up + LED, **NOR /CS
> (PD2) 10 K pull-up** (HW backstop → NOR defaults deselected). STM32 streams the bitstream at boot. The W25Q128
> has SPI3 to itself (no CS arbitration with the FPGA, no SPI_SO constraint).

---

## 2. Screen — Adafruit 2090 ILI9341, 8-bit parallel 8080 (FPGA-driven)

The panel hangs off the iCE40 PPU over an **8-bit 8080 bus** (LCD_D0–D7 + WR + DC + CS, RD tied directly to +5V_LCD (module VIN),
RST from an FPGA GPIO). **The screen uses one STM32 pin** — PC6 drives the backlight (LITE); everything else is on the
FPGA, which the STM32 feeds over SPI1. Module is the Adafruit 2090 (breadboard-confirmed in 8080 mode), mounted removable on 2.54 mm headers.

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 15 | 1 | LCD module (parallel-capable) | **Adafruit 2090** — 2.8" ILI9341 TFT breakout v2, capacitive touch | — (Adafruit) | 8-bit 8080 ← iCE40 (D0–7 + WR + DC + CS + RST); RD → +5V_LCD; LITE ← STM32 PC6 | ✅ | Hand | Breadboard-confirmed in 8080 mode. Module facts from Adafruit's Eagle files (original + rev D): an onboard **3.3 V LDO fed from VIN** (MIC5225-3.3, 150 mA, original; AP2112K-3.3 on rev D) powers the ILI9341, the 74LVC245 level shifters and the backlight; the module's own **APX803 reset supervisor** (IC4 on the 2090 — not a part we buy) + 10 kΩ pull-up sits on RST, so the panel resets itself at power-up. Touch controller = FT6206 (CST026 on boards since Sept 2020) — unused. **Feed VIN from +5V_LCD (§8)** — the LDO drops ~0.25–0.3 V, so it needs VIN ≳ 3.6 V to hold 3.3 V; on bare SYS the module rail sags below a ~3.6 V cell and the backlight fades out. **Backlight:** 4 parallel white LEDs, each from the module's 3.3 V through 10 Ω, common cathode switched by Q4 (BSS138) whose gate is LITE; panel spec **60 mA typ / 80 mA max total** → module draw ≈ 70–100 mA. **LITE → STM32 PC6 (pin 37), open-drain, TIM3_CH1 (AF2) PWM:** the module pulls LITE to ~4.47 V (1 kΩ to VIN + 2.2 kΩ to 3.3 V), so the backlight is ON by default and at boot; PC6 low = off (sinks ~6 mA). PC6 is 5 V-tolerant (FT) — **keep its internal pull-up/down OFF** (the node exceeds VDD+0.3 V). Never wire LITE to an iCE40 pin (3.6 V abs-max). **RD → +5V_LCD (module VIN) directly, no resistor:** the module has a 10 kΩ pull-down on RD and RD sets its data-buffer direction (low = the module drives D0–D7 out toward the FPGA) — a 10 kΩ pull-up would leave it at 1.65 V (undefined). Tie it to the module's own supply, not +3V3: at switch-on the module's rail comes up first (fed straight through the boost's inductor + diode) while +3V3 is still soft-starting, so an RD on +3V3 would briefly point the buffer at the unpowered FPGA. The header inputs are the module's 5 V-tolerant side; costs 0.5 mA. Cap-touch (FT6206) + microSD unused → NC. Removable via two header sockets (#16/#17). **Mechanical (Adafruit Eagle + product page, both revisions):** board 81.3 × 62.5 mm; JP1 and JP2 centered on the short edges, **76.2 mm apart** center-to-center; four 3.0 mm plated mounting holes at **76.2 × 57.15 mm**, in line with the headers → matching standoff holes on the mainboard. Datasheets: SPEC-DT280QV10-CT_Rev.B.pdf (panel), ILI9341.pdf, adafruit-2090-cap-touch{,-revD}.sch. |
| 16 | 1 | LCD socket, 8-bit side (JP1, electrical) | HX PM2.54-1x20P ZC (1x20 female, square-hole) | C41417332 | JP1: 1 GND · 2 VIN (+5V_LCD) · 3 CS · 4 C/D (DC) · 5 WR · 6 RD → +5V_LCD · 7 RST · 8 LITE ← PC6 · 9 CTP_IRQ · 10 SCL · 11 SDA · 12 GND · 13–20 D0–D7 | ✅ | JLC | Female sockets on the mainboard, male pins on the 2090 → the screen unplugs. THT, soldered by JLC (through-hole assembly, Standard PCBA; 2026-10-13). Pinout identical on both 2090 revisions (Adafruit Eagle). Touch pins 9–11 → NC. **Module confirmed:** Adafruit product 2090 (the pinout is unchanged across revisions incl. the Dec-2022 EYESPI rev D, per the product page), and the working 8080 breadboard proves this JP1 wiring and that SJ1–SJ4 are open. **Layout:** the module sits screen-up with its pins pointing down into the sockets — check J3/J4 pin-1 orientation against the physical module (as seen from the mainboard side) when placing the footprints. |
| 17 | 1 | LCD socket, far side (JP2, mechanical) | HX PM2.54-1x20P ZC (1x20 female, square-hole) | C41417332 | JP2: pins 1 + 11 → GND; all others NC | ✅ | JLC | Second socket on the 2090's opposite (SPI/SD) header for mechanical support. v1 also fitted two sockets, but the other way round (JP2 electrical for SPI, JP1 mechanical). **Leave pin 2 (VIN), pin 3 (3Vo = LDO output) and pins 15–18 (IM3–IM0) NC** — never drive 3Vo, never strap IM: the module selects 8-bit 8080 (IM[3:0]=0000) through its own 10 kΩ pull-downs **only while its solder jumpers SJ1–SJ4 are open** (v1's SPI-mode module had IM1–IM3 bridged). *(RD and RST are nets, not parts — see #15/#16.)* THT, soldered by JLC (through-hole assembly, Standard PCBA; 2026-10-13). |
| 18 | 2 | LCD VIN decoupling at the socket | 100 nF (C14663) + 10 µF (C1713) | C14663 / C1713 | +5V_LCD at JP1 pin 2 → GND | ✅ | JLC | 100 nF + 10 µF on +5V_LCD at the socket (the module carries its own LDO caps). Shared reels. |
| 79 | 1 | LCD_WR series termination | 0603WAF330JT5E (33 Ω 1%) | C23140 | iCE40 pin 38 (LCD_WR_FPGA) → J3 pin 5 (LCD_WR) | ✅ | JLC | **Place within a few mm of iCE40 pin 38.** The ILI9341 latches each byte on WR's rising edge; the iCE40's ~1–2 ns edges reach the module's 74LVC245 (no hysteresis) through the socket and an unterminated stub toward JP2, so ringing could create a second rising edge → an extra byte → the rest of the frame's colour pairing scrambled. 33 Ω + the ~20–50 Ω driver ≈ the trace impedance (source termination) and slows the edge ~0.7 ns — irrelevant against 50 ns bus phases. Data/DC/CS are only sampled at that edge, so WR alone needs it. *(Numbered 79 to keep row numbers stable.)* |

> **Module power + backlight (from Adafruit's 2090 Eagle files, original + rev D):** VIN → the module's own 3.3 V LDO
> (MIC5225-3.3 / AP2112K-3.3, ~0.25–0.3 V dropout) → ILI9341 + level shifters + backlight. The 4 parallel LEDs run from that
> 3.3 V through 10 Ω each: **60 mA typ / 80 mA max total** per the panel spec (§8) — not 240 mA. v2 feeds VIN from the §8
> MT3608 **+5V_LCD** rail so the LDO never drops out across the cell's 3.0–4.2 V range (bare SYS dims the backlight below a
> ~3.6 V cell; +3V3 would be too dim from the start). LITE idles at ~4.47 V on the module → backlight ON by default; STM32
> PC6 (open-drain) pulls it low for off / PWM. Panel interface confirmed: ILI9341, 8-bit 8080 (IM[3:0]=0000 on-board, SJ1–SJ4 open),
> IOVCC/VCI 3.3 V max.

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
> teardown for pops. Class-D output stays **filterless** (no EMC ferrite/cap pads — dropped).

---

## 4. Motion (IMU) + Haptics + shared I2C bus

LSM6DSOX IMU (0x6A), DRV2605L haptic driver (0x5A), and the PCA9555 expander (0x20, §6) share **STM32 I2C1
(PB8 SCL / PB7 SDA)** — SDA moved from PB9 2026-10-10: with SDA on PB7 the MCU's right side reads SCL, SDA, IMU INT1 top to bottom, the order they arrive at the IMU (no crossing).

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 24 | 1 | IMU (6-axis) | LSM6DSOXTR | C481766 | I2C1 0x6A (PB8/PB7) + INT1→PB5 | ✅ | JLC | LGA-14. VDD/VDDIO to 3.3 V (no 5 V tolerance). Use INT-driven data-ready (INT1→PB5, EXTI5) + FIFO watermark, not fast polling. RTOS: WHO_AM_I(0x0F)=0x6C, CTRL1_XL/CTRL2_G, OUTX/Y/Z. **Mandatory strap nets:** CS(pin12)→3V3 selects I2C (else boots SPI, never answers); SA0(pin1)→GND sets addr 0x6A; **SDx(pin2) + SCx(pin3) → GND** (Mode-1 I2C-slave: the unused master/aux pins must not float). INT2 (pin 9) → NC (output forced low by default); OCS_Aux/SDO_Aux: soldered-NC. |
| 25 | 1 | IMU INT1 anti-I3C-latch pulldown | 10 kΩ 0603 | C99198 | INT1(pin4) → GND | ✅ | JLC | **10 kΩ pulldown at INT1 + PB5 GPIO input-no-pull through boot** — a pull-up/driven-high INT1 at POR latches the part I3C-only = permanently dead IMU (F411 has no I3C). |
| 26 | 1 | IMU VDD decoupling | 100 nF 0603 | C14663 | VDD(pin8) → GND | ✅ | JLC | Datasheet-required decoupling. |
| 27 | 1 | IMU VDDIO decoupling | 100 nF 0603 | C14663 | VDDIO(pin5) → GND | ✅ | JLC | Same 3.3 V net as VDD. |
| 28 | 1 | Haptic driver | DRV2605LDGSR | C527464 | I2C1 0x5A (PB8/PB7), EN→3V3 | ✅ | JLC | VSSOP-10, closed-loop LRA driver, motor straight to OUT± (no FET/flyback). **VDD on 3.3 V, not SYS** (its 3.3 V logic pins would exceed VDD+0.3 if VDD sagged under a motor pulse). RTOS: EN high → wait 0x5A ACK → load the LRA resonant config → GO. **Mandatory strap net:** IN/TRIG→GND = I2C-only mode (floating → spurious playback). EN → 3V3 (no GPIO). **VDD/NC (pin 6) → tie to the VDD net** (datasheet-preferred; the optional 2nd supply pin). |
| 29 | 1 | Haptic REG cap (LDO stability) | 1 µF 0603 X5R (CL10A105KB8NNNC) | C15849 | REG → GND | ✅ | JLC | **MANDATORY** — the internal LDO on REG needs ~1 µF low-ESR within a few mm or it oscillates and the part never enumerates on I2C. |
| 30 | 1 | Haptic VDD HF bypass | 100 nF 0603 | C14663 | VDD → GND | ✅ | JLC | HF bypass at VDD. |
| 31 | 1 | Haptic VDD local bulk | 10 µF 0805 | C1713 | VDD → GND | ✅ | JLC | Local bulk — sources the pulsed motor/overdrive current. |
| 32 | 1 | Actuator (LRA) | Vybronics VG0832022D — Ø8 mm coin LRA | DigiKey 9974288 | DRV2605L OUT± | ✅ | Hand | 235 Hz resonant, 1.8 Vrms, 22.5 Ω, 90 mArms max, wire leads (RED=+, BLUE=−). **Record 235 Hz → the DRV2605L LRA config** (resonant-freq register / drive-time). 3.3 V develops the 1.8 Vrms across OUT±. **Direct-solder the leads to OUT± (no connector).** |
| 33 | 2 | Shared I2C pull-ups (SDA + SCL) | 1.5 kΩ 1% 0603 (0603WAF1501T5E) | C22843 | I2C1 SDA(PB7)/SCL(PB8) → 3V3 | ✅ | JLC | 1.5 kΩ meets the 400 kHz rise-time budget with ~5× margin on the routed board (2026-10-12, `gb2/route/i2c_check.py`: ~115 mm per line across the board, ~45–50 pF with the pins dominating → t_r ≈ 60 ns vs 300 ns; Fast mode would allow up to ~7 kΩ). Sink current 1.9 mA (≤ 3 mA). No tuning pads (dropped — 1.5 kΩ already meets the rise-time budget). |

> **I2C address map (clash-free):** LSM6DSOX 0x6A · DRV2605L 0x5A · PCA9555 0x20. **Escape bus:** I2C3
> (PA8 SCL + PC9 SDA, AF4 — PB4 now reads Vol+) is free if BT-EN stays off PA8; I2C2's SCL pin (PB10) is free again — the IMU INT moved to PB5 (2026-10-10).

---

## 5. Battery + Charging + Power-Path + on/off

**BQ24073 power-path → SYS_SRC → P-FET power switch (#80, set by slide switch #55) → SYS → TPS63021 buck-boost → 3.3 V; 3.3 V → AP2127K → iCE40 1.2 V core.** The LCD 5 V boost (§8)
+ MAX98357A VDD hang off SYS (post-switch). No USB-PD sink (plain CC, ~500 mA input); battery % via a resistor divider →
STM32 ADC (no fuel-gauge IC).

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 34 | 1 | USB-C receptacle (5 V in) | TYPE-C-31-M-12 | C165948 | VBUS/GND (power-only) | ✅ | JLC | 16-pin, 5 V in, **power-only** (D+/D− not routed — STM32 flashes via SWD). VBUS → IN cap → BQ24073 IN. **VBUS ESD/surge** covered by the 28 V-tolerant / 6.6 V-OVP charger IN + the 4.7 µF IN cap (the port only ever sees 5 V: CC pull-downs = default USB power, no PD); no VBUS TVS (dropped — add one in a later rev only if the exposed port shows ESD flakiness). Full 16-pin USB-2 connector (rated 5 A/20 V; D+/D−/SBU present but unrouted); SMD contacts + THT shield/retention posts; the two plastic locating-peg holes are **non-plated** (footprint fixed 2026-09-28). **Bond the shell/mid-plate + both retention posts to GND** (unbonded shell hurts ESD immunity); net all 4 VBUS + all 4 GND contacts. |
| 35 | 2 | USB-C CC pulldowns | 5.1 kΩ 1% 0603 | C105580 | CC1/CC2 → GND | ✅ | JLC | Rd = 5.1 kΩ each → advertises a ≤500 mA (SDP) / up-to-1.5 A Type-C sink. This is v2's whole CC network (no PD sink). |
| 36 | 1 | Power-path charger | BQ24073RGTR | C15220 | autonomous DPPM; /CHG,/PGOOD open-drain | ✅ | JLC | QFN-16-EP. **BQ24073 since 2026-10-12** (the BQ24074, C54313, was out of stock at JLC): same family, package, land pattern and pins except pin 15, OUT regulation still 4.4 V; input OVP 6.6 V instead of 10.5 V (fine: the port is 5 V-only). System runs from **OUT = SYS_SRC** (→ power switch Q1 #80 → SYS; works with the cell absent — OUT ≈ 4.4 V). CE→GND. **TD (pin 15) → GND** = termination enabled (the 073's TD must not float; on the 074 this pin was ITERM, left open). **TMR (pin 14) → 68 kΩ to VSS (#76)** — the floating default timer (4 h min) can expire before this cell is full. **Exposed pad → VSS via array** (heat path). Charging is autonomous. **/CHG (pin 9) → STM32 PC1 (pin 9), /PGOOD (pin 7) → PC2 (pin 10)**, both open-drain with **10 kΩ pull-ups to 3.3 V (counted in #10)**. The RTOS reads them for charge status and to gate the battery % (#51); /CHG blinking at 2 Hz = safety-timer fault. Pull-ups to the switched 3.3 V are harmless at true-off. |
| 37 | 1 | EN1/EN2 strap (mandatory) | 0 Ω 0603 (EN2→OUT) + EN1→GND net | C21189 | EN2(pin5)→SYS_SRC (charger OUT, pre-switch); EN1(pin6)→GND | ✅ | JLC | **MANDATORY: EN1→GND + EN2→OUT** = ILIM-resistor mode. Both float to (0,0) = 100 mA input with the ILIM resistor inert otherwise. Strap EN2 to the always-on **OUT (SYS_SRC)** node (3.4–4.4 V < 7 V abs-max) — **never IN**, and never the post-switch SYS (switched off, EN2 would fall to 0 → 100 mA charging). |
| 38 | 1 | Charger IN cap | CL10A475KO8NNNC (4.7 µF/16 V) | C19666 | IN(pin13) → VSS | ✅ | JLC | IN bypass 1–10 µF, **<10 µF** (USB-IF inrush). 16 V (IN sees the OVP window). |
| 39 | 1 | Charger OUT cap (SYS_SRC) | CL21A106KOQNNNE (10 µF/16 V) | C1713 | OUT(pins10/11) (SYS_SRC) → VSS | ✅ | JLC | OUT = SYS_SRC (pre-switch; feeds the power switch Q1 #80 → SYS → buck-boost + LCD 5 V boost + amp). Required OUT bypass (4.7–47 µF) even while switched off — place close to pins 10/11. 16 V. Also the input-bulk brownout guard. |
| 40 | 1 | Charger BAT cap | CL21A106KOQNNNE (10 µF/16 V) | C1713 | BAT(pins2/3) → VSS | ✅ | JLC | BAT bypass. Place near the JST-PH. |
| 41 | 1 | Charge-current resistor (ISET) | 0603WAF2001T5E (2.0 kΩ 1%) | C22975 | ISET(pin16) → VSS | ✅ | JLC | **MANDATORY** (open ISET = no charging). ICHG = KISET/RISET → ~445 mA (~0.22C). DPPM tapers below this on a weak input. 1% required. Cell datasheet confirms 0.2C standard / **1C (2 A) max** charge → 0.22C is safe; TS now pauses charging outside ~3–47 °C (#43). |
| 42 | 1 | Input-limit resistor (ILIM) | 0603WAF3301T5E (3.3 kΩ 1%) | C22978 | ILIM(pin12) → VSS | ✅ | JLC | IIN = KILIM/RILIM → **403 / 462 / 521 mA** (min/typ/max; KILIM 1330/1525/1720 AΩ, 200–500 mA band). Typ is under the 500 mA SDP ceiling; the max corner is ~4 % over (accepted). **Populate it — open ILIM disables all charging.** Stay ≥1.1 kΩ. Same part as #78 (board total C22978 = 2). Sets only the input *ceiling*; charge-while-play is automatic (DPPM). For faster charge-while-play on a known ≥1.5 A source, ~1.1–1.2 kΩ (no VIN-DPM back-off in resistor mode → can brown a weak brick). |
| 43 | 1 | TS battery-temperature NTC | KUU KNTC0603/10KF3950 (10 kΩ ±1%, B ≈ 3950) | C2892547 | TS(pin1) → VSS | ✅ | JLC | **Charge-temperature protection** — the 2-wire cell has no thermistor, so a board NTC stands in. The BQ24073 drives 75 µA into TS and pauses charging above **2.1 V (cold, NTC ≳ 28 kΩ)** or below **0.30 V (hot, ≲ 4 kΩ)**, ~3 °C hysteresis → charge window ≈ **3–47 °C** with B ≈ 3950 (the cell is rated 0–45 °C). Fails safe (open → "cold", short → "hot" → charging paused; the system keeps running from USB). **Place under the cell's footprint, well away from the BQ24073** (up to ~0.9 W while charging) so the charger's own heat doesn't read as a hot battery. Self-heating negligible (~56 µW). |
| 76 | 1 | Charge safety-timer resistor (TMR) | 0603WAF6802T5E (68 kΩ 1% 0603, UNI-ROYAL) | C23231 | TMR(pin14) → VSS | ✅ | JLC | **Needed for a full charge.** tMAXCHG = 10·K_TMR·R_TMR = **6.8 / 9.1 / 11.3 h** (min/typ/max; K_TMR 36/48/60 s/kΩ), covering the worst-case full charge (~5.8 h at 398 mA). The floating default (4 / 5 / 6 h) can expire first → fault, /CHG blinks 2 Hz, charging stops at ~80 %. Don't tie TMR to VSS (that disables the timer — the backstop against a cell that never terminates; TS (#43) only covers temperature). Allowed range 18–72 kΩ; precharge timer = K_TMR·R_TMR ≈ 54 min typ. *(Numbered 76 to keep existing row numbers stable.)* Same fix applies to gameboy-v3 (same charger family, ISET and cell — v3 still uses the BQ24074; if v3 also moves to the BQ24073, tie its pin 15 (TD) to GND, but never ground a BQ24074's pin 15 (ITERM = 0 Ω → no termination)). |
| 44 | 1 | 3.3 V buck-boost | TPS63021DSJR (fixed 3.3 V) | C202140 | SYS → 3.3 V logic | ✅ | JLC | VSON-14-EP, fixed 3.3 V (**C202140**). 1S cell straddles 3.3 V → buck-boost mandatory. ~1.9–2.4 A at the 3.0 V floor. **EN ties to VIN on the post-switch SYS node** (the power switch (#55 → Q1 #80) is the on/off control). Inductor + output caps MUST be the datasheet Table-1 combo. **Exposed pad → PGND + thermal vias.** |
| 45 | 1 | Buck-boost FB tie | 0603WAF0000T5E (0 Ω) | C21189 | FB(pin3) → VOUT | ✅ | JLC | Fixed-version FB MUST tie to VOUT (0 Ω/net) — the internal divider senses through it. Float = no regulation. |
| 46 | 1 | Buck-boost PS/SYNC pull-down (mode control) | 0603WAF1003T5E (100 kΩ 1%) | C25803 | PS/SYNC(pin13) ← STM32 PC3; 100 kΩ → GND | ✅ | JLC | **PS/SYNC is driven by STM32 PC3 (push-pull); the 100 kΩ holds it LOW = power-save through reset** (must not float). Firmware drives it **HIGH = forced PWM** while running: VOUT 3.267–3.333 V. Power-save (below ~100 mA) regulates **2.5–3.5 % high** (≈ 3.40 V typ; datasheet +0.6…+5 % → up to 3.50 V, above the iCE40's 3.46 V VCCIO max) and steps as the load crosses the threshold — which also moves the ADC reference (VDDA = VREF+, battery % reads ~0.1 V low). Use power-save only when idle. Same part as #51/#81 (board total C25803 = 4). |
| 47 | 1 | Buck-boost VINA bypass + **VIN tie** | CC0603KRX7R9BB104 (100 nF) | C14663 | VINA(pin1) → GND (cap) **AND VINA → VIN/SYS (net)** | ✅ | JLC | **VINA (pin 1) = the UVLO-sensed control-stage supply — MUST be tied to VIN/SYS** (same input rail as pins 10/11), or the converter sits in permanent UVLO → **dead 3.3 V rail**. The 100 nF is only a bypass (**hard limit 0.22 µF**) — it cannot power VINA. ⚠️ TI's datasheet Fig 7/28 *dropped the drawn VINA→VIN wire* (Rev G) — misleading; VINA must still connect to VIN. Direct copper net (or 0 Ω link). |
| 48 | 2 | Buck-boost input cap | CL21A106KOQNNNE (10 µF/16 V) | C1713 | VIN(pins10/11) → PGND | ✅ | JLC | 2×10 µF local to VIN/PGND (at the converter pins). **16 V required** (SYS 4.2 V + DC-bias derate). |
| 49 | 3 | Buck-boost output cap | CL21A226MAQNNNE (22 µF/25 V) | C45783 | VOUT(pins4/5) → PGND | ✅ | JLC | 3×22 µF (min 2), **stability-critical** — must land in the datasheet Table-1 L/C matrix. Place close to VOUT/PGND. |
| 50 | 1 | Buck-boost inductor | MWSA0503S-1R5MT (1.5 µH) | C408407 | L1/L2 | ✅ | JLC | 1.5 µH shielded, Isat **7.2 A guaranteed / 9 A typ**, DCR 25 mΩ (≫ the ~4 A switch limit). 5.2×5.4×2.8 mm — flag footprint. Matches the TPS63021 Table-1 "typical application" combo (1.5 µH + 3×22 µF). |
| 51 | 2 | Battery-sense divider (fuel gauge) | 0603WAF1003T5E (100 kΩ 1%) | C25803 | **BAT node** → PC0 tap → GND | ✅ | JLC | Two 100 kΩ 1% from the **cell/BAT node** (NOT SYS/OUT — DPPM-regulated ~4.4 V reads the charger, not SoC) to GND; tap → PC0 (ADC1_IN10; moved from PA0 2026-10-10, which became USART2_CTS). VREF 3.3 V: 4.2 V→2.10 V, 3.0 V→1.50 V. Firmware: raw×6600/4095 → mV → OCV LUT + IIR; **correct with VREFINT** (VDDA is the ADC reference and isn't exactly 3.3 V — especially in power-save, #46). **Reads high while charging** (I_charge×ESR) → firmware gates the % on /CHG or /PGOOD. 1% (ratio = SoC accuracy). |
| 52 | 1 | Battery-ADC filter/reservoir | CC0603KRX7R9BB104 (100 nF) | C14663 | PC0 → GND | ✅ | JLC | Filters divider noise + charges the ADC S/H cap (50 kΩ source Z). Firmware: long ADC sample time (≥480 cycles). |
| 53 | 1 | LiPo cell | Adafruit 2011 — 1S 2000 mAh, JST-PH, integral PCM | — (Adafruit) | JST-PH → BQ24073 BAT | ✅ | Hand | ~5.5–6.5 h screen-on. 3.7 V nom (4.2/3.0), integral PCM. PKCELL LP-803860: 0.2C std / **1C (2 A) max charge**, 2 A continuous discharge (> board peak). **~60 × 38.5 × 8.5 mm max → confirm enclosure fit.** PCM trip thresholds unpublished (R5402N101KD IC). **Verify polarity** (red = + → BAT). |
| 54 | 1 | Battery connector | S2B-PH-SM4-TB(LF)(SN) — JST-PH 2.0 mm 2-pin SMD R/A | C295747 | mates the cell → BQ24073 BAT | ✅ | JLC | 2 A. **Polarity: + pad = Adafruit red; reversed = destroyed cell.** Pad 1 = BAT sits where Adafruit's own chargers put VBAT (physical pad positions compared against their Eagle files — their pad numbering is reversed); still check the cell's red lead on arrival. Distinct pitch from speaker/LRA. Footprint carries a printable **"+"** by pad 1 (the original 0.06 mm dot was below JLC's minimum; fixed 2026-09-28). |

**On/off — GBA-style side-edge slide switch (#55) driving a high-side P-MOSFET (#80) in the SYS system-power path:**

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 55 | 1 | Power switch (side-edge slide, drives Q1's gate) | SHOU HAN MSK12C02-HB — SPDT SMD right-angle slide, 12 V / 50 mA, 8 × 2.8 mm | C431541 | pin 2 (common, datasheet ②) → Q1 gate (PWR_SW); pin 1 → GND (ON); pin 3 open (OFF); pin 4 (metal shell/tabs) → GND | ✅ | JLC | **GBA-style side-edge switch:** right-angle, so the lever exits the board edge through a slot in the shell side — place it at the board edge with the actuator slightly overhanging. **It only switches Q1's gate (#80), so it carries µA** — the 50 mA rating is ample; it could NOT carry the ~1.2 A system current itself. Pinout per the SHOU HAN drawing: ② = common, ①/③ = throws (② is 1.5 mm from ③, 3 mm from ①, matching footprint pads 2/3/1), ④ = shell. Lever is tiny (1.6 mm travel) → put a shell-molded slider over it. Footprint's two 0.85 mm plastic-peg holes fixed to non-plated. Swap pins 1/3 if the ON direction must flip (footprint unchanged). 10k-cycle life. |
| 80 | 1 | System power switch (high-side P-MOSFET) | AO3401A (AOS, SOT-23, P-ch 30 V / 4 A) | C15127 | S ← SYS_SRC (charger OUT); D → **SYS** (buck-boost VIN + LCD 5 V-boost in + amp VDD); G ← PWR_SW | ✅ | JLC | **In the SYS→system-power path** so one switch cuts every switched load → a true "off"; the **charger sits upstream** → charge-while-off preserved. Off = R30 (#81) holds the gate at the source; on = SW1 (#55) grounds the gate **through R31 with C51 gate→drain (soft-start, #82)** (V_GS = −0.91 × V_SYS_SRC = −2.7…−4.0 V vs V_th ≈ −0.9 V). R_DS(on) 60 mΩ @ −4.5 V / 85 mΩ @ −2.5 V → ~70–90 mV and ~0.1 W at the **~1.2 A SYS peak** (LCD 5 V-boost input ~0.2 A + buck-boost input ~0.4 A at a 3.0 V cell + MAX98357A ~0.6 A audio peak; ~0.6 A sustained). **True off:** the body diode (drain→source) blocks SYS_SRC→SYS. **Size the SYS copper (charger OUT → Q1 → buck-boost/boost/amp) for ≥1.5 A.** *(Numbered 80 to keep row numbers stable.)* |
| 81 | 1 | Q1 gate pull-up | 0603WAF1003T5E (100 kΩ 1%) | C25803 | Q1 gate (Q1_GATE) → SYS_SRC | ✅ | JLC | Holds Q1 off whenever SW1 is open; ~38 µA through R30 + R31 while on. Same part as #46/#51 (board total C25803 = 4). |
| 82 | 2 | Q1 soft-start | R31 RC0603JR-0710KL (10 kΩ) + C51 CC0603KRX7R9BB104 (100 nF) | C99198 + C14663 | SW1 → R31 → Q1 gate; C51 gate → drain (SYS) | ✅ | JLC | **Added 2026-10-10.** Without it SW1 snaps Q1 on and dumps SYS_SRC's 10 µF into ~40 µF on SYS: SYS_SRC collapses and, on battery, can trip the BQ24073's OUT short-circuit protection (stuttering / failed power-on). With R31 driving the gate and C51 as a Miller cap, SYS rises at ≈ (V_SRC − V_plateau)/R31/C51 ≈ 1.7 V/ms (~2 ms ramp, ~70 mA inrush into the SYS caps). R31/R30 leave V_GS = −0.91 × V_SRC (−2.7 V at a 3.0 V cell, inside the 85 mΩ R_DS(on) spec). Turn-off is slow (~20 ms through R30) — harmless, the loads brown out through their UVLOs. Both parts reuse existing reels. |

---

## 6. Controls — 12 all-tactile buttons via PCA9555 I2C expander

All buttons hang off the **PCA9555** on the shared I2C1 bus → **buttons cost 0 STM32 GPIO**. 12 buttons use
12 of the 16 pins (4 spare). All-tactile → HASL is fine; internal expander pull-ups
→ no per-button resistors.

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 56 | 1 | GPIO expander (left-grip buttons) | PCA9555PWR | C2864778 | I2C1 (PB8/PB7) base 0x20 + INT | ✅ | JLC | TI TSSOP-24. **Strap A0/A1/A2 → GND for 0x20** (must not float). Serves the 7 left-grip buttons on port 0 (P00 L · P01 LEFT · P02 UP · P03 DOWN · P04 SELECT · P05 START · P06 RIGHT; UP/LEFT and START/SELECT swapped 2026-10-12 for routing — firmware keymap only) (right-grip buttons wired straight to the STM32 2026-10-10: the only path from the left grip past J3 is already full, so only the left-grip buttons use the expander); port 1 + P00 spare. All 16 pins INPUTS at POR. Internal ~100 kΩ pull-up per input → **no external per-button pull-ups** (a tactile just ties pin→GND). VCC 3.3 V. **Polled, INT not connected (2026-10-10)** — frees a track through the full left-grip corridor; poll the input registers at ~200 Hz (a 2-byte read is ~100 µs at 400 kHz). If INT is ever re-added, mind the **INT erratum (§8.4.1.1):** after each input read, re-point the command byte to a non-00h register before another slave read; add a low-rate poll safety net. |
| 57 | 4 | D-pad tactiles (U/D/L/R) | TS-1187A-B-A-B | C318884 | 4× expander pin → GND | ✅ | JLC | XKB 5.1×5.1 mm body (6.5 mm across the terminals) top-actuated, 1.6 N, 100k cycles. Diagonals work (expander reads all pins); a shell rocker keycap restores roll. Software debounce. |
| 58 | 2 | Face tactiles (A/B) | TS-1187A-B-A-B | C318884 | STM32 PB3 (A) / PB14 (B) → GND, internal pull-ups | ✅ | JLC | Same part as the D-pad. Top-actuated. **A/B only (Game Boy layout)** — X/Y dropped 2026-09-29. Direct to the STM32 since 2026-10-10 (see #56). |
| 59 | 2 | Start / Select tactiles | TS-1187A-B-A-B | C318884 | 2× expander pin → GND | ✅ | JLC | Same part, on the expander. |
| 60 | 2 | Volume +/− tactiles | ALPS SKRTLBE010 (side-actuated SMD) | C127481 | STM32 PB4 (Vol+) / PA15 (Vol−) → GND, internal pull-ups | ✅ | JLC | 4.5×3.4 mm, 1.6 N. RTOS maps the edges to software volume. |
| 61 | 2 | L / R bumper tactiles | ALPS SKRTLBE010 | C127481 | L → expander P00; R → STM32 PB9 (internal pull-up) | ✅ | JLC | Same side-actuated part, edge-mounted. Only 100k cycles — upgrade to a higher-life part (ALPS SKHHLNA010/C125031 = 500k) in a later rev if they wear. |
| 62 | 0 | ~~Expander INT pull-up~~ | — | — | — | — | — | **Removed 2026-10-10:** the PCA9555 is polled (see #56); its INT pin is not connected and PA11 is spare. |
| 63 | 1 | PCA9555 VDD decoupling | CC0603KRX7R9BB104 (100 nF) | C14663 | VDD(pin24) → GND | ✅ | JLC | HF decoupling at the expander VDD. |

> **Expander pin budget:** D-pad 4 + face 2 + Start/Select 2 + Volume 2 + L/R 2 = **12 of 16** (4 spare: P06/P07/P16/P17 → NC, held
> by the internal pull-ups). **Pin map (gameboy-v3's, minus X/Y):** P00 UP · P01 DOWN · P02 LEFT · P03 RIGHT · P04 A · P05 B ·
> P10 START · P11 SELECT · P12 VOL+ · P13 VOL− · P14 L · P15 R (each pin → tactile → GND). No
> power button (the slide switch is a hard power switch, not a GPIO). STM32-side cost = PB8/PB7 (I2C; the expander is polled, no EINT) + 5 GPIO/EXTI for the right-grip buttons (PB9 R, PB4 Vol+, PB3 A, PA15 Vol−, PB14 B)
> for INT. **Daughterboard option:** put the expander + switches on the button board on a 5-wire cable
> (SDA/SCL/INT/3V3/GND); keep the INT pull-up mainboard-side.

---

## 7. Bluetooth — Ezurio BT830-SA-01 (CSR8811, UART-HCI)

BT830 on **USART2 4-wire HCI** (PA2 TX / PA3 RX / PA0 CTS / PA1 RTS, all AF7) (swapped 2026-10-10 for routing: USART2's pins face the BT830, USART1's face the console pads). USART2's CTS pin PA0 was the battery ADC; that moved to PC0.

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 64 | 1 | Bluetooth module (onboard) | Ezurio BT830-SA-01 (CSR8811, integrated antenna) | DigiKey | UART-HCI 4-wire → USART2 (PA2/PA3/PA0/PA1) + EN + 3.3 V | ✅ | Hand | Dual-mode, Class-1 +7 dBm, integrated ceramic antenna (no trace, just a keep-out). Castellated pads → hand-solder. Footprint (fixed 2026-09-28): antenna keep-out on **all** copper layers incl. inner planes, and the pin-1 dot next to pad 1 (it was at the far end). **VDD_PADS(pin1) + VREG_IN_HV(pin9) on the regulated 3.3 V rail, NOT SYS** (recommended 3.0–3.6 V). **Enable:** VREG_EN_RST#(pin8) ← STM32 GPIO **PC7** push-pull (was PC3; swapped with PS/SYNC 2026-10-11) (boot low, then high >5 ms); NOT a 10 kΩ strap (internal pull-down holds it < VIH). Keep BT-EN off PA8 to preserve I2C3. |
| 65 | 1 | VREG_OUT_HV stability cap (mandatory) | CC0603KRX5R6BB475 (4.7 µF X5R) | C109456 | VREG_OUT_HV(pin10) → GND | ✅ | JLC | **MANDATORY ≥1.5 µF** low-ESR at the internal HV-LDO output — without it the LDO oscillates and the module won't enumerate. **Cap-to-GND only** (abs-max 2.0 V). |
| 66 | 2 | VREG_IN_HV + VDD_PADS decoupling | CC0603KRX7R9BB104 (100 nF) | C14663 | pin 9 + pin 1 → GND | ✅ | JLC | 100 nF ×2, one at each supply pin. |
| 67 | 1 | 3.3 V bulk / RF-burst reservoir | CC0603KRX5R6BB475 (4.7 µF X5R) | C109456 | 3.3 V feed near pin 9 → GND | ✅ | JLC | 4.7 µF near the module's 3.3 V input — supplies Class-1 TX-slot bursts locally. |

> **BT830 integration:** (1) **UART** RX/TX/CTS/RTS → USART2 4-wire; cross-wire STM32_RTS→module_CTS,
> module_RTS→STM32_CTS (module pins have weak internal pull-ups → idles "not-clear-to-send" safely).
> (2) **Layout:** module at a board edge, antenna end overhanging, **no-copper keep-out on all layers**,
> metal/battery ≥20 mm clear. (3) **Firmware:** the CSR8811 is controller-only → the RTOS runs its own
> **BLE-only host** (NimBLE/BTstack) over H4 (flow control present). No Linux/BlueZ; no A2DP (audio stays
> speaker-only).

---

## 8. LCD 5 V boost — NEW (the 2090's LDO needs VIN ≳ 3.6 V; 5 V is the breadboard-proven value; logically part of §5 power)

Breadboard-proven at **VIN = 5 V**, but v2 has no 5 V rail → a small **SYS→5 V boost** feeds the LCD VIN.
Load ≈ 70–100 mA (the module's 3.3 V LDO feeding its logic + the ~60–80 mA backlight; ≤150 mA on the original board's MIC5225) — the parts below are sized for far more. On the SYS domain, **downstream of the slide
switch (#55 → Q1 #80)** for true-off. **MT3608 boost — all 6 rows (68–73) confirmed live on JLC.**

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 68 | 1 | 5 V boost regulator | **MT3608** (Aerosemi, SOT-23-6) | **C84817** | SYS → +5V_LCD, ~0.1 A | ✅ | JLC | Async boost, 1.2 MHz, **4 A internal switch current limit** (the "2 A" is the title rating), FB ref 0.588 / 0.6 / 0.612 V, internal soft-start, EN V_IH 1.5 V / abs-max 26 V (datasheet: cad/docs/C84817.pdf). SYS (3.0–4.2 V) → 5 V (SYS < 5 V always, so it always boosts). **Needs an external Schottky (#73)** (SW→diode→VOUT); the FB divider (#72) sets 5 V. **EN → VIN** (tie high for always-on; no internal pull) — since the whole boost is post-switch, it powers up with the board. Confirmed live on JLC (Extended tier). |
| 69 | 1 | Boost inductor | Sunlord SWPA4030S4R7MT (4.7 µH, 4×4 mm) | C57269 | MT3608 SW/L | ✅ | JLC | 4.7 µH, **2 A Irms / 3.2 A Isat**, 78 mΩ DCR, 4×4 mm. MT3608 typical value @ 1.2 MHz; worst-case peak I_L ≈ 0.35 A at the real ~0.1 A load (≈ 0.75 A even at 300 mA) → Isat 3.2 A ≫. *(The MT3608's switch current limit is 4 A typ — above this inductor's 3.2 A Isat, but only reached during startup or a fault; the normal peak is ~0.35 A. Thermal: ~0.1 W loss × 250 °C/W θJA ≈ +25 °C.)* |
| 70 | 1 | Boost input cap | CL21A106KOQNNNE (10 µF/16 V) | C1713 | boost VIN → GND | ✅ | JLC | Reuse the 10 µF reel, local to the boost VIN (on SYS). |
| 71 | 1 | Boost output cap | CL21A226MAQNNNE (22 µF/25 V) | C45783 | boost 5 V → GND | ✅ | JLC | Reuse the buck-boost 22 µF reel (#49). MT3608 typical Cout = 22 µF; X5R, ≥10 V. Place close to VOUT. |
| 72 | 2 | Boost FB divider | R1 73.2 kΩ 1% (0603WAF7322T5E) + R2 10 kΩ ±5% (RC0603JR, reel #10) | R1 = C14890 / R2 = C99198 | FB → 5 V / GND | ✅ | JLC | Sets 5 V from the MT3608's 0.6 V FB ref: VOUT = 0.6·(1+R1/R2); **73.2 k / 10 k → 4.99 V nominal** (~60 µA divider current); with R2's ±5 % plus FB-reference tolerance, ≈ 4.7–5.3 V — fine for the module LDO (AP2112K ≤ 6 V). R2 reuses the 10 kΩ reel (#10, C99198); R1 = 73.2 kΩ 1% (75 V, ample). |
| 73 | 1 | Boost rectifier (Schottky) | SS34 (MDD, 3 A/40 V, SMA/DO-214AC) | C8678 | MT3608 SW → 5 V out | ✅ | JLC | **Required by the async MT3608** (SW→diode→VOUT). 3 A/40 V ≫ our ~0.2 A and V_R = 5 V; Vf 550 mV@3 A (lower at our current). **JLC Basic tier** (no placement fee). |

> **Why not skip the boost:** the module's LDO needs VIN ≳ 3.6 V. Bare SYS (3.0–4.4 V) dims the backlight below a
> ~3.6 V cell; +3V3 would be too dim from the start; back-driving the module's 3Vo (LDO output) pin isn't supported.
> The 5 V boost matches the proven breadboard exactly.

---

## 9. HSE crystal — STM32 clock accuracy (for reliable Bluetooth)

Replaces the ±4 %-over-temp HSI as the PLL source. **Costs PH0/PH1** (GPIO-capable pins, now OSC_IN/OSC_OUT) and no other
GPIO. Added because the BT830 HCI UART framing can't tolerate the HSI's ±4 % over temp (audio pitch also
stabilizes). **Firmware clock-init contract (record in the RTOS init):** switch the PLL source HSI→HSE;
e.g. M=8, N=200, P=2 → 100 MHz SYSCLK (AHB /1, APB1 /2, APB2 /1). Enable **PLLI2S** for the I2S BCLK (the
MAX98357A needs no MCLK) — **48 kHz is exact from an 8 MHz HSE** (set the PLLI2S divisors per ST's clock
config; verified achievable). `board.dts` already carries the `hse` node. The OSC_OUT series R is
**fitted as 0 Ω (#77)** — an empty series pad would open the oscillator loop, and the CSV flow has no DNP.

| # | Qty | Role | Part | LCSC # | Interface | Src | Fit | Note |
|---|-----|------|------|--------|-----------|-----|-----|------|
| 74 | 1 | HSE crystal | KDS 1C208000BC0R (8 MHz, CL = 12 pF, ±20 ppm, SMD3225-4P) | C57131 | PH0/PH1 (OSC_IN/OSC_OUT) | ✅ | JLC | 8 MHz → PLL /M=8 → 1 MHz PLL input (in ST's 1–2 MHz range). Fundamental, ±20 ppm. **CL = 12 pF → 15 pF load caps (#75).** No external feedback R (F411 internal); series R on OSC_OUT = #77 (0 Ω). Pins 2/4 (not connected inside) → GND. **Gain margin:** gm_crit = 4·ESR·(2πf)²·(C0+CL)² = 0.79 mA/V at the KDS worst case (ESR 400 Ω, C0 2 pF) vs the F411's Gm_crit_max 1 mA/V → in spec, 1.26× headroom. Keep #77 at 0 Ω (series R eats margin); a crystal with ESR ≤ 200 Ω would double the headroom. |
| 75 | 2 | Crystal load caps | YAGEO CC0402JRNPO9BN150 (15 pF, NP0, ±5%, 50 V, 0402) | C106997 | OSC_IN/OSC_OUT → GND | ✅ | JLC | C_load ≈ 2·(CL − C_stray) = 2·(12 − ~5) ≈ **15 pF** for the CL = 12 pF crystal (#74). NP0/C0G; ±5 % is fine for crystal loads. **Verify 8 MHz on bring-up** without loading the crystal (HSE on MCO2 = PC9, or the console UART's bit timing); change the value only on measurement. |
| 77 | 1 | HSE series resistor (OSC_OUT) | 0603WAF0000T5E (0 Ω) | C21189 | PH1 (OSC_OUT) → crystal pin 3 | ✅ | JLC | Fitted 0 Ω keeps the AN2867 series-R footprint (no DNP in the CSV flow; an empty series pad would open the loop). Keep it 0 Ω — the crystal's gain headroom is only 1.26× (#74). Same 0 Ω reel as #37/#45 → board total C21189 = 3. |

---

## Pin budget (F411 LQFP64, 50 GPIO)

**33 of 50 GPIO used (incl. PH0/PH1 for the HSE), 17 spare.** Tight on *function* (every hardware-flow-control USART and every
spare I2C bus except I2C3 — PA8 + PC9 (SDA AF4), kept free as the escape bus — is consumed), not on count. The 5 right-grip buttons cost 5 GPIO (PB9/PB4/PB3/PA15/PB14; PB4 was I2C3_SDA, so I2C3's escape SDA moved to PC9); the left-grip ones are on the expander. Verified against the F411 AF table.

| Subsystem | Pins | Which |
|-----------|------|-------|
| iCE40 config + PPU (SPI1) | 5 | PA4 (select: config strap / active-high runtime select) / PA5 (SCK) / PA7 (MOSI) + PB1 (CRESET) + PB2 (CDONE) — write-only; PA6 (MISO) NC |
| NOR (SPI3) | 4 | PC10/11/12 + PD2 (NOR /CS) |
| Audio (I2S2) | 3 | PB12 (WS) / PB13 (CK) / PB15 (SD) |
| Shared sensor + button I2C (I2C1) | 2 | PB8 (SCL) / PB7 (SDA, AF4) — IMU + haptics + PCA9555 |
| Right-grip buttons (direct) | 5 | PB9 (R) / PB4 (Vol+) / PB3 (A) / PA15 (Vol−) / PB14 (B) — EXTI9/4/3/15/14, internal pull-ups; PB3 = SWO, so no SWO trace |
| IMU INT1 | 1 | PB5 (EXTI5) — on U1's right side, facing the IMU (moved from PB10 2026-10-10) |
| Bluetooth (USART2 4-wire) | 4 | PA2 (TX) / PA3 (RX) / PA0 (CTS) / PA1 (RTS) |
| Bluetooth EN | 1 | PC7 (keeps PA8/I2C3 free; swapped with PS/SYNC 2026-10-11 so it leaves under the MCU into the west channel) |
| Battery ADC | 1 | PC0 (ADC1_IN10) |
| Charger status (BQ24073) | 2 | PC1 (/CHG, EXTI1) / PC2 (/PGOOD, EXTI2) — open-drain, 10 kΩ pull-ups (#10); swapped 2026-10-10 for routing. EXTI2 is shared with PB2 (CDONE) — keep CDONE polled |
| LCD backlight (LITE) | 1 | PC6 (TIM3_CH1, AF2) — open-drain, internal pulls OFF (5 V-tolerant FT pin) |
| Buck-boost mode (PS/SYNC) | 1 | PC3 — push-pull (was PC7; swapped with BT EN 2026-10-11, the buck-boost is north of the MCU); high = forced PWM, 100 kΩ pull-down = power-save through reset (#46) |
| HSE crystal | 2 | PH0 (OSC_IN) / PH1 (OSC_OUT) — §9 |
| Console/debug UART (USART1) | 2 | PA9 (TX) / PA10 (RX) — PA12 spare |
| SWD | 2 | PA13 / PA14 |
| Clock out (test) | 1 | PC9 MCO2 → TP9 (HSE / SYSCLK check) |
| Buttons (12) | 0 | all on the PCA9555 expander |
| Power on/off | 0 | slide switch + P-FET in the SYS power path (#55/#80) — not a GPIO |
| **Total** | **33** | of 50 (BOOT0/NRST are dedicated, non-GPIO) |

**Layout/firmware rules:**
1. **No spare SPI** — SPI1 = FPGA config + PPU, I2S2 = audio, SPI3 = NOR. New peripherals go on I2C / UART / GPIO only.
2. **USART2 = Bluetooth, USART1 = console** (swapped 2026-10-10 for routing: USART2's pins face the BT830, USART1's face the console pads). USART2's flow-control pins (PA0/PA1) are free now that the battery ADC is on PC0. The Nucleo keeps its console on USART2 (ST-Link VCP) — so Bluetooth can't be brought up on the Nucleo without opening its VCP solder bridges; gb2 gets its own board config.
3. **Buttons on the PCA9555 expander** → 0 GPIO.
4. **No system 5 V rail** — one local MT3608 boost (§8) makes +5V_LCD for the display only; the amp runs from SYS; buck-boost makes 3.3 V; AP2127K makes the iCE40 1.2 V core. The power switch (#55 → P-FET #80) in the SYS power path gives a true "off"; the amp self-gates (SD_MODE on the switched rail).
5. **Battery divider taps the BAT/cell node** (not SYS/OUT), VREF = 3.3 V.
6. **All-tactile buttons → HASL** (no ENIG); no per-button pull-ups.
7. ~~**SPI3 CS mutual-exclusion**~~ — retired 2026-10-12: the FPGA left SPI3 (it configures over SPI1), so the NOR has the bus alone; its /CS pull-up (PD2, #10) stays as the deselect-through-reset backstop.
8. **Keep PA8 (I2C3_SCL) free** (BT-EN on PC7) → preserves the one hardware I2C escape bus.

---

## Power tree

- **Battery ↔ BQ24073 power-path → SYS_SRC → slide switch (#55) → SYS (~3.0–4.4 V, not 5 V).**
- **SYS → TPS63021 buck-boost → 3.3 V logic** (STM32, iCE40 I/O, NOR, sensors, expander, BT830).
- **3.3 V → AP2127K LDO → 1.2 V iCE40 core.**
- **On SYS directly:** the **LCD 5 V boost** (+5V_LCD → the 2090's own 3.3 V LDO, which powers its logic + ~60–80 mA backlight), MAX98357A VDD, charge current — all downstream of the power switch (#55 → Q1 #80) except the charger, which is upstream (charge-while-off).
- When plugged, USB-C 5 V feeds the charger IN (≤500 mA plain CC) and charges/supplements; OUT (SYS_SRC → SYS) then regulates at ~4.4 V. On battery alone, SYS ≈ the cell voltage.

**Net names (for the CSVs):** VBUS · BAT (cell) · SYS_SRC (charger OUT, pre-switch) · SYS (post-switch) · +3V3 ·
+3V3A (STM32 VDDA after the ferrite) · +1V2 · +1V2_PLL · +5V_LCD · GND.

**3.3 V rail load ≈ 200–300 mA worst (estimate — MEASURE the SYS rail on silicon);** the buck-boost's
~1.9–2.4 A gives large margin (no DDR / RGB panel). All SYS-domain caps ≥10 V (SYS reaches 4.4 V).

---

## Open items + bring-up

- **LCD 5 V boost — RESOLVED** (§8, rows 68–73): the 2090 is breadboard-proven at **VIN = 5 V** and v2 has no
  5 V rail, so a **SYS→5 V MT3608 boost** (gated by the power switch (#55 → Q1 #80)) feeds the LCD VIN. Fully sourced.
- **HSE crystal — DECIDED: add it** (§9, #74/#75): an 8 MHz crystal + 2 load caps on PH0/PH1 (costs only those 2 pins)
  replaces the ±4 %-over-temp HSI as the PLL source, so the BT830 HCI framing is reliable over temperature.
  **Follow-up (firmware):** switch the RTOS PLL source HSI→HSE (the `board.dts` `hse` node is already in).
- **BT830 antenna keep-out** (layout): datasheet §12.1.1 = 40 mm top/bottom, 30 mm L/R — the 60 × 38.5 mm LiPo will
  violate it. Corner-mount the module, keep the battery/shield out of that quadrant, validate range in-enclosure.
- **`fpga/gameboy-v2.pcf`** (FPGA tooling): create it from ice40-breakout.pcf with the v2 map above (LCD D0–D7 reversed onto 36…25, LCD_RST = pin 37 open-drain);
  LITE is on STM32 PC6, not the FPGA. Keep config pins package-fixed.
- **Verify-on-arrival:** any *replacement* 2090 must have its IM solder jumpers SJ1–SJ4 open (8080 8-bit; un-bridge IM1–IM3 on a
  module set up for SPI); Adafruit 2011 charge C-rate; enclosure fit for the 2011 (~60 × 38.5 × 8.5 mm — see row 53) + the side-edge power switch (8 × 2.8 mm, lever through a shell-side slot).
- **Mounting holes:** layout-only (no net). Test points: see #83 (bare back-side pads, added 2026-10-10; the console header
  became test pads 2026-10-12). SWD stays a header (#13).
- **No DNP pads** (the flow places every part): the OSC_OUT series R is fitted as 0 Ω (#77); the optional VBUS TVS, I2C parallel
  pads and Class-D ferrite + 1 nF EMC pads are dropped.
- **Schematic capture (CSV flow):** tooling done (`manual:SolderPad_1x02` symbol + footprint added — gameboy-v3 uses it too;
  STM32 PC1 retyped bidirectional + the C49257 header pins passive for ERC; `gb2/` KiCad project created) and the CSVs are
  written + statically checked (pin coverage, kinds, counts vs this BOM, predicted pin-not-driven). **Next:** on a machine with
  `kicad-cli`, run `cad/generate_schematic.py --bom gb2-bom.csv --nets gb2-nets.csv --project projects/gameboy-v2/pcb/gb2` — its
  validate gate (ERC + netlist match) hasn't run yet.
- **Deep-discharge behavior:** on a fully depleted cell, USB-only power won't light the LCD until the cell charges past the BQ24073 DPPM/supplement region — expected, not a fault.
- **Firmware items surfaced by the hardware:** FPGA config + PPU commands both on SPI1, runtime select active-high on PA4, SPI1 fast while streaming the bitstream then back to the slow runtime rate (2026-10-12); PCA9555 INT-erratum workaround (#56);
  battery-% gating during charge (#51); long ADC sample time + VREFINT correction (#51/#52); PC3 high = forced PWM while running (#46); SPI3 / SPI1 SCK + MOSI GPIO speed = medium (OSPEEDR 01: ~10 ns edges, margin + less EMI); optional gateware: register `lcd_data` on the negative clock edge so data never switches on WR's edges (pixel loop currently changes both on the same edge; #79); BLE host stack (§7); RTOS softvol +
  ≤0.8 W cap (§3); IMU I3C-avoid pin config (#25); PC6 LITE open-drain, no pull, TIM3 PWM (#15); LCD_RST open-drain +
  SWRESET in the PPU init (#2); /CHG 2 Hz blink = charge-timer fault (#36).
- **Bring-up order:** power + iCE40 config (CDONE LED goes out) + NOR + SWD/UART console → screen (8080 solid
  background) → buttons (`evtest`-style diagonals) → audio (verify no clipping at 6 dB) → sensor I2C (IMU +
  haptics) → battery telemetry (ADC divider) → Bluetooth (BLE HCI on USART2) → measure true-off current — expect
  ≈ 40–43 µA (BAT divider ~21 µA + the EN2 internal pull-down on SYS_SRC ~15 µA + BQ24073 IBAT(PDWN) 4.3–6.5 µA, same for the whole family); PC0 doesn't back-feed.

## Related docs
- `../board.dts` — Nucleo bring-up pin map (console on USART2 = ST-Link VCP); gb2's UARTs are swapped (see Pinout notes #2) and need their own config.
- `../../gameboy/gameboy-pcb/BOM.md` — v1 (proven parts reused here).
- `../../ice40-breakout/BOM.md` + `README.md` — iCE40 core/power + 8080 LCD reference.
- `../../gameboy-v3/pcb/BOM.md` — sibling board; source of the shared feature parts (charger, buck-boost, IMU, haptics, LRA, PCA9555, BT830, battery, speaker).
- Datasheets: `../../../cad/docs/` (STM32F411RE, iCE40UP5K, ILI9341, MAX98357A, LSM6DSOX, DRV2605L, Vybronics, BQ2407x (bq24074.pdf = the family datasheet, BQ24073 included), TPS63021, PCA9555, BT830, W25Q128, CES-2704, MT3608 = C84817.pdf, KDS crystal = C57131.pdf, DT280QV10 panel, side power switch MSK12C02-HB = C431541.pdf (drawing + pinout) + C431541-spec.pdf, AO3401A = C15127.pdf).
- Adafruit 2090 Eagle schematics: `../../../cad/docs/adafruit-2090-cap-touch.sch` (original) + `-revD.sch` — source for the #15–#17 module facts (LDO, backlight, LITE/RD/RST circuits, JP1/JP2 pinouts).
