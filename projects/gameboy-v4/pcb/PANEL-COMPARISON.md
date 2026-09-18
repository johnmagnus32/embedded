# gameboy-v4 — Panel Comparison (RK3568 / MIPI-DSI)

Research/planning, 2026-09-06. Candidate ~5" **MIPI-DSI** panels for the [RK3568 build](BOM-RK3568.md)
(VOP2 + `dw-mipi-dsi`, mainline). **Capacitive touch is REQUIRED** — no-touch panels are excluded.

**Legend — the key column is "Mainline":**
- **✅✅ drop-in** — the *exact panel* has an in-tree `compatible` → zero new panel code.
- **✅ IC in-tree** — the driver IC has a mainline driver, but this SKU isn't listed → add a small
  per-panel DSI init table + a new `compatible` (minor, well-documented).
- **❌** — no mainline driver for the IC → you'd write one (avoid).

*Touch: cap = capacitive (GT911/FT5x06 all mainline). Price = qty-1, drifts. Stock ⚠️ confirm on the
live page before ordering (project sourcing rule). Landscape = rotate the portrait-native panel 90°.*

## Panels — MIPI-DSI **with capacitive touch**

| Panel (MPN) | Vendor | Size · Res · Type | Touch | Driver IC | Mainline | ~Price · where |
|---|---|---|---|---|---|---|
| **WF50DTYA3MNG10** | Winstar | 5.0" 720×1280 IPS | ✅ cap | **ILI9881C** | ✅ IC in-tree | **~$61 · Digikey** |
| ST0500V1WCYOL-RSLW-C | Santek | 5.0" 720×1280 | ✅ cap (I²C) | **ST7703** | ✅ IC in-tree | $80 · Digikey |
| E50RD-I-MW420-C | Focus LCDs | 5.0" 720×1280 IPS | ✅ GT911 | **ILI9881C** | ✅ IC in-tree (both ICs named) | $132 · Digikey |
| RPi Touch Display 2 (5") | Raspberry Pi | 5.0" 720×1280 IPS | ✅ 5-pt | ILI9881C | **✅✅ drop-in** (`raspberrypi,dsi-5inch`) | $40 — **complete module, not a bare panel** |
| RK055HDMIPI4MA0 | NXP/Rocktech | 5.5" 720×1280 IPS | ✅ GT911 | RM68200 (some revs HX8394) | ✅ IC in-tree | $91 NXP / $136 Digikey |
| RK055AHD091-CTG / RK055MHD091A0-CTG | Rocktech | 5.5" 720×1280 IPS | ✅ GT911 | RM68200 / HX8394-F | ✅ IC in-tree | **RFQ / MOQ only** |
| Waveshare 7" DSI (C) / RPi Display 2 (7") | Waveshare / RPi | 7.0" 720×1280 IPS | ✅ cap | ILI9881C | ✅✅ drop-in | ~$45–65 — module, **>5"** |

## Avoid (has touch, but **no mainline driver** / IC unknown)

| Panel (MPN) | Vendor | Size · Res | Touch | Driver IC | Why avoid |
|---|---|---|---|---|---|
| E55RB-I-MW400-C | Focus LCDs | 5.5" 1080×1920 IPS | ✅ cap | HX8399-A | ❌ distinct from HX8394 — no DSI panel driver |
| E43GB-I-MW405-C | Focus LCDs | 4.3" 480×800 IPS | ✅ GT911 | ILI9806E | ❌ no mainline DSI panel driver |
| GF-M40XGF20416-02 | Gold Fame | 5.0" 800×480 | ✅ cap | unknown | IC not stated → mainline unknown |
| LCD087-050 / LCD197-050 | Lincoln | 5.0" 1080×1920 IPS | cap variant (+$26) | unknown | IC unknown; FHD = more GPU load |

## Top picks (touch required, mainline)

- **Best value, true 5.0" + touch → Winstar WF50DTYA3MNG10 (~$61).** 720×1280 IPS, ILI9881C + cap;
  ships as a bare panel, add a small init table. **Recommended default.**
- **Cheapest + zero panel code → RPi Touch Display 2, 5" ($40).** Exact in-tree drop-in — *but it's a
  complete module*, so verify the bare panel/FPC can integrate into your enclosure before relying on it.
- **Cleanest mainline (both display + touch ICs named in-tree) → Focus E50RD-I-MW420-C ($132).**
  ILI9881C + GT911; priciest but least guesswork.
- **Bigger 5.5" option → NXP RK055HDMIPI4MA0 ($91–136).** RM68200/HX8394 + GT911.

All beat the gb3 panel (720p DSI + touch, mainline). Pick on **size** (5.0" vs 5.5") and **bare panel
vs module**. *(Removed: Xingbangda XBD599 — ships fused in PinePhone plastic casing + is service-only,
qty-1, PinePhone-owners; not a viable custom-board BOM part. All no-touch panels removed per requirement.)*
Then drop the choice into [BOM-RK3568.md](BOM-RK3568.md).
