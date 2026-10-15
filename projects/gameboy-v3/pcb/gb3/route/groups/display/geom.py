"""geom.py - gb3 display bus geometry (route group 40), pad-relative. Pure python: build(P) -> segs, vias, info.

U1 (T113-S3, B.Cu) -> P1 (display FPC, F.Cu). The 22 bus nets enter P1's pads from the SOUTH, under the connector
body (the C9160 drawing marks no "no pattern" area there); P1's north side is left to the backlight feed and the
plane pads. Topology = a 90-degree crossbar, one via per net (U1 and P1 are on opposite sides, so one via is the
floor anyway):

  B rows  east, PB pitch, in U1 pin order N->S: DISP VSYNC HSYNC B2..B7 G2..G5 (E column 52-64, fanned out from
          0.4 mm), then the S-row pins 67-74 wrapped round U1's SE corner: G6 G7 R3 R2 R4..R7.
  F cols  north, >= PF pitch, in P1 pad order W->E: R2..R7 G2..G7 B2..B7 DCLK DISP HSYNC VSYNC DE.
  via of a net = its row x its column: a block staircase (red bottom-left, C1 top-right), so every one of the 167
          single-layer crossings costs nothing beyond the via each net needs anyway.
  North of the field the columns turn NE (parallel 45-degree jog; the solver below picks each turn point) and drop
          straight into their pads.
  DCLK (R50 on F) and DE (via at its own pin) cross nothing else: they run on F round the outside (SE) of the
          staircase, so RGB_DCLK has 0 vias and In1 GND under it all the way; DCLK keeps 0.6 mm c-c (3 W) to B7.

All positions derive from pads (P("U1.52") ...) and the constants below. Board mm, y down.
"""
import math

R2 = math.sqrt(2.0)
W = 0.20                 # track width, all bus nets (Default netclass)
VIA_D, VIA_H = 0.60, 0.30
CLR = 0.15               # Default clearance
PB = 0.60                # B row pitch in the via field
PF = 0.60                # F column pitch in the via field
GAP_DCLK = 1.20          # min column gap B7 -> DCLK
GAP_RG = 2.40            # min column gap R7 -> G2 (lengthens the shortest nets G2/G3: +1.0 mm skew margin)
S_JOG = 0.40             # jog diagonals: c-c spacing between neighbours (min 0.35 = W + CLR)
S_DCLK = 0.60            # ... and between B7 and DCLK (3 W; DCLK-DISP keeps S_JOG: DISP is static)
YE_MIN = 66.40           # lowest end point of a jog diagonal (pad south edge 66.17; 0.27 mm to the next pad)
XMIN_S = 73.40           # S-row (red/G6/G7) vias + F columns: east of the F decap island (C35/C36 end at x 72.98)
XMIN_E = 76.60           # E-column vias: east of the B fan-out jogs (they end at x <= 75.6)
XS_SOUTH = 73.20         # x where the south-most E-column lane starts its fan-out jog (pad toes at 72.86)
XS_STEP = 0.15           # each lane further north starts its jog this much further east
WRAP_K = 0.40            # chamfer leg of the S-row south->east corners
VIA_TRACK = VIA_D / 2 + CLR + W / 2 + 0.02   # via-centre to track-centre, 0.02 mm over the 0.55 minimum

E_NETS = ["PANEL_DISP", "RGB_VSYNC", "RGB_HSYNC", "RGB_B2", "RGB_B3", "RGB_B4", "RGB_B5", "RGB_B6", "RGB_B7",
          "RGB_G2", "RGB_G3", "RGB_G4", "RGB_G5"]                    # U1.52 .. U1.64 (N -> S)
S_NETS = ["RGB_G6", "RGB_G7", "RGB_R3", "RGB_R2", "RGB_R4", "RGB_R5", "RGB_R6", "RGB_R7"]  # U1.67 .. U1.74 (E -> W)
P_ORDER = ["RGB_R2", "RGB_R3", "RGB_R4", "RGB_R5", "RGB_R6", "RGB_R7",
           "RGB_G2", "RGB_G3", "RGB_G4", "RGB_G5", "RGB_G6", "RGB_G7",
           "RGB_B2", "RGB_B3", "RGB_B4", "RGB_B5", "RGB_B6", "RGB_B7",
           "RGB_DCLK", "PANEL_DISP", "RGB_HSYNC", "RGB_VSYNC", "RGB_DE"]
