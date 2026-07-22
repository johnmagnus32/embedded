# FPGA Blink — STM32-configured iCE40UP5K on the ice40-breakout board

The "hello world" for the [ice40-breakout](../ice40-breakout) board: an
STM32F411RE streams an FPGA bitstream into the iCE40UP5K **over SPI at boot**,
and the configured FPGA then toggles one GPIO to blink an **external** LED.

This is the smallest end-to-end exercise of the board's whole reason for
existing — the STM32 as the sole master of the FPGA config bus (no onboard
config flash, no USB programmer). See
[ice40-breakout/README.md](../ice40-breakout/README.md) for the
design spec and the config sequence this implements.

Built as an **RTOS app** against the in-tree RTOS (`../../rtos`): the FPGA is
configured by the shared iCE40 loader driver
([rtos/drivers/fpga/ice40.c](../../rtos/drivers/fpga/ice40.c)) over the config
SPI bus, driven entirely from the device tree ([board.dts](board.dts)). This
replaced an earlier bare-metal build that bit-banged the config sequence by hand.

**Status: the bare-metal predecessor ran on real hardware** (fabbed board,
2026-07) — FPGA configured over SPI, external LED blinked at ~1.4 Hz. **The
current RTOS-app version builds clean but has NOT yet been re-run on silicon.**

## Layout

```
board.dts   device tree: USART2 console + config SPI bus (spi3) + iCE40 loader node
.config     RTOS Kconfig: CLOCK/UART/GPIO/SPI/FPGA_ICE40 + SCHED/SYNC/SYSTICK
Makefile    top-level: builds bitstream, then the RTOS firmware
fpga/       iCE40 bitstream — toggles one user I/O from the internal oscillator
  blink.v       pure counter logic (simulation-testable)
  blink_top.v   hard-IP wrapper (SB_HFOSC) -> blink
  blink.pcf     pin constraint: LED = FPGA pin 48 (IOB_4A) -> header H3 pin 8
  tb_blink.v    testbench
  Makefile      sim / synth / header (embeds the bitstream as build/bitstream.h)
src/    STM32F411RE firmware (RTOS app)
  main.c        device_init_all() -> fpga_load() -> idle task under the scheduler
  linker.ld     flash/RAM layout incl. the .device_area device-model section
build/  all artifacts (git-ignored): bitstream, bitstream.h, ELF, sim, config.h, devicetree.h
```

## Wiring (STM32F411RE ↔ ice40-breakout)

The breakout exposes everything on two 1×25 headers, **H3** and **H4**.
Config bus — Nucleo to breakout header **H3** (from [board.dts](board.dts)):

| STM32 | ice40-breakout | iCE40 pin | role |
|-------|----------------|-----------|------|
| PC10  | H3-22 SPI_SCK   | 15 | config clock (SPI3_SCK, AF6) |
| PC12  | H3-24 SPI_SI    | 17 | config data in (SPI3_MOSI, AF6) |
| PB6   | H3-23 SPI_SS_B  | 16 | slave-mode strap / select (GPIO) |
| PB1   | H3-15 CRESET_B  | 8  | config reset (GPIO) |
| PB2   | H3-14 CDONE     | 7  | config-done (GPIO in) |

Power & LED:
- Feed **3.3 V into breakout header H3-1**; common **GND** on H3-6/13 or
  H4-5/10/15/20 (share a ground with the STM32). H4-1 is the 1.2 V rail
  **output** — probe only, don't drive.
- **External LED:** anode → **H3 pin 8** (FPGA pin 48, IOB_4A), cathode →
  ~330 Ω → GND. (No onboard RGB LED / SB_RGBA_DRV — deliberately the simplest
  path.)

## Prerequisites

```bash
# FPGA toolchain (this repo uses the OSS CAD Suite bundle on PATH):
#   yosys, nextpnr-ice40, icepack, iverilog, vvp
# ARM cross-compiler (the Makefile points at the Zephyr SDK):
#   ~/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc
# plus openocd for flashing.
```

## Build & run

```bash
make sim        # (optional) run the FPGA logic testbench — prints PASS/FAIL
make            # synth bitstream -> embed as build/bitstream.h -> build RTOS elf
make flash      # flash via ST-Link; the firmware configures the FPGA on reset
```

The console is USART2 (PA2/PA3) bridged to the Nucleo ST-Link virtual COM port,
so on `/dev/ttyACM0` @ 115200 8N1 you'll see `FPGA config OK` (or `FAILED`)
after each reset. On success the breakout's CDONE LED goes **dark** (wired
active-low) and your **external LED blinks at ~1.4 Hz**.

## How it works

1. **FPGA** ([fpga/blink_top.v](fpga/blink_top.v)): `SB_HFOSC` (÷4 → 12 MHz)
   drives a 24-bit counter; bit 22 (~1.4 Hz) drives the `LED` output on pin 48.
2. **Bitstream → firmware**: `make header` converts `blink.bin` into a
   `const uint8_t[]` in `build/bitstream.h`, so the bitstream ships inside the
   STM32 flash image — no FPGA-side flash.
3. **STM32 at boot** ([src/main.c](src/main.c)): `device_init_all()` brings up
   the console + config SPI bus + the iCE40 loader device (all from the device
   tree), then `fpga_load()` runs the iCE40 SPI-slave sequence (TN1248) via the
   shared driver: CRESET low → SS low (strap slave) → CRESET high → wait
   1200 µs → clock the bitstream (mode 0, MSB first) → trailing clocks → SS high
   → check CDONE. The FPGA then free-runs the blink while the STM32 idles.

## Notes

- **No 96 MHz PLL bump needed.** The old bare-metal build had to raise the core
  clock to 96 MHz so its *bit-banged* config SCK cleared the iCE40 slave-config
  **1 MHz minimum**. The RTOS build clocks the bitstream through the SPI
  **peripheral** at ~8 MHz (BR=/2) straight off the 16 MHz HSI — comfortably
  inside the 1–25 MHz window — so the whole PLL dance is gone.
- **CDONE readback uses a pull-down** (the loader driver configures CDONE as
  input + pull-down), so a floating pin reads 0 and only a driven-high
  (configured) CDONE reads 1 — the readback is trustworthy.
- **The board originally had a power bug** (every FPGA supply pin AC-coupled
  through its decoupling cap instead of tied to the rail) that made config
  impossible; fixed in the design before the working fab. If a future respin
  regresses, the tell is SPI_VCCIO1 (pin 22) / VPP_2V5 (pin 24) not reading
  3.3 V.
