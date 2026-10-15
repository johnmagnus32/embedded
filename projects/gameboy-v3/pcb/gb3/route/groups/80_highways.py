# 80_highways -- everything that crosses the board to the top strip (T1) and on into the right grip: T0 from U17, H1
# from W4, the M1 bundle from U16 / S1, the strip itself (trial s4 strip_lanes), the F neck (T1n) and the right grip's
# switch drops (PLAN sections 5 and 6).
#
# T1's west end (x ~124) has a fixed order, N -> S (the east half's drops need it): /PB, PWR_EN (T0) | CHG_PGOOD, SCL,
#   SDA, GAUGE, R1, VOL+, VOL-, A, X, SELECT, B, Y, CHG_FAST (M1) | SPK_EN, DOUT, BCLK, LRCK (T1n).
# West half (step 4, the plan's open item):
#   T0 (F): /PB and PWR_EN east from U17's vias (/PB first east along y 35.6, so its SE diagonal stays NE of PWR_EN's
#     and of U17's GND via), dipping to y 43.4 / 43.7 under D4's two vias (the +5V clamp stub stands north of them),
#     back up east of x 119.95 to their T1 lanes.
#   M1 (F): one northbound bundle at 0.3 mm pitch, x 121.4 (CHG_PGOOD) .. 125.0 (CHG_FAST), east of CC_SENSE's F end
#     (group 30 stops it at x 120.8) and of the USB pair's D+ via; each lane turns east at its T1 y. The lanes come up
#     to a gate at y 57.0 from U16 (the buttons) and S1 / S2 (the charger lines).
#   H1's west end is one 0.3-pitch band of six lanes between the SD via row (V13, y 50.85) and U3's GND via (62.2, 53.6):
#     DOUT, BCLK, LRCK, CC_SENSE (group 30, y 52.3), SCL, SDA (N -> S, the order the W4 risers turn east in). East of
#     the via row the I2S lanes step north to 0.6 pitch (y 50.4 / 51.0 / 51.6, 3W gaps for BCLK) and the I2C pair south
#     to 53.4 / 53.7, either side of CC_SENSE's B hop via (64.8, 52.3), clear of NOR_IO0's F hop and the U6 GND vias.
#   I2C1 joins the bundle from H1; CHG_PGOOD (the bundle's west lane) crosses it once on B (vias (120.8, 54.3) /
#     (120.8, 52.8), south of the I2S B hop). Spurs off the H1 trunk feed U8, FPC1 and U16 (7 vias, one SCL crossing).
#   I2S drops to B west of U8 and crosses under the bundle (the second review's B hop: 2 extra vias per line, 0.6 mm
#     apart on B north of nothing but the SYS trunk at y 49.3), coming up east of it on a NE via line into T1's south lanes.
#   SPK_EN (E1, x 101) goes round the south of U16 on B (y 84.0), up on F along the board's east edge (x 126.0 -> 125.3)
#     and into T1 as the T1n group's north lane.
#   Buttons: U16's east pins -> two via columns east of U16 -> F east, each lane north at its bundle x (a staircase).
#   CHG_PGOOD / GAUGE come from S1 along S2 (B), rise to F and pass under the button lanes on B (x 121.4 / 122.3 from y 83.2); their
#     top vias sit north-west of R1's turn (GAUGE's at x 122.0, 0.6 off R1's lane); CHG_FAST comes straight up east of them.
W = 0.15
GATE = 57.0                                                        # the bundle's south gate (maze ends)
BUNDLE = [("CHG_PGOOD", 121.4, 43.0), ("I2C1_SCL", 121.7, 43.3), ("I2C1_SDA", 122.0, 43.6), ("GAUGE_ALRT", 122.3, 43.9),
          ("BTN_R1", 122.6, 44.2), ("BTN_VOLUP", 122.9, 44.5), ("BTN_VOLDOWN", 123.2, 44.8), ("BTN_A", 123.5, 47.8),
          ("BTN_X", 123.8, 48.1), ("BTN_SELECT", 124.1, 48.4), ("BTN_B", 124.4, 48.7), ("BTN_Y", 124.7, 49.0),
          ("CHG_FAST", 125.0, 49.3)]
BX = {n: x for n, x, y in BUNDLE}; BY = {n: y for n, x, y in BUNDLE}

