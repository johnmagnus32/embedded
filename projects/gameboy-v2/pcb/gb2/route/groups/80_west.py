# West: MCU -> J3 (crossing between its pins on the back, W1, two traces per gap) -> the left grip.
# J3's gaps, north -> south: CTS, RX | RTS, TX | SCL, SDA | PWR_SW, BT_EN - the order the far ends want them in:
# the four UART lines wrap round the BT830's south-east corner onto its bottom row (CTS innermost), I2C turns
# straight up to the expander beside J3, PWR_SW drops to the front and runs south to SW1, BT_EN is the outermost
# BT line. The MCU gives them in a different order (RTS, TX, CTS top row; RX left side; the hub group from under
# the body), so CTS and RX make their way west on the FRONT (over RTS / TX on the back) and drop in above them.
import math
W, VC = 0.15, 30.0
J3y = lambda k: P("J3.%d" % k)[1]
X3 = P("J3.1")[0]
def slots(a, b):                                  # the two trace positions in the gap between pins a (upper) and b
    lo = J3y(a) + 0.8 + 0.255; hi = J3y(b) - 0.8 - 0.255
    return math.ceil(lo / 0.05 - 1e-6) * 0.05, math.floor(hi / 0.05 + 1e-6) * 0.05
GA, GB, GC, GD = slots(20, 19), slots(19, 18), slots(18, 17), slots(17, 16)
thru = lambda y: [(X3 + 2.0, y, "B"), (X3 - 1.25, y, "B")]
WX = X3 - 1.25                                    # where the east legs end, just west of J3's pads
m = MAZE((54.0, 44.0, 102.0, 63.0))

# W1 lanes on the back between the hub's west exit and J3 (x 80 -> 62), north -> south: RTS, TX (their slots),
# SCL, SDA (their slots), PWR_SW 53.85, BT_EN 54.3 (kept 0.45 above the PPU's MOSI hop at 54.75); PWR_SW / BT_EN
# drop to the bottom gap's slots in the strip between J3 and the FPGA's back decaps.
LANE = {"PWR_SW": 53.85, "BT_EN": 54.3}
drop = lambda net, x, slot: [(x, LANE[net], "B"), (x - (slot - LANE[net]), slot, "B")]
# ---- PWR_SW (SW1 -> R31 -> Q1 gate, a uA DC line): R31 sits beside Q1, north-east of the MCU, and the MCU's north
# side is closed on both layers (crystal, battery-sense divider, charger lines), so it goes west UNDER the MCU: down
# the back east of the pin ring, in through the C4|R1 gap, over the hub's I2C on the front (inside the pin ring,
# where the front is empty), back down at the west exit, along W1 to J3. Routed after the I2C (north of it).
(rx, ry) = P("R31.1"); pv = (rx - 0.7, ry + 0.95)
T("PWR_SW", "F", W, [(rx, ry), (rx, pv[1] - 0.7), pv]); VIA("PWR_SW", *pv, d=0.45, h=0.25)
# ---- I2C west branches: a via on each pin's east escape (the stagger column x 95.2), into the hub through the
# C4|R1 / R1|TP8 gaps, out of its west exit, up to their lanes
for net, pin, gap_y, lane, slot in (("I2C1_SCL", "U1.61", 53.0, 55.0, GC[0]), ("I2C1_SDA", "U1.59", 54.65, 55.6, GC[1])):
    py = P(pin)[1]
    VIA(net, 95.2, py, d=0.45, h=0.25); m.commit(NET(net).GetNetname(), [(95.2, py, "F"), (95.2, py, "B")], W)
    AUTOROUTE(m, net, (95.2, py, "B"), (WX, slot, "B"), layers="B",
              through=[(92.6, gap_y, "B"), (83.0, lane, "B"), (81.5, lane, "B"), (81.5 - (lane - slot), slot, "B")] + thru(slot)[:1])
