# gameboy-v4 — BOM (RK3568 build)

Working draft, 2026-09-06. The **RK3568** path (Unity/indie-3D-class, self-routable no-HDI-*ish* DDR3
board, fully-open GPU). Supersedes the [STM32MP157 BOM](BOM.md) *if* this chip is chosen; that one is
kept as the safe fallback. See [CHIP-COMPARISON.md](CHIP-COMPARISON.md) for why.

**Sourcing:** LCSC C-numbers below were live-confirmed on 2026-09-06 — but per the project rule they
stay **⚠️ until *you* load each live LCSC page** and mark them ✅. Prices are qty-1; drop hard at volume.

## Core (clone Rockchip's RK3568 reference power tree + DDR topology)

| # | Role | Part (MPN) | LCSC | Qty | Notes |
|---|------|-----------|------|-----|-------|
| 1 | **SoC** | **Rockchip RK3568B2** | **C3345225** ⚠️ | 1 | Quad A55 + Mali-G52 + 0.8-TOPS NPU. **FCCSP-636L, 0.65 mm → no-HDI** (via-in-pad on inner rows = JLC-standard POFV, *not* microvia HDI). X-ray req, MSL 3. Current respin; ~$19. |
| 2 | **PMIC** | **Rockchip RK817-5** | **C5179490** ⚠️ | 1 | PMU + rails + RTC + **audio codec + single-cell Li-ion charger + fuel gauge** — the battery-handheld PMIC. QFN-68 7×7; ~$2.90. *(Wall/USB-only alt: RK809-5 / C2939628, no charger.)* Don't add a separate codec/RTC. |
| 3 | **DRAM** | **Micron MT41K512M16VRP-107 IT:P** | **C2831669** ⚠️ | **2** | 8 Gbit **×16 DDR3L** (1.35 V), FBGA-96, −40…+95 °C. **2 chips → single-rank 32-bit = 2 GB.** Rated 1866; **clock the controller at DDR3-1600** for margin. Extended part, X-ray req, MSL 3 → buy spares. |
| 4 | Clocks | 24 MHz HSE + 32.768 kHz LSE crystals | — 🔲 | 2 | Per Rockchip load-cap spec. |
| 5 | DDR support | VREF divider (2× 1 kΩ 1%), ZQ 240 Ω, RZQ 120 Ω, decoupling | — 🔲 | several | Per [RK3568-DDR3-ROUTING-STUDY.md](RK3568-DDR3-ROUTING-STUDY.md); no VTT (ODT). |
| 6 | Boot flash | eMMC or SPI-NOR | — 🔲 | 1 | Match Rockchip's reference to reuse the DDR-init blob. |

> **2 GB is the cap here** (single-rank ×16 DDR3L). 4 GB needs dual-rank (4 chips) or DDR4 — deliberately not doing that.

## Display — recommended fresh (not the gb3 panel)

| # | Role | Part | LCSC | Notes |
|---|------|------|------|-------|
| 7 | **Panel + touch** | **Winstar WF50DTYA3MNG10** — 5.0" 720×1280 IPS, MIPI-DSI, **ILI9881C** + integrated **capacitive touch** | **Digikey ~$61** ⚠️ | Mainline `panel-ilitek-ili9881c` on RK3568 VOP2 — add a per-SKU DSI init table + new `compatible`. Portrait-native → rotate to landscape 1280×720. Bare panel (own enclosure). **Confirm the touch controller IC on the datasheet** (GT911/FT5x06 → `goodix`/`edt-ft5x06`). See [PANEL-COMPARISON.md](PANEL-COMPARISON.md). |
| 8 | FPC connector | 30/40-pin 0.5 mm FFC — **match the WF50DTYA3MNG10 flex pinout** | ⚠️ verify | LCSC-sourceable; JLC solders this, the panel plugs in. Pull the panel datasheet for pin count/pitch first. |
| 9 | Backlight | WLED boost + I²C dimmer (if not on the panel FPC) | ⚠️ verify | Check whether the panel's flex includes the backlight driver or needs an external one. |

## Still needed before orderable
Buttons/expander, analog stick + ADC, IMU, haptics, audio amp+speaker, USB-C, microSD, battery — carry
the gb3 selections (all chip-agnostic I²C/I²S/SDIO). Resolve every ⚠️/🔲 to a live LCSC page + JLC tier.
Clone Rockchip's editable DDR3-1066 Allegro reference for the DDR island (legal-gray — confirm reuse).
