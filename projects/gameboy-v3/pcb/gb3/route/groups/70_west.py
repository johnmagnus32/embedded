# 70_west -- U1's west side: I2C1's escape to its pull-ups, UART3 to the BT830 (W1 / W5), BT_EN_RST, and the I2S /
# JACK_DET staircase into W4 with the DAC branch (step-3 escape + PLAN channels W1 / W4 / W5 / S1).
#
# I2C1 (N row): U1.3 SCL / U1.4 SDA west on B to the 2k2 pull-ups R30 / R31; F risers x 56.75 / 57.5 (W4) north to
#   y 63.0, where group 80 takes the bus east (H1 -> M1 -> U16 / FPC1 / U8, T1 -> U15 / U13).
# UART3 (W1 -> W5, B): U1.118-121 west, north at x 50.2-51.4 (0.4 pitch), west through the vacated core-buck zone and
#   north into U18's pins 4-7. The lanes arrive in the order CTS, TX, RTS, RX (N -> S) and the pads need CTS, RX, RTS,
#   TX: two swaps, so RX and TX each take one F hop (PLAN W5: 2 via pairs at the U18 end); CTS and RTS stay on B. No
#   copper under the module's body (its bottom sits on the B side).
# BT_EN_RST (U1.84 PB4, O1): DAC_XSMT / HP_EN (U1.85 / 86, west of it) head south-west into the audio core, BT_EN_RST
#   north-west to U18, so it crosses their stubs once: an F hop right below the pin row (past U1.91's AGND via), then
#   B west at y 86.3 (north of the audio core A1c and of U1's bulk caps C37 / C38) and up the left grip to U18.8.
# W4 staircase: I2S1 DOUT / BCLK / LRCK and JACK_DET (U1.124-127) west on B to four vias, then F: north to y 66.0 (the
#   U9 branch, group 80 continues into H1) and south to y 78.0. The DAC branch carries the three I2S lines on to the 33R
#   series R34 / R32 / R33 at the DAC; JACK_DET stops at y 78.0 for group 75 (the audio block is routed first there).
W = 0.15

# ---- I2C1
T("I2C1_SCL", "B", W, [(P("U1.3")[0], 68.69), (P("U1.3")[0], 67.9), (P("R30.1")[0], 67.9), P("R30.1")]); VIA("I2C1_SCL", 56.75, 67.9)
T("I2C1_SCL", "F", W, [(56.75, 67.9), (56.75, 63.0)])
T("I2C1_SDA", "B", W, [(P("U1.4")[0], 68.69), (P("U1.4")[0], 67.3), (P("R31.1")[0], 67.3), P("R31.1")]); VIA("I2C1_SDA", 57.5, 67.3)
T("I2C1_SDA", "F", W, [(57.5, 67.3), (57.5, 63.0)])

# ---- W4 staircase (B -> via -> F north and south)
STAIR = (("I2S1_DOUT", 127, (53.25, 70.95)), ("JACK_DET", 126, (54.0, 71.5)), ("I2S1_BCLK", 125, (54.75, 71.9)), ("I2S1_LRCK", 124, (55.5, 72.45)))
for n, pin, (vx, vy) in STAIR:
    y = P("U1.%d" % pin)[1]
    T(n, "B", W, [(56.44, y), (vx, y)] if abs(vy - y) < 1e-6 else [(56.44, y), (vx + abs(vy - y), y), (vx, vy)])
    VIA(n, vx, vy); T(n, "F", W, [(vx, vy), (vx, 78.0)])
    if n != "JACK_DET": T(n, "F", W, [(vx, vy), (vx, 66.0)])

# ---- UART3 W1 escapes
UART = (("UART3_CTS", 121, 51.4, 56.8), ("UART3_TX", 120, 51.0, 57.2), ("UART3_RTS", 119, 50.6, 57.6), ("UART3_RX", 118, 50.2, 58.0))
for n, pin, xr, yw in UART:
    y = P("U1.%d" % pin)[1]; T(n, "B", W, [(56.44, y), (xr, y), (xr, 66.0)])

