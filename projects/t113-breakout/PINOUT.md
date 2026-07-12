# T113-S3 Breakout — Header Pinout Reference

Complete pin→header assignment for the six 1×13 GPIO breakout headers, the
functional/debug headers, and the deliberately-unused (No-Connect) pins.

**Routing strategy:** each 1×13 header maps to ONE chip edge, pins listed in
physical pin-number order, so traces fan straight out with minimal crossing.
Final pin↔position tweaks happen at layout (swap adjacent GPIO within a header
to kill any last crossings).

**GPIO total:** 72 user I/O. Committed: PF0–5 (SD, 6), PC2–7 (flash, 6),
PE2/PE3 (UART0 console, 2) → **58 free GPIO** broken out below.

---

## GPIO breakout headers (6 × 1×13)

### Header 1 — top edge (PG-low + PF6 + PE13/12)
| Hdr pin | Net | SoC pin |
|---|---|---|
| 1 | PG6 | 1 |
| 2 | PG7 | 2 |
| 3 | PG8 | 3 |
| 4 | PG9 | 4 |
| 5 | PG10 | 5 |
| 6 | PG11 | 6 |
| 7 | PF6 | 13 |
| 8 | PE13 | 31 |
| 9 | PE12 | 32 |
| 10 | GND | — |
| 11 | GND | — |
| 12 | GND | — |
| 13 | GND | — |

### Header 2 — left edge (PE bank)
| Hdr pin | Net | SoC pin |
|---|---|---|
| 1 | PE11 | 36 |
| 2 | PE10 | 37 |
| 3 | PE9 | 38 |
| 4 | PE8 | 39 |
| 5 | PE7 | 40 |
| 6 | PE6 | 41 |
| 7 | PE5 | 42 |
| 8 | PE4 | 43 |
| 9 | PE0 | 44 |
| 10 | PE1 | 45 |
| 11 | GND | — |
| 12 | GND | — |
| 13 | GND | — |

### Header 3 — left-bottom (PD low) — full, no spares
| Hdr pin | Net | SoC pin |
|---|---|---|
| 1 | PD22 | 52 |
| 2 | PD21 | 53 |
| 3 | PD20 | 54 |
| 4 | PD0 | 55 |
| 5 | PD1 | 56 |
| 6 | PD2 | 57 |
| 7 | PD3 | 58 |
| 8 | PD4 | 59 |
| 9 | PD5 | 60 |
| 10 | PD6 | 61 |
| 11 | PD7 | 62 |
| 12 | PD8 | 63 |
| 13 | PD9 | 64 |

### Header 4 — bottom (PD high, incl. PD15)
| Hdr pin | Net | SoC pin |
|---|---|---|
| 1 | PD10 | 67 |
| 2 | PD11 | 68 |
| 3 | PD13 | 69 |
| 4 | PD12 | 70 |
| 5 | PD14 | 71 |
| 6 | PD15 | 72 |
| 7 | PD16 | 73 |
| 8 | PD17 | 74 |
| 9 | PD18 | 75 |
| 10 | PD19 | 76 |
| 11 | GND | — |
| 12 | GND | — |
| 13 | GND | — |

### Header 5 — bottom (PB bank) — sparse (PB has only 6 free pins)
| Hdr pin | Net | SoC pin |
|---|---|---|
| 1 | PB7 | 79 |
| 2 | PB6 | 80 |
| 3 | PB5 | 82 |
| 4 | PB4 | 84 |
| 5 | PB3 | 85 |
| 6 | PB2 | 86 |
| 7 | GND | — |
| 8 | GND | — |
| 9 | GND | — |
| 10 | GND | — |
| 11 | GND | — |
| 12 | GND | — |
| 13 | GND | — |

### Header 6 — right edge (PG-high)
| Hdr pin | Net | SoC pin |
|---|---|---|
| 1 | PG1 | 118 |
| 2 | PG2 | 119 |
| 3 | PG0 | 120 |
| 4 | PG3 | 121 |
| 5 | PG5 | 122 |
| 6 | PG4 | 123 |
| 7 | PG12 | 124 |
| 8 | PG13 | 125 |
| 9 | PG14 | 126 |
| 10 | PG15 | 127 |
| 11 | GND | — |
| 12 | GND | — |
| 13 | GND | — |

**GPIO count:** H1=9, H2=10, H3=13, H4=10, H5=6, H6=10 = **58 free GPIO** ✓

---

## Functional / device connections (not on GPIO headers)

### SD card (SDC0 / PF bank) → microSD socket + SD debug tap
| Signal | SoC pin |
|--------|---------|
| SD_DAT1 | PF0 (7) |
| SD_DAT0 | PF1 (8) |
| SD_CLK  | PF2 (9) |
| SD_CMD  | PF3 (10) |
| SD_DAT3 | PF4 (11) |
| SD_DAT2 | PF5 (12) |

### SPI-NOR flash (SPI0 / PC bank) → flash + QSPI debug tap
| Signal | SoC pin |
|--------|---------|
| SPI_CLK  | PC2 (19) |
| SPI_CS   | PC3 (18) |
| SPI_MOSI/IO0 | PC4 (17) — also BOOT-SEL0 |
| SPI_MISO/IO1 | PC5 (16) — also BOOT-SEL1 |
| SPI_WP/IO2   | PC6 (15) |
| SPI_HOLD/IO3 | PC7 (14) |

### UART0 console → 1×3 header
| Signal | SoC pin |
|--------|---------|
| UART0_TX | PE2 (35) |
| UART0_RX | PE3 (33) |
| GND | — |

### Clocks
| Signal | SoC pin |
|--------|---------|
| DXIN (24 MHz) | 23 |
| DXOUT (24 MHz) | 22 |
| X32KIN (RTC) | 25 |
| X32KOUT (RTC) | 24 |

### Other dedicated pins
| Pin | SoC pin | Connection |
|-----|---------|-----------|
| RESET | 27 | 10K→+3V3 pull-up, 100nF→GND, reset button→GND |
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
- **PD bank (H3/H4)** = the LCD0 / i8080 parallel-display bus. Jumper from these to
  drive an ILI9341 in 8-bit parallel if desired.
- **PC4/PC5** double as BOOT-SEL straps (boot-media select) + SPI0 flash data.
- Grounds shown at header ends for simplicity; move some mid-header at layout for
  cleaner logic-analyzer probing.
- This is the schematic-capture assignment; verify/adjust against footprint geometry
  at PCB layout.
