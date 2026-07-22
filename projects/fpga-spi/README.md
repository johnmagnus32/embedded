# FPGA SPI — STM32 drives two FPGA-controlled LEDs over the runtime SPI bus

One step past [fpga-blink](../fpga-blink): after the STM32F411RE configures the
iCE40UP5K at boot, it talks to the **configured** FPGA over the **runtime SPI
bus** to turn two GPIO-driven LEDs on and off. The STM32 is a "dumb" pattern
generator — it just walks the two LEDs through every state on a fixed cadence —
while the FPGA does the real work of latching each command and driving the pins.

Those same two FPGA output pins are also wired back to two STM32 inputs, so the
firmware **reads its own commands back and self-verifies the link** every step,
printing a per-step `OK`/`FAIL` with running pass/fail counts on the UART.

Built as an **RTOS app** against the in-tree RTOS (`../../rtos`), with two SPI
buses driven entirely from the device tree ([board.dts](board.dts)):
- **config bus (SPI3):** the shared iCE40 loader driver
  ([rtos/drivers/fpga/ice40.c](../../rtos/drivers/fpga/ice40.c)) streams the
  bitstream in at boot (same path as fpga-blink).
- **runtime bus (SPI1):** the rtos SPI driver drives the LED command channel,
  clocked **slow** (`br=5` → /64 → ~250 kHz) so the FPGA's oversampled slave can
  track it.

**Status: builds clean; not yet run on hardware.** The FPGA logic passes its
testbench (`make sim`) and the full bitstream + firmware build end-to-end. The
[ice40-breakout](../ice40-breakout) board it targets exists and ran fpga-blink;
this reuses that hardware-proven config path and adds the runtime SPI channel.

## Layout

```
board.dts   device tree: USART2 console + config bus (spi3) + runtime bus (spi1, br=5) + iCE40 loader node
.config     RTOS Kconfig: CLOCK/UART/GPIO/SPI/FPGA_ICE40 + SCHED/SYNC/SYSTICK
Makefile    top-level: builds bitstream, then the RTOS firmware
fpga/   iCE40 bitstream — SPI slave that drives two LEDs from a command byte
  spi_slave.v     pure SPI-slave RX logic (simulation-testable, no hard IP)
  spi_led_top.v   hard-IP wrapper (SB_HFOSC) + latches byte -> LED0/LED1
  spi.pcf         pin constraints (real ice40-breakout pins, NOT iCEBreaker)
  tb_spi_slave.v  testbench for spi_slave
  Makefile        sim / synth / header (embeds the bitstream as build/bitstream.h)
src/    STM32F411RE firmware (RTOS app)
  main.c          device_init_all() -> fpga_load() -> led_task (SPI-master + readback)
  linker.ld       flash/RAM layout incl. the .device_area device-model section
build/  all artifacts (git-ignored): bitstream, bitstream.h, ELF, sim, config.h, devicetree.h, .vcd
```

## Wiring (STM32F411RE ↔ ice40-breakout)

Everything is broken out on the breakout's two 1×25 headers, **H3** (chip left
half) and **H4** (chip right half). Pins are the **real as-built package pins**
from [ice40-breakout/BOM.md](../ice40-breakout/BOM.md) — the stock iCEBreaker
`.pcf` numbers are wrong for this board (fpga-blink learned this the hard way).

**Config bus** (SPI3, used once at boot — identical to fpga-blink) → header **H3**:

| STM32 | ice40-breakout | iCE40 pin | role |
|-------|----------------|-----------|------|
| PC10  | H3-22 SPI_SCK   | 15 | config clock (SPI3_SCK, AF6) |
| PC12  | H3-24 SPI_SI    | 17 | config data in (SPI3_MOSI, AF6) |
| PB6   | H3-23 SPI_SS_B  | 16 | slave-mode strap / select (GPIO) |
| PB1   | H3-15 CRESET_B  | 8  | config reset (GPIO) |
| PB2   | H3-14 CDONE     | 7  | config-done (GPIO in) |

**Runtime bus** (SPI1, every cadence step; STM32 is SPI *master*, one-way):

| STM32 | ice40-breakout | iCE40 pin | role |
|-------|----------------|-----------|------|
| PA5   | H4-23 RUNTIME_SPI_CLK  | 20 | runtime SPI clock (SPI1_SCK, AF5, mode 0) |
| PA7   | H4-22 RUNTIME_SPI_MOSI | 21 | runtime SPI data (SPI1_MOSI, AF5, MSB first) |
| PA4   | H3-25 RUNTIME_SPI_CS   | 18 | runtime SPI select (hardware NSS, active low) |

**LED outputs + feedback** (each FPGA pin fans out to an external LED *and* an
STM32 input):

