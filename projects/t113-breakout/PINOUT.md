# T113-S3 Breakout — Header Pinout Reference

Complete pin→header assignment for the four 1×25 breakout headers, the on-board
device connections, and the deliberately-unused (No-Connect) pins.

> **Source of truth:** this file is generated from the final layout
> (`production/netlist.ipc` / `production/positions.csv`). If it ever disagrees
> with the `.kicad_pcb`, the PCB wins.

**Header layout:** four 1×25 female headers (`2044-1X25G00SA`), placed as two
side-by-side pairs that each read like a 2×25 block (BeagleBone / Nucleo style):

```
        H10  H11                         H13  H12
        (col A/B, left edge)   [ U1 ]    (col B/A, right edge)
```

- **Left block** — H10 + H11 (2.54 mm apart): the PG / SD / SPI-NOR / PE /
  UART0 nets (top & left chip edges).
- **Right block** — H12 + H13 (2.54 mm apart): the PB / PD nets (bottom edge =
  the LCD0 / i8080 parallel-display bus).

Grounds are interleaved on the PD block (H12/H13) for clean logic-analyzer
probing, and grouped at the header ends on the left block.

**GPIO total:** 72 user I/O. Committed: PF0–5 (SD, 6), PC2–7 (flash, 6),
PE2/PE3 (UART0 console, 2) → **58 free GPIO**. The SD, flash and UART0 nets are
*also* fanned out to the headers (in addition to the on-board socket / flash /
console pads) so every pin is probeable.

---

## Header 1 — H10 (left block, column A)
| Hdr pin | Net | SoC pin |
|---|---|---|
| 1 | +3.3V | — |
| 2 | PG1 | 118 |
| 3 | PG0 | 120 |
| 4 | PG5 | 122 |
| 5 | PG12 | 124 |
| 6 | PG14 | 126 |
| 7 | PG6 | 1 |
| 8 | PG8 | 3 |
| 9 | PG10 | 5 |
| 10 | SD_DAT1 | 7 (PF0) |
| 11 | SD_CLK | 9 (PF2) |
| 12 | SD_DAT3 | 11 (PF4) |
| 13 | PF6 | 13 |
| 14 | SPI_WP / IO2 | 15 (PC6) |
| 15 | SPI_MOSI / IO0 | 17 (PC4) |
| 16 | SPI_CLK | 19 (PC2) |
| 17 | PE13 | 31 |
| 18 | UART0_RX | 33 (PE3) |
| 19 | PE11 | 36 |
| 20 | PE9 | 38 |
| 21 | PE7 | 40 |
| 22 | PE5 | 42 |
| 23 | PE0 | 44 |
| 24 | GND | — |
| 25 | GND | — |

## Header 2 — H11 (left block, column B)
| Hdr pin | Net | SoC pin |
|---|---|---|
| 1 | +5V | — |
| 2 | PG2 | 119 |
| 3 | PG3 | 121 |
| 4 | PG4 | 123 |
| 5 | PG13 | 125 |
| 6 | PG15 | 127 |
| 7 | PG7 | 2 |
| 8 | PG9 | 4 |
| 9 | PG11 | 6 |
| 10 | SD_DAT0 | 8 (PF1) |
| 11 | SD_CMD | 10 (PF3) |
| 12 | SD_DAT2 | 12 (PF5) |
| 13 | SPI_HOLD / IO3 | 14 (PC7) |
| 14 | SPI_MISO / IO1 | 16 (PC5) |
| 15 | SPI_CS | 18 (PC3) |
| 16 | GND | — |
| 17 | PE12 | 32 |
| 18 | UART0_TX | 35 (PE2) |
| 19 | PE10 | 37 |
| 20 | PE8 | 39 |
| 21 | PE6 | 41 |
| 22 | PE4 | 43 |
| 23 | PE1 | 45 |
| 24 | GND | — |
| 25 | GND | — |

## Header 3 — H12 (right block, PB + PD, column A)
| Hdr pin | Net | SoC pin |
|---|---|---|
| 1 | +0V9 | — |
| 2 | PB3 | 85 |
| 3 | PB5 | 82 |
| 4 | GND | — |
| 5 | PB7 | 79 |
| 6 | PD18 | 75 |
| 7 | GND | — |
| 8 | PD16 | 73 |
| 9 | PD14 | 71 |
| 10 | GND | — |
| 11 | PD13 | 69 |
| 12 | PD10 | 67 |
| 13 | GND | — |
| 14 | PD8 | 63 |
| 15 | PD6 | 61 |
| 16 | GND | — |
| 17 | PD4 | 59 |
| 18 | PD2 | 57 |
| 19 | GND | — |
| 20 | PD0 | 55 |
| 21 | PD21 | 53 |
| 22 | GND | — |
| 23 | GND | — |
| 24 | GND | — |
| 25 | GND | — |

