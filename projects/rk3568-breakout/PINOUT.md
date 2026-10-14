# PINOUT.md — RK3568 Breakout Pin Assignment

**Status:** 🔬 Pre-schematic. Drives the netlist for [BOM.md](./BOM.md). Nothing routed yet.
**Package:** RK3568B2, FCCSP-636L, 0.65 mm, **636 balls**.

**Provenance.** Every ball/mux/domain below comes from Rockchip's `RK3568_PinOut_V1.1_20210202.xlsx`
(sheet `05.PinOut List`), cross-checked against sheet `04.Ball Map`, the
`IO Power Domain Description and Checklist`, the `Processing instructions for unused pins` workbook, the
Hardware Design Guide V1.2, the Schematic/PCB Review Checklist, and the AIoT reference schematic — then
cross-checked again against mainline `rk3568-pinctrl.dtsi`. Three machine checks ran clean:
**636/636 balls agree** between the two sheets · every mainline `rockchip,pins` triple resolves to the
matching function name at the matching mux index (**Function 0 = GPIO**, 1 = first alternate) ·
**52 assignments = 52 unique pins on 52 unique balls, 0 collisions.**

**GPIO inventory: 152 of 160 bonded out.** Not bonded (never reference these):
`GPIO0_D2`, `GPIO0_D7`, `GPIO2_C7`, `GPIO4_D3…D7`.

> ✅ **No function on this board is forced into a 1.8 V-only GPIO domain.** The only 1.8 V signal we use is
> `SARADC_VIN0` (a dedicated analog ball) — so **no level shifters are needed.**

---

## 1. Assignments

"reset pull" = the on-die pull before software runs (16–43 kΩ, 26 kΩ typ).

### 1.1 Previously decided — all seven verified correct

| Function | Ball | Bank/pin | mux | Domain | V | reset pull |
|---|---|---|---|---|---|---|
| UART2 console TX (1.5 Mbaud) | **AH24** | GPIO0_D1 | 1 `UART2_TX_M0` | PMUIO2 | 3.3 | up |
| UART2 console RX | **AC20** | GPIO0_D0 | 1 `UART2_RX_M0` | PMUIO2 | 3.3 | up |
| I²C0 SCL (RK809 0x20 + RK860-0 0x40) | **AF24** | GPIO0_B1 | 1 `I2C0_SCL` | PMUIO2 | 3.3 | up |
| I²C0 SDA | **AB21** | GPIO0_B2 | 1 `I2C0_SDA` | PMUIO2 | 3.3 | up |
| TSADC_SHUT → RESETn net | **AG26** | GPIO0_A1 | 1 `TSADC_SHUT_M0` | PMUIO1 | 3.3 (fixed) | High-Z |
| PMIC_SLEEP (→ RK860-0 VSEL) | **AG28** | GPIO0_A2 | 1 `PMIC_SLEEP` | PMUIO1 | 3.3 (fixed) | down |
| PMIC_INT | **AA22** | GPIO0_A3 | 0 | PMUIO1 | 3.3 (fixed) | up |
| SDMMC0 card-detect **+ boot strap** | **Y22** | GPIO0_A4 | 1 `SDMMC0_DET` | PMUIO1 | 3.3 (fixed) | up |
| `FLASH_VOL_SEL` **strap, must be LOW** | **AG25** | GPIO0_A7 | 1 | PMUIO1 | 3.3 (fixed) | **up** ⚠️ |
| SDMMC0_D0 / D1 | **J25 / J24** | GPIO1_D5 / D6 | 1 | VCCIO3 | 3.3 | up |
| SDMMC0_D2 *(=SWCLK)* / D3 *(=SWDIO)* | **H26 / J23** | GPIO1_D7 / GPIO2_A0 | 1 | VCCIO3 | 3.3 | up |
| SDMMC0_CMD / CLK | **H27 / H28** | GPIO2_A1 / A2 | 1 | VCCIO3 | 3.3 | up / down |
| FSPI_CLK | **A22** | GPIO1_D0 | 1 | VCCIO2 | 3.3 (strap) | down |
| FSPI_CS0n | **C23** | GPIO1_D3 | 1 | VCCIO2 | 3.3 (strap) | up |
| FSPI_D0 (NOR DI) / D1 (NOR DO) | **C24 / D23** | GPIO1_D1 / D2 | 1 | VCCIO2 | 3.3 (strap) | up |
| FSPI_D2 (NOR /WP) | **F20** | GPIO1_C7 | **2** | VCCIO2 | 3.3 (strap) | **down** ⚠️ |
| FSPI_D3 (NOR /HOLD) | **A27** | GPIO1_D4 | 1 | VCCIO2 | 3.3 (strap) | up |

