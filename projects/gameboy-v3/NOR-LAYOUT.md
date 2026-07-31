# SPI-NOR component layout — shared contract (U-Boot + custom bootloader)

The on-board **W25Q128 SPI-NOR (16 MB)** holds the boot *components* (kernel, DTB,
initramfs) but **NOT a bootloader**. The bootloader itself is delivered separately
via **FEL** (loaded into SRAM/RAM over USB, then executed); it then reads these
components from NOR at the fixed offsets below.

## Why no bootloader in NOR (the load-bearing invariant)

**NOR offset 0 is kept BLANK (no `eGON.BT0` header).** The T113 BROM, finding no
valid eGON at offset 0, falls through to **FEL on every power-up** — so the board
is never stranded and is always remotely reflashable. If a valid eGON is ever
written to offset 0, the BROM boots it and FEL is lost (this stranded the board
once — see t113-breakout/REV-B-FIXES.md Fix 5). **Never write a bootable image to
NOR offset 0 until a hardware force-FEL circuit exists.**

This makes the dev loop: `power-cycle → FEL → xfel loads the bootloader into
SRAM/RAM → bootloader reads components from NOR → boots`. Production-fidelity for
everything *downstream* of bootloader delivery (real DDR init, real SPI-NOR reads,
real component layout, real kernel handoff); only the bootloader's own delivery is
via FEL instead of BROM-from-NOR (that step needs the load-switch rig to test).

## Layout (64 KB-aligned; W25Q128 erase units are 4K/32K/64K)

```
NOR offset   reserved            contents
─────────────────────────────────────────────────────────────────────
0x000000     64 KB               GUARD + component table.
                                  - offset 0 is NOT a valid eGON  → forces FEL
                                  - a small component table lives here (below)
                                  - rest of the 64 KB left 0xFF
0x010000     6 MB  (→0x610000)    zImage           (currently 5.33 MB)
0x610000     64 KB (→0x620000)    board DTB        (currently 18 KB)
0x620000     2 MB  (→0x820000)    initramfs.cpio.gz(currently 0.76 MB)
0x820000     ~7.9 MB free                          spare / future
```

Each component starts on a 64 KB boundary so `xfel spinor erase <off> <len>` can
rewrite one without disturbing its neighbors.

## Component table (at NOR 0x000000)

A tiny fixed record so loaders don't hardcode sizes (and so the initramfs size —
needed for the kernel `initrd=` cmdline arg — is always known). Little-endian
(ARMv7 LE). Magic is deliberately **not** `eGON.BT0`, so the BROM still rejects
offset 0 and drops to FEL.

```
off  size  field
0x00  8    magic       = "GV3NOR1\0"   (ASCII; MUST NOT be "eGON.BT0")
0x08  4    version     = 1
0x0C  4    (reserved)
0x10  4    kernel_off  = 0x010000
0x14  4    kernel_size = <actual zImage bytes>
0x18  4    dtb_off     = 0x610000
0x1C  4    dtb_size    = <actual DTB bytes>
0x20  4    initrd_off  = 0x620000
0x24  4    initrd_size = <actual initramfs bytes>
0x28  4    crc32       (of bytes 0x00..0x27; 0 = "unchecked")
0x2C ...   0xFF (rest of the 64 KB guard)
```

Both loaders: read this 40-byte table from NOR 0x0 → validate magic → read each
component from its {off,size} into the DRAM load address below.

## DRAM load addresses (identical for both loaders; match boot.cmd / bootloader/main.c)

```
kernel   → 0x41000000
DTB      → 0x41800000
initramfs→ 0x41C00000
```

## How each loader consumes this

**U-Boot** (needs `sf` + a NOR DT node): a boot script does
`sf probe; sf read 0x41000000 0x010000 <ksize>; sf read 0x41800000 0x610000 <dsize>;
sf read 0x41C00000 0x620000 <isize>; setenv bootargs "... initrd=0x41C00000,<isize>";
fdt ...; bootz 0x41000000 0x41C00000:<isize> 0x41800000`. (Sizes read from the table,
or over-read to the reserved size.)

**Custom bootloader** (needs `spinor.c`): Stage 3 calls `spinor_read(off, dram, size)`
for each component (replacing the SD/FAT `fat_load`), reusing all existing DDR-init,
FDT-patch, and `boot_kernel` code. Uses `nor_layout.h` (the C form of this table).
The kernel slot holds a **zImage** either way — the mainline self-decompressing
zImage or our custom kernel's zImage-headed image (`b _start`@0x00, magic@0x24). Both
are entered at the load address (0x41000000), so the bootloader just jumps there; no
per-kernel handling, matching U-Boot's `bootz`.

## Flashing NOR (over FEL — always available since offset 0 is blank)

**Preferred: the two-script loop** — `build.sh` compiles a config into a
self-contained bundle, `flash.sh` flashes it to NOR AND FEL-boots it in one command
(it stages the bundle to the rig itself, so you run it from the dev host):

```
make image                                   # bundle (defaults: custom+linux+busybox)
make image KERNEL=custom BOOTLOADER=custom LIBC=gv3 COREUTILS=gv3   # any config
make image LCD=ili9341                       # + ILI9341 panel driver
tools/flash.sh build/bundles/<cfg> nor           # flash NOR + FEL-boot, watch UART
```

`flash.sh <bundle> nor` does, on the rig: power-cycle→FEL (t113power.sh) → xfel
spinor write kernel/dtb/initramfs + the GV3NOR1 table → FEL-deliver the loader →
tail the console. `MEDIA=sd`/`emmc` give a clear "not remotely flashable" error
(xfel can't write SD; this board has no eMMC).

Under the hood the NOR write is:
```
xfel spinor write 0x010000 zImage
xfel spinor write 0x610000 <board>.dtb
xfel spinor write 0x620000 initramfs.cpio.gz
xfel spinor write 0x000000 nor-table.bin      # the GV3NOR1 component directory
# NEVER: xfel spinor write 0x000000 <a bootable eGON>   ← would strand the board
```

(The old `05-nor-flash.sh` / `06-nor-boot.sh` were removed 2026-07-30 — `flash.sh`
supersedes them and is verified for BOTH loaders on silicon.)

**Verified on silicon (2026-07):** both loaders boot Linux to a shell from these
NOR components, via `flash.sh <bundle> nor`:
- Custom bootloader (`loader-fel.bin`): its own DDR init + SPI-NOR read (spinor.c) +
  FDT patch + boot_kernel → `cores online: 1`.
- U-Boot (`uboot-proper.bin`): xfel DDR + U-Boot proper + `sf read` + `bootz` →
  `cores online: 2` (PSCI). flash.sh re-arms FEL after the NOR writes before the
  U-Boot DRAM load — an xfel quirk: `spinor write` then `ddr`+high-DRAM-write fails
  (rc=255) without a fresh FEL entry.
Note the custom bootloader's start.S sets CNTFRQ=24MHz (else the kernel's arch
timer div-by-zeros), and fdt.c is built -O0 (a GCC10 -Os miscompile hangs the
FDT walk).

_Single source of truth for both loaders + `flash.sh`. Keep offsets here in sync
with `bootloader/nor_layout.h`, `forge/backends/image.sh` (manifest), and the U-Boot boot
script._