## Header 4 — H13 (right block, PB + PD, column B)
| Hdr pin | Net | SoC pin |
|---|---|---|
| 1 | +3.3V | — |
| 2 | PB2 | 86 |
| 3 | PB4 | 84 |
| 4 | GND | — |
| 5 | PB6 | 80 |
| 6 | PD19 | 76 |
| 7 | GND | — |
| 8 | PD17 | 74 |
| 9 | PD15 | 72 |
| 10 | GND | — |
| 11 | PD12 | 70 |
| 12 | PD11 | 68 |
| 13 | GND | — |
| 14 | PD9 | 64 |
| 15 | PD7 | 62 |
| 16 | GND | — |
| 17 | PD5 | 60 |
| 18 | PD3 | 58 |
| 19 | GND | — |
| 20 | PD1 | 56 |
| 21 | PD20 | 54 |
| 22 | PD22 | 52 |
| 23 | GND | — |
| 24 | GND | — |
| 25 | GND | — |

**Free-GPIO count (deduped across all four headers):** PB (6) + PD (23) +
PE0–PE1/PE4–PE13 (12) + PF6 (1) + PG0–PG15 (16) = **58 free GPIO** ✓
(SD PF0–5, flash PC2–7, UART0 PE2/PE3 are committed and not counted here.)

**Power / GND on the headers:** +5V (H11-1), +3.3V (H10-1, H13-1),
+0V9 core (H12-1), plus the interleaved / end-of-header GND pins above.

---

## On-board device connections

These nets land on dedicated on-board devices. SD / flash / UART0 are *also*
mirrored to the headers above; clocks, RESET, DZQ, USB and the codec references
are **header-exclusive to the SoC / passives** (not broken out).

### SD card (SDC0 / PF bank) → microSD socket (U4) + SD debug tap
| Signal | SoC pin | Also on header |
|--------|---------|----------------|
| SD_DAT1 | PF0 (7)  | H10-10 |
| SD_DAT0 | PF1 (8)  | H11-10 |
| SD_CLK  | PF2 (9)  | H10-11 |
| SD_CMD  | PF3 (10) | H11-11 |
| SD_DAT3 | PF4 (11) | H10-12 |
| SD_DAT2 | PF5 (12) | H11-12 |

### SPI-NOR flash (SPI0 / PC bank) → W25Q128 (U3) + QSPI debug tap
| Signal | SoC pin | Also on header |
|--------|---------|----------------|
| SPI_CLK      | PC2 (19) | H10-16 |
| SPI_CS       | PC3 (18) | H11-15 |
| SPI_MOSI/IO0 | PC4 (17) — also BOOT-SEL0 | H10-15 |
| SPI_MISO/IO1 | PC5 (16) — also BOOT-SEL1 | H11-14 |
| SPI_WP/IO2   | PC6 (15) | H10-14 |
| SPI_HOLD/IO3 | PC7 (14) | H11-13 |

### UART0 console
| Signal | SoC pin | Header |
|--------|---------|--------|
| UART0_TX | PE2 (35) | H11-18 |
| UART0_RX | PE3 (33) | H10-18 |

### Clocks (not broken out)
| Signal | SoC pin |
|--------|---------|
| DXIN (24 MHz) | 23 |
| DXOUT (24 MHz) | 22 |
| X32KIN (RTC) | 25 |
| X32KOUT (RTC) | 24 |

### Other dedicated pins (not broken out)
| Pin | SoC pin | Connection |
|-----|---------|-----------|
| RESET | 27 | 10K→+3V3 pull-up, 100nF→GND, reset button (SW1)→GND |
| DZQ | 47 | 240Ω 1% → GND (DDR ZQ calibration) |
| USB0-DP / USB0-DM | 115 / 114 | USB-C data pair (90Ω diff) |
| VRA1 | 92 | 100nF → GND (codec reference bypass) |
| VRA2 | 90 | 100nF → GND (codec reference bypass) |

---

## No-Connect pins (deliberately unused — place NC flag / X)

Minimal board: no audio, no analog video, no WiFi, no touch panel.

| Pin(s) | Signal | Why unused |
|--------|--------|-----------|
| 21 | REFCLK-OUT | 24 MHz WiFi clock fanout — no WiFi |
| 101 | GPADC0 | general-purpose ADC — no analog input needed |
| 112, 113 | USB1-DP/DM | USB2.0 HOST port — not used (USB0 only) |
| 78 | TVOUT0 | CVBS (analog TV) output — unused |
| 108, 109 | TVIN0/TVIN1 | CVBS video inputs — unused |
| 110, 111 | TVIN-VRP/VRN | CVBS ADC references — unused |
| 106 | NC0 | factory no-connect pin |
| 87, 88 | MICIN3P/N | mic input — no audio |
| 93, 94 | FMINR/L | FM-radio audio input — no audio |
| 95, 96 | LINEINR/L | line input — no audio |
| 98, 99 | HPOUTR/L | headphone output — no audio |
| 100 | HPOUTFB | headphone amp feedback — no audio |
| 102–105 | TP-X1/X2/Y1/Y2 | resistive touch-panel ADC — unused |

*(VRA1/VRA2 are NOT in this list — they get bypass caps, not NC, since AVCC is powered.)*

---

## Notes
- **PD bank (H12/H13)** = the LCD0 / i8080 parallel-display bus. Jumper from
  these to drive an ILI9341 in 8-bit parallel if desired.
- **PC4/PC5** double as BOOT-SEL straps (boot-media select) + SPI0 flash data.
- **GND** is interleaved through the PD block (H12/H13) and grouped at the ends
  of the left block (H10/H11) for clean logic-analyzer probing.
- SD / flash / UART0 nets appear on both the on-board device and the headers —
  keep header stubs short and probe at a slow clock.
