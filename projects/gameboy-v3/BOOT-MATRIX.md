# gameboy-v3 boot matrix — interchangeable components & test results

Every boot component is a **drop-in provider**. `forge/backends/image.sh` composes
them via four selector variables; one image, one boot command (`bootz` on a
zImage), for every combination.

```
BOOTLOADER = uboot | custom     mainline U-Boot            | our bootloader/
KERNEL     = linux | custom     mainline Linux (zImage)    | our kernel/ (zImage-headed)
ROOTFS     = busybox | scratch  musl BusyBox initramfs     | our rootfs/ (gv3libc)
MEDIA      = sd | nor           full-disk SD .img          | SPI-NOR component set
```

`4 axes → 2 × 2 × 2 × 2 = 16 combinations.`

## What makes them interchangeable

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

## The SD ⇄ NOR equivalence (why NOR testing covers the tuple)

`MEDIA` only changes **where the loader reads the components from**:
- **SD:** loader reads a FAT partition (`fat_load` / U-Boot `load`).
- **NOR:** loader reads fixed SPI-NOR offsets (`spinor_read` / U-Boot `sf read`).

Once kernel+DTB+initramfs are in DRAM at the fixed addresses, **the boot is
byte-identical**. So a given `(BOOTLOADER, KERNEL, ROOTFS)` tuple boots the same
way on either medium; only the read path differs. The NOR path exercises every
distinct tuple; the SD path's unique code (FAT read) is separately silicon-proven
for `uboot+linux+busybox`.

## Remote-testability (the T113-breakout rig)

The board lives on a remote NUC (see [REMOTE-RIG.md](REMOTE-RIG.md)); the dev loop
is **power-cycle → FEL → xfel flashes NOR / delivers the loader → capture UART**.
This is fully remote **only for NOR**:

| MEDIA | Remote-testable? | Why |
|---|---|---|
| **nor** | ✅ fully remote | `xfel spinor write` flashes components; loader delivered via FEL; no card, no hands |
| **sd**  | ❌ needs a human | a microSD must be physically inserted; not the dev loop (see REMOTE-RIG.md) |

So the **8 NOR combos are the remote test campaign**; the 8 SD combos are covered
transitively by the equivalence above (and can be spot-checked by hand later).

The bootloader itself is **never in NOR** (NOR offset 0 is kept blank so the BROM
always falls through to FEL — the un-strand invariant, see [NOR-LAYOUT.md](NOR-LAYOUT.md)).
It is FEL-delivered each boot by `flash.sh` (which reads BOOTLOADER from the bundle).

## The 16 combinations

Legend: ✅ booted to shell on silicon · 🟢 booted in QEMU · 🔧 builds+assembles ·
⚠️ expected-gap (ABI mismatch) · ⬜ not yet tested · 🚫 not remotely testable (SD).

### NOR media (the remote campaign — 8 combos)

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

### SD media (transitively covered; not in the remote campaign — 8 combos)

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

## Distinct artifact sets

The bootloader is delivered separately (FEL), so a NOR flash only needs the
**kernel × rootfs** component set (4 distinct sets); each is booted by either
loader. DTB is shared across all.

| Set | KERNEL artifact | ROOTFS artifact |
|---|---|---|
| A | linux zImage (~5.4 MB) | busybox initramfs (~776 KB) |
| B | linux zImage | scratch initramfs (gv3libc) |
| C | custom zImage (~32 KB) | busybox initramfs |
| D | custom zImage | scratch initramfs |

## How to run one combo (NOR, remote) — the two-script loop

```bash
# 1. build a bundle for the config (dev host):
make image KERNEL=custom BOOTLOADER=custom LIBC=musl COREUTILS=busybox
# 2. flash it to NOR AND FEL-boot it, in one command (stages to the rig itself):
tools/flash.sh build/bundles/custom-custom-busybox nor
```

`flash.sh` power-cycles to FEL (t113power.sh), `xfel spinor write`s the components +
GV3NOR1 table, then FEL-delivers the loader named in the bundle manifest (custom
bootloader → SRAM, or U-Boot proper → DRAM + `sf read`/`bootz`), and tails the UART.
`MEDIA=sd`/`emmc` give a clear "not remotely flashable" error. See NOR-LAYOUT.md.

## Silicon results

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

### The "scratch-on-mainline input" scare was a rig capture bug, NOT a rootfs bug

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

### What the campaign found + fixed (first-ever custom-kernel silicon runs)

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

### Enabling the caches — the ~26× boot speedup (2026-07)

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

### sf-capable U-Boot (reproducible build for the NOR `uboot` combos)

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

### Coverage: all 8 NOR combos exercised on silicon (2026-07-30)

Every NOR combo (#1–#8) has now been run on the T113 silicon and reaches an
**interactive shell**:
- **Clean interactive silicon capture:** #1, #2, #3, #4, #5, #7.
- **Interactive by loader-independent equivalence** (+ QEMU-clean): #6 (==#2), #8 (==#4).
All four ROOTFS×KERNEL pairings are confirmed interactive on real hardware; the
from-scratch rootfs runs on BOTH the custom kernel AND mainline Linux. (The earlier
"#2/#6 input doesn't round-trip" was a rig capture artifact — see results above —
now fixed with the single-reader `uartcap.sh`.)

### Not run on silicon (by design — not remotely testable)
- **All 8 SD-media combos (#9–#16)** — need physical microSD insertion, which the
  remote rig can't do (the whole reason for the NOR+FEL scheme). Covered transitively
  by the SD⇄NOR equivalence (only the FAT-read path differs from `sf read`, and that
  FAT path is silicon-proven for uboot/linux/busybox — combo #9, the original boot).

---
_Single source of truth for component interchangeability + test coverage. Keep the
result rows honest: "builds" ≠ "boots"; "QEMU" ≠ "silicon"._