# ======================================================================== T0
T("/power-ctrl/PWR_BTN", "F", W, [(96.22, 35.6), (97.8, 35.6), (103.85, 41.65), (112.0, 41.65), (113.75, 43.4), (119.95, 43.4),
                                  (121.7, 41.65), (124.6, 41.65)])
T("PWR_EN", "F", W, [(98.85, 37.45), (103.85, 42.45), (112.25, 42.45), (113.5, 43.7), (120.25, 43.7), (121.5, 42.45),
                     (137.5, 42.45)])                              # ends at U12.12's EN via (group 10)

# ======================================================================== H1: I2S (to the hop) and the I2C1 trunk
# W4's north ends (group 70) -> north -> east under the via row (right turn: the east lane turns first) -> past R9.2's
# via (62.75, 50.85) a NE step to 0.6 pitch (north lane first, 0.15 stagger keeps the 45 deg legs 0.3 apart) -> the
# hop's west vias
T("I2S1_DOUT", "F", W, [(53.25, 66.0), (53.25, 51.4), (63.0, 51.4), (64.0, 50.4), (112.2, 50.4)])
T("I2S1_BCLK", "F", W, [(54.75, 66.0), (54.75, 51.7), (63.15, 51.7), (63.85, 51.0), (112.8, 51.0)])
T("I2S1_LRCK", "F", W, [(55.5, 66.0), (55.5, 52.0), (63.3, 52.0), (63.7, 51.6), (113.4, 51.6)])
# the I2C1 trunk: the W4 risers (group 70) north, east south of CC_SENSE, a SE step past U3's GND via (south lane
# first), east at 53.4 / 53.7, north into the bundle (left turn: SCL first)
T("I2C1_SCL", "F", W, [(56.75, 63.0), (56.75, 52.6), (62.6, 52.6), (63.4, 53.4), (BX["I2C1_SCL"], 53.4), (BX["I2C1_SCL"], BY["I2C1_SCL"])])
T("I2C1_SDA", "F", W, [(57.5, 63.0), (57.5, 52.9), (62.4, 52.9), (63.2, 53.7), (BX["I2C1_SDA"], 53.7), (BX["I2C1_SDA"], BY["I2C1_SDA"])])

# ======================================================================== the B hop (I2S + SPK_EN) under the bundle
HOP = [("I2S1_DOUT", (112.2, 50.4), (125.9, 50.4), (126.4, 49.9)),
       ("I2S1_BCLK", (112.8, 51.0), (126.5, 51.0), (127.3, 50.2)),
       ("I2S1_LRCK", (113.4, 51.6), (127.1, 51.6), (128.2, 50.5))]
for net, west, east, top in HOP:
    VIA(net, *west); T(net, "B", W, [west, east]); VIA(net, *east)
    T(net, "F", W, [east, top])                                    # NE into its T1 lane (the east half continues)

# ======================================================================== SPK_EN: round U16 on B, up the east edge on F
T("SPK_EN", "B", W, [(101.0, P("U1.41")[1]), (103.0, P("U1.41")[1]), (103.0, 84.0), (126.0, 84.0)]); VIA("SPK_EN", 126.0, 84.0)
T("SPK_EN", "F", W, [(126.0, 84.0), (126.0, 60.0), (125.3, 59.3), (125.3, 49.6)])

# ======================================================================== M1: the bundle north of the gate
for net, x, y in BUNDLE:
    if net in ("I2C1_SCL", "I2C1_SDA"): continue                  # they arrive from H1 (above)
    if net == "CHG_PGOOD":                                         # one B crossing of the I2C1 trunk
        T(net, "F", W, [(x, GATE), (x, 54.9), (120.8, 54.3)]); VIA(net, 120.8, 54.3)
        T(net, "B", W, [(120.8, 54.3), (120.8, 52.8)]); VIA(net, 120.8, 52.8)
        T(net, "F", W, [(120.8, 52.8), (x, 52.2), (x, y)])
    else:
        T(net, "F", W, [(x, GATE), (x, y)])

# ======================================================================== T1, T1n and the right grip (trial s4 strip_lanes)
X0 = 124.0
def path(x0, y0, jogs, xe):
    pts, y = [(x0, y0)], y0
    for xs, yt in jogs:
        if abs(yt - y) > 1e-9:
            pts += [(xs, y), (round(xs + abs(yt - y), 4), yt)]; y = yt
    return pts + [(xe, y)]
