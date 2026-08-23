# gameboy-v3 — T113-S3 Pin Assignment (schematic-capture reference)

Complete SoC pin → net → destination map for the gameboy-v3 mainboard. Companion to [`BOM.md`](BOM.md).

**Sources (all authoritative):**
- **Ball map + physical pin numbers** — the silicon-proven [`../t113-breakout/PINOUT.md`](../t113-breakout/PINOUT.md) (same T113-S3 SoC / eLQFP-128 → identical ball→GPIO; only destinations differ here).
- **Signal-per-pin (mux)** — mainline `pinctrl-sun20i-d1.c` + `sunxi-d1s-t113.dtsi` pin groups (`lcd_rgb666_pins`, `mmc0_pins`, `spi0_pins`, `uart3_pb_pins`, driver mux options for i2c1/i2c2/i2s1/uart0).
- **Net destinations** — this project's [`BOM.md`](BOM.md).

**Status:** buses + fixed-function pins are firm. **Single-GPIO pins marked *(proposed)* are free-pin picks to confirm at capture.** Two items are copper-committing and still open — see **Open items**.

---

## Committed buses

| Bus | SoC pins | Signal → net | Destination |
|-----|----------|--------------|-------------|
| **RGB666 LCD** (TCON-LCD0) | PD0–17 data, PD18=DCLK, PD19=DE, PD20=HSYNC, PD21=VSYNC, **PD22=DISP** | see RGB666 table below | Zettler panel #25 via display FPC #26 (C9160) |
| **SPI0-NOR** | PC2=CLK, PC3=CS, PC4=MOSI/IO0, PC5=MISO/IO1, PC6=WP/IO2, PC7=HOLD/IO3 | boot flash | W25Q128 #11. ⚠️ PC4/PC5 are also BOOT-SEL0/1 straps at reset |
| **microSD SDC0** (mmc0) | PF0=DAT1, PF1=DAT0, PF2=CLK, PF3=CMD, PF4=DAT3, PF5=DAT2 | SD 4-bit | socket #12 + ESD #13 + pull-ups #14 |
| **UART0 console** | PE2=TX, PE3=RX | debug console | header/test-point (console=ttyUSB1 on rig) |
| **BT UART3 (HCI)** | PG0=TX, PG1=RX, PG2=RTS, PG3=CTS | 4-wire HCI | BT830 #99 (chosen over UART1 — its flow-ctrl PG6-9 clashes i2c1) |
| **I2S1 audio** | PG12=LRCK, PG13=BCLK, PG15=DOUT; PG11=MCLK *(unused→static)*, PG14=DIN *(unused)* | I2S out (multi-drop) | MAX98357A amp #47 + PCM5102A DAC #103 (both receivers), series-R #104 |
| **i2c1** (peripheral bus) | **PG8=SCL, PG9=SDA** | shared I2C, ≥400 kHz | PCA9555 0x20 #85 · MAX17048 0x36 #77 · FT7311 0x38 #25/#27 · INA226 0x40 #81 · DRV2605L 0x5A #41 · pull-ups #97 |
| **i2c2** (IMU only) | **PE4=SCL, PE5=SDA** | dedicated I2C | LSM6DSOX 0x6A #35 |
| **GPADC0** | dedicated ADC ball (pin 101) | battery voltage (backup) | divider #75/#76 midpoint |
| **USB0 (USB-C)** | USB0-DP=pin115, USB0-DM=pin114 | USB2 + charge | receptacle #2 + ESD #98 + CC→CH224K #61 |

---

## RGB666 data mapping — the copper-critical part

