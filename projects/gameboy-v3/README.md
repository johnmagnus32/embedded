# gameboy-v3 — Allwinner T113-S3 Linux handheld

**Status:** 🚧 Bring-up. Goal right now: **boot a Linux image on the
[t113-breakout](../t113-breakout) board and get a working serial console.**
Display, sound, and input come later, once Linux boots reliably.

**Build: complete (Steps 0–4 ✅).** All four artifacts + a flashable SD image
(`build/output/gameboy-v3-sd.img`) build reproducibly from [`scripts/`](scripts/).
**Not yet run on hardware** — flashing + first boot is [FLASH.md](FLASH.md); that
is where the chain gets validated on real silicon.

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

**Reproducibility:** each step is a numbered, idempotent script under
[`scripts/`](scripts/). [`scripts/env.sh`](scripts/env.sh) is the single source
of truth — it pins every component version + checksum and defines the build
layout. All build inputs/outputs land in `build/` (git-ignored); the whole tree
rebuilds from `scripts/` + the pins, so `rm -rf build/` then re-run is the
reproducibility test. To bump a version, edit the pin in `env.sh` (deliberately),
never a floating "latest".

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

Prepares everything needed before building anything, via two functions:
`setup_host_make()` and `setup_cross_toolchain()`. It fetches + checksum-verifies
+ extracts **two** pinned Bootlin cross toolchains and sanity-checks each emits
32-bit ARM, and ensures **GNU Make ≥ 4.0** is available (the kernel requires it —
see Step 2); it uses the host's own make if new enough, otherwise builds a pinned
one into `build/hostmake/`.

- **glibc toolchain** (`arm-buildroot-linux-gnueabihf-`) → U-Boot + kernel (they
  don't link a target libc, so libc choice is irrelevant there).
- **musl toolchain** (`arm-buildroot-linux-musleabihf-`) → the rootfs only.
  musl-static BusyBox is ~34% smaller than glibc-static and has a lighter Linux
  syscall surface — both a size win now and groundwork for a hand-written kernel.

```bash
./scripts/00-toolchain.sh          # idempotent; --force to rebuild both
source scripts/env.sh              # puts cross-gcc + make on PATH, sets CROSS_COMPILE/ARCH
```

- Pinned: `armv7-eabihf--glibc--stable-2021.11-1` (GCC 10.3.0), verified against
  Bootlin's published SHA256. The script **refuses to proceed on a checksum
  mismatch** — an unverified toolchain never enters the build.
- **Why not the newest toolchain?** This build host is **glibc 2.26**. Bootlin's
  2024+/2025 toolchains ship a binutils `ld` that needs GLIBC_2.27, so its `ld`
  won't run here — the build dies at the first *link* step (a compile-only check
  passes, so it surfaces mid-build, confusingly). 2021.11 gives GCC 10.3.0
  (satisfies U-Boot's host-gcc-≥10 requirement) with a `ld` needing only
  GLIBC_2.14. On a newer host you can bump the pin to `2025.08-1` in `env.sh`.
- Installs to `build/toolchain/`; `CROSS_COMPILE=arm-buildroot-linux-gnueabihf-`
  (Bootlin builds their toolchains with Buildroot, hence the `buildroot` triple).
- After `source scripts/env.sh`, `${CROSS_COMPILE}gcc --version` works from any
  CWD. Every later step sources `env.sh`, so this is set once.

### Step 1 — U-Boot (SPL + U-Boot proper) ✅ implemented

Clones mainline U-Boot at a pinned tag, applies our board's console + host-tool
config, overlays the control DTB, builds, and copies the artifact to `output/`:

```bash
./scripts/01-uboot.sh            # idempotent; --clean to re-clone from scratch
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
2. `scripts/uart0-console.dtsi` overlay (`#include`d into the board `.dts`) →
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
./scripts/02-kernel.sh           # idempotent; --clean to re-clone
# → build/output/zImage  and  build/output/sun8i-t113s-mangopi-mq-r-t113.dtb
```

- **Pinned Linux v6.12.95** (LTS; T113 board DTS landed v6.4, settled into the
  `allwinner/` subdir at v6.5), `sunxi_defconfig`. Enables the essentials as `=y`:
  `SERIAL_8250_DW` (UART), `MMC_SUNXI` (SD), the D1/T113 CCU + pinctrl.
- **DTB:** `sun8i-t113s-mangopi-mq-r-t113.dtb` (SoC dtsi cross-includes the RISC-V
  D1's sun20i dtsi files — expected; same die).
- **Console → UART0:** the script `#include`s the **same `scripts/uart0-console.dtsi`**
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
./scripts/03-rootfs.sh           # idempotent; --clean to rebuild from scratch
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
- **`/init` is PID 1** ([scripts/init](scripts/init)): mounts `/proc`, `/sys`,
  devtmpfs `/dev`, prints a banner + `nproc`, then `exec … /bin/sh`. If PID 1
  ever exits, the kernel panics — so it hands off to a shell that stays running.
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
MEDIA=sd ./scripts/build.sh      # → build/output/gameboy-v3-<cfg>-sd.img (~64 MiB)
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
  ([scripts/boot.cmd](scripts/boot.cmd) → compiled to `boot.scr` with mkimage)
  sets `console=ttyS0,115200`, loads the three files to U-Boot's built-in sunxi
  load addresses (`kernel_addr_r=0x41000000`, `fdt_addr_r=0x41800000`,
  `ramdisk_addr_r=0x41C00000`), and runs
  `bootz ${kernel_addr_r} ${ramdisk_addr_r}:${filesize} ${fdt_addr_r}` (the
  `:size` is required for a raw cpio.gz ramdisk).

**Flashing + first boot is a separate, hardware + root step — see
[FLASH.md](FLASH.md)** for the `dd` command, serial wiring, expected boot log,
and troubleshooting. That is where the whole chain gets validated on real silicon;
the build itself can only verify the image *structure* (which it does).

### Recovery (can't-brick backstop)

Use **`xfel`** (not `sunxi-fel`) for the sun20i generation — it fully supports
the T113 (FEL, DDR init, SPI-NOR/NAND). The BROM auto-enters FEL over USB-C when
it finds no valid boot image, so a bad SD card always recovers. `lsusb` shows
`1f3a:efe8` in FEL mode.

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