def start(net):                                                    # where a bundle lane's T1 run starts
    return (BX[net], BY[net])
# north lanes
T("CHG_PGOOD", "F", W, [start("CHG_PGOOD"), (147.9, 43.0), (148.5, 42.4)])
T("I2C1_SCL", "F", W, path(*start("I2C1_SCL"), [(149.2, 42.65), (163.0, 41.0)], 177.65) + [(177.9, 41.25)])
T("I2C1_SDA", "F", W, path(*start("I2C1_SDA"), [(149.33, 42.95), (163.13, 41.3), (176.9, 41.8)], 178.65))
T("GAUGE_ALRT", "F", W, path(*start("GAUGE_ALRT"), [(149.46, 43.25), (163.26, 41.6), (176.6, 42.35)], 182.3))
# /PB: from T0 on F at the north edge, B hop under the north lanes (B is free at x 124-133.5 above T3)
VIA("/power-ctrl/PWR_BTN", 124.6, 41.65)
T("/power-ctrl/PWR_BTN", "B", W, [(124.6, 41.65), (124.6, 43.95), (127.6, 46.95)]); VIA("/power-ctrl/PWR_BTN", 127.6, 46.95)
# staircase: via k at (SX + 0.7k, SY + 0.3k), N->S order = the B neck order = the grip fan order
SX, SY = 174.6, 43.15
STAIR = ["BTN_R1", "BTN_VOLUP", "BTN_VOLDOWN", "/power-ctrl/PWR_BTN", "BTN_A", "BTN_X", "BTN_SELECT", "BTN_B", "BTN_Y"]
def sv(k): return (round(SX + 0.7 * k, 4), round(SY + 0.3 * k, 4))
LANES = [
    ("BTN_R1",              44.2,  [(149.59, 43.55), (166.0, 43.375)]),
    ("BTN_VOLUP",           44.5,  [(149.72, 43.85), (166.13, 43.675)]),
    ("BTN_VOLDOWN",         44.8,  [(150.0, 45.3), (167.0, 43.975)]),
    ("/power-ctrl/PWR_BTN", 47.5,  [(150.0, 45.6), (167.13, 44.275)]),
    ("BTN_A",               47.8,  [(150.13, 45.9), (167.26, 44.575)]),
    ("BTN_X",               48.1,  [(150.26, 46.2), (167.39, 44.875)]),
    ("BTN_SELECT",          48.4,  [(150.39, 46.5), (167.52, 45.175)]),
    ("BTN_B",               48.7,  [(150.52, 46.8), (167.65, 45.475)]),
    ("BTN_Y",               49.0,  [(150.65, 47.1), (167.78, 45.775)]),
]
GRIP = {
    "BTN_R1":              ("NE", 190.665, (192.99, 40.6),  ["SW16.2"]),
    "BTN_VOLUP":           ("S", 205.5, (206.3, 45.99),     ["SW12.2"]),
    "BTN_VOLDOWN":         ("S", 204.8, (205.6, 53.99),     ["SW13.2"]),
    "/power-ctrl/PWR_BTN": ("S", 204.1, (204.9, 61.99),     ["SW14.2"]),
    "BTN_A":               ("S", 202.4, (203.2, 71.8),      ["SW7.1"]),
    "BTN_X":               ("S", 195.1, (195.9, 63.95),     [(195.2, 64.65), "SW8.1"]),
    "BTN_SELECT":          ("S", 194.7, (195.2, 47.0),      ["SW11.1"]),
    "BTN_B":               ("S", 193.9, (194.4, 79.9),      [(195.2, 80.7), "SW9.1"]),
    "BTN_Y":               ("S", 187.4, (187.9, 71.9),      [(187.2, 72.6), "SW6.1"]),
}
for k, (net, y0, jogs) in enumerate(LANES):
    vx, vy = sv(k)
    if net == "/power-ctrl/PWR_BTN": x_start, pre = 128.15, [(127.6, 46.95)]
    else: x_start, pre = max(X0, BX[net]), ([] if BX[net] >= X0 else [(BX[net], y0)])
    T(net, "F", W, pre + path(x_start, y0, jogs, round(vx - 0.225, 4)) + [(vx, vy)])
    VIA(net, vx, vy)
    kind, xt, via, stub = GRIP[net]
    yb = round(vy - 0.225, 4)
    if kind == "NE": b = [(vx, vy), (round(vx + 0.225, 4), yb), (xt, yb), via]
    else: b = [(vx, vy), (round(vx + 0.225, 4), yb), (xt, yb), (via[0], round(yb + via[0] - xt, 4)), via]
    T(net, "B", W, b); VIA(net, *via)
    T(net, "F", W, [via] + [P(s) if isinstance(s, str) else s for s in stub])
