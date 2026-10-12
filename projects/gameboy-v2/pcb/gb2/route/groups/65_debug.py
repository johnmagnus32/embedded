# Debug pads: the SWD + console test-pad row along the bottom edge (TP9 MCO, TP11 TX, TP12 RX, TP13 SWDIO, TP10 GND,
# TP14 SWCLK, TP15 +3V3 on a 2.54 mm pitch, back side) and NRST's own pad TP16 east of C6. They replaced the J1/J2
# headers in the right grip (2026-10-12), so none of these lines cross J4 any more.
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
AUTOROUTE(m, "USART1_RX", (*P("U1.43"), "F"), (*P("TP12.1"), "B"), through=[(90.2, 62.65, "B")], via_cost=VC)
AUTOROUTE(m, "SWDIO", (*P("U1.46"), "F"), (*P("TP13.1"), "B"), through=[(94.2, 62.5, "B")], via_cost=VC)
# SWCLK: right-side pin -> via in the east stagger column -> down the back to its pad
(px, py) = P("U1.49")
T("SWCLK", "F", W, [(px, py), (95.2, py)]); VIA("SWCLK", 95.2, py, d=0.45, h=0.25)
AUTOROUTE(m, "SWCLK", (95.2, py, "B"), (*P("TP14.1"), "B"), layers="B")
# NRST: its pin's north lane is closed by R3, so it goes inward under the body to a via, round C6's GND pad to C6
# (the reset filter cap, on the back under the pin), then on to its pad
nv = (88.75, 52.6)
T("NRST", "F", W, [P("U1.7"), nv]); VIA("NRST", *nv, d=0.45, h=0.25)
T("NRST", "B", W, [nv, (89.75, 51.6), (89.75, 50.25), (89.3, 49.8), P("C6.1")])
AUTOROUTE(m, "NRST", (*P("C6.1"), "B"), (*P("TP16.1"), "B"), layers="B")
