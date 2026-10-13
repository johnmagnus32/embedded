# B.Cu group (green + red).  exec'd after lib.py.
# Escape U1 on B.Cu, one NE bundle (x+y = 158.7 + 0.566k, 0.40 pitch) under P1, then per colour a via staircase just
# north of the pads: each via sits 0.06 mm off its pad line (so every straight F stub clears the
# neighbouring via by 0.56 mm) and the F stub drops straight into the pad centre.
esc = {}
for n, py in [("RGB_G2", 81.75), ("RGB_G3", 82.15), ("RGB_G4", 82.55), ("RGB_G5", 82.95)]:
    esc[n] = [(72.13, py)]
LN = [85.365, 85.72, 86.075, 86.43]          # S-row channel lanes (east), pad tips 85.11 .. cap pads 86.70
for i, (n, px) in enumerate([("RGB_G6", 69.9), ("RGB_G7", 69.5), ("RGB_R3", 69.1), ("RGB_R2", 68.7)]):
    u = 0.2 + 0.25 * i
    esc[n] = [(px, 84.38), (px, LN[i] - u), (px + u, LN[i])]
# R4/R5 down the C30/C18 gap, R6/R7 down the C12/C30 gap (below); all four return east UNDER the caps
# (cap pads end at y 88.90), never between the two pads of a cap.
esc["RGB_R4"] = [(68.3, 84.38), (68.3, 86.23), (68.1, 86.43), (67.49, 86.43), (67.37, 86.55), (67.37, 89.0), (67.52, 89.15)]
esc["RGB_R5"] = [(67.9, 84.38), (67.9, 85.875), (67.7, 86.075), (67.21, 86.075), (67.01, 86.275), (67.01, 89.2), (67.36, 89.55)]
# R6/R7 dive 45-degrees SW (parallel to R5's corner at P pitch) to lanes Y7/Y6 that pass UNDER a 0.6 mm via
# straight south of U1.77 at (65.90, 85.48) (its +3V3 escape; 0.58 above R7) and over C30.1, then turn
# south into the C12/C30 gap.
P = 0.355
S6 = 67.21 + 86.075 - P * R2; S7 = S6 - P * R2      # x+y of the R6 / R7 diagonals
Y7 = 86.06; Y6 = Y7 + P
C6 = 0.18; C7 = C6 + 0.36 + P - P * R2               # corner chamfers, uniform pitch round the corner
esc["RGB_R6"] = [(67.5, 84.38), (67.5, S6 - 67.5), (S6 - Y6, Y6), (65.58 + C6, Y6), (65.58, Y6 + C6), (65.58, 89.16), (66.37, 89.95)]
esc["RGB_R7"] = [(67.1, 84.38), (67.1, S7 - 67.1), (S7 - Y7, Y7), (65.22 + C7, Y7), (65.22, Y7 + C7), (65.22, 89.35), (66.22, 90.35)]

BG = ["RGB_G2", "RGB_G3", "RGB_G4", "RGB_G5", "RGB_G6", "RGB_G7", "RGB_R3", "RGB_R2", "RGB_R4", "RGB_R5", "RGB_R6", "RGB_R7"]
GP = {"RGB_G2": 96.45, "RGB_G3": 95.95, "RGB_G4": 95.45, "RGB_G5": 94.95, "RGB_G6": 94.45, "RGB_G7": 93.95}
RP = {"RGB_R3": 99.95, "RGB_R2": 100.45, "RGB_R4": 99.45, "RGB_R5": 98.95, "RGB_R6": 98.45, "RGB_R7": 97.95}
OFF = 0.06                                   # via offset from the pad line
BPITCH = 0.40                                # bundle pitch (perpendicular) on the diagonal and vertical legs
GX0, GH0 = 91.73, 61.40                      # G2's vertical x, G2's via row (staircase +0.56 per step down)
RX0, RC0, RR0 = 94.30, 159.25, 61.40         # R3's vertical x, R3's upper diagonal x+y, R3's via row

for k, n in enumerate(BG):
    c = 158.7 + BPITCH * R2 * k
    e = esc[n]; y = e[-1][1]
    pts = e + [(c - y, y)]
    if k < 6:   # green: north on B.Cu, apex east on B.Cu to the via, F stub straight down
        X = GX0 + BPITCH * k
        h = GH0 + 0.56 * k
        p = GP[n]; v = p - OFF
        ch = min(0.3, v - X)
        pts += [(X, c - X), (X, h + ch), (X + ch, h)]
        if X + ch < v - 1e-6: pts.append((v, h))
        poly(n, BK, pts); via(n, v, h)
        poly(n, F, [(p, h), (p, 65.0)])          # starts inside the via, 0.06 off its centre
    else:       # red: north (shifts the bundle NW), NE, east on B.Cu over the lower vias, via, F stub
        j = k - 6
        X = RX0 + BPITCH * j
        cu = RC0 + 0.5 * j
        rr = RR0 + 0.56 * j
        p = RP[n]; v = p + OFF if n == "RGB_R2" else p - OFF
        pts += [(X, c - X), (X, cu - X), (cu - rr, rr), (v, rr)]
        poly(n, BK, pts); via(n, v, rr)
        poly(n, F, [(p, rr), (p, 65.0)])
