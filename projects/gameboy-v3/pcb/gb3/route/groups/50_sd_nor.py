# 50_sd_nor -- the microSD bus (N1) and the SPI NOR (N2) (step-3 escape + trials s4 sd / nor).
#
# SD (B, 0 signal vias; DRU skew 5 mm, measured 3.77 with every tap): U1.5 SD_CD (PG10, A2) + U1.7-12 north out of the
#   pin row, one planar fan NW (diagonals x - y = c) to y 61.5, then a parallel 45 deg bundle up to y 54.2 and the
#   explicit lanes to the socket (x 48.65-60.45 under D1/D2). With A8 (SP3004 remap) and P4 (the support row) every ESD
#   tap and 10k pull-up is a stub off its own lane. Plane pins: the reserved SD via row at y 50.85 (V13), west of the USB
#   riser, or south where F is free.
# NOR (W25Q128, U3 SOIC-8 rot 90): U1.14-19 straight north to y 62.0, then U3's pin order needs 6 single-layer crossings:
#   CLK and IO0 pass under U3's body to its E column, IO1 / IO2 round the W column's west side, CS straight up; IO3
#   (westmost at U1, E column at U3) and IO0 (west of CS) hop on F south of H1's lanes (DRU <= 2 vias). U3's own plane
#   pins and C7 first.
W = 0.15

# ---- U1 escape: SD fan (N1) and the NOR stubs (N2)
for j, (n, pin) in enumerate((("SD_CD", 5), ("SD_D1", 7), ("SD_D0", 8), ("SD_CLK", 9), ("SD_CMD", 10), ("SD_D3", 11), ("SD_D2", 12))):
    x = P("U1.%d" % pin)[0]; yb = 66.9 - 0.1 * j
    T(n, "B", W, [(x, 68.69), (x, yb), (x - 1.8, yb - 1.8), (x - 1.8, 61.5)])
for pin, n in ((14, "NOR_IO3"), (15, "NOR_IO2"), (16, "NOR_IO1"), (17, "NOR_IO0"), (18, "NOR_CS"), (19, "NOR_CLK")):
    x = P("U1.%d" % pin)[0]; T(n, "B", W, [(x, 68.69), (x, 63.5 if n == "NOR_IO3" else 62.0)])   # IO3 turns NW to its hop

# ---- SD: socket -> lanes -> taps -> plane pins -> the fan into the escape (trial s4 sd)
# ---- lanes, socket -> y 54.2 (W -> E: CD, D1, D0, CLK, CMD, D3, D2)
T("SD_CD",  "B", W, [P("U4.9"), (49.8, 47.8), (48.65, 48.95), (48.65, 54.2)])
T("SD_D1",  "B", W, [P("U4.8"), (50.9, 54.2)])
T("SD_D0",  "B", W, [P("U4.7"), (52.0, 48.9), (51.3, 49.6), (51.3, 54.2)])
T("SD_CLK", "B", W, [P("U4.5"), (54.2, 49.6), (54.5, 49.9), (54.5, 54.2)])
T("SD_CMD", "B", W, [P("U4.3"), (56.4, 48.5), (56.2, 48.7), (56.2, 54.2)])
T("SD_D3",  "B", W, [P("U4.2"), (57.5, 48.9), (58.3, 49.7), (58.3, 54.2)])
T("SD_D2",  "B", W, [P("U4.1"), (58.6, 48.2), (60.45, 50.05), (60.45, 54.2)])
# ---- ESD taps (SP3004 remap A8) and pull-up taps
T("SD_CD",  "B", W, [(48.65, 50.3), P("D1.6")])
T("SD_CD",  "B", W, [(48.65, 48.95), (47.85, 48.95), (47.05, 48.15), P("R48.1")])
T("SD_D1",  "B", W, [(50.9, 51.3), P("D1.4")]);   T("SD_D1",  "B", W, [(50.9, 53.6), P("R8.1")])
T("SD_D0",  "B", W, [(51.3, 51.3), P("D1.3")]);   T("SD_D0",  "B", W, [(51.3, 53.6), P("R7.1")])
T("SD_CLK", "B", W, [(54.5, 50.3), P("D1.1")])
T("SD_CMD", "B", W, [(56.2, 50.3), P("D2.6")]);   T("SD_CMD", "B", W, [(56.2, 53.6), P("R6.1")])
T("SD_D3",  "B", W, [(58.3, 51.3), P("D2.3")]);   T("SD_D3",  "B", W, [(58.3, 52.1), P("R10.1")])
T("SD_D2",  "B", W, [(60.45, 50.3), P("D2.1")]);  T("SD_D2",  "B", W, [(59.7, 49.3), P("R9.1")])
# ---- plane pins -> the SD via row (y 50.85), west of the riser, or south
VIA("+3V3", 49.35, 50.85); T("+3V3", "B", 0.2, [P("D1.5"), (50.3, 50.85), (49.35, 50.85)]); T("+3V3", "B", 0.2, [P("R8.2"), (49.75, 51.25), (49.35, 50.85)])
VIA("GND", 52.85, 50.85);  T("GND", "B", 0.2, [P("D1.2"), (52.2, 50.85), (52.85, 50.85)])
VIA("+3V3", 53.70, 50.85); T("+3V3", "B", 0.2, [P("R7.2"), (52.45, 51.65), (53.2, 51.65), (53.7, 51.15), (53.7, 50.85)])
VIA("+3V3", 55.35, 50.85); T("+3V3", "B", 0.2, [P("U4.4"), (55.3, 48.35), P("C11.1"), (55.35, 49.2), (55.35, 50.85)]); T("+3V3", "B", 0.2, [P("R6.2"), (55.35, 50.85)])
VIA("+3V3", 56.775, 50.85); T("+3V3", "B", 0.2, [P("D2.5"), (57.5, 50.85), (56.775, 50.85)])
VIA("GND", 59.80, 50.85);  T("GND", "B", 0.2, [P("D2.2"), (59.1, 50.85), (59.8, 50.85)])
VIA("+3V3", 62.75, 50.85); T("+3V3", "B", 0.2, [P("R9.2"), (62.75, 50.85)]); T("+3V3", "B", 0.3, [(62.75, 50.85), (62.3, 51.3)])   # C40.1 (pre-fab: was a 0.02 mm corner touch)
VIA("+3V3", 59.35, 54.55); T("+3V3", "B", 0.2, [P("R10.2"), (59.35, 54.55)])
VIA("GND", 61.80, 55.00);  T("GND", "B", 0.2, [P("C40.2"), (61.8, 55.0)])
VIA("+3V3", 44.60, 46.91); T("+3V3", "B", 0.2, [P("R48.2"), (44.6, 46.91)])
T("GND", "B", 0.25, [P("U4.6"), (P("U4.6")[0], P("C11.2")[1]), P("C11.2")])                       # socket GND -> C11's GND pad, and along the socket's
T("GND", "B", 0.25, [P("U4.6"), (53.1, 45.8), (49.4, 45.8)])       # pad row (north side, clear of the prohibition bars) to the shell
# ---- fan: y 54.2 -> the U1 escape stubs (y 61.5): parallel 45 deg diagonals x - y = c, c = -1.40 .. 2.00 in 0.566 steps (0.4 mm
#      pitch), the west three run east first under R8/R7 (north of W5's UART3 at y 56); every lane keeps its order
T("SD_D2",  "B", W, [(60.45, 54.2), (60.45, 58.45), (60.9, 58.9), (60.9, 61.5)])
T("SD_D3",  "B", W, [(58.3, 54.2), (58.3, 56.87), (60.5, 59.07), (60.5, 61.5)])
T("SD_CMD", "B", W, [(56.2, 54.2), (56.2, 55.33), (60.1, 59.23), (60.1, 61.5)])
T("SD_CLK", "B", W, [(54.5, 54.2), (59.7, 59.4), (59.7, 61.5)])
T("SD_D0",  "B", W, [(51.3, 54.2), (51.3, 54.4), (54.13, 54.4), (59.3, 59.57), (59.3, 61.5)])
T("SD_D1",  "B", W, [(50.9, 54.2), (50.9, 54.8), (53.97, 54.8), (58.9, 59.73), (58.9, 61.5)])
T("SD_CD",  "B", W, [(48.65, 54.2), (48.65, 55.2), (53.8, 55.2), (58.1, 59.5), (58.1, 61.5)])