| iCE40 pin | ice40-breakout | STM32 in | LED |
|-----------|----------------|----------|-----|
| 48 (IOB_4A) | H3-8 | PC0 | LED0 (command bit 0) |
| 2  (IOB_6A) | H3-9 | PC1 | LED1 (command bit 1) |

- **External LEDs:** FPGA pin → ~330 Ω → LED anode … cathode → GND, for each of
  H3-8 and H3-9 (active-high: a `1` command lights the LED).
- **Feedback taps:** jumper H3-8 → STM32 **PC0** and H3-9 → STM32 **PC1**. These
  are read-only; PC0/PC1 are inputs with pull-downs so a driven-high pin reads 1
  and a disconnected wire reads 0 (a broken tap shows up as a `FAIL`).

Power & ground:
- Feed **3.3 V into breakout H3-1**; common **GND** on H3-6/13 (share ground
  with the STM32). H4-1 is the 1.2 V rail **output** — probe only, don't drive.

> **Note — PA5 is also the Nucleo user LED (LD2).** The runtime SCK on PA5 will
> also flicker the onboard green LED. That's harmless (it just mirrors SCK
> activity); if you want a clean onboard LED, move the runtime bus to a free
> SPI-capable pin and update `board.dts` + `spi.pcf`.

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
make sim        # (optional) run the FPGA SPI-slave testbench — prints PASS/FAIL
make            # synth bitstream -> embed as build/bitstream.h -> build RTOS elf
make flash      # flash via ST-Link (openocd)
```

On `/dev/ttyACM0` @ 115200 8N1 you'll first see `FPGA config OK`, then a line
per step as the LEDs cycle `00 → 01 → 10 → 11 → …`:

```
FPGA config OK
cmd=0b00 fb=0b00 OK  (ok=1 fail=0)
cmd=0b01 fb=0b01 OK  (ok=2 fail=0)
cmd=0b10 fb=0b10 OK  (ok=3 fail=0)
cmd=0b11 fb=0b11 OK  (ok=4 fail=0)
...
```

`cmd` is what the STM32 sent; `fb` is what it read back from the FPGA's output
pins. They should always match — a mismatch (`FAIL`) means the SPI command or a
feedback tap isn't getting through.

## How it works

1. **FPGA** ([fpga/spi_led_top.v](fpga/spi_led_top.v)): `SB_HFOSC` (÷4 → 12 MHz)
   clocks an SPI slave ([fpga/spi_slave.v](fpga/spi_slave.v)) that oversamples
   the runtime bus through a 3-stage synchronizer and shifts in one byte per CS
   frame (MSB first, mode 0). The byte is latched; bit 0 drives `LED0` (pin 48),
   bit 1 drives `LED1` (pin 2).
2. **Bitstream → firmware**: `make header` converts `spi_led_top.bin` into a
   `const uint8_t[]` in `build/bitstream.h`, shipped inside the STM32 flash image
   — no FPGA-side flash.
3. **STM32 at boot** ([src/main.c](src/main.c)): `device_init_all()` brings up
   the console + both SPI buses + the iCE40 loader (all from the device tree),
   then `fpga_load()` configures the FPGA over SPI3 (TN1248 — same as
   fpga-blink). A scheduler task then walks the four LED states: for each it
   sends one CS-framed command byte over the runtime SPI (SPI1), waits, reads
   PC0/PC1, compares to the command, and prints the result. Cadence ~300 ms/step.

## Notes

- **Two SPI clock rates, both from the device tree.** The config bus (SPI3)
  runs at the driver default BR=/2 (~8 MHz off the 16 MHz HSI), inside the iCE40
  slave-config 1–25 MHz window. The runtime bus (SPI1) sets `br = <5>;` (/64 →
  ~250 kHz) because the FPGA's SPI slave oversamples with a 12 MHz clock +
  3-stage synchronizer and cannot track a multi-MHz SCK. The `br` property was
  added to the shared SPI driver ([rtos/drivers/spi/spi_stm32.c](../../rtos/drivers/spi/spi_stm32.c));
  it defaults to 0 (/2) when a node omits it, so existing boards are unaffected.
- **No 96 MHz PLL bump.** The old bare-metal build needed 96 MHz so its
  bit-banged config SCK cleared the 1 MHz floor; clocking through the SPI
  peripheral removes that constraint entirely.
- **Feedback/CDONE inputs use pull-downs** so a driven-high pin reads 1 and a
  floating/disconnected pin reads 0 — the readback is trustworthy.
- **The board had an early power bug** (every FPGA supply AC-coupled through its
  decap instead of tied to the rail) that made config impossible; fixed before
  the working fab. If a respin regresses, the tell is SPI_VCCIO1 (pin 22) /
  VPP_2V5 (pin 24) not reading 3.3 V.
