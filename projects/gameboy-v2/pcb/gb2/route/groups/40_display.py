# Display bus: U2 (PPU) -> J3 (the 2090's JP1 socket). All on the front over the In1 GND plane except where a line
# has to dodge the board's inner corner below U2.
#   data D7..D0: U2's west pins 25-28 / 31 / 32 / 34 / 35 -> J3 pins 20..13 on J3's chip-facing side. The pin order
#   matches the socket order, so the fan is monotonic: the upper four leave west and climb in staggered columns (the
#   higher the target, the further east it turns), the lower four drop west/south-west. 0 crossings, 0 vias.
#   control (J3 east side, below U2 - channel W2): RST (pin 36) drops off U2's west side on the front; WR (pin 37)
#   turns the corner through its series R29; DC / CS (pins 42 / 43) and LCD_LITE (MCU PC6) can't fit round that
#   corner on the front, so they cross under U2 on the back (W2b) in the order that lets each peel off to its pin
#   without a crossing, CS hopping back to the front for its last stretch.
W = 0.15

# ---- data, upper four: west, then north in staggered columns, 45 deg into the pad
for net, pin, col, pad in (("LCD_D7", "U2.25", 60.45, "J3.20"), ("LCD_D6", "U2.26", 60.05, "J3.19"),
                           ("LCD_D5", "U2.27", 59.65, "J3.18"), ("LCD_D4", "U2.28", 59.25, "J3.17")):
    (px, py), (jx, jy) = P(pin), P(pad)
    T(net, "F", W, [(px, py), (col, py), (col, jy + (col - 57.95)), (57.95, jy), (jx, jy)])
# ---- data, lower four
(px, py), (jx, jy) = P("U2.31"), P("J3.16")          # D3: under the PLL / core vias, north in the 58.85 column
T("LCD_D3", "F", W, [(px, py), (58.85, py), (58.85, jy + 0.9), (57.95, jy), (jx, jy)])
(px, py), (jx, jy) = P("U2.32"), P("J3.15")          # D2
T("LCD_D2", "F", W, [(px, py), (58.45, py), (58.45, jy + 0.5), (57.95, jy), (jx, jy)])
(px, py), (jx, jy) = P("U2.34"), P("J3.14")          # D1: nearly level
T("LCD_D1", "F", W, [(px, py), (58.4, py), (58.4 - (jy - py), jy), (jx, jy)])
(px, py), (jx, jy) = P("U2.35"), P("J3.13")          # D0: west, then 45 deg down to pin 13
T("LCD_D0", "F", W, [(px, py), (60.4, py), (60.4 - (jy - py), jy), (jx, jy)])

# ---- RST: off U2's west side below D0, down the corridor's west column
(px, py), (jx, jy) = P("U2.36"), P("J3.7")
c = 59.6
T("LCD_RST", "F", W, [(px, py), (60.6, py), (c, py + 1.0), (c, jy - (c - 57.95)), (57.95, jy), (jx, jy)])
# ---- WR: pin 37 -> R29 (sits in the corner) -> down the east column -> J3.5
(px, py), (rx, ry) = P("U2.37"), P("R29.1")
T("LCD_WR_FPGA", "F", W, [(px, py), (px, ry - (px - rx)), (rx, ry)])     # down, then 45 deg into R29
(rx, ry), (jx, jy) = P("R29.2"), P("J3.5")
T("LCD_WR", "F", W, [(rx, ry), (rx, jy - (rx - 57.95) + 0.0), (57.95, jy), (jx, jy)])

# ---- DC / CS / LITE on the back under U2. Rows (top -> bottom) LITE 63.55, CS 63.85, DC 64.3; turning south the
# bottom row turns first, so columns (west -> east) are LITE 60.3, CS 60.7, DC 61.2 - the order the pins peel off.
dcv, csv_ = (64.44, 64.3), (65.45, 64.3)
T("LCD_DC", "F", W, [P("U2.42"), dcv]); VIA("LCD_DC", *dcv, d=0.45, h=0.25)
T("LCD_CS", "F", W, [P("U2.43"), (64.942, 63.95), (65.2, 64.2), csv_]); VIA("LCD_CS", *csv_, d=0.45, h=0.25)
(jx, jy) = P("J3.4")
T("LCD_DC", "B", W, [dcv, (61.2, 64.3), (61.2, jy - (61.2 - 57.95)), (57.95, jy), (jx, jy)])
csf = (60.7, 83.6)                                  # above DC's peel (y 84.3), below WR's (82.1)
T("LCD_CS", "B", W, [csv_, (65.0, 63.85), (61.1, 63.85), (60.7, 64.25), csf]); VIA("LCD_CS", *csf, d=0.45, h=0.25)
(jx, jy) = P("J3.3")
T("LCD_CS", "F", W, [csf, (60.7, jy - (60.7 - 57.95)), (57.95, jy), (jx, jy)])
# LITE: MCU PC6 (pin 37, bottom side) straight down to a via, then west on the back under the E2 channel and U2
lv = (86.25, 62.4)
T("LCD_LITE", "F", W, [P("U1.37"), lv]); VIA("LCD_LITE", *lv, d=0.45, h=0.25)
(jx, jy) = P("J3.8")
T("LCD_LITE", "B", W, [lv, (85.1, 63.55), (60.6, 63.55), (60.3, 63.85), (60.3, jy - (60.3 - 57.95)), (57.95, jy), (jx, jy)])