# ---- NOR (trial s4 nor)
KEEP_NOR = [("F", [(58.0, 50.0), (76.0, 50.0), (76.0, 54.3), (58.0, 54.3)]),     # H1's west lanes (I2S / CC_SENSE / I2C1) on F
            ("F", [(71.9, 54.3), (76.0, 54.3), (76.0, 70.0), (71.9, 70.0)]),     # RESET's F riser (x 72.45) / the NE1 side
            ("B", [(57.6, 69.85), (71.4, 69.85), (71.4, 83.65), (57.6, 83.65)])]  # U1's pin ring
T("+3V3", "B", 0.25, [P("U3.8"), (71.0, 57.4), P("C7.1")])
VIA("GND", 71.0, 60.85); T("GND", "B", 0.25, [P("C7.2"), (71.0, 60.85)])
T("GND", "B", 0.25, [P("U3.4"), (62.0, 53.6)])          # into C40.2 (its via at 61.8, 55.0); pre-fab: no via in C40.2's pad
# IO3: westmost at U1, E column at U3 -> one F hop over the five others (its via west of the 0.4-pitch bundle)
T("NOR_IO3", "B", 0.15, [(63.5, 63.5), (62.9, 62.9)]); VIA("NOR_IO3", 62.9, 62.9)
T("NOR_IO3", "F", 0.15, [(62.9, 62.9), (69.2, 56.6)]); VIA("NOR_IO3", 69.2, 56.6)
T("NOR_IO3", "B", 0.15, [(69.2, 56.6), (69.67, 56.13), P("U3.7")])
m = MAZE((58.0, 50.0, 76.0, 70.0), keepouts=KEEP_NOR)

AUTOROUTE(m, "NOR_CLK", (65.5, 62.0, "B"), "U3.6", w=0.15, layers="B")
AUTOROUTE(m, "NOR_CS",  (65.1, 62.0, "B"), "U3.1", w=0.15, layers="B")
AUTOROUTE(m, "NOR_IO1", (64.3, 62.0, "B"), "U3.2", w=0.15, layers="B")
AUTOROUTE(m, "NOR_IO2", (63.9, 62.0, "B"), "U3.3", w=0.15, layers="B")
AUTOROUTE(m, "NOR_IO0", (64.7, 62.0, "B"), "U3.5", w=0.15, layers="FB", via_cost=30)
