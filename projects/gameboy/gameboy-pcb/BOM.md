# Gameboy PCB — Bill of Materials

**46 placements, 24 unique part numbers**

| # | Part Number | Description | Qty | LCSC # | Notes |
|---|-------------|-------------|-----|--------|-------|
| 1 | STM32F411RET6 | MCU, LQFP-64, 512KB Flash, 128KB RAM | 1 | C94355 | — |
| 2 | ME6211C33M5G-N | 3.3V LDO, SOT-23-5, 500mA | 1 | C82942 | Replaces Nucleo's LD3985M33R. Same pinout/dropout, JLCPCB Basic. |
| 3 | W25Q128JVSIQ | 128Mbit SPI NOR Flash, SOIC-8 | 1 | C97521 | IQ variant: QE=1, /WP and /HOLD disabled. Tie IO2/IO3 to VCC or float. |
| 4 | MAX98357AETE+T | I2S Class-D Mono Amp, TQFN-16 | 1 | C910544 | — |
| 5 | CC0603KRX5R7BB105 | 1µF 0603 X5R ceramic cap | 3 | C106215 | LDO in/out (C1, C2), VDDA bypass (C9) |
| 6 | CL21A106KOQNNNE | 10µF 0805 X5R ceramic cap (bulk input) | 1 | C1713 | Replaces Nucleo's AVX tantalum. Cheaper, no fire risk. ~6µF effective at 5V. |
| 7 | CC0603KRX7R9BB104 | 100nF 0603 X7R ceramic cap | 7 | C14663 | VDD bypass ×3 (C4-C6), NRST filter (C11), flash bypass (C14), amp VDD (C16), + 1 spare |
| 8 | CC0603KRX7R9BB103 | 10nF 0603 X7R ceramic cap | 1 | C100042 | VDDA high-frequency bypass (C17) |
| 9 | CC0603KRX5R6BB475 | 4.7µF 0603 X5R ceramic cap | 2 | C109456 | VCAP_1 (C10), VDD bulk per datasheet (C7) |
| 10 | CL21A106KPFNNNE | 10µF 0805 ceramic cap | 1 | C17024 | MAX98357A PVDD bypass (C15) |
| 11 | RC0603JR-0710KL | 10KΩ 0603 resistor | 1 | C99198 | BOOT0 pull-down (R2) |
| 12 | RC0603FR-071ML | 1MΩ 0603 resistor | 1 | C105578 | MAX98357A SD/MODE pull-up for (L+R)/2 mode (R5) |
| 13 | RC0603FR-07100KL | 100KΩ 0603 resistor | 1 | C14675 | MAX98357A GAIN_SLOT to GND for 15dB (R6) |
| 14 | 0603WAF5100T5E | 510Ω 0603 resistor | 1 | C23193 | Status LED current limit ~3mA (R11) |
| 15 | RC0603FR-075K1L | 5.1KΩ 0603 resistor | 2 | C105580 | USB-C CC1/CC2 pull-down for 5V (R12, R13) |
| 16 | GZ1608D601TF | Ferrite bead 600Ω 0603 | 1 | C1002 | VDD→VDDA filter. Replaces Nucleo's Tai-tech FCM1608KF-601T05. |
| 17 | AFC07-S18ECA-00 | 18-pin 0.5mm FPC connector (EyeSPI display) | 1 | C262640 | JUSHUO. Replaces Molex 5034801800 (out of stock). |
| 18 | WAFER-MX1.25-2PWB | MX1.25 (PicoBlade) 1.25mm 2-pin SMD right-angle (speaker) | 1 | C3029359 | MAX98357A bridge-tied output → speaker. Replaces mislabeled JST PH (which is actually 2.0mm). NOTE: 1.25mm pitch — requires new footprint + re-route vs old 2.0mm part. Rated 1A: OK for 8Ω, marginal for 4Ω at high volume. |
| 19 | TYPE-C-31-M-12 | USB-C power connector (5V input) | 1 | C165948 | Power only — VBUS/GND. CC resistors (#15) provide 5V advertisement. |
| 20 | Pin Header 1x5 | 2.54mm header (SWD + Display SPI1 debug) | 2 | C358687 | SWD: 3.3V, SWDIO, SWCLK, GND, NRST. Display: SCK, MOSI, MISO, CS, GND. |
| 21 | Pin Header 1x3 | 2.54mm header (UART + I2S2 debug) | 2 | C49257 | UART: TX, RX, GND. I2S2: BCLK, WS, SD. |
| 22 | TS-1187A-B-A-B | 6x6mm SMD tactile switch | 8 | C318884 | Buttons A, B, Up, Down, Left, Right, Start, Select (PC0–PC7) |
| 23 | 19-213SYGC/S530-E2/5T | Green LED, 0603 | 1 | C2986027 | Power/status indicator |
| 24 | KH-2.54PH180-1X4P-L11.5 | 2.54mm header 1x4 (Flash SPI3 debug) | 1 | C2905435 | SCK, MOSI, MISO, CS. For Saleae probes. |
| 25 | 2.54-1x6P Straight pin | 2.54mm header 1x6 | 1 | C37208 | — |
| 26 | KH-2.54PH180-1X7P-L11.5 | 2.54mm header 1x7 | 1 | C2932700 | — |
| 27 | HX PM2.54-1x20P ZC | 2.54mm header 1x20 FEMALE, square-hole (TFT display mount) | 2 | C41417332 | Female sockets on mainboard; male pins go on the Adafruit 2.8" TFT breakout so the screen is removable. Top + bottom rows. |

---

## Pin Mapping

| STM32 Pin | Function | Connects To |
|-----------|----------|-------------|
| PA4 | SPI1 CS | J1 (EyeSPI TCS) |
| PA5 | SPI1 SCK | J1 (EyeSPI SCK) via R7 |
| PA6 | SPI1 MISO | J1 (EyeSPI MISO) |
| PA7 | SPI1 MOSI | J1 (EyeSPI MOSI) via R8 |
| PB5 | GPIO | J1 (EyeSPI DC) |
| PA9 | USART1 TX | J5 (UART header pin 1) |
| PA10 | USART1 RX | J5 (UART header pin 2) |
| PB0 | GPIO CS | U3 W25Q128 /CS |
| PC10 | SPI3 SCK | U3 W25Q128 CLK via R9 |
| PC11 | SPI3 MISO | U3 W25Q128 DO |
| PC12 | SPI3 MOSI | U3 W25Q128 DI via R10 |
| PB12 | I2S2 WS | U4 MAX98357A LRCLK |
| PB13 | I2S2 SCK | U4 MAX98357A BCLK |
| PB15 | I2S2 SD | U4 MAX98357A DIN |
| PC0 | GPIO (EXTI) | SW1 (Button A - Jump) |
| PC1 | GPIO (EXTI) | SW2 (Button B - SFX) |
| PC2 | GPIO (EXTI) | SW3 (Button Left) |
| PC3 | GPIO (EXTI) | SW4 (Button Right) |
| PC4 | GPIO (EXTI) | SW5 (Button Up) |
| PC5 | GPIO (EXTI) | SW6 (Button Down) |
| PC6 | GPIO (EXTI) | SW7 (Button Start) |
| PC7 | GPIO (EXTI) | SW8 (Button Select) |
| PA13 | SWDIO | J3 (SWD header) |
| PA14 | SWCLK | J3 (SWD header) |

---

## Design Notes

1. **Buttons** connect between GPIO pin and 3.3V. Internal pull-down enabled in firmware; rising edge triggers interrupt.
2. **W25Q128JVSIQ** — "IQ" suffix means QE=1 (factory set). /WP and /HOLD functions disabled. IO2/IO3 can float or tie to VCC.
3. **MAX98357A** — SD/MODE: 1MΩ to VIN gives (L+R)/2 at 3.3V. GAIN_SLOT: 100KΩ to GND = 15dB.
4. **EyeSPI connector** uses: VIN, GND, SCK, MOSI, MISO, TCS, DC. RST and Lite tied to 3.3V on display side.
5. **SWD header** pinout: Pin 1 = 3.3V (VAPP), Pin 2 = SWDIO (PA13), Pin 3 = SWCLK (PA14), Pin 4 = GND, Pin 5 = NRST.
6. **UART header** pinout: Pin 1 = TX (PA9), Pin 2 = RX (PA10), Pin 3 = GND.
7. **USB-C** is power-only. Two 5.1KΩ on CC1/CC2 to GND per USB-C spec.
8. **NRST** has internal 30–50kΩ pull-up (no external pull-up needed). 100nF filter cap for noise protection.
9. **No HSE crystal** — firmware runs on 16MHz internal HSI oscillator.
10. **MCU decoupling** — 3× 100nF on VDD pins, 4.7µF on VCAP_1, 4.7µF bulk on VDD, 1µF+10nF on VDDA (through ferrite bead).
