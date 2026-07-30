# Flashing & booting gameboy-v3 (Phase 1)

How to put the built image on a microSD, wire up the serial console, and boot
the T113-S3 on the t113-breakout board to a shell prompt.

Build the SD image (toolchain once, then one command):

```bash
./scripts/00-toolchain.sh                 # once
MEDIA=sd ./scripts/build.sh               # → build/output/gameboy-v3-<cfg>-sd.img
# defaults to custom+linux+busybox; add BOOTLOADER=uboot / KERNEL=... / ROOTFS=... to vary
```

---

## 1. Flash the microSD

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

## 2. Wire the serial console (UART0 / PE2-PE3)

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

## 3. Power on

Power the board via **USB-C** (5 V in; on-board bucks make 3.3 V + 0.9 V core).
No separate supply needed. Watch the serial console.

### Expected boot sequence

```
U-Boot SPL 2026.04 ...              <- SPL: DRAM init succeeded
...
U-Boot 2026.04 ...                  <- U-Boot proper (console now on UART0)
...
== gameboy-v3 boot.scr ==           <- our boot script ran
Booting: zImage + DTB + initramfs (... bytes) ...
## Flattened Device Tree blob at 41800000
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

## Troubleshooting

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

## What this proves (and doesn't)

Reaching the shell validates the entire Phase-1 chain on real silicon:
BROM → SPL (DRAM init) → U-Boot → boot.scr → kernel → initramfs → BusyBox shell,
with the console on our board's UART0 and both CPU cores online.

It does **not** yet exercise: the kernel's SD/MMC driver + a real root
filesystem mount (we use a RAM initramfs — see the README for why), display,
audio, or input. Those are later phases.
