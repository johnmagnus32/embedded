# gameboy-v3 — Allwinner T113-S3 Linux handheld

**Status:** 🚧 Bring-up. Goal right now: **boot a Linux image on the
[t113-breakout](../t113-breakout) board and get a working serial console.**
Display, sound, and input come later, once Linux boots reliably.

**Build: complete (Steps 0–4 ✅).** All four artifacts + a flashable SD image
(`build/output/gameboy-v3-sd.img`) build reproducibly via `make image` through the
shared engine [`forge/`](../../forge/) (see the [boot matrix](#boot-matrix--interchangeable-components--silicon-results)
for the provider matrix). **Runs on real T113 silicon** — both the custom and
all-OSS stacks boot to an interactive shell (flashing: [first boot](#flashing--first-boot-generic--local-bench)).

## Contents

This is the single doc for the product. Major sections:

- **Overview & build** — [what this is](#what-this-is) · [boot chain](#boot-chain-phase-1-everything-on-microsd) · [build plan (Steps 0–4)](#build-plan--build-it-by-hand-once-then-buildroot) · [UART console](#uart-console) · [both cores (SMP)](#both-cores-smp) · [references](#references)
- **[Boot matrix](#boot-matrix--interchangeable-components--silicon-results)** — the provider-interchange matrix + on-silicon results (formerly BOOT-MATRIX.md).
- **[SPI-NOR layout](#spi-nor-layout--the-loader-contract)** — the loader contract: NOR offsets, GV3NOR1 table, DRAM load addrs (formerly NOR-LAYOUT.md).
- **[Flashing & first boot](#flashing--first-boot-generic--local-bench)** — dd a card, wire the console, expected boot, troubleshooting (formerly FLASH.md).
- **[Remote rig](#remote-bring-up-rig-ssh-driven-nuc)** — the SSH/NUC bring-up runbook: power-cycle, UART capture, FEL (formerly REMOTE-RIG.md).
- **[ILI9341 LCD](#ili9341-spi-lcd-later-phase)** — the SPI display peripheral (formerly LCD-ILI9341.md).

## What this is

The third iteration of the handheld. Where v1 was bare-metal STM32 and v2 was
STM32 + iCE40 FPGA, **v3 moves to a full Linux SBC**: the dual-core ARM
Cortex-A7 **Allwinner T113-S3** (with 128 MB in-package DDR3) running mainline
Linux. Game logic, display, and audio all run as Linux userspace/kernel on the
one SoC — no separate CPU/PPU split.

For Phase 1 the **whole boot image lives on the microSD card** (SPI-NOR left
blank) so the dev loop is just "reflash the card." See
[Boot chain](#boot-chain-phase-1-everything-on-microsd) below.

For bring-up, the SoC lives on the **t113-breakout** board (see
[../t113-breakout/README.md](../t113-breakout) — *note: that project currently
has `BOM.md` + `PINOUT.md`, not a README*). Peripherals (ILI9341 display,
audio, buttons) get wired to the breakout's headers **later**; this phase is
boot + console only.

## Phase 1 goal (current)

1. Build/obtain a bootable Linux image (SPL/U-Boot + kernel + rootfs) as a
   **single microSD image** — SPI-NOR left blank.
2. `dd` the image to a card and boot the T113-S3 on the breakout board from it.
3. Get a **UART serial console** at the login prompt.

Everything below the display/audio/input line is out of scope until this works.

## Boot chain (Phase 1: everything on microSD)

For bring-up the **entire boot image lives on the microSD card** and the
on-board SPI-NOR is left **blank**. This is the standard Allwinner sunxi
bring-up path and the simplest possible loop: build one image, `dd` it to the
card, boot.

```
BROM  →  microSD  →  SPL (boot0)  →  U-Boot  →  kernel + rootfs
         (SPI-NOR blank → BROM finds no valid boot header there and boots SD)
```

Why all-SD first:
- **One artifact, one reflash.** A single full-disk `.img` (SPL + U-Boot + boot
  partition + rootfs) is written with one `dd`. To change anything, re-`dd` the card.
- **Avoids the SPI-NOR chicken-and-egg.** Writing SPL/U-Boot to the W25Q128
  needs *either* FEL tooling (`xfel`) *or* an already-booted U-Boot — you can't
  "flash the chip" on a bare board without one of those. SD boot needs neither.
- **FEL over USB-C is still the backstop.** If an SD image is bad, the BROM
  drops to Allwinner FEL (USB boot) and the board is recoverable — unbrickable.

A blank (0xFF) SPI-NOR has no valid Allwinner eGON boot header, so the BROM
skips it and boots the SD. Nothing needs to be on SPI-NOR for this to work; the
PC4/PC5 BOOT-SEL straps are moot while the flash is empty.

### SD image layout (standard sunxi)

- **SPL (boot0)** at **8 KB offset** (sector 16).
- **U-Boot** right after — the combined `u-boot-sunxi-with-spl.bin` covers both
  in a single write at the 8 KB offset.
- **Partitions** start well past that (e.g. first partition at 1 MB / 16 MB):
  a boot partition (kernel + DTB + boot script) and an ext4 rootfs.

```bash
# Option A: write just the SPL+U-Boot blob to a raw card, partition the rest yourself
sudo dd if=u-boot-sunxi-with-spl.bin of=/dev/sdX bs=1024 seek=8 conv=fsync

# Option B: dd a full prebuilt disk image (SPL+U-Boot+boot+rootfs) — one shot
sudo dd if=t113-gameboy-v3.img of=/dev/sdX bs=4M conv=fsync status=progress
```

> **Migrate to SPI-NOR later.** Once the software stack is proven, SPL/U-Boot
> (or a full boot image) can move to the W25Q128 for a more product-like,
> SD-optional boot. Not needed now — SPI-NOR stays blank for Phase 1.

## Build plan — build it by hand once, then Buildroot

**Approach (optimizing for understanding):** build each layer of the boot chain
**by hand, in boot order**, so every handoff is something you configured and
watched happen over UART. *Then* adopt Buildroot to make it reproducible. This
is the standard Bootlin/embedded-Linux teaching order — the from-scratch pass
builds the mental model, Buildroot becomes the tool you graduate to (it's small
and transparent, so after the manual pass you recognize every piece it
automates). Yocto is deliberately skipped — right tool for production, wrong
first teacher.

We're **using a prebuilt toolchain** (not building our own — that's a separate
rabbit hole). Everything else we build.

**Reproducibility & structure (forge refactor):** the build is driven by the
shared engine [`forge/`](../../forge/) (at the repo root, reused by any product),
not per-product scripts. `make image` from here resolves this product's selection
([config.mk](config.mk)) + board inputs and orchestrates the generic build
backends in [`forge/core/`](../../forge/core/) (the idempotent fetch/build
recipes — the old `NN-*.sh`, now engine-owned). Pins split by tier:
- **Engine** ([`forge/core/resolve.mk`](../../forge/core/resolve.mk)) — the
  HOST-constrained cross-toolchain / arch pins (chosen by the build host, not this
  product); the build mechanism itself lives in [`forge/core/run-recipe.sh`](../../forge/core/run-recipe.sh).
- **Product** ([`versions.env`](versions.env)) — the OSS component pins (kernel /
  U-Boot / BusyBox tags + checksums), as data. Bump a version here.
- **Board** ([`boards/t113-gameboy/`](boards/t113-gameboy/)) — `board.conf`, the
  single board config (provider targets + defconfigs/board-DT/console + NOR/DRAM/SD
  layout), plus the `*.dtsi` overlays + `boot.cmd` + `genimage.cfg`.

All build inputs/outputs land in `build/` (git-ignored); `rm -rf build/` then
`make image` is the reproducibility test. Never a floating "latest" — bump a pin
deliberately.

**Target facts (all mainline, verified against U-Boot/kernel git):**
- T113-S3 = **32-bit ARMv7-A** dual Cortex-A7 → an **`armv7-eabihf` glibc**
  toolchain (NOT `arm-none-eabi`, which is bare-metal). We use Bootlin's
  prebuilt, whose actual prefix is **`arm-buildroot-linux-gnueabihf-`** (Bootlin
  builds it with Buildroot; still a standard armv7 hard-float glibc GCC).
- **No TF-A / BL31 needed.** BL31 is a 64-bit (ARMv8 EL3) component; the generic
  sunxi docs' BL31 step applies only to A64/H616-class parts. The T113 is
  ARMv7 with no EL3, so U-Boot builds standalone.
- **DDR3 init is fully upstreamed** — no vendor fork required. It lives in
  U-Boot SPL at `drivers/ram/sunxi/dram_sun20i_d1.c`; the 128 MB DDR3 timings
  are baked into the board defconfig.
- **Both Cortex-A7 cores come up** on mainline — via PSCI supplied by U-Boot, not
  a kernel SMP method. `MACH_SUN8I_R528` selects U-Boot's built-in ARMv7 PSCI
  monitor (`arch/arm/cpu/armv7/sunxi/psci.c`), which releases CPU1 from reset and
  patches `enable-method="psci"` + a `/psci` node into the kernel DTB at boot. So
  the kernel needs no `platsmp.c` change and no static DT edit. (Correcting an
  earlier note — see [Both cores (SMP)](#both-cores-smp).)

### Step 0 — Build environment (toolchain + host make) ✅ implemented

Prepares everything needed before building anything. `make toolchain` provisions the
base host-tool set through the host-package subsystem
(`host_provision make toolchain-glibc toolchain-musl gen_init_cpio`; see
docs/LINUX_HOST_PACKAGE_MODEL.md). It fetches + checksum-verifies + extracts **two**
pinned Bootlin cross toolchains and sanity-checks each emits 32-bit ARM, and ensures
**GNU Make ≥ 4.0** is available (the kernel requires it — see Step 2); it uses the
host's own make if new enough, otherwise builds a pinned one into `build/hostmake/`.

- **glibc toolchain** (`arm-buildroot-linux-gnueabihf-`) → U-Boot + kernel (they
  don't link a target libc, so libc choice is irrelevant there).
- **musl toolchain** (`arm-buildroot-linux-musleabihf-`) → the rootfs only.
  musl-static BusyBox is ~34% smaller than glibc-static and has a lighter Linux
  syscall surface — both a size win now and groundwork for a hand-written kernel.

```bash
make toolchain                     # idempotent; provisions the base host-tool set (pure Make)
```

- Pinned: `armv7-eabihf--glibc--stable-2021.11-1` (GCC 10.3.0), verified against
  Bootlin's published SHA256. The script **refuses to proceed on a checksum
  mismatch** — an unverified toolchain never enters the build.
- **Why not the newest toolchain?** This build host is **glibc 2.26**. Bootlin's
  2024+/2025 toolchains ship a binutils `ld` that needs GLIBC_2.27, so its `ld`
  won't run here — the build dies at the first *link* step (a compile-only check
  passes, so it surfaces mid-build, confusingly). 2021.11 gives GCC 10.3.0
  (satisfies U-Boot's host-gcc-≥10 requirement) with a `ld` needing only
  GLIBC_2.14. On a newer host you can bump the pin (PKG_VERSION/PKG_SHA256) in the
  `forge/hostpackages/toolchain-*` recipes (toolchain pins are engine-tier — host-constrained).
- Installs to `build/toolchain/`; `CROSS_COMPILE=arm-buildroot-linux-gnueabihf-`
  (Bootlin builds their toolchains with Buildroot, hence the `buildroot` triple).
- [`forge/core/run-recipe.sh`](../../forge/core/run-recipe.sh) loads the build env at
  every node, which puts both cross toolchains + make on PATH — set once, per build.

### Step 1 — U-Boot (SPL + U-Boot proper) ✅ implemented

Clones mainline U-Boot at a pinned tag, applies our board's console + host-tool
config, overlays the control DTB, builds, and copies the artifact to `output/`:

```bash
make bootloader BOOTLOADER=uboot   # idempotent; runs providers/bootloader/uboot/build.sh
# → build/output/u-boot-sunxi-with-spl.bin  (SPL + U-Boot + DTB, one ~480K blob)
```

- **Pinned U-Boot v2026.04**, `mangopi_mq_r_defconfig` (`CONFIG_MACH_SUN8I_R528=y`;
  T113-S3 shares the R528 die). T113 support has been in mainline since v2024.01;
  there is no generic `sun20i`/`t113` defconfig.
- **No TF-A/BL31** — 32-bit ARMv7 has no EL3; `SUNXI_BL31_BASE=0` for R528, so the
  build emits no BL31/SCP warning. (The generic sunxi docs' BL31 step is 64-bit-only.)
- **Both Cortex-A7 cores come up** at runtime: `MACH_SUN8I_R528` selects U-Boot's
  built-in **ARMv7 PSCI** monitor, which powers on CPU1 and patches
  `enable-method="psci"` into the kernel DTB at boot. No kernel patch needed.
  (This corrects the earlier "single-core only" note — see [Both cores](#both-cores-smp).)
- DRAM timings are baked into the defconfig (`DRAM_CLK=792`, `DRAM_ZQ=8092667`,
  `TPR0/11/12/13`, `SUNXI_MINIMUM_DRAM_MB=128`); same in-package die as the MQ-R,
  so they should port unchanged — only re-tune if the board proves unstable.

**Console → UART0/PE2-PE3 (the [#1 trap](#uart-console)):** the script applies
**both** required changes and it's verified in the built DTB:
1. `CONFIG_CONS_INDEX=1` (defconfig) → SPL console pinmux to PE2/PE3.
2. `boards/t113-gameboy/uart0-console.dtsi` overlay (`#include`d into the board `.dts`) →
   sets `stdout-path=serial0`, `serial0=&uart0`, enables `&uart0` on a
   `uart0_pe_pins` (PE2/PE3) group, disables `&uart3`. Needed because U-Boot
   *proper* uses DM serial and reads the console from the DTB — `CONS_INDEX`
   alone gives a split-brain console (SPL on UART0, U-Boot proper back on UART3).

**Host-tool trims** (baked into the config fragment; this build host lacks the
libs and a sunxi image doesn't need them): `TOOLS_LIBCRYPTO=n` (host OpenSSL
1.0.2 too old for U-Boot's signing tools; no signing needed), `TOOLS_KWBIMAGE=n`
(Marvell, pulls in libcrypto), `TOOLS_MKEFICAPSULE=n` (needs gnutls headers).
The script also builds an isolated **python venv** (`build/pyenv/`) with
binman's deps (pyelftools/pyyaml/setuptools/importlib_resources) because the
Bootlin toolchain ships its own python3 that lacks them.

**Learning checkpoint:** `dd` this to a card (Step 4) with nothing else, power on,
and you should get the U-Boot prompt over UART. That alone proves BROM → SPL
(DRAM init) → U-Boot.

### Step 2 — Kernel + device tree ✅ implemented

Clones mainline Linux at a pinned LTS tag, builds `zImage` + the board DTB with
the UART0 console overlay, copies both to `output/`:

```bash
make kernel KERNEL=mainline        # idempotent; runs providers/kernel/mainline/build.sh
# → build/output/zImage  and  build/output/sun8i-t113s-mangopi-mq-r-t113.dtb
```

- **Pinned Linux v6.12.95** (LTS; T113 board DTS landed v6.4, settled into the
  `allwinner/` subdir at v6.5), `sunxi_defconfig`. Enables the essentials as `=y`:
  `SERIAL_8250_DW` (UART), `MMC_SUNXI` (SD), the D1/T113 CCU + pinctrl.
- **DTB:** `sun8i-t113s-mangopi-mq-r-t113.dtb` (SoC dtsi cross-includes the RISC-V
  D1's sun20i dtsi files — expected; same die).
- **Console → UART0:** the script `#include`s the **same `boards/t113-gameboy/uart0-console.dtsi`**
  overlay used for U-Boot (the kernel DTS is structurally identical). Verified in
  the built DTB: `stdout-path=serial0`, uart0 okay on PE2/PE3, uart3 disabled.
  With `serial0=&uart0`, Linux names uart0 **ttyS0** → the kernel cmdline console
  is `console=ttyS0,115200` (set in the boot script in Step 4, not the DTB).
- **Both cores:** the kernel DTS's `cpu@1` has no `enable-method` — that's fine;
  U-Boot's PSCI patches it in at boot (see [Both cores](#both-cores-smp)).

**Host workarounds** (this build host is old): the kernel needs **GNU Make ≥ 4.0**,
which **Step 0** provides (uses the host's if new enough, else builds a pinned
Make 4.4.1 into `build/hostmake/`) — this step just verifies it. And
`CONFIG_GCC_PLUGINS` is disabled here because the host lacks `gmp.h` (a hardening
feature, not needed to reach a console).

**Learning checkpoint:** boot `zImage` + DTB from U-Boot and watch kernel log
spew over UART. First rootfs iteration is easiest over **NFS** (no reflash per
build) before committing to an SD rootfs.

### Step 3 — Root filesystem (static BusyBox initramfs) ✅ implemented

The highest teaching-per-effort step: a static BusyBox + a tiny `/init`, packed
as a `cpio.gz` initramfs — the simplest thing that boots straight to a shell
(no root partition, no `root=`; U-Boot loads kernel + DTB + initramfs together).

```bash
make rootfs KERNEL=mainline LIBC=musl PACKAGES=busybox   # runs forge/steps/rootfs/build.sh
# → build/output/initramfs.cpio.gz   (~792K: musl-static busybox + 401 applets)
```

- **Pinned BusyBox 1.36.1** (last upstream "stable"-labeled release), SHA256-verified.
- **Built with the musl toolchain** (`ROOTFS_CROSS_COMPILE`), not glibc: the
  static binary is 1.38 MB vs 2.08 MB, so the initramfs is **792 KB vs 1.15 MB
  (31% smaller)**. Selected via BusyBox's `CONFIG_CROSS_COMPILER_PREFIX` **and**
  `CROSS_COMPILE=` on both the build and `make install` lines — the install step
  re-checks deps and would otherwise silently rebuild with the glibc env compiler.
- **Static build:** `make defconfig`, then `CONFIG_STATIC=y` + `CONFIG_TC=n` via a
  scripted `.config` edit + `make oldconfig`. `CONFIG_TC=n` avoids the `tc.c`
  `TCA_CBQ_MAX` break (kernel headers ≥ 6.8 removed it; unfixed upstream) and
  isn't needed for a minimal rootfs anyway.
- **`/init` is PID 1** — a SINGLE portable [overlay/init.sh](overlay/init.sh)
  shipped for EVERY rootfs provider (PID-1 policy is the product's, not the
  provider's). Written in the shell subset both our gv3 shell and busybox parse:
  best-effort mounts `/proc`, `/sys`, devtmpfs `/dev` (real on mainline; kernel
  no-op success on the custom stack), a static banner, then `exec /bin/sh`. If
  PID 1 ever exits the kernel panics — so it hands off to a shell that stays up.
- **Packaged with the kernel's `gen_init_cpio`** (reused from Step 2's tree): it
  emits a root-owned cpio and creates `/dev/console` (char 5,1) **without needing
  root** — that node must exist in the initramfs or PID 1 gets no stdin/out/err.

Verified by listing the built cpio (`cpio -itv`): `/init` 0755 root, `bin/busybox`
static ARM, `/dev/console` present, 401 applet symlinks → `/bin/busybox`.

> **Learning note:** to *see* the "clicks" for real, temporarily remove `/init`
> from the image and boot — the kernel panics with "No init found." Add it back
> and the shell appears. That failure→fix is the whole lesson in how userspace
> starts.

**Then graduate to Buildroot** for a reproducible rootfs (and, if you like, to
drive the U-Boot + kernel builds too): start from the community
`utuM/Buildroot-T113-S3` (board `mq-dual`) and adjust the kernel/U-Boot DT for
our console. After this manual pass, its generated `.config` and `output/build/`
tree will read like a checklist of things you already understand.

### Step 4 — Assemble the SD image ✅ implemented

Assembles the four artifacts into one flashable full-disk image:

```bash
make image MEDIA=sd              # → build/output/gameboy-v3-<cfg>-sd.img (~64 MiB)
```

Image layout (verified after build):

```
offset 0      MBR partition table (p1 = FAT32 LBA @ 1 MiB)
offset 8 KiB  U-Boot SPL + U-Boot proper           (eGON header — BROM boots this)
offset 1 MiB  FAT partition 1:  zImage, sun8i-t113s-mangopi-mq-r-t113.dtb,
                                initramfs.cpio.gz, boot.scr
```

- Built **without root**: `sfdisk` partitions a plain file, `mkfs.vfat` + `mcopy`
  populate the FAT fs, `dd conv=notrunc` splices U-Boot + the fs into the image.
- U-Boot's `distro_bootcmd` auto-scans the partition (prefixes `/`, `/boot/`) for
  `boot.scr` and runs it — no `bootcmd` editing. The script
  ([boards/t113-gameboy/boot.cmd](boards/t113-gameboy/boot.cmd) → compiled to `boot.scr` with mkimage)
  sets `console=ttyS0,115200`, loads the three files to U-Boot's built-in sunxi
  load addresses (`kernel_addr_r=0x41000000`, `fdt_addr_r=0x41800000`,
  `ramdisk_addr_r=0x41C00000`), and runs
  `bootz ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}` (the
  `:size` is required for a raw cpio.gz ramdisk).

**Flashing + first boot is a separate, hardware + root step — see
[FLASH.md](#flashing--first-boot-generic--local-bench)** for the `dd` command, serial wiring, expected boot log,
and troubleshooting. That is where the whole chain gets validated on real silicon;
the build itself can only verify the image *structure* (which it does).

### Recovery (can't-brick backstop)

Use **`xfel`** (not `sunxi-fel`) for the sun20i generation — it fully supports
the T113 (FEL, DDR init, SPI-NOR/NAND). The BROM auto-enters FEL over USB-C when
it finds no valid boot image, so a bad SD card always recovers. `lsusb` shows
`1f3a:efe8` in FEL mode.

## UART console

> ### ⚠️ #1 first-boot trap: our console is UART0, mainline defaults to UART3
>
> The MangoPi MQ-R (whose defconfig + DTB we build from) puts its debug console
> on **UART3 (PB6/PB7)** — U-Boot sets `CONFIG_CONS_INDEX=4` (sunxi counts from
> 1, so 4 = UART3) and the kernel DT's `stdout-path` points at `serial3`.
> **Our t113-breakout wires the console to UART0 (PE2/PE3)** — the *opposite*
> pins (chosen because the SoC's default PF2/PF4 collide with the microSD).
>
> **If you flash a stock MQ-R image unchanged, you get NO output on our header.**
> Two required changes:
> 1. **U-Boot:** in a board-derived defconfig set `CONFIG_CONS_INDEX=1` (UART0).
> 2. **Kernel:** repoint the DT `serial0` alias / `stdout-path` to `uart0`, and
>    make the kernel cmdline agree: `console=ttyS0,115200`. Add `earlycon=on`
>    to see early boot. A console-name/DT-alias mismatch is the classic "stuck
>    at `Starting kernel ...`" / "unable to open an initial console" failure.

The breakout routes **UART0 to PE2/PE3**. After the changes above, U-Boot and
the Linux console run here:

| Signal   | SoC pin   | Breakout header |
|----------|-----------|-----------------|
| UART0_TX | PE2 (35)  | H11-18          |
| UART0_RX | PE3 (33)  | H10-18          |
| GND      | —         | any header GND  |

- **115200 baud, 8N1, 3.3 V** USB-serial adapter.
- Cross the lines: adapter **RX → PE2 (TX)**, adapter **TX → PE3 (RX)**, and a
  **common GND**.
- Caveat: the BROM's *earliest* chatter comes out on the SoC default PF2/PF4
  (= SD pins, not on the console header), so it won't be visible — but SPL,
  U-Boot, and Linux all run on the console UART, which is what matters. See
  [../t113-breakout/BOM.md](../t113-breakout/BOM.md) (UART0 console note) for
  the full rationale.

Example (Linux host):

```bash
# adjust device node to your adapter (ttyUSB0 / ttyACM0)
picocom -b 115200 /dev/ttyUSB0
# or: screen /dev/ttyUSB0 115200
```

> **Alternative if you'd rather not touch the defconfig/DT for first boot:** the
> console is a *software* pin-mux choice, so you *could* instead probe UART3
> (PB6=H4? / PB7) on the breakout and use the stock image as-is to get to a
> prompt fastest, then switch to UART0 once booting. But since our board commits
> to UART0, doing the UART0 change up front is cleaner. (PB6/PB7 = SoC pins
> 80/79, on header H13-5 / H12-5 per the breakout PINOUT.)

## Power

The breakout is **USB-C powered (5 V in)**; on-board bucks make the 3.3 V and
0.9 V core rails. Just plug in USB-C — no separate supply needed for boot bring-up.

## Later phases (not started)

- **Display** — ILI9341 over the PD-bank i8080 8-bit parallel bus (LCD0), wired
  to breakout headers H12/H13.
- **Audio** — TBD (I2S codec / amp on the breakout headers).
- **Input** — buttons on spare GPIO.
- Migrate off the breakout onto an integrated gameboy-v3 PCB once the software
  stack is proven.

## Both cores (SMP)

**Both Cortex-A7 cores come up on mainline out of the box** — no kernel patch,
no TF-A. The mechanism is easy to miss because it's split across the boot chain:

- The mainline *kernel* has no T113 SMP method (nothing in
  `arch/arm/mach-sunxi/platsmp.c`, and the DTS `cpu@1` has no `enable-method`) —
  which is why a kernel-only look suggests "single-core."
- But mainline **delegates T113 SMP to PSCI provided by U-Boot**. U-Boot's
  `CONFIG_MACH_SUN8I_R528` auto-selects `ARCH_SUPPORT_PSCI`; its ARMv7 secure
  monitor (`arch/arm/cpu/armv7/sunxi/psci.c`) releases CPU1 from reset (the
  T113/NCAT2 "CPUX" block at `0x09010000` — reset-only, no power clamp), and at
  boot U-Boot **patches the kernel DTB** (`fdt_psci`) to add
  `enable-method="psci"` to each CPU plus a `/psci` node.

So as long as you boot the kernel via our U-Boot (Step 1), Linux gets a `psci`
enable-method and starts the second A7. Verify after boot with `nproc` (→ 2) or
`cat /proc/cpuinfo`. If you ever saw only one core, the usual cause is booting
the kernel *without* U-Boot's PSCI (e.g. a raw `booti` of a kernel whose DTB was
not the U-Boot-patched one).

## References

**This board:**
- [../t113-breakout/BOM.md](../t113-breakout/BOM.md) — board BOM, power rails,
  boot/debug notes.
- [../t113-breakout/PINOUT.md](../t113-breakout/PINOUT.md) — full header pinout
  (authoritative net→pin→header map).

**Upstream (verified against git during planning):**
- U-Boot sunxi board doc — build/flash mechanics:
  <https://docs.u-boot.org/en/latest/board/allwinner/sunxi.html>
- `mangopi_mq_r_defconfig` (T113 board config, DRAM timings):
  <https://source.denx.de/u-boot/u-boot/-/blob/master/configs/mangopi_mq_r_defconfig>
- Kernel board DTS `sun8i-t113s-mangopi-mq-r-t113.dts` (mainline ≥ v6.5).
- linux-sunxi wiki — [MangoPi MQ-R](https://linux-sunxi.org/MangoPi_MQ-R),
  [T113-s3](https://linux-sunxi.org/T113-s3), [FEL](https://linux-sunxi.org/FEL).
- `xfel` (sun20i FEL/recovery tool): <https://github.com/xboot/xfel>
- Bootlin prebuilt toolchains: <https://toolchains.bootlin.com>
- Armbian forum thread 29360 — real-world T113 "can't reach login prompt" (the
  UART console-handoff failure mode):
  <https://forum.armbian.com/topic/29360-t113-s3-based-board-cant-reach-login-prompt/>

> **Note on distro variants:** the MangoPi MQ-R ships as both an ARM **T113-S3**
> and a RISC-V **D1/F133** silicon variant on the same silk-screen. We are the
> **ARM T113-S3** — do not follow D1/RISC-V guides (different toolchain, and
> mainline U-Boot for the RISC-V variant lags). Allwinner's vendor **Tina Linux**
> BSP (kernel 5.4, u-boot-2018, its own UART0 image) is a fallback/reference
> only; we target mainline.


---

## Boot matrix — interchangeable components & silicon results

Every boot component is a **drop-in provider**. `forge/steps/image/build.sh` composes
them via four selector variables; one image, one boot command (`bootz` on a
zImage), for every combination.

```
BOOTLOADER = uboot | custom     mainline U-Boot            | our bootloader/
KERNEL     = linux | custom     mainline Linux (zImage)    | our kernel/ (zImage-headed)
ROOTFS     = busybox | scratch  musl BusyBox initramfs     | our rootfs/ (gv3libc)
MEDIA      = sd | nor           full-disk SD .img          | SPI-NOR component set
```

`4 axes → 2 × 2 × 2 × 2 = 16 combinations.`

### What makes them interchangeable

The unification that made this a true matrix (rather than special-cased paths):

- **One kernel format.** Both kernels are **zImages** booted by `bootz` (or jumped
  in place by the custom bootloader). Mainline Linux is a real self-decompressing
  zImage; our custom kernel carries a **zImage-compatible header** (`b _start`@0x00,
  magic `0x016f2818`@0x24, `zi_start`=0, `zi_end`=image size — see
  `kernel/arch/arm/start.S`). So no per-kernel boot command, no uImage wrapper.
- **One handoff contract.** Every kernel is entered at its load address 0x41000000,
  MMU/caches off, `r0=0 / r1=~0 / r2=DTB` (ARM `Documentation/arm/booting.rst`).
- **Loader-agnostic components.** Kernel/DTB/initramfs land at the same DRAM
  addresses (0x41000000 / 0x41800000 / 0x41C00000) regardless of who loaded them.

### The SD ⇄ NOR equivalence (why NOR testing covers the tuple)

`MEDIA` only changes **where the loader reads the components from**:
- **SD:** loader reads a FAT partition (`fat_load` / U-Boot `load`).
- **NOR:** loader reads fixed SPI-NOR offsets (`spinor_read` / U-Boot `sf read`).

Once kernel+DTB+initramfs are in DRAM at the fixed addresses, **the boot is
byte-identical**. So a given `(BOOTLOADER, KERNEL, ROOTFS)` tuple boots the same
way on either medium; only the read path differs. The NOR path exercises every
distinct tuple; the SD path's unique code (FAT read) is separately silicon-proven
for `uboot+linux+busybox`.

### Remote-testability (the T113-breakout rig)

The board lives on a remote NUC (see [REMOTE-RIG.md](#remote-bring-up-rig-ssh-driven-nuc)); the dev loop
is **power-cycle → FEL → xfel flashes NOR / delivers the loader → capture UART**.
This is fully remote **only for NOR**:

| MEDIA | Remote-testable? | Why |
|---|---|---|
| **nor** | ✅ fully remote | `xfel spinor write` flashes components; loader delivered via FEL; no card, no hands |
| **sd**  | ❌ needs a human | a microSD must be physically inserted; not the dev loop (see REMOTE-RIG.md) |

So the **8 NOR combos are the remote test campaign**; the 8 SD combos are covered
transitively by the equivalence above (and can be spot-checked by hand later).

The bootloader itself is **never in NOR** (NOR offset 0 is kept blank so the BROM
always falls through to FEL — the un-strand invariant, see [NOR-LAYOUT.md](#spi-nor-layout--the-loader-contract)).
It is FEL-delivered each boot by `flash.sh` (which reads BOOTLOADER from the bundle).

### The 16 combinations

Legend: ✅ booted to shell on silicon · 🟢 booted in QEMU · 🔧 builds+assembles ·
⚠️ expected-gap (ABI mismatch) · ⬜ not yet tested · 🚫 not remotely testable (SD).

#### NOR media (the remote campaign — 8 combos)

| # | BOOTLOADER | KERNEL | ROOTFS | Build | Silicon (NOR) | Notes |
|---|---|---|---|---|---|---|
| 1 | custom | linux  | busybox | 🔧 | ✅ | booted `~ #` (combo "A, custom loader"); `cores online: 1` |
| 2 | custom | linux  | scratch | 🔧 | ✅ | scratch rootfs interactive on mainline (`gv3$ echo CLEAN_42`); earlier "no input" was a rig capture artifact — see results |
| 3 | custom | custom | busybox | 🔧 | ✅ | **interactive `/ #`**, echoes typed commands. (Originally looked "silent" — was a capture window ending during a slow caches-OFF boot; enabling I/D caches cut the initramfs unpack from ~16 s to ~0.6 s — see "Enabling caches" below.) |
| 4 | custom | custom | scratch | 🔧 | ✅ | **interactive `gv3$`** — first custom-kernel shell on silicon (9 KB rootfs, boots fast) |
| 5 | uboot  | linux  | busybox | 🔧 | ✅ | `sf`+`bootz`→`~ #`; `cores online: 2` (U-Boot PSCI); sf-U-Boot reproducible build |
| 6 | uboot  | linux  | scratch | 🔧 | ✅ | scratch on mainline, interactive by equivalence to #2 (loader-independent) + QEMU-clean |
| 7 | uboot  | custom | busybox | 🔧 | ✅ | **bootz ACCEPTED the custom zImage** ✓; same custom-kernel+busybox as #3 → interactive `/ #` (loader-independent) |
| 8 | uboot  | custom | scratch | 🔧 | ✅ | bootz → custom kernel → unpacks scratch initramfs; downstream == #4 (interactive `gv3$`), loader-independent |

#### SD media (transitively covered; not in the remote campaign — 8 combos)

| # | BOOTLOADER | KERNEL | ROOTFS | Build | Silicon (SD) | Notes |
|---|---|---|---|---|---|---|
| 9  | uboot  | linux  | busybox | 🔧 | ✅ | the original full boot to `~ #` (SD) |
| 10 | uboot  | linux  | scratch | 🔧 | 🚫 | ⚠️ + SD |
| 11 | uboot  | custom | busybox | 🔧 | 🚫 | bootz+zImage on SD |
| 12 | uboot  | custom | scratch | 🔧 | 🚫 | ⚠️ + SD |
| 13 | custom | linux  | busybox | 🔧 | 🚫 | |
| 14 | custom | linux  | scratch | 🔧 | 🚫 | ⚠️ + SD |
| 15 | custom | custom | busybox | 🔧 | 🚫 | |
| 16 | custom | custom | scratch | 🔧 | 🚫 | ⚠️ + SD |

### Distinct artifact sets

The bootloader is delivered separately (FEL), so a NOR flash only needs the
**kernel × rootfs** component set (4 distinct sets); each is booted by either
loader. DTB is shared across all.

| Set | KERNEL artifact | ROOTFS artifact |
|---|---|---|
| A | linux zImage (~5.4 MB) | busybox initramfs (~776 KB) |
| B | linux zImage | scratch initramfs (gv3libc) |
| C | custom zImage (~32 KB) | busybox initramfs |
| D | custom zImage | scratch initramfs |

### How to run one combo (NOR, remote) — the two-script loop

```bash
# 1. build a bundle for the config (dev host):
make image KERNEL=custom BOOTLOADER=custom LIBC=musl PACKAGES=busybox
# 2. flash it to NOR AND FEL-boot it, in one command (stages to the rig itself):
tools/flash.sh build/bundles/custom-custom-busybox nor
```

`flash.sh` power-cycles to FEL (t113power.sh), `xfel spinor write`s the components +
GV3NOR1 table, then FEL-delivers the loader named in the bundle manifest (custom
bootloader → SRAM, or U-Boot proper → DRAM + `sf read`/`bootz`), and tails the UART.
`MEDIA=sd`/`emmc` give a clear "not remotely flashable" error. See NOR-LAYOUT.md.

### Silicon results

On-silicon campaign on the T113-breakout (NOR media, fully remote via the NUC rig).
Method: `xfel spinor write` the component set + GV3NOR1 table → power-cycle to FEL →
FEL-deliver the loader → capture UART0/ttyUSB1 @115200. "Interactive" = the shell
echoed a test command typed to the console.

**Headline:** this campaign is the **first time the from-scratch custom kernel has
ever run on T113 silicon** (all prior silicon boots used mainline Linux). It boots
fully — DRAM/NOR handoff, MMU on, GIC, 24 MHz timer, initramfs unpack, `/bin/sh`
spawned as pid 1 — and reaches an **interactive shell with BOTH the from-scratch
rootfs (fast) AND musl BusyBox (~75 s boot; slow in-kernel gunzip, not a hang)**.
Both custom-kernel combos boot under BOTH loaders (custom bootloader; U-Boot bootz
accepts the zImage). Three kernel cache/TLB hardening fixes landed along the way.

| # | Combo (BL/K/RFS) | Date | Result | Evidence / notes |
|---|---|---|---|---|
| D | custom / **custom** / **scratch** | 2026-07-29 | ✅ **interactive `gv3$`** | Custom kernel + our from-scratch gv3 shell: `gv3$`, echoed typed commands. **First custom-kernel interactive shell on silicon.** 9 KB rootfs → boots fast. |
| C | custom / **custom** / busybox | 2026-07-29 | ✅ **interactive `/ #`** | Same custom kernel + musl BusyBox → banner (`T113-S3 is alive`, `Linux 0.9-gv3`, `cores online: 1`), `/ #`, echoed `SIL_LIVES_121`. **Reached only after a ~75 s boot** — the in-kernel gunzip of the 792 KB initramfs + per-syscall overhead is ~3–4× slower than QEMU; earlier "no output" was the capture window ending mid-boot, NOT a hang (a pid-1 syscall trace showed steady progress, byte-identical to QEMU). |
| A (custom loader) | custom / linux / busybox | 2026-07-29 | ✅ interactive `~ #` | Custom bootloader loads the 5.4 MB mainline **zImage** → `Linux 6.12.95`, `cores online: 1`, `~ #`, echoed input. Proves the custom bootloader's zImage handoff + BusyBox on mainline. |
| A (U-Boot loader) | **uboot** / linux / busybox | 2026-07-29 | ✅ interactive `~ #` | sf-capable U-Boot (FEL-delivered) `sf probe`→`w25q128 16 MiB`, `sf read`, `bootz`→`Linux 6.12.95`, **`cores online: 2`** (U-Boot PSCI), `~ #`, echoed input. Reproducible sf build (below). |
| C (U-Boot loader) | **uboot** / **custom** / busybox | 2026-07-29 | ✅ bootz + interactive | **bootz ACCEPTED the custom kernel's zImage** — `Kernel image @ 0x41000000 [0x000000 - 0x007f1c]` (exactly our zi_start=0/zi_end), no "Bad magic". Same downstream as combo C above (loader-independent) → reaches `/ #`. (During diagnosis this path showed a `mmu selftest: MISMATCH` under U-Boot's handoff — see below; it was a SELFTEST artifact, not an MMU fault: the kernel ran fine translated.) |
| #8 | **uboot** / **custom** / **scratch** | 2026-07-30 | ✅ boots (interactive by equiv.) | bootz → custom kernel (`Stage 10`, caches on, mmu selftest MATCH) → unpacks the 9 KB scratch initramfs. Downstream is **byte-identical to combo D** (same custom kernel + same scratch rootfs, same 0x41000000 entry — the loader can't affect userspace), which is proven interactive `gv3$`. Confirmed booting to the unpack; the `gv3$` round-trip is inherited from D. |
| #2 | custom / linux / **scratch** | 2026-07-30 | ✅ **interactive `gv3$`** | **From-scratch rootfs on MAINLINE Linux 6.12** (custom bootloader). `/init` runs (`-- from scratch, gv3libc`), reaches `gv3$`, and cleanly echoed `CLEAN_42` + ran `pwd`/`ls`. (An earlier "input doesn't round-trip" reading was a RIG capture artifact — multiple leftover `cat` readers splitting bytes — not a rootfs/tty bug; see below.) |
| #6 | **uboot** / linux / **scratch** | 2026-07-30 | ✅ (interactive by equiv.) | Same scratch-on-mainline as #2; loader is upstream of userspace. Interactive by equivalence to #2 (and QEMU-clean). |

#### The "scratch-on-mainline input" scare was a rig capture bug, NOT a rootfs bug

Earlier notes here claimed the from-scratch rootfs couldn't round-trip input on
mainline (and speculated it "targets the custom-kernel ABI"). **Both were wrong.**
The rootfs is meant to run on both kernels, each component as simple as possible,
and it DOES: verified cleanly interactive on mainline in QEMU (`echo`/`pwd`/`ls`)
AND on silicon (`gv3$ echo CLEAN_42` → `CLEAN_42`).

Root cause of the false symptom: **leftover `cat /dev/ttyUSB1` capture processes on
the NUC**. The session's capture pattern (`nohup cat & … kill $pid`) leaked a reader
whenever a kill raced or an ssh session died; they accumulated until TWO `cat`s were
reading the same tty at once. The kernel hands each incoming byte to whichever
reader is scheduled, so each `cat` got a random ~half of the stream — producing the
fragmented input (`gv3$ 4` / `gv3$ e`), the garbled boot logs (`gameboy-v3 cuom
SRAM`), and likely inflated boot-time readings, ALL SESSION. Fixed with
`scripts`/rig helper **`uartcap.sh`** (staged at `~/t113boot/matrix/` on the NUC):
it `pkill`s every existing tty reader, starts exactly one, and asserts `readers: 1`.
Clean captures jumped from ~2–4 KB of garbage to full legible logs (14.8 KB).
LESSON: exactly one process may read the console tty; verify no leaked readers
before trusting any UART capture.

#### What the campaign found + fixed (first-ever custom-kernel silicon runs)

Two anomalies surfaced; investigation showed one was a real robustness gap (fixed)
and one was a diagnosis artifact. Three kernel hardening changes landed, all
mainline-style and QEMU-golden-clean:

1. **"BusyBox no interactive shell" — was a CAPTURE-TIMING artifact, not a bug.**
   A pid-1 syscall trace on silicon was byte-identical to the (working) QEMU trace
   and showed steady forward progress (fork/wait4/rt_sigprocmask looping through
   `/init`'s commands). The custom kernel's in-kernel `gunzip` of the 792 KB
   BusyBox initramfs + slower per-syscall path makes the whole boot ~75 s (vs
   near-instant in QEMU), so every earlier UART capture window ended before the
   prompt. With a long enough window it reaches an interactive `/ #` and echoes a
   typed command. (The 9 KB scratch rootfs, combo D, boots fast — which is why it
   looked interactive immediately and BusyBox didn't.) No code change needed for
   this; it was a measurement problem.

2. **`mmu selftest: MISMATCH` under U-Boot's bootz — was a SELFTEST artifact.**
   The old selftest wrote through a freshly-mapped VA and read the same physical
   location back through its identity-mapped VA. Under U-Boot's handoff those two
   VA aliases weren't coherent for the read, so it falsely reported MISMATCH —
   even though the hardware translation was correct (verified via `ATS1CPR`/`PAR`)
   and the kernel ran fine translated (DTB parse, GIC, timer, initramfs unpack,
   shell). Fixed by rewriting `mmu_selftest` to verify translation via the
   ATS1CPR→PAR register (ground truth, alias-immune).

3. **Kernel cache/TLB hardening (correct regardless — mainline does all of it):**
   - `_start` now invalidates D-cache (set/way) + I-cache + branch-predictor + TLB
     before building the MMU (`mmu_flush_caches_tlb`) — don't inherit a loader's
     cache/TLB state (U-Boot hands off with branch prediction enabled, SCTLR.Z=1).
   - `mmu_enable_sctlr` now does TLBIALL+BPIALL *after* TTBR0 is programmed and
     immediately before setting SCTLR.M, so a speculative walk of the old table
     can't leave stale entries.
   - `sync_icache()` (clean D-cache + ICIALLU + BPIALL + barriers) after the kernel
     writes code — execve (`exec_commit`), fork (`do_fork`, post `vm_copy`), initial
     `spawn_proc`, and `mmap2` with `PROT_EXEC` (ld.so segment loads) — so
     freshly-loaded code isn't executed from stale I-cache/BP state on silicon.

#### Enabling the caches — the ~26× boot speedup (2026-07)

The custom kernel had run with **both caches disabled since S6** ("simplest and
correct"). Correct, but every fetch/load/store hit uncached DDR3 — the whole
system ran ~150× slow. Measured on silicon (24 MHz counter): the 792 KB busybox
initramfs `gunzip` took **14.5 s** caches-off. That was the real reason combo #3
"took forever" (not a hang — a slow boot).

Fix: enable SCTLR.C+I after the MMU is up (`mmu_enable_caches`), which required the
coherency maintenance an all-caches-off kernel never needed — verified by a 3-lens
adversarial review that caught **three silicon-only bugs** (all invisible under
QEMU's coherent model), now fixed:
  - **ACTLR.SMP** was never set — Cortex-A7 requires SMP=1 before enabling
    caches/MMU or any set/way op, else the D-cache + maintenance are architecturally
    UNPREDICTABLE. Set in `_start` before the first cache op.
  - **`l2_for` break-before-make** — replacing a valid 1 MB section with a 4 KB
    coarse table had no TLB invalidate; a stale section TLB entry could shadow the
    new heap/mmap pages → wrong (Device) PA = silent corruption. Added
    `mmu_tlbimva` of the 1 MB VA.
  - **`sys_mmap2` PROT_EXEC** — the ld.so path copied shared-lib/PIE code without an
    I-cache sync → dynamically-linked programs could run stale instructions. Added
    `sync_icache()` when `prot & PROT_EXEC`.
  - Page-table descriptor writes (`mmu_map_section`, `vm.c`) now clean their line to
    PoC before the TLB op — TTBR0 walks are non-cacheable, so a dirty descriptor in
    the D-cache would be invisible to the walker.

**Measured on silicon, caches ON:** gunzip **14.5 s → 566 ms (~26×)**, cpio extract
1385 ms → 46 ms. Boots to interactive `/ #`, echoes commands, no corruption. QEMU
golden 6/6 throughout (incl. the `dynamic` ld.so case).

#### sf-capable U-Boot (reproducible build for the NOR `uboot` combos)

`01-uboot.sh` builds a console-only U-Boot; the `sf` command needs extra config.
The correct enable order matters — the whole SPI-flash Kconfig menu is behind
`if MTD`, so **`MTD` must be enabled FIRST** or `olddefconfig` silently drops the
rest (this bit us; the old note omitted MTD):
```
scripts/config --enable MTD --enable SPI_FLASH --enable DM_SPI_FLASH \
               --enable SPI_FLASH_WINBOND --enable SPI_FLASH_SFDP_SUPPORT --enable CMD_SF
make olddefconfig      # verify CONFIG_CMD_SF=y + CONFIG_SPI_FLASH=y survived
```
Plus the NOR DT overlay so `sf probe` finds the chip — append to the *upstream*
DTS (v2026.04 vendors device trees under `dts/upstream/src/arm/allwinner/`, NOT
the old `arch/arm/dts/`): `#include "nor-spi.dtsi"` (declares `flash@0`,
`jedec,spi-nor`, CS0/PC3). Verified: `sf probe` → `Detected w25q128 ... 16 MiB`.

#### Coverage: all 8 NOR combos exercised on silicon (2026-07-30)

Every NOR combo (#1–#8) has now been run on the T113 silicon and reaches an
**interactive shell**:
- **Clean interactive silicon capture:** #1, #2, #3, #4, #5, #7.
- **Interactive by loader-independent equivalence** (+ QEMU-clean): #6 (==#2), #8 (==#4).
All four ROOTFS×KERNEL pairings are confirmed interactive on real hardware; the
from-scratch rootfs runs on BOTH the custom kernel AND mainline Linux. (The earlier
"#2/#6 input doesn't round-trip" was a rig capture artifact — see results above —
now fixed with the single-reader `uartcap.sh`.)

#### Not run on silicon (by design — not remotely testable)
- **All 8 SD-media combos (#9–#16)** — need physical microSD insertion, which the
  remote rig can't do (the whole reason for the NOR+FEL scheme). Covered transitively
  by the SD⇄NOR equivalence (only the FAT-read path differs from `sf read`, and that
  FAT path is silicon-proven for uboot/linux/busybox — combo #9, the original boot).


---

## SPI-NOR layout — the loader contract

The on-board **W25Q128 SPI-NOR (16 MB)** holds the boot *components* (kernel, DTB,
initramfs) but **NOT a bootloader**. The bootloader itself is delivered separately
via **FEL** (loaded into SRAM/RAM over USB, then executed); it then reads these
components from NOR at the fixed offsets below.

### Why no bootloader in NOR (the load-bearing invariant)

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

### Layout (64 KB-aligned; W25Q128 erase units are 4K/32K/64K)

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

### Component table (at NOR 0x000000)

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

### DRAM load addresses (identical for both loaders; match boot.cmd / bootloader/main.c)

```
kernel   → 0x41000000
DTB      → 0x41800000
initramfs→ 0x41C00000
```

### How each loader consumes this

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

### Flashing NOR (over FEL — always available since offset 0 is blank)

**Preferred: the two-script loop** — `build.sh` compiles a config into a
self-contained bundle, `flash.sh` flashes it to NOR AND FEL-boots it in one command
(it stages the bundle to the rig itself, so you run it from the dev host):

```
make image                                   # bundle (defaults: custom+custom+coreutils)
make image KERNEL=custom BOOTLOADER=custom LIBC=custom PACKAGES=coreutils   # any config
tools/flash.sh build/bundles/<cfg> nor           # flash NOR + FEL-boot, watch UART
# (peripherals like the ILI9341 LCD are board-applied DT+config fragments, not a flag —
#  see "ILI9341 SPI LCD" below)
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
- Custom bootloader (`fel-loader.bin`): its own DDR init + SPI-NOR read (spinor.c) +
  FDT patch + boot_kernel → `cores online: 1`.
- U-Boot (`fel-loader.bin`): xfel DDR + U-Boot proper + `sf read` + `bootz` →
  `cores online: 2` (PSCI). flash.sh re-arms FEL after the NOR writes before the
  U-Boot DRAM load — an xfel quirk: `spinor write` then `ddr`+high-DRAM-write fails
  (rc=255) without a fresh FEL entry.
Note the custom bootloader's start.S sets CNTFRQ=24MHz (else the kernel's arch
timer div-by-zeros), and fdt.c is built -O0 (a GCC10 -Os miscompile hangs the
FDT walk).

_Single source of truth for both loaders + `flash.sh`. Keep offsets here in sync
with `bootloader/nor_layout.h`, `forge/steps/image/build.sh` (manifest), and the U-Boot boot
script._


---

## Flashing & first boot (generic / local bench)

How to put the built image on a microSD, wire up the serial console, and boot
the T113-S3 on the t113-breakout board to a shell prompt.

Build the SD image (toolchain once, then one command):

```bash
make toolchain                 # once
make image MEDIA=sd               # → build/output/gameboy-v3-<cfg>-sd.img
# defaults to custom+linux+busybox; add BOOTLOADER=uboot / KERNEL=... / ROOTFS=... to vary
```

---

### 1. Flash the microSD

**⚠️ `dd` to the wrong device destroys that disk. Identify the card carefully.**

```bash
lsblk        # before inserting the card
# insert the card, run again, and note the NEW device (e.g. /dev/sdX or /dev/mmcblk0)
lsblk
```

The card is the whole-disk node (`/dev/sdb`, `/dev/mmcblk0`), **not** a partition
(`/dev/sdb1`, `/dev/mmcblk0p1`). Then:

```bash
sudo dd if=build/output/gameboy-v3-sd.img of=/dev/sdX bs=4M conv=fsync status=progress
sync
```

The image is a full-disk image (partition table + U-Boot at 8 KiB + a FAT boot
partition), so it goes to the raw device, not a partition.

> On-board **SPI-NOR stays blank** — the BROM skips it (no valid header) and
> boots the SD. Nothing to flash there for Phase 1.

---

### 2. Wire the serial console (UART0 / PE2-PE3)

Use a **3.3 V** USB-serial adapter (NOT 5 V — it can damage the SoC). Cross TX/RX:

| Adapter pin | → t113-breakout | SoC pin |
|-------------|-----------------|---------|
| RX          | PE2 (H11-18)    | 35 (UART0-TX) |
| TX          | PE3 (H10-18)    | 33 (UART0-RX) |
| GND         | any header GND  | —       |

Open the console at **115200 8N1**:

```bash
picocom -b 115200 /dev/ttyUSB0      # or: screen /dev/ttyUSB0 115200
```

(Adjust `/dev/ttyUSB0` → `/dev/ttyACM0` etc. for your adapter.)

---

### 3. Power on

Power the board via **USB-C** (5 V in; on-board bucks make 3.3 V + 0.9 V core).
No separate supply needed. Watch the serial console.

#### Expected boot sequence

```
U-Boot SPL 2026.04 ...              <- SPL: DRAM init succeeded
...
U-Boot 2026.04 ...                  <- U-Boot proper (console now on UART0)
...
== gameboy-v3 boot.scr ==           <- our boot script ran
Booting: zImage + DTB + initramfs (... bytes) ...
### Flattened Device Tree blob at 41800000
Starting kernel ...

[    0.000000] Booting Linux on physical CPU 0x0
...                                 <- kernel log
[    x.xxxxxx] Run /init as init process
===================================
  gameboy-v3 initramfs — T113-S3 is alive.
  Linux ... armv7l
  cores online: 2                   <- BOTH A7s up (U-Boot PSCI)
===================================

/ #                                 <- BusyBox shell. Success.
```

At the prompt, sanity-check:

```sh
nproc            # -> 2   (both Cortex-A7 cores, via U-Boot PSCI)
cat /proc/cpuinfo
uname -a
ls /
```

---

### Troubleshooting

**Nothing on the console at all**
- Check TX/RX aren't swapped (adapter RX↔board TX). Swapping is the #1 mistake.
- Confirm 115200 8N1 and the right `/dev/tty*`.
- Confirm 3.3 V adapter and a common GND with the board.
- Note: the BROM's *earliest* bytes come out on the SoC-default PF2/PF4 (the SD
  pins, not on our header) — that's expected and invisible. SPL/U-Boot/Linux all
  use UART0/PE2-PE3, which is what you should see.

**SPL banner appears, then silence** (console dies after "U-Boot SPL")
- This would be the UART0-vs-UART3 console split. Our build fixes it in both the
  U-Boot config (`CONFIG_CONS_INDEX=1`) and both DTBs (`uart0-console.dtsi`), so
  it shouldn't happen — but if it does, re-run Steps 1–2 and re-verify the DTB
  console with the checks in the READMEs.

**"Starting kernel ..." then hangs**
- Usually a console/DT mismatch or a missing `/dev/console`. Our initramfs
  includes `/dev/console` and the kernel cmdline is `console=ttyS0,115200`
  (matches the DT `serial0`/uart0). `earlycon=on` is set to surface early output.

**Kernel panics "No init found" / "Kernel panic - not syncing: VFS: Unable to
mount root fs"**
- The initramfs didn't load or `/init` isn't executable. Re-run Step 3 and
  re-verify with `cpio -itv` (see the README Step 3 checks). Confirm `boot.scr`
  loaded `initramfs.cpio.gz` and `bootz` got the `addr:size` third argument.

**Board does nothing / no SPL banner**
- The SD may not be flashed correctly, or U-Boot isn't at the 8 KiB offset.
  Re-run Step 4 and re-check `sfdisk -l` + the `eGON.BT0` header at offset 8192.
- Recovery backstop: with no valid boot media the BROM drops into **FEL** (USB).
  Connect USB-C and `xfel version` should identify the chip (USB id `1f3a:efe8`);
  from there you can push U-Boot over USB. (`xfel`, not `sunxi-fel`, for sun20i.)

---

### What this proves (and doesn't)

Reaching the shell validates the entire Phase-1 chain on real silicon:
BROM → SPL (DRAM init) → U-Boot → boot.scr → kernel → initramfs → BusyBox shell,
with the console on our board's UART0 and both CPU cores online.

It does **not** yet exercise: the kernel's SD/MMC driver + a real root
filesystem mount (we use a RAM initramfs — see the README for why), display,
audio, or input. Those are later phases.


---

## Remote bring-up rig (SSH-driven NUC)

This is the operational runbook for the **remote** t113-breakout setup: the board
lives on a NUC and everything (reset, flash, serial console) is driven over SSH —
no physical access needed. This is distinct from [FLASH.md](#flashing--first-boot-generic--local-bench), which is
the generic "dd a card on your own bench" procedure.

> Companion docs: [FLASH.md](#flashing--first-boot-generic--local-bench) (build + local flash), the t113-breakout
> [REV-B-FIXES.md](../t113-breakout/REV-B-FIXES.md) (why a VBUS bulk cap + UART
> series resistors are needed), and [PINOUT.md](../t113-breakout/PINOUT.md).

---

### The rig at a glance

```
 dev box ──ssh──> john-NUC9i5QNX ──USB──> MEGA4 hub ──USB-C──> T113 breakout
 (you type here)  (johmagnu-nuc)          (power switch)      │
                       │                                       ├─ USB-C: power + FEL data
                       └──USB──> CP210x USB-serial ────────────┴─ UART0 (PE2/PE3) console
```

- **Host:** `john-NUC9i5QNX`, reached as **`ssh johmagnu-nuc`** (key auth, passwordless
  sudo configured — `sudo -n` works).
- **Power / reset:** the board's USB-C is plugged into a **UUGear MEGA4 PPPS hub**,
  which genuinely cuts VBUS (bench-confirmed). Cutting + restoring its port power =
  a real reset. `uhubctl` drives it.
- **Console:** a **CP210x** 3.3 V USB-serial adapter wired to UART0 (board TX
  PE2/H11-18 → adapter RX; GND common; optional adapter TX → 1 kΩ → board RX
  PE3/H10-18). Appears on the NUC as a `/dev/ttyUSB*` (see "Identify the console"
  below — the number is not stable across replugs).
- **Flash:** either `dd` a full SD image, or push into DRAM over USB **FEL** with
  `xfel` (installed on the NUC).

> ⚠️ **Hardware caveat — VBUS bulk cap.** Rev-A needs a bodge bulk cap on +5V or it
> browns out under sustained load (big FEL/SD transfers, kernel decompression).
> With the cap in place the board is stable. See REV-B-FIXES.md, Fix 1.

---

### 0. Names & addresses (fill in for your session)

| Thing | Value | How to (re)confirm |
|---|---|---|
| SSH host | `johmagnu-nuc` (`john-NUC9i5QNX`) | `ssh johmagnu-nuc hostname` |
| **Board POWER** | MEGA4 `2109:2817` hub **`1-6.4.4` port 3** | `sudo uhubctl -l 1-6.4.4` |
| **Board reset** | `uhubctl -l 1-6.4.4 -p 3 -a cycle -d 3` | §1 (cut ONE port; verify by FEL leaving lsusb) |
| **UART console** | CP210x `10c4:ea60` at `1-10.2.4` → ttyUSB1 | separate dongle; §3 |
| FEL device id | `1f3a:efe8` | `lsusb -d 1f3a:efe8` |

> The MEGA4's USB-2 face (`1-6.4.4`) DOES gate VBUS on its own — cutting port 3 is a
> real power cut (verify via `1f3a:efe8` leaving `lsusb`, NOT the LED — the rail caps
> keep the LED lit for seconds after; see §1). No need to touch the USB-3 face.
>
> USB **location** paths (`1-6.4.4`) are stable as long as nothing is physically
> re-cabled; `/dev/ttyUSB` **numbers** are not (they shuffle on replug). Prefer
> addressing hubs by location and re-identifying the tty each session.

---

### 1. Power-cycle (reset) the board — use `tools/t113power.sh`

**Preferred: the wrapper `tools/t113power.sh`** (staged on the NUC at
`~/t113boot/matrix/t113power.sh`). It addresses the MEGA4 by its **stable vendor id
`2109`** (`uhubctl -n 2109`), NOT by location path — so it keeps working after the
hub is unplugged/replugged or USB re-enumerates (the thing that broke a whole
session). It always uses the fixed **port 3** the board sits on, and it VERIFIES a
cut by the FEL device leaving `lsusb` (not the lying LED):

```bash
ssh johmagnu-nuc '~/t113boot/matrix/t113power.sh reset'      # off 3s -> on (normal reboot)
ssh johmagnu-nuc '~/t113boot/matrix/t113power.sh felwait'    # cycle + wait for FEL, print xfel version
ssh johmagnu-nuc '~/t113boot/matrix/t113power.sh status'     # port state + is-FEL-present
ssh johmagnu-nuc '~/t113boot/matrix/t113power.sh off|on'     # manual
```

**Raw command** (if the script isn't handy) — the T113's USB-C power is on the MEGA4
(`2109`, hub location `1-6.4.4` *at the moment*), **port 3**. Prefer `-n 2109` over
`-l 1-6.4.4` because the location changes on replug:

```bash
ssh johmagnu-nuc 'sudo uhubctl -n 2109 -p 3 -a cycle -d 3'   # VID-addressed, replug-proof
```

Cutting **just port 3** genuinely drops VBUS — no need to touch the USB-3 face, a
different hub, or replug anything. (Verified 2026-07-30 via `-n 2109`: `1f3a:efe8`
leaves `lsusb` on `off`, returns on `on`.)

#### ⚠️ HOW TO VERIFY A CUT WORKED — do NOT trust the LED (this wasted hours once)

The board's **green power LED STAYS LIT for seconds after VBUS is actually cut**,
because of the bulk caps on the 3.3V rail (the breadboard 100µF/10µF/1µF + a
board-side cap). The caps hold the rail up briefly. So a lit LED does **NOT** mean
power is still on. Likewise, `uhubctl`'s own `Port 3: 0000 off` readout only means
the *command* was accepted, not that VBUS physically dropped.

**The ONLY reliable "is power actually cut?" test is USB enumeration** — the FEL
device disappears from `lsusb`:

```bash
# BEFORE cut: should be present
ssh johmagnu-nuc 'lsusb -d 1f3a:efe8'                    # -> "...FEL/flashing mode" if powered+in-FEL
# cut, then check it's GONE:
ssh johmagnu-nuc 'sudo uhubctl -l 1-6.4.4 -p 3 -a off; sleep 3; lsusb -d 1f3a:efe8 || echo "FEL GONE = power really cut"'
# restore, then check it's BACK:
ssh johmagnu-nuc 'sudo uhubctl -l 1-6.4.4 -p 3 -a on; sleep 5; lsusb -d 1f3a:efe8 && echo "FEL BACK"'
```

(If the board is *booted* rather than in FEL it won't show `1f3a:efe8` either way —
in that case watch `dmesg` for a fresh `usb 1-6.4.4.3: USB disconnect` / re-`New USB
device` around the toggle, which is the equivalent proof.)

#### Board POWER vs UART CONSOLE are different USB devices — don't confuse them

- **Board power (USB-C):** MEGA4 `1-6.4.4` **port 3** — enumerates as `1f3a:efe8`
  (FEL) when the board is in FEL. This is what you cut to reset.
- **UART console:** a SEPARATE CP210x dongle (`10c4:ea60`, ttyUSB1) on the Genesys
  hub chain **`1-10.2.4`** — a different branch entirely. Cutting the MEGA4 does NOT
  power the console dongle down (and vice-versa). Don't cut `1-10` expecting to
  reset the board — that only kills the console.

**Find which port the T113 is on** (if the layout ever changes): remove the SD card
so the board sits in FEL, then cut/restore one MEGA4 port at a time and watch which
toggle makes `1f3a:efe8` leave/return in `lsusb`.

> ⚠️ **UART back-power during a power cut.** If the serial adapter's **TX** is
> driven while the board's VBUS is off, current back-feeds through the SoC's ESD
> diode and can latch/damage the chip (this killed Rev-A board #1). Mitigate by
> ANY of: (a) the 1 kΩ TX series-R bodge (in place), (b) put the UART adapter on a
> MEGA4 port too and cut both together, or (c) run RX-only (TX disconnected) during
> power-cycles. RX-only is always safe (adapter RX sources no current).

---

### 2. Watch the UART console

The console is UART0 (PE2 TX / PE3 RX) at **115200 8N1** on a CP210x → `/dev/ttyUSB?`.

**Quick one-shot capture** (good for scripting; catches a boot):

```bash
ssh johmagnu-nuc 'sudo stty -F /dev/ttyUSB1 115200 raw -echo; sudo timeout 30 cat /dev/ttyUSB1'
```

**Interactive session** (type commands to U-Boot / the shell). Needs TX wired:

```bash
ssh -t johmagnu-nuc 'sudo picocom -b 115200 /dev/ttyUSB1'    # Ctrl-A Ctrl-X to exit
```

**Send a command non-interactively** (e.g. poke a waiting shell):

```bash
ssh johmagnu-nuc 'printf "uname -a\r\n" | sudo tee /dev/ttyUSB1 >/dev/null'
```

Tip: to catch a one-shot boot banner, start the `cat` capture FIRST, then power-cycle.

---

### 3. Identify the console tty (numbers shuffle on replug)

```bash
# list serial adapters + their USB location + serial string
ssh johmagnu-nuc 'for d in /sys/bus/usb-serial/devices/ttyUSB*; do \
  u=$(readlink -f "$d/../.."); \
  echo "$(basename $d) <- $(cat $u/idVendor):$(cat $u/idProduct) @ $(basename $u)"; done'
```

The T113 console CP210x has historically been at USB location **`1-10.2.4`**. If in
doubt, unplug it and see which `ttyUSB*` disappears, or watch `dmesg` on replug:

```bash
ssh johmagnu-nuc 'sudo dmesg | grep -iE "cp210x.*ttyUSB" | tail'
```

Beware: an ST-Link on the bench shows up as `/dev/ttyACM0` and may emit unrelated
output — that is NOT the T113.

---

### 4. Flash — two ways

#### 4a. SD card (the normal path)

The board boots from an SD image at the 8 KiB offset + a FAT partition. Build it
with `make image MEDIA=sd` (see FLASH.md), copy to the NUC, and `dd` it.

```bash
# from the dev box: stage the image on the NUC
scp build/output/gameboy-v3-sd.img johmagnu-nuc:~/t113boot/

# on the NUC: the card reader is /dev/sda (VERIFY: ~29 GB, removable, USB)
ssh johmagnu-nuc 'lsblk -d -o NAME,SIZE,TRAN,RM | grep -i sda'      # sanity check
ssh johmagnu-nuc 'sudo umount /dev/sda1 2>/dev/null; \
  sudo dd if=~/t113boot/gameboy-v3-sd.img of=/dev/sda bs=4M conv=fsync status=none; sync'

# verify the write (compare md5 of the first N bytes)
ssh johmagnu-nuc 'IMGSZ=$(stat -c%s ~/t113boot/gameboy-v3-sd.img); \
  sudo dd if=/dev/sda bs=4M count=16 2>/dev/null | head -c $IMGSZ | md5sum'
```

> ⚠️ **Guard the dd target.** `/dev/sda` = the SD card reader (29 GB removable USB).
> `/dev/nvme0n1` = the NUC's system SSD — NEVER touch it. Always re-check `lsblk`;
> the reader reads `size=0` when no card is inserted.
>
> The card physically moves between the NUC reader (to flash) and the T113 socket
> (to boot). Remember to move it back each time.

#### 4b. FEL — push into DRAM over USB-C (no SD, no persistence)

The BROM enters **FEL** when it finds no boot media (SD out + SPI-NOR blank) — it
enumerates as `1f3a:efe8`. `xfel` is built + installed on the NUC
(`/usr/local/bin/xfel`, from github.com/xboot/xfel; T113 = chip "R528/T113").

```bash
ssh johmagnu-nuc 'sudo xfel version'          # handshake: prints AWUSBFEX ID=...R528/T113
ssh johmagnu-nuc 'sudo xfel ddr t113-s3'      # bring up DDR3 (make-or-break)
ssh johmagnu-nuc 'sudo xfel write 0x41000000 ~/t113boot/zImage'          # load addrs from boot.cmd:
ssh johmagnu-nuc 'sudo xfel write 0x41800000 ~/t113boot/*.dtb'           #   kernel 0x41000000
ssh johmagnu-nuc 'sudo xfel write 0x41c00000 ~/t113boot/initramfs.cpio.gz'#  fdt 0x41800000, ramdisk 0x41c00000
# (jumping a bare zImage needs r2=DTB; simplest reliable path is still SD-boot or
#  FEL-booting U-Boot and letting it bootz. See notes below.)
```

Notes / gotchas:
- **Needs the VBUS bulk cap** (Rev-A: bodge on C3). With it, stock `xfel` pushes
  the full 5.4 MB zImage in ~7 s, link stable. Without it, bulk writes brown out
  mid-transfer ("usb bulk send error", device drops).
- **Use STOCK upstream xfel** (`~/src/xfel` at git HEAD, 128 KB chunks, no retry).
  A local "retry on bulk error" patch was tried and REVERTED — retrying a partial
  transfer desyncs FEL's stateful protocol and wedges the link at a deterministic
  point. Don't reintroduce it.
- **Flash the way you flash:** one `xfel write <addr> <file>` per artifact (one FEL
  session each). Do NOT loop `write32` hundreds of times to "test" — that opens a
  new FEL session per call and is not representative.
- If the device wedges/keeps dropping, power-cycle (MEGA4) or press reset for a
  clean FEL, then retry.
- FEL enumerates at full-speed (12 Mbps) — that's normal for this BROM and does
  NOT limit bulk transfer.

---

### 5. Typical remote iterate loop

```bash
# 1. (SD pre-flashed with the image under test, card in the T113)
# 2. start capturing the console
ssh johmagnu-nuc 'sudo stty -F /dev/ttyUSB1 115200 raw -echo; sudo timeout 40 cat /dev/ttyUSB1' &
# 3. power-cycle to reset into the new boot
ssh johmagnu-nuc 'sudo uhubctl -l 1-6.4.4 -a off; sudo uhubctl -l 4-2.4.4 -a off; sleep 1; \
                  sudo uhubctl -l 1-6.4.4 -a on;  sudo uhubctl -l 4-2.4.4 -a on'
# 4. read the captured boot log; iterate on the build; reflash; repeat
```

To change the *image*, reflash the SD (§4a) — which means moving the card to the
NUC reader, `dd`, and moving it back. For rapid kernel-only iteration without
card-shuffling, FEL RAM-boot (§4b) is the faster loop once its handoff is sorted.

---

### Expected boot (SD, U-Boot path)

```
U-Boot SPL 2026.04 ... DRAM: 128 MiB ... Trying to boot from MMC1
U-Boot 2026.04 ... CPU: Allwinner R528 (SUN8I) ... Model: MangoPi MQ-R-T113
... mmc0 is current device ... Found U-Boot script /boot.scr
5592384 bytes read (zImage) ... Starting kernel ...
[    0.000000] Booting Linux ...
  gameboy-v3 initramfs — T113-S3 is alive.
~ #                     <- BusyBox shell
```

### Quick troubleshooting

| Symptom | Likely cause / check |
|---|---|
| No UART output at all, board LED on | UART wire loose (esp. GND, or board-TX→adapter-RX at H11-18); wrong ttyUSB; confirm board is actually resetting |
| `uhubctl` says off but LED stays on | wrong hub/port, or device powered elsewhere — confirm you're cutting the MEGA4 (`2109:0817/2817`), not the NUC's internal hub (which does NOT cut VBUS) |
| `xfel` "usb bulk send error" / drops | missing/again-loose VBUS bulk cap (brownout), or wedged FEL — press reset for a clean FEL |
| Board won't enter FEL | ensure SD removed + SPI-NOR blank; then power-cycle |
| Boots but no kernel output after "Starting kernel" | bootargs/console issue — the DTB must get `console=ttyS0,115200` (see boot.cmd `fdt set`) |
| `dd` target confusion | `/dev/sda` = card reader; `/dev/nvme0n1` = NUC SSD (never touch) |
```


---

## ILI9341 SPI LCD (later phase)

Driving an **Adafruit 2.4" 240×320 ILI9341 SPI TFT** ([product 2090](https://www.adafruit.com/product/2090))
from the T113 under Linux. This panel is first-class in mainline: the DRM "tiny"
driver `drivers/gpu/drm/tiny/ili9341.c` binds `compatible = "adafruit,yx240qv29"`
— which *is* this exact module — over **MIPI-DBI on SPI** (write-only; MISO unused).

### Why SPI1 (not the broken-out SPI0)

The header's SPI pins (PC2–PC7) are **SPI0 = the on-board W25Q128 NOR flash bus** —
don't share it with a display. **SPI1** is free, is also broken out (PD bank,
H12/H13), and is the T113's DBI-capable controller. So the LCD rides SPI1.

### Wiring (jumpers: header → panel)

| Panel pin | T113 pin | Header | Role |
|-----------|----------|--------|------|
| SCK  | **PD11** | H13 | SPI1-CLK (mux func `spi1`) |
| MOSI | **PD12** | H13 | SPI1-MOSI (mux func `spi1`) |
| TCS  | **PD10** | H12 | SPI1-CS0 (mux func `spi1`) |
| D/C  | **PD14** | H12 | plain GPIO (bank D=3, pin 14) |
| RST  | **PD15** | H13 | plain GPIO (bank D=3, pin 15), active-LOW |
| VIN  | 3V3 | — | panel logic + regulator |
| GND  | GND | — | — |
| LITE | 3V3 | — | backlight **always on** (not switched) |
| MISO | — | — | **not connected** — the driver never reads |

D/C, RST, and LITE choices are captured in the DT overlay; change the GPIOs there
if you rewire.

### Build

The LCD is **not a build knob** — a peripheral is invariant board data (its DT node +
driver kconfig), not a software-implementation choice. It's specified as two board-owned
fragment files that already live in `boards/t113-gameboy/`:

- `lcd-ili9341.dtsi` — the DT overlay (`spi1` + the `adafruit,yx240qv29` panel node)
- `kernel-lcd-ili9341.config` — the kernel-config fragment (`TINYDRM_ILI9341` + FBDEV + `/dev/fb0`)

They sit **ready but unapplied** (display is a later phase — the board ships headless while
Linux bring-up stabilizes). To bring the panel live, append them to the board's two
unconditional lists in `boards/t113-gameboy/board.conf` (exactly how the UART0 console
overlay is applied every build — no toggle anywhere):

```makefile
KERNEL_DTB_OVERLAYS="uart0-console.dtsi lcd-ili9341.dtsi"
KERNEL_CONFIG_FRAGMENTS="${FORGE_DIR}/defaults/kernel-host.config ${BOARD_DIR}/kernel-lcd-ili9341.config"
```

Then any `make image KERNEL=mainline …` (or the custom kernel via its own `dtb` step) builds
the panel in. What the config fragment enables:
- `CONFIG_DRM_FBDEV_EMULATION` + `CONFIG_TINYDRM_ILI9341` + `CONFIG_FB_DEVICE`.
  `TINYDRM_ILI9341` **selects** `DRM_MIPI_DBI` / `DRM_KMS_HELPER` / `DRM_GEM_DMA_HELPER` /
  `BACKLIGHT_CLASS_DEVICE` automatically — verified by actually compiling `ili9341.o` +
  `drm_mipi_dbi.o` (`DRM_MIPI_DBI` is a select-only tristate that kconfig may not even write
  to `.config`, so we trust the build, not a grep — same lesson as the U-Boot `sf`/MTD gotcha).
- The `.dtsi` overlay enables `spi1` + the panel node, applied by the same overlay mechanism
  as the UART0 console overlay.

### Device tree (`boards/t113-gameboy/lcd-ili9341.dtsi`)

Enables `spi1` (`spi@4026000`, was `status="disabled"`), defines the PD10-12 pinmux
group (`function = "spi1"`), and adds the panel child:

```dts
&spi1 {
    pinctrl-0 = <&spi1_pins>; status = "okay";
    display@0 {
        compatible = "adafruit,yx240qv29", "ilitek,ili9341";
        reg = <0>;                       /* CS0 = PD10 */
        spi-max-frequency = <10000000>;  /* binding fixes ILI9341 DBI @10 MHz */
        dc-gpios   = <&pio 3 14 0>;       /* PD14, active-high */
        reset-gpios= <&pio 3 15 1>;       /* PD15, active-low  */
        rotation   = <0>;
    };
};
```

`&pio` uses 3 cells: `<bank pin flags>`, bank D = 3. Flags: 0 = active-high (D/C),
1 = active-low (RESET, which the ILI9341 asserts low).

### Bring-up + test on the rig

With the two fragments appended to the board's lists (above), build + flash+boot the
image the normal way — the panel is now part of the board, so no extra flag:
```sh
make image KERNEL=mainline BOOTLOADER=uboot LIBC=musl PACKAGES=busybox
tools/flash.sh build/bundles/uboot-linux-musl-busybox nor
```
Then on the console:

```sh
# 1. driver bound + panel probed?
dmesg | grep -iE "ili9341|mipi-dbi|drm"          # expect "[drm] ... ili9341" + a card
ls /dev/dri/ /dev/fb0                             # card0 (DRM) + fb0 (FBDEV emulation)

# 2. cheapest "pixels light up" test — fill the framebuffer:
cat /dev/urandom > /dev/fb0                       # noise across the panel
# or solid: dd if=/dev/zero of=/dev/fb0           # black
```

Panel is 240×320, RGB565 via the DRM fb. If it's blank but the driver bound, the
usual suspects are: D/C on the wrong GPIO, RESET polarity, or SCK/MOSI swapped.

### Status — WORKING ON SILICON (2026-07-30)

**Pixels confirmed on the panel.** The ILI9341 driver probed and the framebuffer
console is mapped to the LCD; text echoed to `/dev/tty0` (`GAMEBOY-V3 LCD LINE
1..8`) was visibly rendered on the screen. Boot dmesg:
```
[drm] Initialized ili9341 1.0.0 for spi0.0 on minor 0
Console: switching to colour frame buffer device 30x40   (= 240x320 px)
ili9341 spi0.0: [drm] fb0: ili9341drmfb frame buffer device
```
(The `spi0.0` in the label is just DRM's controller-index naming; the panel is on
spi1@4026000 per the DTB, correct.) `/dev/dri/card0` present. Wiring (SCK/MOSI/CS
= PD11/12/10, D/C = PD14, RST = PD15) all correct — nothing swapped.

#### `/dev/fb0` needs FB_DEVICE (fixed)

First boot had NO `/dev/fb0` node (`/proc/fb` empty) even though the DRM fbdev
console worked — `sunxi_defconfig` has `FB_CORE=y` + `FRAMEBUFFER_CONSOLE=y` but
`# CONFIG_FB is not set`, and `FB_DEVICE` (which creates the `/dev/fb*` char node)
defaults to FB → off. The `kernel-lcd-ili9341.config` fragment therefore also sets
**`CONFIG_FB_DEVICE=y`** (only needs FB_CORE; NOT the full legacy FB stack). With it, `/dev/fb0` exists and
`cat /dev/urandom > /dev/fb0` / `dd` draws directly. Until a kernel with FB_DEVICE
is flashed, draw via the console (`echo ... > /dev/tty0`) or `/dev/dri/card0`.

#### Rig gotchas hit during bring-up (see also the boot-matrix memory)

- **Wedged USB (`error -71`, "not accepting address"):** the board can get stuck
  present-but-not-enumerating; hub-port VBUS cuts do NOT clear it — only a physical
  USB-C unplug/replug does. Independent of the LCD (happened with it disconnected).
- **VBUS brownout under LCD load:** with the panel on the 3.3V rail, the board has
  enumerated-as-FEL-then-dropped (`USB disconnect` mid-transfer) — the C3-cap
  failure class again, now on the LCD's rail. Local caps at the panel help; a bulk
  cap (47–100µF) on the board's 3.3V rail may be needed for reliable FEL+LCD.

#### Later / nice-to-have
- DBI mode proper (SPI1 has a hardware DBI block; current path is plain SPI + D/C
  GPIO, which is what the mainline driver expects and is simplest).
- Real graphics beyond the fb console (a DRM/KMS test app on card0).