P_PAD = {n: "P1.%d" % p for n, p in zip(P_ORDER, [7, 8, 9, 10, 11, 12, 15, 16, 17, 18, 19, 20,
                                                   23, 24, 25, 26, 27, 28, 30, 31, 32, 33, 34])}
U_PIN = {n: "U1.%d" % (52 + i) for i, n in enumerate(E_NETS)}
U_PIN.update({n: "U1.%d" % (67 + j) for j, n in enumerate(S_NETS)})
U_PIN["RGB_DE"] = "U1.76"
U_PIN["LCD0_CLK"] = "U1.75"
COLUMNED = [n for n in P_ORDER if n != "RGB_DE"]   # DE runs one straight diagonal outside everything


def plen(pts):
    return sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(pts, pts[1:]))


def clean(pts):
    out = [pts[0]]
    for p in pts[1:]:
        if math.hypot(p[0] - out[-1][0], p[1] - out[-1][1]) > 1e-6: out.append(p)
    res = [out[0]]
    for i in range(1, len(out) - 1):
        a, b, c = res[-1], out[i], out[i + 1]
        if abs((b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0])) < 1e-9: continue
        res.append(b)
    res.append(out[-1])
    return res


def build(P, r39=None):
    """P(key) -> (x, y). r39 = (x, y) of R39.1 if the pull-up sits at the DISP via (B stub added).
    Returns {'segs': [(net, layer, pts, w)], 'vias': [(net, x, y, d, h)], 'info': {...}}."""
    segs, vias, info = [], [], {}

    def seg(net, layer, pts, w=W):
        segs.append((net, layer, clean(pts), w))

    def via(net, x, y):
        vias.append((net, x, y, VIA_D, VIA_H))

    # ------------------------------------------------------------------ rows (B lanes)
    Y0 = P(U_PIN["PANEL_DISP"])[1]                    # row 0 = DISP's own pin line
    row = {}
    for i, n in enumerate(E_NETS): row[n] = Y0 + PB * i
    for j, n in enumerate(S_NETS): row[n] = Y0 + PB * (len(E_NETS) + j)
    pad = {n: P(P_PAD[n]) for n in P_ORDER}

    # ------------------------------------------------------------------ columns + jog (solved east -> west)
    cols = COLUMNED
    K = len(cols)
    pitch = {n: PF for n in cols}
    pitch["RGB_DCLK"] = GAP_DCLK
    pitch["RGB_G2"] = GAP_RG
    spc = {n: S_JOG for n in cols}                    # spc[n] = spacing between n and its east neighbour
    spc["RGB_B7"] = S_DCLK

    def solve(xs, yentry):
        vy = [row[n] if n in row else yentry for n in cols]
        ya = [None] * K
        for k in range(K - 1, -1, -1):
            lim = vy[k]
            if k + 1 < K:
                g = xs[k + 1] - xs[k]
                lim = min(lim, ya[k + 1] + g - spc[cols[k]] * R2)        # parallel to the east neighbour
                if vy[k + 1] < vy[k]:                                     # passes the east neighbour's via
                    lim = min(lim, vy[k + 1] + g - VIA_TRACK * R2)
            ya[k] = lim
        return ya

    xs = [pad[n][0] - 10.5 for n in cols]
    yentry = 82.0
    kd = cols.index("RGB_DCLK")
    for _ in range(60):
        ya = solve(xs, yentry)
        nx = []
        for k, n in enumerate(cols):
            x = pad[n][0] - (ya[k] - YE_MIN)
            x = max(x, XMIN_E if n in E_NETS else XMIN_S)
            if k: x = max(x, nx[-1] + pitch[n])
            nx.append(x)
        cstair = max(nx[k] + row[n] for k, n in enumerate(cols[:kd]))
        cD = cstair + VIA_TRACK * R2 + 0.01           # DCLK's diagonal passes SE of every staircase via
        ne = cD - nx[kd]
        done = max(abs(a - b) for a, b in zip(nx, xs)) < 1e-7 and abs(ne - yentry) < 1e-7
        xs, yentry = nx, ne
        if done: break
    ya = solve(xs, yentry)
    col = dict(zip(cols, xs))
    jog = dict(zip(cols, ya))
    info.update(col=col, jog=jog, row=row, cD=cD, entry=yentry)

    # ------------------------------------------------------------------ B lanes: E column fan-out
    nE = len(E_NETS)
    for i, n in enumerate(E_NETS):
        px, py = P(U_PIN[n])
        d = row[n] - py
        xs_i = XS_SOUTH + XS_STEP * (nE - 1 - i)       # south lanes jog first
        pts = [(px, py)]
        if d > 1e-6: pts += [(xs_i, py), (xs_i + d, py + d)]
        pts += [(col[n], row[n])]
        if n == "PANEL_DISP" and r39 is not None:     # on to the pull-up R39 beside the via
            vx, vy_ = col[n], row[n]
            dy = vy_ - r39[1]
            pts += [(vx + dy, r39[1]), r39]
        seg(n, "B", pts)
        via(n, col[n], row[n])
    # ------------------------------------------------------------------ B lanes: S row wrap
    for j, n in enumerate(S_NETS):
        px, py = P(U_PIN[n])
        y = row[n]
        seg(n, "B", [(px, py), (px, y - WRAP_K), (px + WRAP_K, y), (col[n], y)])
        via(n, col[n], y)

    # ------------------------------------------------------------------ F lanes: column -> jog -> pad
    def f_tail(n, start):
        x, a = col[n], jog[n]
        px, py = pad[n]
        return [(x, start), (x, a), (px, a - (px - x)), (px, py)]

    for n in cols:
        if n == "RGB_DCLK": continue
        seg(n, "F", f_tail(n, row[n]))

    # ------------------------------------------------------------------ DCLK: U1.75 -> via (in the ring) -> R50 (F) -> P1.30
    u75 = P(U_PIN["LCD0_CLK"])
    r1, r2 = P("R50.1"), P("R50.2")
    vlcd = (u75[0], u75[1] - 0.73 - 0.625)            # island via plan (viafit2): 0.62 mm inside the pad heel
    seg("LCD0_CLK", "B", [u75, vlcd])
    via("LCD0_CLK", *vlcd)
    seg("LCD0_CLK", "F", [vlcd, r1])
    yD = row["RGB_R7"] + VIA_TRACK + 0.01            # under the lowest (R7) via
    xD0 = r2[0] + 0.80                                 # its southbound leg, east of DE's
    xDe = col["RGB_DCLK"]
    dclk = [r2, (xD0, r2[1] + 0.80), (xD0, yD - 0.6), (xD0 + 0.6, yD), (cD - yD, yD), (xDe, cD - xDe)]
    dclk += f_tail("RGB_DCLK", cD - xDe)[1:]
    seg("RGB_DCLK", "F", dclk)
    # ------------------------------------------------------------------ DE: U1.76 -> via straight south -> F outside DCLK
    u76 = P(U_PIN["RGB_DE"])
    vde = (u76[0], max(u76[1] + 1.92, r2[1] + 0.432 + VIA_D / 2 + CLR + 0.03))
    seg("RGB_DE", "B", [u76, vde])
    via("RGB_DE", *vde)
    yE = yD + 0.60
    # its diagonal: SE of DCLK's by 0.6 and SE of the C1 jog diagonals by S_JOG
    cC1 = max(col[n] + jog[n] for n in ("PANEL_DISP", "RGB_HSYNC", "RGB_VSYNC"))
    cE = max(cD + 0.60 * R2, cC1 + S_JOG * R2 + 0.3)
    ped = pad["RGB_DE"]
    de = [vde, (vde[0], yE - 0.6), (vde[0] + 0.6, yE), (cE - yE, yE), (ped[0], cE - ped[0]), ped]
    seg("RGB_DE", "F", de)
    info["cE"] = cE

    info["len"] = {}
    for net, layer, pts, w in segs:
        info["len"][net] = info["len"].get(net, 0.0) + plen(pts)
    return {"segs": segs, "vias": vias, "info": info}


