# Right grip, front: the I2S bus (E2 band -> J4's lower gaps -> the amp), BTN_B -> SW7, and the locals (speaker,
# SD_MODE strap, power LED, the switches' A/B pad pairs).
# The MCU's bottom pins (WS 33, CK 34, BTN_B 35, SD 36, west -> east) drop south past the console / LCD_LITE / BT_EN
# vias and run east along the bottom-edge band. The amp takes them in the opposite rotation (DIN on its west side,
# BCLK then LRCLK along its top), so the I2S bundle has to reverse once on the way: that happens in the free band
# west of J4 (the maze places the hops), not at either package.
W = 0.15
import math
J4 = lambda k: P("J4.%d" % k)[1]
X4 = P("J4.1")[0]
def slots(k):                                     # as in 70_east: the two trace positions in the gap below pin k + 1
    lo = J4(k + 1) + 0.8 + 0.255; hi = J4(k) - 0.8 - 0.255
    return math.ceil(lo / 0.05 - 1e-6) * 0.05, math.floor(hi / 0.05 + 1e-6) * 0.05
S61, S63 = slots(14), slots(13)
thru = lambda y, L: [(X4 - 2.0, y, L), (X4 + 1.25, y, L)]
VC = 30.0
m = MAZE((83.0, 44.0, 166.5, 70.0))
# escapes: straight south off the pins, one 45, then east along the band; the eastern pin turns highest, so nothing
# crosses: north -> south SD, BTN_B, CK, WS. The console / LCD_LITE vias above all sit at y <= 62.4 (BT_EN's goes
# under the MCU body), so the band starts at 62.95 and the four run at ~0.5 mm pitch: BCLK keeps >= 0.3 mm to both
# neighbours the whole way. BTN_B's lane is its J4 slot. The maze takes over at x 117 (east of R20).
BAND = {"I2S2_SD": ("U1.36", 62.95, 0.2), "BTN_B": ("U1.35", S63[1], 0.4),
        "I2S2_CK": ("U1.34", 64.03, 0.4), "I2S2_WS": ("U1.33", 64.5, 0.4)}
XE = 117.0
for net, (pin, y, D) in BAND.items():
    px, py = P(pin)
    pts = [(px, py), (px, y - D), (px + D, y), (XE, y)]
    T(net, "F", W, pts, miter_=False)
    m.commit(NET(net).GetNetname(), [(x, y_, "F") for x, y_ in pts], W)
# J4 (pins 13 / 14 / 15 gaps), north -> south: WS, CK (back), SD, BTN_B (front) - the order the amp / SW7 take them
# in. WS dives first (x 118) and runs north under CK's front lane to the back lane y = WS slot; CK dives 1.5 mm later
# to the lane south of it. Diving in that order IS the WS / CK swap, so the reversal costs four vias in all.
# (Both back lanes pass under J8's pads, which are front-only, and clear SWDIO's via at (125.6, 60.0).)
for net, xv, lane, dl in (("I2S2_WS", 118.0, S61[0], 0.0), ("I2S2_CK", 119.5, S61[1], 0.3)):
    y = BAND[net][1]                              # CK runs 0.3 below its slot (0.6 from WS) until J4's approach
    T(net, "F", W, [(XE, y), (xv, y)]); VIA(net, xv, y, d=0.45, h=0.25)
    pts = [(xv, y), (xv, lane + dl + 0.7), (xv + 0.7, lane + dl)] + \
          ([(129.5, lane + dl), (129.5 + dl, lane)] if dl else []) + [(X4 + 1.25, lane)]
    T(net, "B", W, pts, miter_=False)
    m.commit(NET(net).GetNetname(), [(XE, y, "F"), (xv, y, "F"), (xv, y, "B")] + [(x, y_, "B") for x, y_ in pts[1:]], W)