# ---- BT_EN_RST: the F hop over the DAC_XSMT / HP_EN stubs
# its first via sits between GAUGE_ALRT's and DAC_XSMT's stubs (x 63.9 / 62.7), the second west of U1.91's AGND via
T("BT_EN_RST", "B", W, [(P("U1.84")[0], 84.81), (P("U1.84")[0], 85.7), (63.3, 85.9)]); VIA("BT_EN_RST", 63.3, 85.9)
T("BT_EN_RST", "F", W, [(63.3, 85.9), (61.75, 85.9), (60.9, 86.75), (59.0, 86.75)]); VIA("BT_EN_RST", 59.0, 86.75)

# ---- the long legs, maze-routed (A1c / A1f are group 75's: kept clear)
A1C = ("B", [(35.5, 86.6), (49.6, 86.6), (49.6, 113.8), (35.5, 113.8)])
A1F = ("F", [(40.4, 99.8), (43.3, 99.8), (43.3, 108.9), (40.4, 108.9)])
U18_BODY = ("B", [(30.05, 33.6), (36.45, 33.6), (36.45, 44.9), (30.05, 44.9)])       # inside U18's pad ring
m = MAZE((22.0, 40.0, 66.0, 118.0), keepouts=[A1C, A1F, U18_BODY], limit=3000000)
# CTS / RTS explicit on B: north to the turn, west south of the SD fan (y > 55.2), one NW 45 deg bundle (x - y = -9.4 /
# -10.2) to y 48.6 / 49.4, west, and straight north into the pads; RX and TX then find their F hops (2 via pairs).
T("UART3_CTS", "B", W, [(51.4, 66.0), (51.4, 56.4), (47.0, 56.4), (39.2, 48.6), (P("U18.4")[0], 48.6), P("U18.4")])
T("UART3_RTS", "B", W, [(50.6, 66.0), (50.6, 57.2), (47.0, 57.2), (39.2, 49.4), (P("U18.6")[0], 49.4), P("U18.6")])
m.commit("UART3_CTS", [(51.4, 66.0, "B"), (51.4, 56.4, "B"), (47.0, 56.4, "B"), (39.2, 48.6, "B"), (P("U18.4")[0], 48.6, "B"), (*P("U18.4"), "B")], W)
m.commit("UART3_RTS", [(50.6, 66.0, "B"), (50.6, 57.2, "B"), (47.0, 57.2, "B"), (39.2, 49.4, "B"), (P("U18.6")[0], 49.4, "B"), (*P("U18.6"), "B")], W)
AUTOROUTE(m, "UART3_RX", (50.2, 66.0, "B"), "U18.5", w=W, layers="FB", via_cost=20)
AUTOROUTE(m, "UART3_TX", (51.0, 66.0, "B"), "U18.7", w=W, layers="FB", via_cost=20)
# BT_EN_RST: west north of A1c, up the left edge (x 23.2: between the board edge and H1's keep-out), into U18.8 from the
# west between C72 and C73 (U18.8 is boxed in by U18.9 / U18.30 north and south and the body east)
AUTOROUTE(m, "BT_EN_RST", (59.0, 86.75, "B"), "U18.8", w=W, layers="B",
          through=[(55.0, 86.3, "B"), (30.0, 86.3, "B"), (23.2, 80.0, "B"), (23.2, 50.0, "B"), (24.5, 44.6, "B")])
for n, pin, (vx, vy) in STAIR:                                 # the DAC branch: W4 south ends -> R34 / R32 / R33
    if n == "JACK_DET": continue
    r = {"I2S1_DOUT": "R34.1", "I2S1_BCLK": "R32.1", "I2S1_LRCK": "R33.1"}[n]
    AUTOROUTE(m, n, (vx, 78.0, "F"), r, w=W, layers="FB", via_cost=30)