AUTOROUTE(m, "PWR_SW", (*pv, "B"), (WX, GD[0], "B"), via_cost=VC,
          through=[(94.6, 50.2, "B"), (94.6, 52.0, "B"), (91.0, 52.55, "B"), (91.0, 52.55, "F"),
                   (84.9, 56.2, "F"), (84.9, 56.2, "B"), (81.5, 56.2, "B"), (81.5 - (56.2 - LANE["PWR_SW"]), LANE["PWR_SW"], "B")]
                  + drop("PWR_SW", 62.0, GD[0]) + thru(GD[0])[:1])
# ---- BT_EN: pin 38 (bottom row) -> via just inside the pin ring -> north-west through the hub, out of its west exit
(px, py) = P("U1.38"); bv = (px, 59.9)
T("BT_EN", "F", W, [(px, py), bv]); VIA("BT_EN", *bv, d=0.45, h=0.25)
AUTOROUTE(m, "BT_EN", (*bv, "B"), (WX, GD[1], "B"), layers="B",
          through=[(px, 58.9, "B"), (84.75, 56.9, "B"), (81.5, 56.9, "B"), (81.5 - (56.9 - LANE["BT_EN"]), LANE["BT_EN"], "B")]
                  + drop("BT_EN", 62.6, GD[1]) + thru(GD[1])[:1])
# ---- UART: RTS / TX through their stagger vias, west above the decap row (RTS clear of TX's via), one 45 down to
# their gap's lane
for net, pin, vy, xd, slot in (("USART2_RTS", "U1.15", 47.95, 81.0, GB[0]), ("USART2_TX", "U1.16", 48.55, 81.5, GB[1])):
    px = P(pin)[0]
    T(net, "F", W, [P(pin), (px, vy)]); VIA(net, px, vy, d=0.45, h=0.25)
    pts = [(px, vy), (xd, vy), (xd - (slot - vy), slot), (X3 + 2.0, slot)]
    T(net, "B", W, pts, miter_=False)
    m.commit(NET(net).GetNetname(), [(*P(pin), "F"), (px, vy, "F")] + [(x, y, "B") for x, y in pts], W)
    AUTOROUTE(m, net, (X3 + 2.0, slot, "B"), (WX, slot, "B"), layers="B")
# CTS (top row) north between RTS's and +3V3A's vias, west on the front above everything; RX (left side) west then
# north-west on the front to the lane below it; both drop to the back west of the FPGA config lane, above RTS / TX
(px, py) = P("U1.14")
pts = [(px, py), (px, 47.6), (px - 0.3, 47.3), (65.0, 47.3)]
T("USART2_CTS", "F", W, pts, miter_=False); m.commit(NET("USART2_CTS").GetNetname(), [(x, y, "F") for x, y in pts], W)
AUTOROUTE(m, "USART2_CTS", (65.0, 47.3, "F"), (WX, GA[0], "B"), via_cost=VC, through=thru(GA[0])[:1])
(px, py) = P("U1.17")
pts = [(px, py), (81.0, py), (81.0 - (py - 48.75), 48.75), (65.5, 48.75)]
T("USART2_RX", "F", W, pts, miter_=False); m.commit(NET("USART2_RX").GetNetname(), [(x, y, "F") for x, y in pts], W)
AUTOROUTE(m, "USART2_RX", (65.5, 48.75, "F"), (WX, GA[1], "B"), via_cost=VC, through=thru(GA[1])[:1])

# ======================================================================== left grip (west of J3)
mw = MAZE((20.0, 34.0, 60.0, 101.0))
def lay(net, layer, pts):                          # hand geometry, marked for the maze
    T(net, layer, W, pts, miter_=False); mw.commit(NET(net).GetNetname(), [(x, y, layer) for x, y in pts], W)
def via(net, x, y):
    VIA(net, x, y, d=0.45, h=0.25); mw.commit(NET(net).GetNetname(), [(x, y, "F"), (x, y, "B")], W)
