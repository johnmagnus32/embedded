# gameboy-v3 custom bootloader

Writing our own bootloader to replace U-Boot on the T113-S3 — as a learning
exercise, to understand exactly what the boot chain does. It **boots from SD**
(the same `dd`-to-8 KiB path as our U-Boot), so it's a true drop-in replacement.

> This is separate from the main build (Steps 0–4, which use mainline U-Boot).
> It reuses the Step 0 toolchain, and later stages will reuse the Step 2/3
> kernel + initramfs as this bootloader's payload.

## Status: Stage 4 ✅ — a complete bootloader that boots Linux

All four stages are built. This is now a from-scratch SD bootloader that (in
principle) takes the T113-S3 from power-on to a Linux shell, replacing U-Boot.

| Stage | Goal | Needs |
|-------|------|-------|
| **1 ✅** | Print "hello" on UART0 from our code in SRAM | eGON header, CPU setup, UART0 |
| **2 ✅** | Initialize the 128 MB DDR3, run a memtest | DRAM init (U-Boot sun20i driver) |
| **3 ✅** | Read kernel/DTB/initramfs off the SD (FAT) into DRAM | SMHC0 driver + FAT16/32 reader |
| **4 ✅** | **Patch the DTB + jump to Linux** | FDT bootargs patch + ARM boot handoff |

Stage 4 adds `fdt.c` (set `/chosen/bootargs` in place) and a `boot_kernel`
routine in `start.S` (the ARM Linux handoff: `r0=0`, `r1=~0`, `r2=DTB`, MMU +
caches off, branch to the kernel). `main()` builds the kernel command line
(console, `mem=128M@0x40000000`, `initrd=<addr>,<size>`), patches it into the
DTB, and jumps.

### How Stage 4 avoids a full FDT editor (design note)

Editing a flattened device tree in place is fiddly (growing the strings block,
fixing every offset). We sidestep almost all of it by splitting fixed vs.
runtime data:

