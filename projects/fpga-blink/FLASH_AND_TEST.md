# fpga-blink — Flash & Test on Hardware (hand-off runbook)

Self-contained instructions to flash and verify the **RTOS-app** fpga-blink
build on the fabbed **ice40-breakout** board. Written so a fresh session with no
prior context can run it. All hardware is attached to a remote machine reached
over SSH as **`johmagnu-nuc`**; the board sits on that machine's USB, so every
openocd/serial command runs *there*, not on the dev box.

## The setup (as verified 2026-07-21)

- The **ice40-breakout** (iCE40UP5K) is driven by an **STM32F411RE Nucleo**.
- The Nucleo's on-board **ST-Link/V2.1**, serial **`066AFF505787884867113539`**,
  appears on the NUC as USB `0483:374b` and its console VCP is **`/dev/ttyACM0`**
  (USART2, 115200 8N1).
- **A second ST-Link is usually also plugged in** (the gameboy board's ST-Link/V2
  `0483:3748` on `/dev/ttyUSB0`). Because two ST-Links are present, you **MUST**
  pass `-c "adapter serial 066AFF505787884867113539"` to every openocd command,
  or it may grab the wrong board.
- This target needs **connect-under-reset** to attach (a plain attach reports
  "examination failed"). Always include:
  `-c "reset_config srst_only srst_nogate connect_assert_srst"`.

Confirm what's connected first:
```bash
ssh johmagnu-nuc 'lsusb | grep -iE "0483:374b|0483:3748"; ls /dev/ttyACM* /dev/ttyUSB*'
# Expect: 0483:374b (this board) present, and /dev/ttyACM0 present.
```

## Wiring (STM32F411RE Nucleo ↔ ice40-breakout, config bus on header H3)

| Nucleo | ice40-breakout | iCE40 pin | role |
|--------|----------------|-----------|------|
| PC10   | H3-22 SPI_SCK  | 15 | config clock (SPI3_SCK) |
| PC12   | H3-24 SPI_SI   | 17 | config data in (SPI3_MOSI) |
| PB6    | H3-23 SPI_SS_B | 16 | slave-mode strap / select |
| PB1    | H3-15 CRESET_B | 8  | config reset |
| PB2    | H3-14 CDONE    | 7  | config done (input) |
| GND    | H3-6/13 or H4-5/10/15/20 | — | common ground (required) |
| 3V3    | H3-1           | — | board power in |

Do NOT drive H4-1 (that's the breakout's 1.2 V rail *output*, probe only).
External LED (optional, for visual confirm): anode → **H3-8** (FPGA pin 48),
cathode → ~330 Ω → GND.

## 1. Build (on the dev box, in this directory)

```bash
cd projects/fpga-blink
make            # builds the FPGA bitstream, embeds it, builds the RTOS firmware
# Output: build/fpga-blink.elf  (ARM toolchain is the Zephyr SDK, per the Makefile)
```
`make sim` optionally runs the FPGA logic testbench (prints PASS/FAIL, no HW).

## 2. Copy the ELF to the NUC

```bash
scp build/fpga-blink.elf johmagnu-nuc:/tmp/fpga-blink.elf
```

## 3. Flash (runs on the NUC; note the serial + connect-under-reset)

```bash
ssh johmagnu-nuc '
openocd -f interface/stlink.cfg \
  -c "adapter serial 066AFF505787884867113539" \
  -c "reset_config srst_only srst_nogate connect_assert_srst" \
  -f target/stm32f4x.cfg \
  -c "program /tmp/fpga-blink.elf verify reset exit"'
```
Expect: `** Programming Finished **` and `** Verified OK **`.

## 4. Test — capture the console while the board resets

The firmware prints `FPGA config OK` (or `FPGA config FAILED`) once, right after
reset. Start the capture BEFORE resetting so you catch it:

```bash
ssh johmagnu-nuc '
stty -F /dev/ttyACM0 115200 raw -echo 2>/dev/null
( timeout 8 cat /dev/ttyACM0 > /tmp/ice40.log 2>&1 ) &
sleep 0.5
openocd -f interface/stlink.cfg \
  -c "adapter serial 066AFF505787884867113539" \
  -c "reset_config srst_only srst_nogate connect_assert_srst" \
  -f target/stm32f4x.cfg \
  -c "init" -c "reset run" -c "shutdown" >/dev/null 2>&1
sleep 7
cat /tmp/ice40.log'
```

### Pass criteria
- Console shows **`FPGA config OK`**.
- The breakout's **CDONE LED goes dark** (it is wired active-low: lit = not
  configured, dark = configured).
- If an external LED is on H3-8, it **blinks at ~1.4 Hz**.

### If it prints `FPGA config FAILED` or nothing
- Re-check the 5 config wires + common GND (table above); the SS_B/SCK/SI/CRESET
  connections are the usual culprits.
- Confirm board power: 3.3 V on H3-1; the on-board 1.2 V rail should read ~1.2 V
  on H4-1.
- The console read is trustworthy because the loader configures CDONE (PB2) as
  input **with pull-down**, so a floating/disconnected CDONE reads 0 (FAILED),
  and only a genuinely-configured FPGA (CDONE driven high by the board's 10k
  pull-up) reads 1 (OK).
- Historical note: the first board revision had a power bug (FPGA supply pins
  AC-coupled through their decoupling caps instead of tied to the rail) that made
  config impossible. If a respin regresses, meter SPI_VCCIO1 (FPGA pin 22) and
  VPP_2V5 (pin 24) — they must read 3.3 V.

## Gotchas / house rules for the remote shell
- SSH command output here is sometimes prefixed with terminal-title escape noise
  (`]0;...`) and a spurious `Killed by signal 1.` line — filter with
  `grep -viE 'killed by signal|^\]0;'` if it's in the way.
- Do NOT send multi-byte strings with `\n` through nested heredocs to the target;
  they get mangled. Keep openocd command lists as separate `-c "..."` args.
- Leave the board **running** after tests (end with `reset run`, not a bare
  `halt`) — a left-halted CPU looks "dead" (blank display / no blink).