# ---- I2C and PWR_SW take the strip between J3 and the switch column, on the front (crossing over the BT lines,
# which run west on the back): SCL / SDA drop through a via each straight up into the expander's right column and
# down the strip, then cross under the Start / Select / Down column on the back to the haptics driver; PWR_SW
# runs down the strip to the side switch.
X_PS, X_SCL, X_SDA = 53.35, 53.8, 54.25            # +5V_LCD runs down J3's west side at x 55.0 (0.4)
(sx, sy), (dx, dy) = P("U11.22"), P("U11.23")
lay("I2C1_SCL", "B", [(WX, GC[0]), (X_SCL, GC[0])]); via("I2C1_SCL", X_SCL, GC[0])
lay("I2C1_SCL", "F", [(X_SCL, GC[0]), (X_SCL, sy + 0.37), (X_SCL - 0.37, sy), (sx, sy)])
lay("I2C1_SCL", "F", [(X_SCL, GC[0]), (X_SCL, 76.0)]); via("I2C1_SCL", X_SCL, 76.0)
lay("I2C1_SDA", "B", [(WX, GC[1]), (WX - 0.2, 53.6), (X_SDA, 53.6)]); via("I2C1_SDA", X_SDA, 53.6)
lay("I2C1_SDA", "F", [(X_SDA, 53.6), (X_SDA, dy + 0.42), (X_SDA - 0.42, dy), (dx, dy)])
lay("I2C1_SDA", "F", [(X_SDA, 53.6), (X_SDA, 76.6)]); via("I2C1_SDA", X_SDA, 76.6)
lay("PWR_SW", "B", [(WX, GD[0]), (WX - 0.45, GD[0] - 0.45), (X_PS, GD[0] - 0.45)]); via("PWR_SW", X_PS, GD[0] - 0.45)
lay("PWR_SW", "F", [(X_PS, GD[0] - 0.45), (X_PS, 92.0)])
AUTOROUTE(mw, "PWR_SW", (X_PS, 92.0, "F"), (*P("SW1.2"), "F"), layers="F")
AUTOROUTE(mw, "I2C1_SCL", (X_SCL, 76.0, "B"), (*P("U8.2"), "F"), via_cost=VC)
# SDA comes up straight under its pin, leaving the GND pin (4) beside it its own via just below the row
(qx, qy) = P("U8.3"); sv = (qx, 88.4)
AUTOROUTE(mw, "I2C1_SDA", (X_SDA, 76.6, "B"), (*sv, "B"), layers="B")
via("I2C1_SDA", *sv); lay("I2C1_SDA", "F", [sv, (qx, qy)])
(gx, gy) = P("U8.4"); lay("GND", "F", [(gx, gy), (gx, 88.5), (gx + 0.4, 88.9)]); via("GND", gx + 0.4, 88.9)
# ---- BT830 UART + enable (back): west under the expander, south past the module's east pads, and each in turn
# west under its bottom row and up into its pad - CTS innermost. C48 (VDD_PADS decap) sits under pins 1-3, so
# CTS / RX pass above it and RTS / TX / BT_EN below; BT_EN goes on round the module's GND pad 30 into pin 8.
BT = (("USART2_CTS", GA[0], 39.2, 60.2, "U12.4"), ("USART2_RX", GA[1], 39.65, 60.65, "U12.5"),
      ("USART2_RTS", GB[0], 40.1, 62.3, "U12.6"), ("USART2_TX", GB[1], 40.55, 62.75, "U12.7"))
for net, slot, xt, yl, pad in BT:
    AUTOROUTE(mw, net, (WX, slot, "B"), (*P(pad), "B"), layers="B", through=[(xt, yl, "B"), (P(pad)[0], yl, "B")])
(bx, by) = P("U12.8")
AUTOROUTE(mw, "BT_EN", (WX, GD[1], "B"), (bx, by, "B"), layers="B",
          through=[(41.0, 63.2, "B"), (27.9, 63.2, "B"), (27.9, by, "B")])