- **RAM size** is fixed → a `/memory` node (128 MiB @ 0x40000000) is baked into
  the DTB at build time via `scripts/uart0-console.dtsi`. (U-Boot normally
  supplies this; our bootloader doesn't, so we state it in the overlay.)
- **initrd** address is fixed but its **size** is runtime → passed on the kernel
  command line as `initrd=<addr>,<size>` (parsed by the kernel's `early_initrd`),
  so no `linux,initrd-start/end` DTB properties are needed.
- The command line lives in `/chosen/bootargs`. The overlay reserves a
  **fixed-length `bootargs` placeholder**, so the bootloader only **overwrites
  that string's value in place** — no length change, no structural edit. `fdt.c`
  is therefore a read-only token walker + one in-place `memcpy`.

**Verified end-to-end where possible:**
- **FAT reader** (`fat.c`): compiled natively against the real
  `gameboy-v3-sd.img` → loads all three files **byte-exact** (incl. LFN names).
- **FDT patcher** (`fdt.c`): compiled natively against the real kernel DTB →
  sets bootargs, and `dtc` re-parses the result as a valid tree with the exact
  cmdline and the `/memory` node intact.
- **`boot_kernel`**: disassembly confirms `r0=0` / `r1=~0` / `r2=dtb` / MMU+cache
  off / `bx` to the entry — the exact ARM boot contract.
- The **SD driver register pokes** and the **actual jump-to-Linux** can only be
  confirmed on silicon.

Stage 1 proved the **BROM handoff**, **eGON format + checksum**, and **bare-metal
UART**. Stage 2 adds the hard part — **DDR3 bring-up** — then a quick memory test
to confirm RAM actually works. After Stage 2, `main()` prints `DRAM OK: 128 MiB`
and `Memory test PASSED` (on real hardware).

### On the DRAM driver (`dram.c` / `dram.h`)

`dram.c` is **U-Boot's `drivers/ram/sunxi/dram_sun20i_d1.c`, copied verbatim**
(GPL-2.0+), and `dram.h` is its header. This is the deliberate "lift, don't
reinvent" call from the design notes: the DDR3 init sequence is ~1450 lines of
register pokes + PHY training that originated as decompiled Allwinner boot0 code
— nobody writes it from scratch (mainline U-Boot didn't either). We reuse the
proven sequence and supply only:

- `dram_shim.h` — freestanding replacements for the handful of U-Boot facilities
  the driver expects (`readl`/`writel`/bit helpers, `udelay`, `BIT`/`max`,
  `DIV_ROUND_UP`, base addresses) **and the board tunables** (`CONFIG_DRAM_*`),
  taken verbatim from the MangoPi MQ-R defconfig we verified (same DDR3 die).
- `dram_shim.c` — a tiny `printf` (routes to UART0) for the driver's messages,
  including the `"ZQ calibration error, check external 240 ohm resistor"` line.

The entry point is `sunxi_dram_init()`, which returns the detected size in bytes.
Everything *we* wrote (start.S, uart.c, main.c, mk-egon.c) is still from scratch.

## How it works

```
BROM  ── reads 8 KiB offset on SD, loads to SRAM 0x20000, jumps to offset 0
  │
  ▼
start.S  .text.head = eGON.BT0 header; word 0 is `b _start` (0xea000016),
  │      jumping over the 96-byte header. Then: SVC mode, IRQ/FIQ masked,
  │      MMU+caches off, set SP, zero BSS, call main().
  ▼
main.c   uart0_init(): mux PE2/PE3 → UART0 (func 6), ungate+dereset the UART0
         bus clock (CCU 0x90c), program the 16550 (8N1, divisor 13 @ 24 MHz).
         Then print the banner. Returns to start.S, which spins.
```

## Files

- `start.S`     — eGON header + CPU init + jump to C
- `main.c`      — orchestration: UART → DRAM → memtest → SD/FAT load
- `uart.c/.h`   — UART0 driver (PE2/PE3), shared with printf
- `dram.c/.h`   — U-Boot's sun20i DDR3 driver, copied verbatim (see above)
- `dram_shim.c/.h` — freestanding shims + tiny printf + `udelay` + board tunables
- `sdcard.c/.h` — SMHC0 controller + SD bring-up + PIO block reads
- `fat.c/.h`    — MBR + FAT16/32 reader (LFN-aware), loads files by name
- `sram.ld`     — link at SRAM 0x00020000, header first
- `mk-egon.c`   — host tool: pad to 512 B, write length, compute BROM checksum
- `Makefile`    — compile → link → objcopy → finalize eGON

## Build

```bash
forge/backends/toolchain.sh (or `make toolchain`)          # once (shared toolchain)
make                                 # → build/gv3boot.egon.bin
```

## Verified register values (sun20i / T113-S3 / R528)

All taken from mainline sources for this exact SoC, not guessed:

| Thing | Value | Source |
|-------|-------|--------|
| SRAM load/run | `0x00020000` | U-Boot `SUNXI_SRAM_ADDRESS` (NCAT2) |
| eGON magic / stamp | `"eGON.BT0"` / `0x5F0A6C39` | U-Boot `sunxi_image.h` |
| Branch over header | `0xea000016` | `0xea000000 \| (96/4-2)` |
| CCU base | `0x02001000` | `cpu_sunxi_ncat2.h` |
| UART0 gate/reset | `CCU+0x90c`: gate `BIT(0)`, reset `BIT(16)` | kernel `ccu-sun20i-d1.c` |
| UART0 base | `0x02500000` | kernel DT `serial@2500000` |
| PIO base / bank size | `0x02000000` / `0x30` | U-Boot `sunxi_gpio.h` (NCAT2) |
| PE bank | `0x020000C0` (`PIO + 4*0x30`) | derived |
| PE2/PE3 mux | func `6` | U-Boot `board.c` (R528 branch) |
| UART divisor | `13` (24 MHz / 16 / 115200) | 16550 standard |

## Flash & boot (⚠️ hardware, verifies nothing until you run it)

Write the eGON image to the SD at the **8 KiB** offset — same as U-Boot. This
*replaces* whatever bootloader is there:

```bash
lsblk                                # find the card — NOT a partition
sudo dd if=build/gv3boot.egon.bin of=/dev/sdX bs=1024 seek=8 conv=fsync
sync
```

The on-board SPI-NOR stays blank; the BROM falls through to the SD. Then wire the
serial console (adapter RX→PE2/H11-18, TX→PE3/H10-18, GND; 115200 8N1 — same as
the main project), power via USB-C, and you should see:

```
===================================================
  gameboy-v3 custom bootloader
  Stage 1: UART0 up (hello from SRAM)
  Stage 2: initializing DDR3 ...
===================================================
DRAM OK: 128 MiB @ 0x40000000
Running quick memory test ...
Memory test PASSED.

Stage 3: reading boot files from SD ...
SD: card up (rca=0x..., HC/block-addr)
FAT16 mounted: part@2048 data@2337 spc=4 clusters=32183
FAT: loaded 'zImage' (5592384 bytes)
FAT: loaded 'sun8i-t113s-mangopi-mq-r-t113.dtb' (18054 bytes)
FAT: loaded 'initramfs.cpio.gz' (1155695 bytes)

All loaded into DRAM:
  kernel   @ 0x41000000  (5592384 bytes)
  dtb      @ 0x41800000  (18054 bytes)
  initramfs@ 0x41c00000  (1155695 bytes)

Stage 4: preparing to boot Linux ...
cmdline: console=ttyS0,115200 earlycon=on mem=128M@0x40000000 initrd=0x41c00000,1155695 panic=10
FDT: bootargs set (88/218 bytes used)
Jumping to kernel. Bye from the bootloader!

[    0.000000] Booting Linux on physical CPU 0x0
... (kernel log) ...
/ #                                 <- BusyBox shell, via OUR bootloader
```

After "Jumping to kernel", the handoff is out of our hands — the zImage
decompresses itself and Linux takes over, exactly as in the U-Boot path (same
`zImage` + DTB + initramfs). Reaching the shell means the full chain worked:
BROM → our SPL → DRAM → SD/FAT → DTB patch → Linux.

**If nothing prints at all:** check TX/RX aren't swapped; confirm 3.3 V adapter +
common GND + 115200. If the board seems dead, the BROM may have rejected the image
— re-check with `../build/u-boot/tools/mkimage -l build/gv3boot.egon.bin` (should
say "Allwinner eGON image"). Recovery: with no valid boot media the BROM drops to
FEL/USB, so a bad image can't brick the board.

**If Stage 1 banner prints but DRAM fails:** the driver prints its own diagnostics
(e.g. `ZQ calibration error, check external 240 ohm resistor`, or `DRAM: simple
test FAIL`). The `udelay` in `dram_shim.h` is a rough busy-loop (PLLs aren't up
during init); if training is flaky, that calibration is the first thing to revisit.

## What's verifiable here vs. on hardware

The build **can** prove the eGON image is structurally valid (U-Boot's own
`mkimage -l` accepts it; checksum recomputes; branch/length fields right) and that
everything fits in SRAM. It **cannot** prove DRAM trains or the memtest passes —
that needs the physical T113 + serial adapter. DRAM init is the stage most likely
to need on-silicon tuning (`udelay` calibration, board timing), so treat the
hardware bring-up here as expected iteration, not a one-shot.
