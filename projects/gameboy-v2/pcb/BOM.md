# gameboy-v2 PCB — Bill of Materials (WORKING DRAFT)

**Status:** Early draft / planning — we iterate on this. The integrated **gameboy-v2 handheld**:
STM32F411RE (CPU) + iCE40UP5K (PPU/graphics) driving an ILI9341, now adding battery, WiFi, haptics,
motion, and console-feel buttons.

**Target feature set (this revision):**

- OK **STM32F411RE** (Cortex-M4) — game logic, input, audio, configures the FPGA at boot
- OK **iCE40UP5K FPGA** — PPU; renders sprites -> **ILI9341 over parallel 8080** (screen hangs off the FPGA, not the STM32)
- OK **ILI9341** 2.4"/2.8" 320x240 SPI/parallel LCD (driven by the FPGA)
- OK **Audio** — MAX98357A I2S Class-D amp + speaker
- NEW **Haptics** — DRV2605L + LRA (vibration feedback)
- NEW **Motion** — LSM6DSOX 6-axis IMU (tilt)
- OK **NOR flash** — W25Q128 (audio/asset storage, on SPI3)
- NEW **WiFi** — ESP32-C3 "smart" module over UART (ESP-AT; MCU has no TCP/IP stack)
- NEW **Battery** — 1S LiPo, USB-C charge + power-path, buck-boost 3.3 V rail
- NEW **Battery % sense** — resistor divider -> STM32 ADC (PA0)
- NEW **Switch-style buttons** — conductive-silicone carbon-contact pads (D-pad + face), + **L/R bumpers**
- NEW **ST-Link V2 (SWD) header** for flashing
- OK **UART header** for debug / flash-upload

**Sourcing legend:**
- OK **Proven** — reused from `gameboy` (v1) or `ice40-breakout`; LCSC # confirmed, silicon-validated.
- WARN **Verify** — plausible pick / confirm LCSC #, stock, JLC tier before ordering.
- TODO — not yet chosen; we pick a concrete part together.

> **The #1 open question for v2 is the PIN BUDGET.** The F411RE is an **LQFP64 (~49 usable GPIO)** —
> much tighter than the T113 in gameboy-v3. The pin budget table below shows it *fits* (~38 of ~49)
> but with less headroom, and the button count is where it gets tight — see the I2C-expander option.

---

## 1. Core — MCU + FPGA + power + clock (mostly PROVEN)

| # | Group | Part | Description | Qty | LCSC # | Src |
|---|-------|------|-------------|-----|--------|-----|
| 1 | MCU | STM32F411RET6 | Cortex-M4, LQFP-64, 512 KB flash / 128 KB RAM | 1 | C94355 | OK |
| 2 | FPGA | iCE40UP5K-SG48I | Lattice PPU, QFN-48 7x7 mm | 1 | C2678152 | OK |
| 3 | Power | AP2127K-1.2TRG1 | 1.2 V LDO, SOT-23-5 — iCE40 **core** rail (VCC+VCCPLL) from 3.3 V | 1 | C151376 | OK |
| 4 | Power | GZ1608D601TF | Ferrite bead 600 ohm 0603 — 1.2 V -> VCCPLL filter | 1 | C1002 | OK |
| 5 | Clock | (none — HSI) | No HSE crystal; STM32 runs on 16 MHz internal HSI; iCE40 uses `SB_HFOSC` | — | — | OK |
| 6 | Decoupling | CC0603KRX7R9BB104 | 100 nF 0603 X7R (per-power-pin: STM32 x4, iCE40 x7, flash, amp, ...) | ~18 | C14663 | OK |
| 7 | Decoupling | CL21A106KOQNNNE | 10 uF 0805 (bulk rails + WiFi/audio transient reservoirs) | ~8 | C1713 | OK |
| 8 | Decoupling | CC0603KRX5R6BB475 | 4.7 uF 0603 (VCAP_1, iCE40 1.2 V bulk, VCCPLL) | ~4 | C109456 | OK |
| 9 | MCU support | RC0603JR-0710KL | 10 Kohm 0603 — BOOT0 pull-down, NRST, misc | ~3 | C99198 | OK |
| 10 | MCU support | CC0603KRX7R9BB103 | 10 nF 0603 — VDDA HF bypass / NRST filter | 1 | C100042 | OK |