# ====================================================================== P1 north side
W_BL = 0.25              # backlight feed (40 mA, 18-19 V; 38 V if the string opens)
Y_GV = 63.75             # plane-via row north of P1's pad tips (pad tips at 64.57)


def build_backlight(P):
    """BL_LED_P: C45.1 -> TP11 -> via -> P1.2 (from the north). BL_LED_N: U6.6 (FB, R38.1 beside it) -> via -> P1.1.
    Both on B north of P1 (y 61.0 / 62.5), short F stubs into the pad tips. TP11 is a bare pad on the LED anode (up to
    the 37-39 V OVP with the string open): everything else keeps 0.6 mm from it (step-3 integration)."""
    segs, vias = [], []
    c45, tp, p2 = P("C45.1"), P("TP11.1"), P("P1.2")
    u6, p1 = P("U6.6"), P("P1.1")
    yP = tp[1]                                         # BL_LED_P runs through TP11's centre
    vP = (p2[0], yP)
    segs.append(("BL_LED_P", "B", clean([c45, (c45[0] - (yP - c45[1]), yP), tp, vP]), W_BL))
    vias.append(("BL_LED_P", vP[0], vP[1], VIA_D, VIA_H))
    segs.append(("BL_LED_P", "F", clean([vP, p2]), W_BL))
    yN = yP - 1.5                                      # 0.625 mm from TP11's bare pad (DRU "TP11 isolation", IPC-2221B B2)
    xN = 97.0 if abs(u6[0] - 102.35) < 3 else u6[0] - 5.35
    vN = (p1[0] - 0.5, yP + 0.8)
    segs.append(("BL_LED_N", "B", clean([u6, (xN, u6[1]), (xN, yN), (vN[0], yN), vN]), W_BL))
    vias.append(("BL_LED_N", vN[0], vN[1], VIA_D, VIA_H))
    segs.append(("BL_LED_N", "F", clean([vN, (p1[0], vN[1] + 0.5), p1]), W_BL))
    return {"segs": segs, "vias": vias}


