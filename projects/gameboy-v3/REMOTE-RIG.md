# Remote bring-up rig — flash, watch UART, power-cycle the T113 over SSH

This is the operational runbook for the **remote** t113-breakout setup: the board
lives on a NUC and everything (reset, flash, serial console) is driven over SSH —
no physical access needed. This is distinct from [FLASH.md](FLASH.md), which is
the generic "dd a card on your own bench" procedure.

> Companion docs: [FLASH.md](FLASH.md) (build + local flash), the t113-breakout
> [REV-B-FIXES.md](../t113-breakout/REV-B-FIXES.md) (why a VBUS bulk cap + UART
> series resistors are needed), and [PINOUT.md](../t113-breakout/PINOUT.md).

---

## The rig at a glance

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

## 0. Names & addresses (fill in for your session)

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

## 1. Power-cycle (reset) the board — use `scripts/t113power.sh`

**Preferred: the wrapper `scripts/t113power.sh`** (staged on the NUC at
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

### ⚠️ HOW TO VERIFY A CUT WORKED — do NOT trust the LED (this wasted hours once)

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

### Board POWER vs UART CONSOLE are different USB devices — don't confuse them

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

## 2. Watch the UART console

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

## 3. Identify the console tty (numbers shuffle on replug)

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

## 4. Flash — two ways

### 4a. SD card (the normal path)

The board boots from an SD image at the 8 KiB offset + a FAT partition. Build it
with `MEDIA=sd ./scripts/build.sh` (see FLASH.md), copy to the NUC, and `dd` it.

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

### 4b. FEL — push into DRAM over USB-C (no SD, no persistence)

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

## 5. Typical remote iterate loop

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

## Expected boot (SD, U-Boot path)

```
U-Boot SPL 2026.04 ... DRAM: 128 MiB ... Trying to boot from MMC1
U-Boot 2026.04 ... CPU: Allwinner R528 (SUN8I) ... Model: MangoPi MQ-R-T113
... mmc0 is current device ... Found U-Boot script /boot.scr
5592384 bytes read (zImage) ... Starting kernel ...
[    0.000000] Booting Linux ...
  gameboy-v3 initramfs — T113-S3 is alive.
~ #                     <- BusyBox shell
```

## Quick troubleshooting

| Symptom | Likely cause / check |
|---|---|
| No UART output at all, board LED on | UART wire loose (esp. GND, or board-TX→adapter-RX at H11-18); wrong ttyUSB; confirm board is actually resetting |
| `uhubctl` says off but LED stays on | wrong hub/port, or device powered elsewhere — confirm you're cutting the MEGA4 (`2109:0817/2817`), not the NUC's internal hub (which does NOT cut VBUS) |
| `xfel` "usb bulk send error" / drops | missing/again-loose VBUS bulk cap (brownout), or wedged FEL — press reset for a clean FEL |
| Board won't enter FEL | ensure SD removed + SPI-NOR blank; then power-cycle |
| Boots but no kernel output after "Starting kernel" | bootargs/console issue — the DTB must get `console=ttyS0,115200` (see boot.cmd `fdt set`) |
| `dd` target confusion | `/dev/sda` = card reader; `/dev/nvme0n1` = NUC SSD (never touch) |
```
