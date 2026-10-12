# Debug access: the console test-pad row along the bottom edge (TP9 MCO, TP11 TX, TP12 RX, TP10 GND on a 2.54 mm
# pitch, back side; it replaced the J2 header 2026-10-12) and the SWD header J1 on the top edge east of the USB-C.
# The bottom-row lines drop through a via just past their pin tips (the front strip below stays free for E2) and run
# to their pads on the back, in pin order, so nothing crosses.
W = 0.15
m = MAZE((84.0, 47.0, 106.0, 65.5))
VC = 30.0
# MCO2: its pad sits under the pin (0.25 east: the W2b channel's edge)
(px, py) = P("U1.40"); tx, ty = P("TP9.1")
T("MCO2", "F", W, [(px, py), (px, 62.4 - (tx - px)), (tx, 62.4)]); VIA("MCO2", tx, 62.4, d=0.45, h=0.25)
T("MCO2", "B", W, [(tx, 62.4), (tx, ty)])
# TX: via under the pin, one 45 to its pad
(px, py) = P("U1.42"); tx, ty = P("TP11.1")
T("USART1_TX", "F", W, [(px, py), (px, 62.35)]); VIA("USART1_TX", px, 62.35, d=0.45, h=0.25)
T("USART1_TX", "B", W, [(px, 62.35), (tx, ty)])                # 1.54 x 1.55: on the 45
# RX: drops below its pin row, steps east to a via level with TX's (so the I2S band below stays clear from y 62.85),
# then along the back above TX's pad and one 45 down into its own
(px, py) = P("U1.43"); tx, ty = P("TP12.1"); rv = (px + 0.6, 62.35)
T("USART1_RX", "F", W, [(px, py), (px, 62.0), (px + 0.35, 62.35), rv], miter_=False); VIA("USART1_RX", *rv, d=0.45, h=0.25)
T("USART1_RX", "B", W, [rv, (tx - (ty - rv[1]), rv[1]), (tx, ty)], miter_=False)
# console GND pad: the I2S band fills the front under it, so its plane via sits up past SWDIO's via, by the NOR's
# GND pin (fanout can't reach that far)
(gx, gy) = P("TP10.1"); gv = (98.6, 62.4)
T("GND", "B", 0.3, [(gx, gy), (gv[0] - (gy - gv[1]), gy), gv]); VIA("GND", *gv, d=0.45, h=0.25)
# ---- SWD -> J1 (top edge). SWCLK: right-side pin -> via in the east stagger column; NRST: its pin's north lane is
# closed by R3, so it goes inward under the body to a via, round C6's GND pad to C6 (the reset filter cap, on the back
# under the pin). The maze takes all three north to the header.
(px, py) = P("U1.49")
T("SWCLK", "F", W, [(px, py), (95.2, py)]); VIA("SWCLK", 95.2, py, d=0.45, h=0.25)
nv = (88.75, 52.6)
T("NRST", "F", W, [P("U1.7"), nv]); VIA("NRST", *nv, d=0.45, h=0.25)
T("NRST", "B", W, [nv, (89.75, 51.6), (89.75, 50.25), (89.3, 49.8), P("C6.1")])
# North to J1. The BAT trunk (front, y 43.6) sits over the amp's SYS feed (back, y 45.2) west of x 121, with no room
# for a via between them, so the SWD lines come up east of the trunk. SWCLK (lane y 59.26, its pin's row) and SWDIO
# (lane y 60.0) run along the back BELOW every E1 line, so the buttons rise to their J4 slots without crossing them,
# come up through vias south of J8, north on the front through the gap between J8's BAT and GND pads, then
# north-west parallel to the trunk's diagonal (x - y offsets 0.96 / 1.4 clear of it) to pins 2 / 3. NRST runs along
# the back strip between SYS and E1 to a via under pin 5's column.
ms = MAZE((84.0, 33.0, 132.0, 65.5))
COLS = {"SWDIO": ((125.6, 60.0), [(126.05, 59.55)], 126.05, 77.56, "J1.2"),   # (via, jog to the column, column x
        "SWCLK": ((127.2, py), [(127.2, 57.2)], 126.4, 78.0, "J1.3")}          #  in the J8 gap, x - y of the diagonal)
for net, ((vx, vy), jog, cx, c, pin) in COLS.items():
    hx, hy = P(pin)
    if net == "SWCLK": jog = jog + [(cx, 57.2 - (127.2 - cx))]                  # 45 in under J8's GND pad corner
    pts = [(vx, vy)] + jog + [(cx, cx - c), (hx, hx - c), (hx, hy)]
    VIA(net, vx, vy, d=0.45, h=0.25); T(net, "F", W, pts, miter_=False)
    ms.commit(NET(net).GetNetname(), [(vx, vy, "B")] + [(x, y, "F") for x, y in pts], W)
hx, hy = P("J1.5"); NV = (hx, 47.8)
VIA("NRST", *NV, d=0.45, h=0.25); T("NRST", "F", W, [NV, (hx, hy)])
ms.commit(NET("NRST").GetNetname(), [(*NV, "B"), (*NV, "F"), (hx, hy, "F")], W)
AUTOROUTE(ms, "NRST", (*P("C6.1"), "B"), (*NV, "B"), layers="B", through=[(90.0, 48.9, "B"), (95.5, 48.9, "B")])   # clear of the crystal
AUTOROUTE(ms, "SWCLK", (95.2, py, "B"), (*COLS["SWCLK"][0], "B"), layers="B")
AUTOROUTE(ms, "SWDIO", (*P("U1.46"), "F"), (*COLS["SWDIO"][0], "B"), through=[(100.0, 60.0, "B")], via_cost=VC)