# CHG_FAST to its drop
T("CHG_FAST", "F", W, path(*start("CHG_FAST"), [(150.78, 48.6)], 162.6))
# T1n: from the hop's F stubs, round the cutout corner (45 deg octagon about the arc's centre), down the wall on F
TAN = 0.41421356
CX, CY = 179.7, 54.3
def octo(r): return [(round(CX + TAN * r, 4), round(CY - r, 4)), (round(CX + r, 4), round(CY - TAN * r, 4))]
T1N = [("SPK_EN", 49.6, [(150.91, 49.2), (168.39, 50.75)], 3.55, [(183.25, 74.25), (185.0, 76.0)], ["R14.2"]),
       ("I2S1_DOUT", 49.9, [(151.04, 49.5), (168.26, 51.05)], 3.25, [(182.95, 77.9), (183.55, 78.5)], [(183.55, 79.45), "U9.1"]),
       ("I2S1_BCLK", 50.2, [(151.17, 49.8), (168.13, 51.35)], 2.95, [(182.65, 79.35), (182.9, 79.6)], [(183.25, 79.95), "U9.16"]),
       ("I2S1_LRCK", 50.5, [(151.30, 50.1), (168.0, 51.65)], 2.65, [(182.35, 82.4), (182.55, 82.6)], [(182.55, 81.65), "U9.14"])]
T1X = {"SPK_EN": 125.3, **{h[0]: h[3][0] for h in HOP}}            # where each T1n lane's T1 run starts
for net, y0, jogs, r, drop, bstub in T1N:
    o = octo(r)
    T(net, "F", W, path(T1X[net], y0, jogs, o[0][0]) + [o[1]] + drop)
    VIA(net, *drop[-1])
    T(net, "B", W, [drop[-1]] + [P(s) if isinstance(s, str) else s for s in bstub])
o = octo(3.85)
T("/charger/CHG_TS", "F", W, path(154.6, 48.0, [(168.6, 50.45)], o[0][0]) + [o[1], (183.55, 59.85), (184.1, 60.4)])
VIA("/charger/CHG_TS", 184.1, 60.4)
T("/charger/CHG_TS", "B", W, [(184.1, 60.4), (183.25, 61.25), P("R17.1")])

# ======================================================================== the fan-in below the gate
# buttons: B stub east to a via (column x 114.4 for R1 / VOL- / X / Y, 115.3 for VOL+ / B / SELECT / A), F east at the
# pin's y, north at the bundle x (the lower the pin, the further east it turns: a planar staircase)
for k, (net, pin) in enumerate((("BTN_R1", 4), ("BTN_VOLUP", 5), ("BTN_VOLDOWN", 6), ("BTN_A", 7), ("BTN_X", 8),
                                ("BTN_SELECT", 9), ("BTN_B", 10), ("BTN_Y", 11))):
    x, y = P("U16.%d" % pin); xv = 114.4 if k % 2 == 0 else 115.3
    T(net, "B", W, [(x, y), (xv, y)]); VIA(net, xv, y)
    T(net, "F", W, [(xv, y), (BX[net], y), (BX[net], GATE)])