The **T113 side is fixed** by `lcd_rgb666_pins` (mainline): it drives the **top 6 bits of each LCD0 channel** (drops D0/D1, D8/D9, D16/D17 — that's what makes it RGB666 vs the RGB888 bus). Each channel is a contiguous PD run, low→high:

| PD pin | LCD0 signal | bit within channel | Allwinner convention* |
|--------|-------------|--------------------|-----------------------|
| PD0 | LCD0-D2 | ch-A LSB (bit2) | Blue[2] |
| PD1 | LCD0-D3 | | Blue[3] |
| PD2 | LCD0-D4 | | Blue[4] |
| PD3 | LCD0-D5 | | Blue[5] |
| PD4 | LCD0-D6 | | Blue[6] |
| PD5 | LCD0-D7 | ch-A MSB (bit7) | Blue[7] |
| PD6 | LCD0-D10 | ch-B LSB | Green[2] |
| PD7 | LCD0-D11 | | Green[3] |
| PD8 | LCD0-D12 | | Green[4] |
| PD9 | LCD0-D13 | | Green[5] |
| PD10 | LCD0-D14 | | Green[6] |
| PD11 | LCD0-D15 | ch-B MSB | Green[7] |
| PD12 | LCD0-D18 | ch-C LSB | Red[2] |
| PD13 | LCD0-D19 | | Red[3] |
| PD14 | LCD0-D20 | | Red[4] |
| PD15 | LCD0-D21 | | Red[5] |
| PD16 | LCD0-D22 | | Red[6] |
| PD17 | LCD0-D23 | ch-C MSB | Red[7] |
| PD18 | LCD0-CLK | DCLK | — |
| PD19 | LCD0-DE | DE | — |
| PD20 | LCD0-HSYNC | HSYNC | — |
| PD21 | LCD0-VSYNC | VSYNC | — |

\* **The R/G/B identity of each channel is NOT in the kernel tree** — the pinctrl driver + DRM only label pins `LCD0-Dn` and select a bus-format (`RGB666_1X18`); they never say which Dn is red/green/blue. The convention column is the Allwinner user-manual default (D[7:0]=Blue, D[15:8]=Green, D[23:16]=Red). **Confirm against the T113-S3 datasheet TCON section before trusting it.**

**→ The panel-side mapping is the copper-committing unknown.** Get **Zettler's R/G/B bit-to-pin map**, then wire: for each color, T113 `D7`(MSB, the high-PD-pin) → panel `bit-MSB`, down to `D2`(LSB) → panel `bit-LSB`. Convention-safe default = drive the panel's **6 MSBs (R7..R2)**, tie its 2 LSBs (R1/R0) to GND. Grounding the wrong pair (panel MSBs) caps each channel at ~25 % → dark/no white. **Do not commit RGB copper until Zettler's map (or a bench test) confirms both the R/G/B channel order and the panel's bit order.**

---

## Single GPIOs — *(proposed free-pin picks; confirm at capture)*

Free banks after the buses: PE0/1/6-13, PB2-7, PF6, PG4/5/6/7/10/14 (~23 pins). Proposed:

| Signal | Proposed pin | Destination | Note |
|--------|-------------|-------------|------|
| Panel **DISP** | **PD22** *(fixed)* | display FPC #26 pin 31 + 10 KΩ pull-up | display-on; floats dark otherwise |
| Backlight **CTRL** | PE6 | TPS61165 #28 | PWM-capable GPIO (bit-banged EasyScale/pwm-gpio) |
| **PCA9555-INT** | PE7 | expander #85 INT + pull-up #89 | EINT, interrupt-driven |
| **touch-INT** | PE8 | touch FFC #27 pin 5 → FT7311 | EINT |
| **touch-RST** | PE9 | touch FFC #27 pin 6 → FT7311 | output |
| **power-btn EINT** | PE10 | button #92 (also drives STM6601 PB) | **must be a native wakeup-capable EINT** (verify) |
| **STM6601 PSHOLD** | PE11 | #94 pin 4 | `gpio-poweroff` handshake |
| **jack-detect** | PE12 | jack #115 detect switch | EINT; `SW_HEADPHONE_INSERT` |
| **speaker-SD** | PE13 | MAX98357A #47 SD/MODE (shares 680 KΩ #48 net) | **open-drain/hi-Z** (hi-Z=mono run, low=mute) |
| **HP-EN** | PB2 | TPA6132A2 #110 EN (+ pull-down #114) | boots off; assert after DAC settles |
| **PCM5102A XSMT** | PB3 | DAC #103 pin 17 | soft-mute when unplugged (not hard-high) |
| **charger /PGOOD** | PB4 | BQ24074 #53 + pull-up #84 | input, open-drain |
| **MAX17048 ALRT** | PB5 | gauge #77 pin 5 | input, needs pull-up |
| **BT VREG_EN_RST#** | PB6 | BT830 #99 pin 8 | boot low, >5 ms, then high; ≤4.7 KΩ if strapped |
| **IMU INT1** | PE0 | LSM6DSOX #35 INT1 (+ 10 KΩ pulldown #40) | EINT (motion/FIFO-watermark IRQ); the pulldown also guarantees I2C-not-I3C at POR |
| **IMU INT2** | PE1 | LSM6DSOX #35 INT2 | EINT (optional 2nd IRQ; leave for data-ready/wake) |

Leaves spare: PB7, PF6, PG4, PG5, PG6, PG7, PG10, PG14 (~8 GPIO).

*Optional/not-yet-assigned (readable another way, add a GPIO only if wanted):* charger **/CHG** (already on the LED node #79) and **INA226 ALERT** (INA226 is polled via I2C hwmon) — neither needs a pin; assign from the spares if you want them as interrupts.

---

## Full pin table (eLQFP-128, physical order)

Legend: **gpio** used · *fixed-fn* · `NC` no-connect. Nets are gameboy-v3 (differ from the breakout's probe-header nets).

| Pin | GPIO/Ball | Signal (mainline) | gameboy-v3 net → destination |
|----:|-----------|-------------------|------------------------------|
| 1 | PG6 | — | *spare* |
| 2 | PG7 | — | *spare* |
| 3 | PG8 | TWI1-SCK | **i2c1 SCL** → bus (#85/#77/#25/#81/#41) + pull-up #97 |
| 4 | PG9 | TWI1-SDA | **i2c1 SDA** → bus + pull-up #97 |
| 5 | PG10 | — | *spare* |
| 6 | PG11 | I2S1-MCLK | I2S1 MCLK **unused → tie as static GPIO** |
| 7 | PF0 | MMC0-D1 | SD DAT1 → socket #12 |
| 8 | PF1 | MMC0-D0 | SD DAT0 → socket #12 |
| 9 | PF2 | MMC0-CLK | SD CLK → socket #12 |
| 10 | PF3 | MMC0-CMD | SD CMD → socket #12 |
| 11 | PF4 | MMC0-D3 | SD DAT3 → socket #12 |
| 12 | PF5 | MMC0-D2 | SD DAT2 → socket #12 |
| 13 | PF6 | — | *spare* |
| 14 | PC7 | SPI0-HOLD/IO3 | NOR #11 |
| 15 | PC6 | SPI0-WP/IO2 | NOR #11 |
| 16 | PC5 | SPI0-MISO/IO1 | NOR #11 — ⚠️ also **BOOT-SEL1** strap |
| 17 | PC4 | SPI0-MOSI/IO0 | NOR #11 — ⚠️ also **BOOT-SEL0** strap |
| 18 | PC3 | SPI0-CS0 | NOR #11 |
| 19 | PC2 | SPI0-CLK | NOR #11 |
| 20 | *LDOA_OUT* | internal LDO-A | bypass cap → GND |
| 21 | `REFCLK-OUT` | — | **NC** (WiFi clock fanout; no WiFi) |
| 22 | *DXOUT* | 24 MHz xtal | crystal #15 |
| 23 | *DXIN* | 24 MHz xtal | crystal #15 |
| 24 | *X32KOUT* | 32.768 kHz xtal | RTC crystal #16 |
| 25 | *X32KIN* | 32.768 kHz xtal | RTC crystal #16 |
| 26 | *LDOA_OUT* | internal LDO-A | bypass → GND |
| 27 | *RESET* | reset | RESET btn #22 + 100 nF; **10 KΩ pull-up → 1.8 V VCC-RTC (NOT 3.3 V)** |
| 28 | *LDOA_OUT* | internal LDO-A | bypass → GND |
| 29 | *VCC-IO* | 3.3 V supply | +3V3 |
| 30 | *LDOB_OUT* | internal LDO-B (1.5 V DDR) | bypass → GND |
| 31 | PE13 | (GPIO) | **speaker-SD** *(proposed)* → MAX98357A #47 |
| 32 | PE12 | (GPIO) | **jack-detect** *(proposed)* → jack #115 |
| 33 | PE3 | UART0-RX | **console RX** |
| 34 | *VCC* | 3.3 V supply | +3V3 |
| 35 | PE2 | UART0-TX | **console TX** |
| 36 | PE11 | (GPIO) | **STM6601 PSHOLD** *(proposed)* → #94 |
| 37 | PE10 | (GPIO/EINT) | **power-btn EINT** *(proposed)* → #92/#94 |
| 38 | PE9 | (GPIO) | **touch-RST** *(proposed)* → #27 |
| 39 | PE8 | (GPIO) | **touch-INT** *(proposed)* → #27 |
| 40 | PE7 | (GPIO) | **PCA9555-INT** *(proposed)* → #85 |
| 41 | PE6 | (GPIO) | **backlight-CTRL** *(proposed)* → #28 |
| 42 | PE5 | TWI2-SDA | **i2c2 SDA** → IMU #35 |
| 43 | PE4 | TWI2-SCK | **i2c2 SCL** → IMU #35 |
| 44 | PE0 | (GPIO/EINT) | **IMU-INT1** *(proposed)* → LSM6DSOX #35 (+ 10 KΩ pulldown #40) |
| 45 | PE1 | (GPIO/EINT) | **IMU-INT2** *(proposed)* → LSM6DSOX #35 |
| 46 | *VDD-CORE* | 0.9 V core | +0V9 |
| 47 | *DZQ* | DDR ZQ-cal | 240 Ω #10 → GND (DDR is in-package SiP) |
| 48 | *LDOB_OUT* | internal LDO-B | bypass → GND |
| 49 | *LDOB_OUT* | internal LDO-B | bypass → GND |
| 50 | *LDOA_OUT* | internal LDO-A | bypass → GND |
| 51 | *VDD-CORE* | 0.9 V core | +0V9 |
| 52 | PD22 | (GPIO) | **panel DISP** → #26 pin 31 + 10 KΩ pull-up |
| 53 | PD21 | LCD0-VSYNC | RGB VSYNC → panel |
| 54 | PD20 | LCD0-HSYNC | RGB HSYNC → panel |
| 55 | PD0 | LCD0-D2 | RGB data (ch-A LSB) → panel |
| 56 | PD1 | LCD0-D3 | RGB data → panel |
| 57 | PD2 | LCD0-D4 | RGB data → panel |
| 58 | PD3 | LCD0-D5 | RGB data → panel |
| 59 | PD4 | LCD0-D6 | RGB data → panel |
| 60 | PD5 | LCD0-D7 | RGB data (ch-A MSB) → panel |
| 61 | PD6 | LCD0-D10 | RGB data (ch-B LSB) → panel |
| 62 | PD7 | LCD0-D11 | RGB data → panel |
| 63 | PD8 | LCD0-D12 | RGB data → panel |
| 64 | PD9 | LCD0-D13 | RGB data → panel |
| 65 | *LDOA_OUT* | internal LDO-A | bypass → GND |
| 66 | *VCC* | 3.3 V supply | +3V3 |
| 67 | PD10 | LCD0-D14 | RGB data → panel |
| 68 | PD11 | LCD0-D15 | RGB data (ch-B MSB) → panel |
| 69 | PD13 | LCD0-D19 | RGB data → panel |
| 70 | PD12 | LCD0-D18 | RGB data (ch-C LSB) → panel |
| 71 | PD14 | LCD0-D20 | RGB data → panel |
| 72 | PD15 | LCD0-D21 | RGB data → panel |
| 73 | PD16 | LCD0-D22 | RGB data → panel |
| 74 | PD17 | LCD0-D23 | RGB data (ch-C MSB) → panel |
| 75 | PD18 | LCD0-CLK | **RGB DCLK** → panel (DNP 0 Ω series-R here) |
| 76 | PD19 | LCD0-DE | **RGB DE** → panel |
| 77 | *VCC* | 3.3 V supply | +3V3 |
| 78 | `TVOUT0` | — | **NC** (CVBS out) |
| 79 | PB7 | — | *spare* |
| 80 | PB6 | (GPIO) | **BT VREG_EN_RST#** *(proposed)* → #99 pin 8 |
| 81 | *VDD-CORE* | 0.9 V core | +0V9 |
| 82 | PB5 | (GPIO) | **MAX17048 ALRT** *(proposed)* → #77 |
| 83 | *VCC* | 3.3 V supply | +3V3 |
| 84 | PB4 | (GPIO) | **charger /PGOOD** *(proposed)* → #53/#84 |
| 85 | PB3 | (GPIO) | **PCM5102A XSMT** *(proposed)* → #103 |
| 86 | PB2 | (GPIO) | **HP-EN** *(proposed)* → #110 |
| 87 | `MICIN3P` | — | **NC** (mic in) |
| 88 | `MICIN3N` | — | **NC** (mic in) |
| 89 | *LDOA_OUT* | internal LDO-A | bypass → GND |
| 90 | *VRA2* | codec analog ref | 100 nF → GND (codec unused; match breakout) |
| 91 | *GND* | ground | GND |
| 92 | *VRA1* | codec analog ref | 100 nF → GND |
| 93 | `FMINR` | — | **NC** (FM in) |
| 94 | `FMINL` | — | **NC** (FM in) |
| 95 | `LINEINR` | — | **NC** (line in) |
| 96 | `LINEINL` | — | **NC** (line in) |
| 97 | *LDOA_OUT* | internal LDO-A | bypass → GND |
| 98 | `HPOUTR` | — | **NC** (internal codec HP out — unused; external DAC #103 instead) |
| 99 | `HPOUTL` | — | **NC** |
| 100 | `HPOUTFB` | — | **NC** |
| 101 | GPADC0 | ADC in | **battery sense** → divider #75/#76 midpoint |
| 102 | `TP-X1` | — | **NC** (resistive-touch ADC) |
| 103 | `TP-X2` | — | **NC** |
| 104 | `TP-Y1` | — | **NC** |
| 105 | `TP-Y2` | — | **NC** |
| 106 | `NC0` | — | **NC** |
| 107 | *VCC* | 3.3 V supply | +3V3 |
| 108 | `TVIN0` | — | **NC** (CVBS in) |
| 109 | `TVIN1` | — | **NC** |
| 110 | `TVIN-VRP` | — | **NC** |
| 111 | `TVIN-VRN` | — | **NC** |
| 112 | `USB1-DP` | — | **NC** (USB1 host unused) |
| 113 | `USB1-DM` | — | **NC** |
| 114 | *USB0-DM* | USB2 D− | USB-C D− (90 Ω diff) → #2 via ESD #98 |
| 115 | *USB0-DP* | USB2 D+ | USB-C D+ (90 Ω diff) → #2 via ESD #98 |
| 116 | *VDD-CORE* | 0.9 V core | +0V9 |
| 117 | *VDD-CORE* | 0.9 V core | +0V9 |
| 118 | PG1 | UART3-RX | **BT RX** → #99 |
| 119 | PG2 | UART3-RTS | **BT RTS** → #99 |
| 120 | PG0 | UART3-TX | **BT TX** → #99 |
| 121 | PG3 | UART3-CTS | **BT CTS** → #99 |
| 122 | PG5 | — | *spare* |
| 123 | PG4 | — | *spare* |
| 124 | PG12 | I2S1-LRCK | **I2S1 LRCK** → amp #47 + DAC #103 |
| 125 | PG13 | I2S1-BCLK | **I2S1 BCLK** → amp #47 + DAC #103 |
| 126 | PG14 | I2S1-DIN0 | I2S1 DIN **unused → spare/static** |
| 127 | PG15 | I2S1-DOUT0 | **I2S1 DOUT** → amp #47 DIN + DAC #103 DIN |
| 128 | *VCC* | 3.3 V supply | +3V3 |
| 129 | *GND pad* | thermal/GND | exposed die-attach pad → GND + via array |

**Power/GND ball summary:** +3V3 = pins 29,34,66,77,83,107,128 · +0V9 core = 46,51,81,116,117 · internal LDO-A bypass = 20,26,28,50,65,89,97 · internal LDO-B (1.5 V DDR) bypass = 30,48,49 · GND = 91 + pad 129. Per-supply-pin 100 nF (#19). *(The breakout tied all +3V3 balls to one rail; if the datasheet distinguishes VCC-IO / VCC-PLL / AVCC, split at capture.)*

---

## Open items — resolve before committing copper

1. **RGB666 R/G/B channel order + panel bit order** — the T113 PD→LCD0-Dn is fixed (table above), but which Dn-run is R/G/B (convention only) and how the panel numbers its bits are NOT resolved. **Get Zettler's bit-to-pin map or bench-test the panel.** Copper-committing.
2. **Single-GPIO pin picks** — the *(proposed)* PE/PB assignments are free-pin defaults; confirm/adjust at capture. Avoid the non-bonded phantom pins **PC0/1, PB0/1/8-12, PE14-17, PG16-18**. Verify the power-btn pin is a wakeup-capable EINT.
3. **FPC contact-side** — #26 (display) mounts top face, #27 (touch) bottom face (opposite orientation); test-fit before locking footprints + pin-1.
4. **FT7311 touch I2C address** (0x38 assumed) — confirm on the panel via `i2cdetect`.
5. **RESET pull-up rail** — to **1.8 V VCC-RTC**, not 3.3 V (confirm the breakout's actual net).
6. **VRA1/VRA2 + AVCC** — internal codec unused; keep 100 nF bypass (matches breakout) and confirm the AVCC supply pin handling at capture.