`FSPI_D2` really is **mux 2** on GPIO1_C7 (mux 1 there is `EMMC_RSTn`) — mainline `fspi_pins` agrees.
Everything else is mux 1.

### 1.2 Newly assigned

| Function | Ball | Bank/pin | mux | Domain | reset pull | Why |
|---|---|---|---|---|---|---|
| **Backlight PWM** → TPS61165 CTRL | **AE23** | GPIO0_C3 | 1 `PWM4` | PMUIO2 | **down** | Rockchip's own `LCD0_BL_PWM4`. A real PWM instance. Reset pull-down ⇒ **boost off until software enables it** — right default for a 23.8 V rail. Escape hatch: `PWM5` = GPIO0_C4 (AD21) is free. |
| **Panel rail enable** → SY6280 #3 EN | **AC21** | GPIO0_C5 | 0 | PMUIO2 | **down** ✅ | ✅ **Resolved from the pinout sheet** — reset pull is **down** (16 K–43 K, typ 26 K) and reset direction is **input**, so the active-high SY6280 EN is **off through power-on**, satisfying LCM&TP R8 (*"the power supply of the screen must be controllable; when powered on, it is not provided by default"*). Rockchip's own pin-usage column for AC21 reads verbatim **"LCD1 panel power enable, active high"** — this is the vendor-intended pin. Alternate **GPIO0_C6 (AD20)** is also pull-down but Rockchip assigns it *"4G module power enable"*. ⛔ **Must get no EN pull-up.** |
| **Panel RESET#** | **AH25** | GPIO0_C7 | 0 | PMUIO2 | **down** | Pull-down ⇒ panel **held in reset** through power-on, exactly what the ILI9881C wants (reset high ≥5 ms after rails). |
| **Panel TE** (optional) | **AG23** | GPIO0_C2 | 0 | PMUIO2 | down | ⚠️ **Mainline consumes nothing** — video mode. Wire it, omit from DTS. |
| **Touch I²C1 SCL** | **AG24** | GPIO0_B3 | 1 `I2C1_SCL` | PMUIO2 | up | **Separate controller from I²C0** → a GT928 lockup can't stall the PMIC bus. Rockchip's description literally says *"for Touch panel"*. |
| **Touch I²C1 SDA** | **AB20** | GPIO0_B4 | 1 `I2C1_SDA` | PMUIO2 | up | ditto |
| **TP_INT** | **AC22** | GPIO0_B5 | 0 | PMUIO2 | up | Rockchip's `TP_INT_L_GPIO0_B5` |
| **TP_RST** | **AA20** | GPIO0_B6 | 0 | PMUIO2 | up | Rockchip's `TP_RST_L_GPIO0_B6`; both GPIOs needed for the 0x5D/0x14 address strap |
| **NOR_EN** (SY6280 #1) | **AG27** | GPIO0_A0 | 0 | **PMUIO1** | down ⚠️ | PMUIO1 is the only **hard-3.3 V, must-be-powered, no-software-config** domain — no chance of a domain mistake on the one net that is the way back from a brick |
| **SD_EN** (SY6280 #2) | **AE24** | GPIO0_A6 | 0 | **PMUIO1** | down ⚠️ | ditto |

> ⛔ **`GPIO0_A5` (AF25) was considered for NOR_EN/SD_EN and REJECTED.** Rockchip's unused-pin workbook:
> it *"defaults to the SDMMC0_PWREN function; **it drives LOW before U-Boot comes up**."* With an
> active-high SY6280 EN that forces the medium **off** during the BootROM probe — the board could never
> boot from it. GPIO0_A0/A6 carry no such warning.

### 1.3 Dedicated (non-iomux) balls

| Function | Ball(s) | Note |
|---|---|---|
| **DSI0 lane 0** P/N | **AH17 / AG17** | |
| **DSI0 lane 1** P/N | **AH16 / AG16** | |
| **DSI0 CLK** P/N | **AH15 / AG15** | clock pair is **in the middle** |
| **DSI0 lane 2** P/N | **AH14 / AG14** | |
| **DSI0 lane 3** P/N | **AH13 / AG13** | |
| DSI0 PHY AVDD 0.9 / 1.8 V | **W16 / Y17** | ferrite bead from `vdda0v9_image` / `vcca1v8_image` — ⚠️ **both OFF at RK809-5 reset** |
| USB2 OTG0 DP / DM | **P27 / P28** | 2.2 Ω series each (Mandatory) |
| USB3_OTG0_VBUSDET | **M24** | ⚠️ **not 5 V tolerant**; 10 k/15 k divider = 3.00 V + 100 nF |
| USB3_OTG0_ID | **L23** | ~200 kΩ internal pull-up ⇒ **leave floating** = device mode |
| **SARADC_VIN0** (recovery) | **B27** | 🔴 **1.8 V ONLY** |
| SARADC_AVDD_1V8 / OTP_VCC18 | **H22 / H20** | both **must be powered** |
| nPOR | **AH27** | 10 kΩ pull-up **must** go to `vcc3v3_pmu` |
| XIN24M / XOUT24M | **AF28 / AF27** | 24 MHz crystal |

**Edge order top→bottom is D0 (17), D1 (16), CLK (15), D2 (14), D3 (13)** — order the flex footprint
D0, D1, CLK, D2, D3 and all five pairs route straight out with no crossings.

**Why DSI0 over DSI1:** both are 4-lane with their own D-PHY, but DSI0 routes off a **single package
edge**, is the reference designs' primary panel, and its PHY is a **combo with LVDS_TX0 on the same
balls** — a free fallback. Our 720×1280@60 is well inside DSI0's 1920×1080@60.

### 1.4 Insurance-header spares — deliberately **all VCCIO5**

One domain, one rail ⇒ "every header pin is 3.3 V" is true *structurally*, not by inspection. VCCIO5 is
the biggest free pool (30 GPIO, none used by the board — we fit no parallel RGB/BT656/BT1120).

| Function | Ball | Bank/pin | mux |
|---|---|---|---|
| I2C5 SCL / SDA | AF1 / AE1 | GPIO3_B3 / B4 | 4 `_M0` |
| SPI0 CLK / MOSI / MISO / CS0 | AC7 / AD7 / AG6 / AC8 | GPIO2_D3 / D1 / D0 / D2 | 3 `_M1` |
| UART3 TX / RX | AD4 / AD2 | GPIO3_B7 / C0 | 4 `_M1` |
| PWM14 / PWM15 | AC3 / AC2 | GPIO3_C4 / C5 | 1 `_M0` |
| HDR_GPIO0–11 | AF5, AF6, AD6, AH5, AH4, AB8, AE5, AG4, AF4, AH3, AG3, AH2 | GPIO2_D4–D7, GPIO3_A0–A7 | 0 |

---

## 2. Collision check

**Direct ball collisions: NONE** (52 → 52 unique, verified programmatically).

**M0/M1/M2 mixing violations: NONE.** This is the rule most likely to bite — HDG §2.1.11.1 and the
checklist both make it *Mandatory*: *"`UART2_TX_M0` + `UART2_RX_M1` is **not supported**… this is the
constraint for all functions with different IOMUX."* Every multiplexed instance here uses one group:
UART2→M0 · I²C5→M0 · UART3→M1 · **SPI0→all four M1** · PWM14/15→M0 · TSADC_SHUT→M0.

**Functions whose every ball is now taken:**

| Function | Status |
|---|---|
| **SPI2** | 🔴 both mux groups blocked unless **VCCIO4 is powered** (M1 = header GPIO, M0 = VCCIO4). Resolved by powering VCCIO4 (§3). |
| **I²C2** | 🟡 M0 is TP_INT/TP_RST; only reachable via M1 in VCCIO6 |
| **UART2** | 🟡 **no alternate mux** — M1 sits on SDMMC0_D0/D1. If the console balls die there is no second UART2; fall back to the header's UART3. |
| **PWM4** | 🟡 single mux. Backlight escape: **PWM5 = GPIO0_C4 (AD21)**, free — worth a DNP 0 Ω option. |
| **CLK32K_IN** | 🟢 only GPIO0_B0 (AD23), and it's **free** — the optional RK809 CLK32K route stays possible |

### The dangerous ones — strap and default-state collisions

**E1. 🔴 `FLASH_VOL_SEL` defaults to the WRONG level.** AG25 has an internal **pull-UP** (16–43 kΩ) and
*"H: VCCIO2 is 1.8 V **(Default)**; L: 3.3 V."* We fit a **3.3 V** W25Q128JV, so it must be held **LOW**.
HDG: a mismatch means *"the function will be abnormal **or the chip may even be damaged**."* And there is
**no software escape** — mainline's `rk3568_iodomain_write()` has `case 3: /* vccio2 */ break;`.

Sizing (calculated, not from Rockchip — they never run VCCIO2 at 3.3 V): worst case R_pu = 16 kΩ to
3.3 V, V_IL(max) = 0.8 V ⇒ R_pd ≤ 5.1 kΩ.

| R_pd | V at AG25 (16 k / 26 k / 43 k) | Verdict |
|---|---|---|
| **1 kΩ** | 0.19 / 0.12 / 0.08 V | ✅ **recommended** (>4× margin) |
| 2.2 kΩ | 0.40 / 0.26 / 0.16 V | ✅ acceptable |
| 4.7 kΩ | 0.75 / 0.50 / 0.32 V | ⚠️ at the limit |
| 10 kΩ | 1.27 / 0.92 / 0.62 V | ⛔ **FAILS** |

Also: *"not recommended as GPIO"* — don't repurpose AG25.

**E2. 🔴 `GPIO0_A5` driven LOW pre-U-Boot** — excluded, see §1.2.

**E3. 🟡 `SDMMC0_DET` is a second boot strap, not just card detect.** Only two system-init straps exist:
`FLASH_VOL_SEL` and `SDMMC0_DET` (Y22). **High** (internal pull-up, no card) ⇒ the six VCCIO3 balls mux
to **ARM JTAG**; **low** (card inserted) ⇒ **SDMMC0**. Consequences:
1. The socket's CD contact **must** short Y22 to GND on insertion — SD boot is impossible without a card, which is correct.
2. The rescue table still works: CD is mechanical and independent of VDD, so "card in, SD_EN off" ⇒ DET low ⇒ ROM probes SD ⇒ unpowered card fails ⇒ falls through ✅
3. **Free bonus:** with no card you get **SWD debug** — SWCLK = **H26**, SWDIO = **J23** (A55 JTAG is SWD-only per the checklist). **Bring these out as 0.7 mm test pads, not header pins** — they are live SD DAT2/DAT3 and a 2.54 mm stub hangs ~15 mm off a 50 MHz bus.

**E4. 🟡 NOR_EN / SD_EN fight their own internal pull-downs.** GPIO0_A0/A6 reset with a 16–43 kΩ
pull-**down**, but SY6280 EN must default **high** (both media ON).

| external pull-up to `vcc3v3_pmu` | V at EN (16 k worst / 43 k) |
|---|---|
| 10 kΩ (BOM's current reel) | **2.03 V** / 2.68 V |
| **4.7 kΩ** | **2.55 V** / 2.98 V |

→ **use 4.7 kΩ, not 10 kΩ.** See §5 item 1 — the SY6280's V_IH is undetermined.

**E5. 🟡 Two drivers on one net.** NOR_EN/SD_EN are driven by the rig's DTR/RTS *and* by SoC GPIOs — both
push-pull. **The rig must win** (the point is recovery from a hung SoC):

```
GPIO0_A0 (AG27) --[1 kΩ]--+
                          +---- SY6280 #1 EN   (4.7 kΩ pull-up to vcc3v3_pmu)
 header pin (rig DTR) ----+
```
SoC drives through 1 kΩ; the rig overrides it; both floating ⇒ the pull-up holds EN high.

**E6. 🔴 `SARADC_VIN0` is 1.8 V-referenced and sits on a 3.3 V header.** Ball B27 is in the fixed 1.8 V
`SARADC_AVDD_1V8` domain. Label the header pin
**"SARADC_VIN0 — short to GND to recover. ⛔ NEVER apply >1.8 V."** It's the only non-3.3 V-safe pin on
the header.

**E7. 🟡 NOR /WP comes up asserted.** FSPI_D2 resets pull-**down** ⇒ the W25Q128JV's **/WP is low**
before software. Checklist (*Mandatory*): *"the resistors connected to FSPI_D2 and FSPI_D3 must be
reserved"* — Rockchip fits **10 kΩ pull-ups** on both. ⚠️ **On this board they must go to the GATED NOR
rail, not VCCIO2** — a deviation from the reference, where those nets are the same. Tied to VCCIO2 they
would inject current into a deliberately-dead flash and partly undo the rescue gate.

**E8. ⛔ Never gate VCCIO2 or VCCIO3 themselves** — both are "must be powered". The SY6280s go in series
with the **NOR VCC** and **microSD VDD** only.

---

## 3. Per-domain summary

| Domain | Ball(s) | Rockchip rule | Capable of | This board |
|---|---|---|---|---|
| **PMUIO0** (= PMUPLL_AVDD_1V8) | Y21 | **must be powered** | 🔴 **1.8 V only** | 1.8 V (`vcca1v8_pmu`) |
| **PMUIO1** | Y20 | **must be powered** | 🟢 **3.3 V only** | 3.3 V (`vcc3v3_pmu`) |
| **PMUIO2** | W19 | **must be powered** | 1.8 / 3.3 | **3.3 V** (`vcc3v3_pmu`) |
| VCCIO1 | H17 | can be unpowered | 1.8 / 3.3 | 3.3 V — insurance |
| **VCCIO2** | H18 | **must be powered** | **strap only** | **3.3 V + strap LOW** |
| **VCCIO3** | L22 | **must be powered** | 1.8 / 3.3 | 3.3 V (`vccio_sd`) |
| VCCIO4 | J21 | can be unpowered | 1.8 / 3.3 | 3.3 V — insurance (SPI2's only home) |
| **VCCIO5** | V10, V11 | can be unpowered | 1.8 / 3.3 | **3.3 V — the whole header** |
| VCCIO6 | R9, U9 | can be unpowered | 1.8 / 3.3 | 3.3 V — insurance |
| VCCIO7 | V12 | can be unpowered | 1.8 / 3.3 | 3.3 V — insurance |

**Three deliberate deviations from `rk3568-rock-3a.dts`, all in one direction — every selectable domain
goes to 3.3 V:** VCCIO2 1.8→**3.3 V** (3.3 V NOR, no eMMC) · VCCIO4/6 1.8→**3.3 V** (nothing of ours is
in them; 3.3 V keeps spares header-compatible and SPI2 reachable) · VCCIO1 `vccio_acodec`→**`vcc_3v3`**
(LDO4 is OFF at RK809-5 reset; moving it to the always-on switch removes a software dependency and frees
LDO4 entirely). Uniformity is itself the main defence against mis-setting one.

```dts
&pmu_io_domains {
	status = "okay";
	pmuio1-supply = <&vcc3v3_pmu>;   /* 3.3 V fixed in silicon; driver no-ops */
	pmuio2-supply = <&vcc3v3_pmu>;
	vccio1-supply = <&vcc_3v3>;      /* NOT vccio_acodec */
	vccio2-supply = <&vcc_3v3>;      /* informational only; driver no-ops */
	vccio3-supply = <&vccio_sd>;     /* 3.3 V pinned, no UHS */
	vccio4-supply = <&vcc_3v3>;      /* NOT vcc_1v8 */
	vccio5-supply = <&vcc_3v3>;
	vccio6-supply = <&vcc_3v3>;      /* NOT vcc_1v8 */
	vccio7-supply = <&vcc_3v3>;
};
```
Verified against `drivers/soc/rockchip/io-domain.c` (`soc_data_rk3568_pmu`, 9 supplies, `grf_offset
0x140`). ⚠️ Rockchip repeats in four documents: *"if it is misconfigured to 1.8 V, the IO of this power
domain will be in an **overvoltage state, and the IO will be damaged** in long-term operation."*

**FLASH_VOL_SEL summary:** VCCIO2 at 3.3 V ⇒ strap **LOW** ⇒ **W25Q128JV** ✅ (1.8 V would be strap HIGH
= fit nothing, and a W25Q128J**W**). No DTS property or register can rescue a wrong strap.

---

## 4. Free-GPIO count

| Domain | Bonded | Used | Free | Usability |
|---|---|---|---|---|
| PMUIO0 | 4 | 0 | 4 | 🔴 **1.8 V only** — keep off the header |
| PMUIO1 | 8 | 7 | 1 | ⛔ GPIO0_A5 only, poisoned (§E2) — treat as zero |
| PMUIO2 | 18 | 11 | **7** | 🟢 **best spares** — 3.3 V, always-on, wake-capable |
| VCCIO1 | 12 | 0 | 12 | 🟢 |
| VCCIO2 | 17 | 6 | 11 | 🟡 **BootROM-probed** — not for straps or early state |
| VCCIO3 | 6 | 6 | 0 | — |
| VCCIO4 | 20 | 0 | 20 | 🟢 also SPI2_M0's only home |
| VCCIO5 | 30 | 22 | 8 | 🟢 |
| VCCIO6 | 28 | 0 | 28 | 🟢 I²C2_M1, I²C4_M0, SPI3_M0 |
| VCCIO7 | 9 | 0 | 9 | 🟢 SPI3_M1, PWM12–15_M1 |
| **Total** | **152** | **53** | **99** | **84 clean 3.3 V spares, 6 always-on** |

The seven PMUIO2 spares: **GPIO0_B0** (AD23, CLK32K_IN), **B7** (AH26, PWM0), **C0** (AD22, PWM1/UART0_RX),
**C1** (AF23, PWM2/UART0_TX), **C4** (AD21, **PWM5** = backlight escape), ~~**C5** (AC21)~~ — **now the panel-rail enable**, **C6** (AD20, PWM7). A whole spare UART0, three remaining PWMs and the 32 kHz input, all always-on. *(53 pins used / 99 free after this assignment.)*

> ⚠️ **Don't use the 11 free VCCIO2 pins for anything needing early state.** The BootROM probes
> **FSPI → NAND → eMMC → SDMMC0**, driving those IOs during each probe — and those eleven balls are
> precisely the NAND/eMMC lines. Fine as post-U-Boot GPIO; never for a strap.

### Suggested 2×25 header

Signals interleaved with GND on rows 1–19 (short returns for probing); slow GPIO paired on 20–25.

| Pin | Signal | Ball | Pin | Signal | Ball |
|---|---|---|---|---|---|
| 1 | +5 V (VBUS) | — | 2 | GND | — |
| 3 | +3V3 (`vcc_3v3`) | — | 4 | GND | — |
| 5 | 🔴 **SARADC_VIN0 — GND to recover, NEVER >1.8 V** | B27 | 6 | GND | — |
| 7 | +1V8 | — | 8 | GND | — |
| 9 | +3V3 (`vcc3v3_pmu`) | — | 10 | GND | — |
| 11 | RK809 **PWRON** | — | 12 | GND | — |
| 13 | **RESETB / nPOR** | AH27 | 14 | GND | — |
| 15 | **UART2_TX** | AH24 | 16 | GND | — |
| 17 | **UART2_RX** | AC20 | 18 | GND | — |
| 19 | UART3_TX | AD4 | 20 | GND | — |
| 21 | UART3_RX | AD2 | 22 | GND | — |
| 23 | I2C5_SCL | AF1 | 24 | GND | — |
| 25 | I2C5_SDA | AE1 | 26 | GND | — |
| 27 | SPI0_CLK | AC7 | 28 | GND | — |
| 29 | SPI0_MOSI | AD7 | 30 | GND | — |
| 31 | SPI0_MISO | AG6 | 32 | GND | — |
| 33 | SPI0_CS0 | AC8 | 34 | GND | — |
| 35 | PWM14 | AC3 | 36 | GND | — |
| 37 | PWM15 | AC2 | 38 | GND | — |
| 39–50 | HDR_GPIO0–11 | AF5, AF6, AD6, AH5, AH4, AB8, AE5, AG4, AF4, AH3, AG3, AH2 | | | |

31 signals + 19 GND = 50. **NOR_EN / SD_EN are deliberately NOT here** — they keep their own 2×2 header,
because they are the only path back from a dead bootloader and shouldn't share a connector you unplug.

Two *Mandatory* checklist items for this header: fit **22–100 Ω series** resistors on board-to-board
connector pins (at minimum the twelve HDR_GPIOs and SPI0_CLK), and **mark pin 1 clearly** — a reversed
50-pin IDC would put 5 V onto GPIO3_A7.

---

## 5. DDR3L mapping — what may be swapped, and what may not

✅ **Resolved from the Hardware Design Guide V1.2** (the question POWER-AND-BLOCKS listed as blocking the
`ddr` block). Verbatim, under **"For DDR3/DDR3L"**:

> Support the entire group swap between Byte; support DQ swap within Byte;
> The CA sequence **cannot be swapped** and must be allocated according to the schematic design;
> If need to support the template with a total bit width of 16bit and a template with a total bit width
> compatible of 16bit/32bit, it's must use the template provided by RK, and no difference is allowed.

So, concretely:

| | Freedom | Consequence for routing |
|---|---|---|
| **Byte lanes** (DQ[7:0] + DQS± + DM as one group) | ✅ **swap whole groups freely** | Assign byte lanes to whichever DRAM is closer; no length-match between lanes is needed either (write-leveling). |
| **DQ within a byte** | ✅ **swap freely** | Untangle the fan-out without vias. This is where most of the layout win is. |
| **CA — address / bank / RAS / CAS / WE / CS / CKE / ODT** | ⛔ **fixed** | Must be wired exactly per the ball's `DDR3_*` name. **No freedom at all.** |
| **16-bit/32-bit dual-width compatibility** | ⛔ template-only | Doesn't bind us — we are fixed single-config 32-bit (2× ×16). Only matters if one layout must boot both widths. |

**Why the CA map looks scrambled:** the controller's internal pins are `AC0…AC29`, and Rockchip's fixed
allocation already permutes them onto DDR3 names. `AC0` is `DDR3_A9`, `AC15` is `DDR3_A0`. That permutation
*is* the "schematic design" the HDG refers to, and it is baked into the ball names — so capture has no
decision to make, it just follows the name.

### 5.1 CA ball map (copy verbatim — 30 nets)

| Ball | DDR3 | Ball | DDR3 | Ball | DDR3 |
|---|---|---|---|---|---|
| E4 | `A0` | A8 | `A8` | F7 | `BA0` |
| E11 | `A1` | B6 | `A9` | D9 | `BA1` |
| F5 | `A2` | D6 | `A10` | C4 | `BA2` |
| F4 | `A3` | B7 | `A11` | D5 | `RASn` |
| B1 | `A4` | E9 | `A12` | E6 | `CASn` |
| A5 | `A5` | A7 | `A13` | F8 | `WEn` |
| C1 | `A6` | B8 | `A14` | B3 | `CKE` |
| C2 | `A7` | C5 | `A15` | F11 | `RESETn` |
| **B4** | **`CLKP`** | **A4** | **`CLKN`** | | |
| D8 | `CS0n` | E8 | `ODT0` | | |
| B2 | `CS1n` *(unused — single rank)* | A2 | `ODT1` *(unused)* | | |

### 5.2 Byte-lane balls (free to permute)

| Lane | DM | DQS± | Half |
|---|---|---|---|
| 0 | H5 | G1 / G2 | A |
| 1 | J4 | L2 / L1 | A |
| 2 | D14 | A11 / B11 | B |
| 3 | E17 | B15 / A15 | B |

The 32-bit bus is **4 byte lanes across two 16-bit halves (`_A`, `_B`)** — one half per DRAM for a
single-rank 2× ×16 build. ⚠️ **A DM/DQS trio moves as a unit with its DQ[7:0]**; swapping "a byte lane"
means moving all eleven nets together, not just the data bits.

> ⚠️ **One caveat on the blob.** The HDG's swap permission is a *hardware* statement. RK3568 DDR init runs
> from Rockchip's closed **`ddr.bin`**, so the permission is only as good as that blob honouring its own
> guide. It is the same blob every Rockchip reference board runs and byte-swapping is a documented,
> widely-used freedom — but if training fails on board #1, **reverting to the template's literal byte order
> is the first thing to try**, before suspecting the layout.

## 6. Explicitly undetermined

1. ✅ **SY6280AAC EN threshold — RESOLVED:** absolute **2 V min** (not a fraction of V_IN), so 4.7 kΩ works (2.55 V) and 10 kΩ does not (2.03 V). Also found a missing mandatory `ISET` resistor (6.8 kΩ → 1 A limit). *Original question: decides whether 4.7 kΩ beats the SoC's internal pull-down
   (§E4). If V_IH is ~1.2 V absolute, 4.7 kΩ is comfortable. **If it's a fraction of V_IN
   (0.7 × 5 V = 3.5 V), no pull-up value works from a 3.3 V domain** and the EN nets need a level shift
   or a different load switch. **Get the datasheet before schematic capture.**
2. ✅ **Winstar flex DSI pair order — RESOLVED (read from `cad/docs/WF50DTYA3MNG10.pdf`).** Panel flex,
   ascending pin number: **D3 (22/23) · D2 (25/26) · CLK (28/29) · D1 (31/32) · D0 (34/35)**, each pair
   separated by a GND pin. The SoC edge, top→bottom, is **D0 · D1 · CLK · D2 · D3**. **The two are exact
   mirrors** — so with the connector oriented so flex pin 40 sits nearest the SoC's AH13 end, all five
   pairs route straight across with **no crossings**. Get the orientation backwards and all five cross.
3. 🟡 **Does the A-MicroTF-1.85A's CD contact short to GND on insertion?** §E3 promotes this from a
   convenience to **the VCCIO3 SDMMC0-vs-JTAG boot strap**. Confirm from the socket drawing.
4. ✅ **RK809 SWOUT1 rating — RESOLVED:** **Switch1 2.1 A max, R_dson 90 mΩ** (Switch2 2.1 A / 100 mΩ),
   datasheet Rev 2.3 §1.2. Our `vcc_3v3` load — six insurance IO domains (idle), the panel's ~55 mA, the
   NOR's ~20 mA and the SD's ~500 mA peak — is well under 1 A, so **2.1 A is comfortable**.
5. 🟡 **Unpowered-NOR invisibility to the BootROM** (already a BOM bench item). The 22 Ω series and
   tying D2/D3 pull-ups to the *gated* rail reduce injection paths, but this is still a measurement.
6. 🟡 **Exact `ili9881c` panel descriptor** for 720×1280. 4 lanes + video mode confirmed; the specific
   `.desc` wasn't matched. Software only, zero pinout impact.