# CHG_PGOOD / GAUGE: under the button lanes on B; GAUGE's top via steps west off its lane (R1 turns north 0.3 east of it)
VIA("CHG_PGOOD", 121.4, 83.2); T("CHG_PGOOD", "B", W, [(121.4, 83.2), (121.4, 74.85)]); VIA("CHG_PGOOD", 121.4, 74.85)
T("CHG_PGOOD", "F", W, [(121.4, 74.85), (121.4, GATE)])
VIA("GAUGE_ALRT", 122.3, 83.2); T("GAUGE_ALRT", "B", W, [(122.3, 83.2), (122.3, 75.9), (122.0, 75.6)]); VIA("GAUGE_ALRT", 122.0, 75.6)
T("GAUGE_ALRT", "F", W, [(122.0, 75.6), (122.3, 75.3), (122.3, GATE)])
# S2 (B): S1's three stubs south, east at y 91.0 / 91.75 / 92.5 (south of RGB_R7's row and via, past C96's south pad;
# 0.75 pitch leaves room for the two risers' vias), so group 85's left-grip F lanes cross over them. S1's order puts
# CHG_FAST north and the bundle wants it east, so it stays on B to (125.0, 85.0) under CHG_PGOOD / GAUGE's F legs,
# which rise at x 112.85 / 113.0 and run NE over SPK_EN's B lane to their B legs.
X79, X80, X82 = (P("U1.%d" % k)[0] for k in (79, 80, 82))
T("CHG_FAST", "B", W, [(X79, 84.81), (X79, 90.6), (X79 + 0.4, 91.0), (119.0, 91.0), (125.0, 85.0)]); VIA("CHG_FAST", 125.0, 85.0)
T("CHG_FAST", "F", W, [(BX["CHG_FAST"], 85.0), (BX["CHG_FAST"], GATE)])     # between A and SPK_EN at 0.3 pitch
T("CHG_PGOOD", "B", W, [(X80, 84.81), (X80, 91.35), (X80 + 0.4, 91.75), (112.85, 91.75)]); VIA("CHG_PGOOD", 112.85, 91.75)
T("CHG_PGOOD", "F", W, [(112.85, 91.75), (BX["CHG_PGOOD"], 83.2)])
T("GAUGE_ALRT", "B", W, [(X82, 84.81), (X82, 91.7), (X82 + 0.8, 92.5), (113.0, 92.5)]); VIA("GAUGE_ALRT", 113.0, 92.5)
T("GAUGE_ALRT", "F", W, [(113.0, 92.5), (BX["GAUGE_ALRT"], 83.2)])
# I2C1 drops (U8, FPC1, U16). SCL, the trunk's north lane, crosses SDA once, on B: a via north of the trunk at x 118.4,
# down between U8's pin rows (under its body) to U8.2 and on to a via south of U8; from there F south to FPC1.4's via (SCL; pad 3 = SDA since the pre-fab rotation)
# and west at y 61.3. SDA (the south lane) drops on F at x 119.0 to FPC1.3's via and branches west at y 60.7 (U8.3's
# via joins it from the north). The west pair passes P1.42's tab on its east (x 106.6 / 107.3) and steps SW into U16's
# pocket: vias west of U16.23 / U16.22 (TOUCH_RST / TOUCH_INT and EXP_INT wall U16's and FPC1's north sides on B).
T("I2C1_SCL", "F", W, [(118.4, 53.4), (118.4, 52.9)]); VIA("I2C1_SCL", 118.4, 52.9)
T("I2C1_SCL", "B", W, [(118.4, 52.9), (118.4, 61.3)]); VIA("I2C1_SCL", 118.4, 61.3)
T("I2C1_SCL", "B", W, [(118.4, 59.45), (118.15, 59.7), P("U8.2")])
T("I2C1_SCL", "F", W, [(118.4, 61.3), (118.4, 69.9), (118.0, 70.3)]); VIA("I2C1_SCL", 118.0, 70.3)
T("I2C1_SCL", "B", W, [(118.0, 70.3), P("FPC1.4")])
T("I2C1_SCL", "F", W, [(118.4, 61.3), (107.3, 61.3), (107.3, 70.2), (105.0, 72.5), (105.0, 77.0)]); VIA("I2C1_SCL", 105.0, 77.0)
T("I2C1_SCL", "B", W, [(105.0, 77.0), (105.27, 76.73), P("U16.22")])
T("I2C1_SDA", "F", W, [(119.0, 53.7), (119.0, 70.3)]); VIA("I2C1_SDA", 119.0, 70.3)
T("I2C1_SDA", "B", W, [(119.0, 70.3), P("FPC1.3")])
T("I2C1_SDA", "F", W, [(119.0, 60.7), (106.6, 60.7), (106.6, 70.2), (104.3, 72.5), (104.3, 75.8)]); VIA("I2C1_SDA", 104.3, 75.8)
T("I2C1_SDA", "B", W, [(104.3, 75.8), (104.58, 76.08), P("U16.23")])
VIA("I2C1_SDA", 116.15, 59.2); T("I2C1_SDA", "B", W, [(116.15, 59.2), P("U8.3")])
T("I2C1_SDA", "F", W, [(116.15, 59.2), (116.15, 60.7)])