# east of J4: WS / CK come up before VOLDOWN's back diagonal and run north-east to the amp's top pins from above it
# (WS outermost: it passes north of CK's pin to drop into pin 14), SD to pin 1 on the amp's west side, inside them
# BTN_B and SD stay on the front: each drops to its J4 slot with one 45 (BTN_B first, so the two diagonals don't
# meet), then BTN_B runs straight on to SW7; SD goes on to the amp after WS / CK
x1, y1 = P("SW7.1"); yb, ys = S63[1], S63[0]
pts = [(XE, yb), (x1 - (y1 - yb), yb), (x1, y1)]
T("BTN_B", "F", W, pts, miter_=False); m.commit(NET("BTN_B").GetNetname(), [(x, y, "F") for x, y in pts], W)
pts = [(XE, 62.95), (128.5, 62.95), (128.5 + (ys - 62.95), ys), (X4 + 1.25, ys)]
T("I2S2_SD", "F", W, pts, miter_=False); m.commit(NET("I2S2_SD").GetNetname(), [(x, y, "F") for x, y in pts], W)
AUTOROUTE(m, "I2S2_WS", (X4 + 1.25, S61[0], "B"), (*P("U6.14"), "F"), through=[(146.0, 49.25, "F"), (150.25, 49.25, "F")], via_cost=VC)
AUTOROUTE(m, "I2S2_CK", (X4 + 1.25, S61[1], "B"), (*P("U6.16"), "F"), through=[(146.6, 49.85, "F")], via_cost=VC)
AUTOROUTE(m, "I2S2_SD", (X4 + 1.25, ys, "F"), (*P("U6.1"), "F"), via_cost=VC)
# switch A/B pad pairs (joined inside the switch, but each pad needs its copper)
T("BTN_B", "F", W, [P("SW7.1"), P("SW7.2")]); m.commit(NET("BTN_B").GetNetname(), [(*P("SW7.1"), "F"), (*P("SW7.2"), "F")], W)
# SW6's pair is tied on the BACK, continuing BTN_A's back line from its via at SW6.1 to a via past SW6.2, so the
# speaker pair can run down between the switch's pads on the front
(ax, ay), (bx, by) = P("SW6.1"), P("SW6.2")
T("BTN_A", "B", W, [(153.1, 60.05), (bx + 1.0, by)]); VIA("BTN_A", bx + 1.0, by, d=0.45, h=0.25)
T("BTN_A", "F", W, [(bx + 1.0, by), (bx, by)])
# locals
AUTOROUTE(m, "AMP_SD_MODE", (*P("U6.4"), "F"), (*P("R11.1"), "F"), layers="F")
T("PWR_LED_A", "F", W, [P("D1.2"), P("R2.2")])
# speaker: OUTP / OUTN (filterless class-D: full-swing ~300 kHz square waves, the speaker coil is the filter) as a
# tight 0.3 mm pair (0.5 mm pitch) the whole way, to keep the radiating loop small: east off the pins, down past the
# amp feed's vias between SW6's pads, west under SW6, and each drops into its J5 pad. The outer line's corners are
# cut longer so the pair stays parallel through every 45. SPK_N (pin 10, north) is the outer line at the first turn.
WS_ = 0.3
yP, yN = P("U6.9")[1], P("U6.10")[1]              # 52.75 / 52.25
xP, xN = 154.75, 155.25                           # columns, clear of the SYS vias (153.5) and SW6.1 / SW6.3 (154.38)
hP, hN = 64.75, 65.25                             # the run west under SW6 (its GND pads end at 64.14)
jP, jN = P("J5.1")[0], P("J5.2")[0]
T("SPK_P", "F", WS_, [P("U6.9"), (xP - 0.6, yP), (xP, yP + 0.6), (xP, hP - 0.45), (xP - 0.45, hP),
                      (jP + 0.45, hP), (jP, hP + 0.45), P("J5.1")], miter_=False)
T("SPK_N", "F", WS_, [P("U6.10"), (xN - 0.893, yN), (xN, yN + 0.893), (xN, hN - 0.743), (xN - 0.743, hN),
                      (jN + 0.45, hN), (jN, hN + 0.45), P("J5.2")], miter_=False)