T("BT_VREG18", "B", W, [P("C47.1"), P("U12.10")])
# ---- each switch's A/B pad pair tied first (everything else routes round the ties)
for sw, n_ in (("SW2", "BTN_UP"), ("SW3", "BTN_DOWN"), ("SW4", "BTN_LEFT"), ("SW5", "BTN_RIGHT"), ("SW8", "BTN_START"),
               ("SW9", "BTN_SELECT")):
    lay(n_, "F", [P(sw + ".1"), P(sw + ".2")])
# ---- buttons (front): the expander's west column (P00..P06 top -> bottom: L, LEFT, UP, DOWN, SELECT, START, RIGHT)
# fans out in the switches' own angular order, so nothing crosses. LEFT goes over SW2's north side and down its
# west side to SW4; UP drops into SW2; DOWN / SELECT / START leave as a 0.45 mm column between SW2's east pads and
# the expander, step 0.45 west together once past SW2 (to clear SW5's west pad), then fan to their switches; RIGHT
# runs down beside the expander's pins straight into SW5.
def esc(net, pin, x, d=0.3):                      # west off the pin, one 45, down the column at x
    (px, py) = P(pin); return [(px, py), (x + d, py), (x, py + d)]
JOG = (58.75, 0.45)                               # where the column steps west, and by how much
for net, pin, x in (("BTN_DOWN", "U11.7", 43.7), ("BTN_SELECT", "U11.8", 44.15), ("BTN_START", "U11.9", 44.6)):
    lay(net, "F", esc(net, pin, x) + [(x, JOG[0]), (x - JOG[1], JOG[0] + JOG[1]), (x - JOG[1], 61.0)])
(rx, ry), (fx, fy) = P("U11.10"), P("SW5.1")
lay("BTN_RIGHT", "F", [(rx, ry), (44.92 + 0.15, ry), (44.92, ry + 0.15), (44.92, fy), (fx, fy)])
for net, x, pad in (("BTN_DOWN", 43.7, "SW3.2"), ("BTN_START", 44.6, "SW8.1"), ("BTN_SELECT", 44.15, "SW9.1")):
    x -= JOG[1]; (qx, qy) = P(pad); k = min(abs(qx - x), 0.47)   # on down the column, one 45 into the pad's row
    lay(net, "F", [(x, 61.0), (x, qy - k), (x + (k if qx > x else -k), qy), (qx, qy)])
(ux, uy), (sx2, sy2) = P("U11.6"), P("SW2.2")
lay("BTN_UP", "F", [(ux, uy), (sx2 + 0.5, uy), (sx2, uy + 0.5), (sx2, sy2)])
(lx, ly), (wx4, wy4) = P("U11.5"), P("SW4.2")
lay("BTN_LEFT", "F", [(lx, ly), (36.3, ly), (35.8, ly + 0.5), (35.8, wy4 - (35.8 - wx4)), (wx4, wy4)])
AUTOROUTE(mw, "BTN_L", (*P("U11.4"), "F"), (*P("SW12.2"), "F"), layers="F")
# ---- haptics: LRA drive pair to J6 (0.3 mm), HAP_REG decap; and U2's VCCIO pin pair
AUTOROUTE(mw, "LRA_N", (*P("U8.9"), "F"), (*P("J6.2"), "F"), w=0.3, layers="F")
AUTOROUTE(mw, "LRA_P", (*P("U8.7"), "F"), (*P("J6.1"), "F"), w=0.3, layers="F")
AUTOROUTE(mw, "HAP_REG", (*P("U8.1"), "F"), (*P("C33.1"), "F"), layers="F")
AUTOROUTE(m, "+3V3", (*P("U2.24"), "F"), (*P("U2.22"), "F"), layers="F")      # VCCIO pins either side of NC pin 23