> **iCE40 config bus (from v1/ice40-breakout):** SPI3 SCK/MOSI/MISO (PC10/11/12) + CRESET (PB1),
> CDONE (PB2), SS_B (PB6); CRESET 10 K pull-up, SS_B 10 K pull-down (slave-boot strap), CDONE 10 K
> pull-up. The STM32 streams the bitstream from its own flash at boot — carried over unchanged.

---

## 2. Screen — ILI9341 (driven by the FPGA)  OK

The FPGA renders to the ILI9341 over an **8-bit parallel 8080 bus** (see `ice40-breakout` docs). The
STM32 only sends sprite data to the FPGA over SPI1. So the screen wiring is **FPGA <-> ILI9341**.

| # | Role | Part | LCSC # | Interface | Src | Note |
|---|------|------|--------|-----------|-----|------|
| 11 | LCD | ILI9341 2.4"/2.8" 320x240 module | module | 8080 parallel <- FPGA | OK | External module on FPC/header (as today). Backlight + VIN — see power note below. |
| 12 | LCD connector | TODO — FPC/header for the ILI9341 module | verify | mechanical | TODO | Match the module (Adafruit EyeSPI 18-pin or the bare panel FPC). |

> **Power note (battery change):** v1 fed the ILI9341 VIN + MAX98357A from **5 V**. On battery there is
> no 5 V rail — per the battery analysis, **both run directly from the battery/SYS node (3.0-4.2 V)**,
> which is in-spec for both (ILI9341 module 3-5 V; MAX98357A 2.5-5.5 V). Only the 3.3 V logic rail is
> regenerated (buck-boost, section 5). Backlight LED runs off 3.3 V or SYS via a resistor as today.

---

## 3. Audio — I2S Class-D amp + speaker  OK

| # | Role | Part | LCSC # | Interface | Src | Note |
|---|------|------|--------|-----------|-----|------|
| 13 | Class-D amp | MAX98357AETE+T | C910544 | I2S2 (PB12 WS / PB13 SCK / PB15 SD) | OK | Runs from SYS (battery domain), not 5 V. Same as v1. |
| 14 | Amp support | 1 Mohm (SD/MODE) + 100 Kohm (GAIN_SLOT) + PVDD caps | C105578 / C14675 | — | OK | (L+R)/2 mode, 15 dB gain — v1 network. |
| 15 | Speaker | TODO — 8 ohm 2 W micro speaker + small sealed cavity | — | amp OUT+/- | TODO | 8 ohm safer on 1S battery. Enclosure baffle matters most for quality. |
| 16 | Speaker connector | TODO — JST-PH/MX1.25 2-pin (match pigtail) | verify | mechanical | TODO | v1 used MX1.25 C3029359. |

> **Audio-quality note (from v1 review):** v1 sounded poor mainly due to **8-bit PCM** + tiny open
> speaker. For v2, switch the asset pipeline to **16-bit PCM** (MAX98357A is 16-bit native) and use a
> speaker in a sealed cavity — biggest wins, software + mechanical, not the amp.

---

## 4. Motion (IMU) + Haptics  NEW  (share one I2C bus)

Both are I2C peripherals on a **shared I2C bus** (STM32 I2C1 on PB8/PB9 — v1 noted these have I2C
pull-ups). Different addresses, no clash.