def build_planes_preview(P, r39=True):
    """Where the step-4 fanout puts P1's plane vias (and R39.2's): proves they fit next to the bus and the backlight
    feed. GND pads 5/6, 13/14, 21/22, 29, 36 north (one via per pair); 3 south under the body (its north side is
    boxed in by BL_LED_P and the +3V3 stub); +3V3 pad 4 north."""
    segs, vias = [], []
    def gv(net, pads_, x, y):
        vias.append((net, x, y, VIA_D, VIA_H))
        for k in pads_:
            px, py = P("P1.%d" % k)
            tip = py - 0.8 if y < py else py + 0.8
            pts = [(x, y), (px, y + (0.5 if y < py else -0.5)) if abs(px - x) > 1e-6 else (px, y), (px, tip), (px, py)]
            segs.append((net, "F", clean(pts), 0.20))
    gv("+3V3", [4], P("P1.4")[0], Y_GV)
    gv("GND", [5, 6], P("P1.6")[0], Y_GV)
    gv("GND", [13, 14], (P("P1.13")[0] + P("P1.14")[0]) / 2, Y_GV)
    gv("GND", [21, 22], (P("P1.21")[0] + P("P1.22")[0]) / 2, Y_GV + 0.25)   # 0.65 mm from TP11 (at Y_GV it was 0.43)
    gv("GND", [29], P("P1.29")[0], Y_GV)
    gv("GND", [36], P("P1.36")[0], Y_GV)
    gv("GND", [3], P("P1.3")[0], P("P1.3")[1] + 1.58)
    if r39:
        x2, y2 = P("R39.2")
        vx, vy = x2 + 0.75, y2 - 0.75                   # NE of pad 2: in the 2 mm band between VSYNC's and DE's jogs
        vias.append(("+3V3", vx, vy, VIA_D, VIA_H))
        segs.append(("+3V3", "B", [(x2, y2), (vx, vy)], 0.25))
    return {"segs": segs, "vias": vias}
