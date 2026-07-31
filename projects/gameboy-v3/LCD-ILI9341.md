# ILI9341 SPI LCD on the t113-breakout (Linux)

Driving an **Adafruit 2.4" 240×320 ILI9341 SPI TFT** ([product 2090](https://www.adafruit.com/product/2090))
from the T113 under Linux. This panel is first-class in mainline: the DRM "tiny"
driver `drivers/gpu/drm/tiny/ili9341.c` binds `compatible = "adafruit,yx240qv29"`
— which *is* this exact module — over **MIPI-DBI on SPI** (write-only; MISO unused).

## Why SPI1 (not the broken-out SPI0)

The header's SPI pins (PC2–PC7) are **SPI0 = the on-board W25Q128 NOR flash bus** —
don't share it with a display. **SPI1** is free, is also broken out (PD bank,
H12/H13), and is the T113's DBI-capable controller. So the LCD rides SPI1.

## Wiring (jumpers: header → panel)

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

## Build

The driver + overlay are **opt-in** (a bare board has no panel), gated on `LCD=ili9341`:

```bash
make kernel KERNEL=mainline LCD=ili9341      # enables TINYDRM_ILI9341 + FBDEV emulation
                                        # + applies board/t113-gameboy/lcd-ili9341.dtsi
make rootfs KERNEL=mainline LIBC=musl COREUTILS=busybox                  # (unchanged)
```

What the `LCD=ili9341` hook does (see `02-kernel.sh` step 4b):
- `scripts/config --enable DRM_FBDEV_EMULATION --enable TINYDRM_ILI9341`, then
  `olddefconfig`. `TINYDRM_ILI9341` **selects** `DRM_MIPI_DBI` / `DRM_KMS_HELPER` /
  `DRM_GEM_DMA_HELPER` / `BACKLIGHT_CLASS_DEVICE` automatically — verified by
  actually compiling `ili9341.o` + `drm_mipi_dbi.o` (`DRM_MIPI_DBI` is a select-only
  tristate that kconfig may not even write to `.config`, so we trust the build, not
  a grep — same lesson as the U-Boot `sf`/MTD gotcha).
- Copies `board/t113-gameboy/lcd-ili9341.dtsi` next to the board DTS and appends an `#include`
  (same mechanism as the UART0 console overlay), enabling `spi1` + the panel node.

## Device tree (`board/t113-gameboy/lcd-ili9341.dtsi`)

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

## Bring-up + test on the rig

Build the LCD kernel into a bundle and flash+boot it in one command:
```sh
make image LCD=ili9341                       # bundle with the ILI9341 kernel + overlay
tools/flash.sh build/bundles/custom-linux-busybox-lcd_ili9341 nor
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

## Status — WORKING ON SILICON (2026-07-30)

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

### `/dev/fb0` needs FB_DEVICE (fixed)

First boot had NO `/dev/fb0` node (`/proc/fb` empty) even though the DRM fbdev
console worked — `sunxi_defconfig` has `FB_CORE=y` + `FRAMEBUFFER_CONSOLE=y` but
`# CONFIG_FB is not set`, and `FB_DEVICE` (which creates the `/dev/fb*` char node)
defaults to FB → off. The `LCD=ili9341` hook now also enables **`CONFIG_FB_DEVICE=y`**
(only needs FB_CORE; NOT the full legacy FB stack). With it, `/dev/fb0` exists and
`cat /dev/urandom > /dev/fb0` / `dd` draws directly. Until a kernel with FB_DEVICE
is flashed, draw via the console (`echo ... > /dev/tty0`) or `/dev/dri/card0`.

### Rig gotchas hit during bring-up (see also the boot-matrix memory)

- **Wedged USB (`error -71`, "not accepting address"):** the board can get stuck
  present-but-not-enumerating; hub-port VBUS cuts do NOT clear it — only a physical
  USB-C unplug/replug does. Independent of the LCD (happened with it disconnected).
- **VBUS brownout under LCD load:** with the panel on the 3.3V rail, the board has
  enumerated-as-FEL-then-dropped (`USB disconnect` mid-transfer) — the C3-cap
  failure class again, now on the LCD's rail. Local caps at the panel help; a bulk
  cap (47–100µF) on the board's 3.3V rail may be needed for reliable FEL+LCD.

### Later / nice-to-have
- DBI mode proper (SPI1 has a hardware DBI block; current path is plain SPI + D/C
  GPIO, which is what the mainline driver expects and is simplest).
- Real graphics beyond the fb console (a DRM/KMS test app on card0).
