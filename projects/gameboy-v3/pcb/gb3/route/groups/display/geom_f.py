# F group (blue + control) and PANEL_DISP.  exec'd after lib.py.
# Blue/ctrl drop to F.Cu at U1 and run as ONE parallel 45-degree bundle (x+y = c_k) straight to the
# rows over P1; each row then turns SE into its pad.  Perp offsets D give 0.40 mm pitch, DCLK 0.50 from
# B7 and 0.61 from HSYNC (the PANEL_DISP via sits in the DCLK/HSYNC pocket at the pads).
FG = ["RGB_B2", "RGB_B3", "RGB_B4", "RGB_B5", "RGB_B6", "RGB_B7", "RGB_DCLK", "RGB_HSYNC", "RGB_VSYNC", "RGB_DE"]
D = dict(zip(FG, [0.0, 0.4, 0.8, 1.2, 1.6, 2.0, 2.5, 3.11, 3.51, 3.91]))
PADX = {"RGB_B2": 92.45, "RGB_B3": 91.95, "RGB_B4": 91.45, "RGB_B5": 90.95, "RGB_B6": 90.45, "RGB_B7": 89.95,
        "RGB_DCLK": 88.95, "RGB_HSYNC": 87.95, "RGB_VSYNC": 87.45, "RGB_DE": 86.95}
Y0, C0 = 60.39, 142.10          # B2 row over P1; B2 diagonal x+y (DE line clears P1 pad 40 + tab 41)
def cF(n): return C0 + R2 * D[n]
def top(n):
    """from the diagonal: NE to the row, east, 45-degree corner, straight south into the pad centre."""
    y = Y0 + D[n]; c = cF(n); p = PADX[n]; r = 0.2 if n == "RGB_DE" else 0.3
    return [(c - y, y), (p - r, y), (p, y + r), (p, 65.0)]

# ---- blue: B.Cu stub straight west from the E-row pin to a staircase via (0.636 x 0.40 steps, each via
# 0.16 below its pin row so the stub above clears it by 0.56; the stub ends inside the via), F.Cu north.
BV = {n: (79.35 + 0.4 * k, 70.96 - 0.636 * (5 - k), 79.51 + 0.4 * k) for k, n in enumerate(FG[:6])}
for n, (py, vx, vy) in BV.items():
    poly(n, BK, [(72.13, py), (vx, py)]); via(n, vx, vy)
    poly(n, F, [(vx, vy), (vx, cF(n) - vx)] + top(n))

# ---- DCLK / DE: vias inside the ring by their S-row pins; DE's F row runs east 0.56 below DCLK's.
n = "RGB_DCLK"
poly(n, BK, [(66.7, 84.38), (66.7, 83.45), (67.61, 82.54)]); via(n, 67.61, 82.54)
XD = 71.57
poly(n, F, [(67.61, 82.54), (XD - 0.3, 82.54), (XD, 82.24), (XD, cF(n) - XD)] + top(n))

# ---- HSYNC / VSYNC: B.Cu straight east into the channel, via, F.Cu north into the bundle
n = "RGB_HSYNC"
poly(n, BK, [(72.13, 78.95), (73.4, 78.95)]); via(n, 73.4, 79.11)
# skew tuning (25 mm rule vs the long red nets): one U trombone per sync line, sized from pass 1
WH = max(0.6, (EXTRA.get(n, 0) + 0.703) / 2) if EXTRA.get(n, 0) > 0 else 0
poly(n, F, [(73.4, 79.11)] + (bump(73.4, 78.3, 77.3, -1, WH) if WH else []) + [(73.4, cF(n) - 73.4)] + top(n))
n = "RGB_VSYNC"
poly(n, BK, [(72.13, 78.55), (74.2, 78.55)]); via(n, 74.2, 78.71)
WV = max(0.6, (EXTRA.get(n, 0) + 0.703) / 2) if EXTRA.get(n, 0) > 0 else 0
poly(n, F, [(74.2, 78.71)] + (bump(74.2, 76.2, 75.2, +1, WV) if WV else []) + [(74.2, cF(n) - 74.2)] + top(n))

# ---- DE: east along 83.1 (inside the ring, south of the blue vias), NE past the channel vias, north
n = "RGB_DE"
poly(n, BK, [(66.3, 84.38), (66.3, 83.2)]); via(n, 66.2, 83.1)
XE, YE = max(74.8, 74.2 + WV + 0.6), 83.1
CE = 154.55                     # NE leg x+y (0.52 perp from DCLK's corner)
poly(n, F, [(66.2, YE), (CE - YE, YE), (XE, CE - XE), (XE, cF(n) - XE)] + top(n))

# ---- PANEL_DISP: B.Cu through the C27/C26 gap, north, NE, east into the pull-up pad R39.1, on south
# out of it to a via, F.Cu straight down into P1.31 between the DCLK and HSYNC drops (a daisy chain).
DVX, DVY = 88.39, 63.45
R39X, R39Y = 88.39, 62.65
poly("PANEL_DISP", BK, [(72.13, 78.15), (78.5, 78.15), (78.5, R39X - 0.9 + R39Y - 78.5), (R39X - 0.9, R39Y), (R39X, R39Y)])
poly("PANEL_DISP", BK, [(DVX, R39Y), (DVX, DVY)])
via("PANEL_DISP", DVX, DVY)
poly("PANEL_DISP", F, [(88.45, DVY), (88.45, 65.0)])