| # | Role | Part | LCSC # | Interface | Src | Note |
|---|------|------|--------|-----------|-----|------|
| 17 | IMU (6-axis) | LSM6DSOXTR | **C481766** OK | I2C 0x6A/0x6B + INT | OK | Confirmed in JLC stock. Accel+gyro for tilt. Adafruit LSM6DSOX breakout for breadboard bring-up. |
| 18 | Haptic driver | DRV2605LDGSR | **C527464** OK | I2C 0x5A, EN->3V3 | OK | Confirmed in JLC stock. Motor wires straight to OUT+/- (no FET/flyback). Adafruit #2305 breakout for bring-up. |
| 19 | Actuator (LRA) | Vybronics VG0832022D — 8 mm coin LRA, 235 Hz, 1.8 VAC | [DigiKey 9974288](https://www.digikey.com/en/products/detail/vybronics-inc/VG0832022D/9974288) | via DRV2605L OUT+/- | OK | **Record 235 Hz -> DRV2605L config.** Hand-added mechanical part; leads -> 2-pin connector (#20). ERM coin (Adafruit #1201) for bench test. |
| 20 | Motor connector | TODO — 2-pin JST-SH/PH on DRV2605L OUT+/- | verify | mechanical | TODO | Connector (not reflow pads) so the LRA couples to the enclosure for good feel. |
| 21 | IMU INT | (uses 1 GPIO, e.g. PB10) | — | EINT | TODO | Optional data-ready interrupt. |
| 22 | I2C pull-ups | 2x 2.2-4.7 Kohm 0603 (SDA/SCL) | C99198 (verify value) | — | WARN | Mandatory for the open-drain bus. |

---

## 5. Battery + Charging + Power-Path  NEW  (resolves the v1 "5 V-vs-cell" tension)

1S LiPo, USB-C charge + run (power-path). **Kills the 5 V rail** — display/amp run from SYS; only the
3.3 V logic rail is regenerated by a buck-boost. (Full rationale = the battery design analysis.)

| # | Role | Part (candidate) | LCSC # | Interface | Src | Note |
|---|------|------------------|--------|-----------|-----|------|
| 23 | USB-C connector | TYPE-C-31-M-12 | C165948 | 5 V in (power) | OK | Power-only; CC resistors advertise 5 V. |
| 24 | USB-C CC pulldowns | 5.1 Kohm 0603 x2 | C105580 | CC1/CC2 | OK | Assume 500 mA USB budget (no PD). |
| 25 | Power-path charger | WARN BQ24074RGTR (alt MCP73871) | verify | autonomous; opt /CHG,/PGOOD | WARN | True DPPM power-path + programmable **input** current limit (~500 mA). System on SYS/OUT, not BAT. |
| 26 | 3.3 V buck-boost | WARN TPS63021DSJR (fixed 3.3 V) | C202140 (verify) | battery -> 3.3 V logic | WARN | 1S cell straddles 3.3 V -> buck-**boost** (replaces v1's ME6211 LDO). WARN NOT C64595. |
| 27 | Buck-boost inductor | WARN ~1.5 uH shielded, >=2 A | verify | — | WARN | Per TPS63021 datasheet. |
| 28 | LiPo cell | TODO — **protected** 1S LiPo pouch, JST-PH, ~1200-2000 mAh | — | JST-PH | TODO | **Protected pouch (integral PCM) = #1 safety item.** Verify polarity! |
| 29 | Battery connector | WARN JST-PH 2.0 mm 2-pin | C160404 (verify) | mechanical | WARN | Distinct from speaker connector. |
| 30 | Fuel-gauge divider | OK 2x 100 Kohm 1% -> STM32 ADC (PA0) | C22807-class | ADC1_IN0 (PA0) | OK | **STM32 ADC VREF = 3.3 V** (NOT 1.8 V like T113) -> 2:1 divider maps 4.2 V->2.10 V. + 100 nF at PA0. Driver already polls adc1 ch0. |

> **Battery % firmware:** the STM32 ADC path was fully specified in the battery analysis — a `battery.c`
> task: `adc_read()` -> mV (`raw*6600/4095` for the 2:1 divider) -> SoC via a LiPo OCV LUT, IIR-filtered,
> shown on-screen via the PPU digit tiles. 12-bit, 3.3 V VREF.

---

## 6. Controls — Switch-style buttons + L/R bumpers  NEW

**Feel:** D-pad + face buttons use **conductive-silicone carbon-contact pads** (the Game Boy/Switch
mechanism: a silicone pad with a carbon pill bridges interleaved copper "combs" on the PCB — the traces
*are* the switch, JLC places nothing). Start/Select + **L/R bumpers** use tactiles + a shell keycap.

> **WARN Board-finish consequence:** carbon-contact pads require **whole-board ENIG** (HASL's domed surface
> can't seat a flat carbon pill — flatness, not oxidation). And **buy the silicone donor pads first, then
> draw the comb footprint to match** (the copper + the enclosure pivot are locked to the specific pad).

| # | Role | Part (candidate) | LCSC # | Interface | Src | Note |
|---|------|------------------|--------|-----------|-----|------|
| 31 | D-pad — carbon pads x4 | copper footprint (custom) + silicone D-pad | — | 4x combs -> GPIO (EXTI) | TODO | Silicone: generic GB DMG/GBC conductive rubber D-pad (~$1). No placed part. |
| 32 | Face buttons — carbon pads x4 | copper footprint + silicone ABXY pad | — | 4x combs -> GPIO | TODO | Silicone: Switch-Pro ABXY conductive pad (~$0.6-0.9). |
| 33 | Start / Select x2 | tactile + shell keycap | C318884 | GPIO -> GND | TODO | Reuse v1 6x6 tactile. |
| 34 | L / R bumpers x2 | tactile (side-actuated or top + shell lever) | C318884 (verify axis) | GPIO -> GND | TODO | NEW shoulder buttons — new for v2. Edge-mounted. |
| 35 | Per-button pull-ups | WARN resistor array (button lines) | verify | GPIO <- 3V3 | WARN | Carbon pill closed-R forms a divider -> define the pressed level. (v1 used internal pull-**downs** + rising-edge; carbon pads change this — revisit.) |

> **Button count vs pins:** v1 wired 8 buttons on PC0-PC7 (direct GPIO EXTI). Adding **L/R = 10**, and a
> full Switch layout (D-pad 4 + ABXY 4 + Start/Select 2 + L/R 2 = **12**) pushes the LQFP64 pin budget.
> **Option to consider:** move buttons to a **PCA9555 I2C GPIO expander** (16 IO on the shared I2C bus,
> ~2 pins + INT) — like gameboy-v3. Reclaims ~10 GPIO and eases the budget. Decision TODO — see pin table.

---

## 7. WiFi — ESP32-C3 "smart" module over UART  NEW

The STM32 has no OS/TCP-IP stack, so v2 needs a **smart** module (stack onboard), NOT a dumb SDIO radio.
ESP32-C3 running **ESP-AT** firmware; the STM32 issues AT commands over UART.

| # | Role | Part (candidate) | LCSC # | Interface | Src | Note |
|---|------|------------------|--------|-----------|-----|------|
| 36 | WiFi module | WARN **ESP32-C3-MINI-1** (ESP-AT firmware) | verify | UART (USART6 PA11/PA12) + EN GPIO | WARN | Onboard PCB antenna, TLS onboard, reflow-friendly. STM32 side = light AT parser (few KB — 128 KB RAM ample). ESP-01S = cheap fallback. |
| 37 | WiFi decoupling | 10 uF + 0.1 uF at module | C1713 / C14663 | — | WARN | ESP TX bursts ~300-500 mA -> bulk cap or WiFi browns out. |

> **WARN UART pin question (v2's real constraint):** the only free UART is **USART6**. Its pins are either
> PC6/PC7 (= Start/Select buttons — taken) or **PA11/PA12 (= USB-OTG pins)**. v2 uses **no USB**, so
> PA11/PA12 are free *at the chip level* — but **confirm the PCB doesn't route them** (needs the
> schematic). Fallback: free **USART2** (the ST-Link-VCP console) per the existing `board.dts` TODO to
> move the console to USART1. No hardware flow control either way -> run 2-wire @115200 + a bigger RX ring.
> **v2 WiFi use cases are thin+ but real:** high-score POST, NTP, cloud saves (save-RAM blob -> W25Q128),
> LAN netplay (a `udp.c` already exists in the RTOS). Not the rich Linux features — those live on v3.

---

## 8. Programming + debug headers  NEW/OK

| # | Role | Part | LCSC # | Interface | Src | Note |
|---|------|------|--------|-----------|-----|------|
| 38 | **ST-Link V2 / SWD header** | 1x5 2.54 mm pin header | C358687 | SWDIO PA13 / SWCLK PA14 / NRST / 3V3 / GND | NEW | Flashing + debug. Standard 5-pin SWD pinout. |
| 39 | **UART header** | 1x3 2.54 mm pin header | C49257 | TX / RX / GND (USART1 PA9/PA10) | OK | Debug + flash-upload path (as v1). |
| 40 | Status LED | Green LED 0603 + 510 ohm | C2986027 / C23193 | GPIO / power | OK | Power/status indicator. |

---

## Pin budget — the key constraint

**STM32F411RE = LQFP64 ~= 49 usable GPIO** (PA0-15, PB0-15, PC0-15, PD2; PC14/15 limited, PH0/1 unused
since HSI). Verdict: **everything fits (~38 of ~49) but tighter than v3 — buttons are the pressure point.**

| Subsystem | Pins | Which |
|-----------|------|-------|
| iCE40 PPU (SPI1) | 4 | PA4-PA7 |
| iCE40 config + NOR (SPI3) | 7 | PC10/11/12 + PB0/PB1/PB2/PB6 |
| Audio (I2S2) | 3 | PB12/PB13/PB15 |
| Buttons (8, direct GPIO) | 8 | PC0-PC7 |
| L/R bumpers | 2 | e.g. PC8/PC9 |
| Shared I2C (IMU+haptics) | 2 | PB8/PB9 |
| IMU INT | 1 | e.g. PB10 |
| WiFi UART (USART6) + EN | 3 | PA11/PA12 + 1 GPIO |
| Battery ADC | 1 | PA0 |
| SWD header | 2 | PA13/PA14 |
| UART header (USART1) | 2 | PA9/PA10 |
| Console (USART2) | 2 | PA2/PA3 *(may free for WiFi)* |
| Haptics / charger | 0 | (I2C / autonomous) |
| **Total** | **~37-39** | of ~49 |

**Key constraints to respect:**

1. **No spare SPI** — SPI1 = PPU (every frame), SPI2 = I2S audio, SPI3 = flash+FPGA-config. New
   peripherals go on **I2C / UART / GPIO** only (this is why WiFi is UART, IMU/haptics are I2C).
2. **WiFi UART = USART6 on PA11/PA12** (USB pins, free since no USB) — *confirm PCB routing*; else free USART2.
3. **Buttons are the pin pressure** — a full Switch layout (12) fits on direct GPIO but leaves little
   headroom; a **PCA9555 I2C expander** (section 6 note) reclaims ~10 GPIO and is worth considering.
4. **Kill the 5 V rail** — display + amp run from SYS (battery domain); buck-boost makes 3.3 V; AP2127K
   makes the iCE40 1.2 V core. Replaces v1's ME6211 LDO.
5. **STM32 ADC VREF = 3.3 V** (unlike T113's 1.8 V) -> battery divider is a clean 2:1 (100 K/100 K).
6. **Carbon-contact buttons -> whole-board ENIG** + per-button pull-ups.
7. **Include VBUS/SYS bulk caps** for WiFi + audio transients (brownout guard).

---

## Immediate TODOs (iterate together)

- [ ] **Pin plan:** decide **buttons direct-GPIO vs PCA9555 I2C expander** (drives the whole GPIO budget);
      assign final pins for L/R, IMU INT, WiFi EN.
- [ ] **Confirm WiFi UART pins** — verify PA11/PA12 are unrouted on the PCB, else plan to free USART2.
- [ ] Confirm **BQ24074 / TPS63021 / JST-PH / ESP32-C3-MINI-1** LCSC #s + JLC tier live (#25/#26/#29/#36).
- [ ] **Buttons (feel):** buy GB D-pad + Switch-Pro ABXY silicone pads first, then draw carbon-contact
      footprints to match; set board finish **ENIG**; add per-button pull-ups; pick L/R bumper switch.
- [ ] Pick **speaker** + enclosure (#15) and connectors (#12/#16/#20/#29).
- [ ] Decide screen: keep the **ILI9341** (FPGA-driven) as-is for v2.
- [ ] Confirm the **I2C bus** (PB8/PB9) pull-up value and that all addresses are distinct
      (LSM6DSOX 0x6A, DRV2605L 0x5A, +PCA9555 0x20 if used).

**Related:**
- `../../gameboy/gameboy-pcb/BOM.md` — v1 BOM (proven parts reused here).
- `../board.dts` — current v2 pin map (SPI1 PPU, SPI3 flash/FPGA, I2S2 audio, USART1/2, ADC, buttons).
- `../../ice40-breakout/` — the iCE40 config/power reference (core rail, boot straps).
- `../../gameboy-v3/pcb/BOM.md` — sibling Linux handheld BOM (shared parts: IMU, haptics, battery, WiFi approach).
