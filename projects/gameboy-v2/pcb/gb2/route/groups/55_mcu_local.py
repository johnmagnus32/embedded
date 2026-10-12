# MCU locals: the HSE crystal, the +3V3A filter, VCAP1, BB_PSYNC. Short fixed geometry first (the crystal area
# carries nothing else on the front, and only GND fanout on the back), then the maze for the longer two.
W = 0.15
# ---- crystal (front only). OSC_OUT: pin 6 straight up into R3 (the drive-limiting series R), R3 -> Y1.3 with one 45,
# Y1.3 straight up into C12. OSC_IN: pin 5 drops below Y1's GND pad row, east under it, up into Y1.1, on into C11.
(px, py) = P("U1.6"); T("HSE_OUT_MCU", "F", W, [(px, py), P("R3.1")])
(rx, ry), (yx, yy) = P("R3.2"), P("Y1.3")
T("HSE_OUT", "F", W, [(rx, ry), (rx, yy + (yx - rx)), (yx, yy)])
T("HSE_OUT", "F", W, [(yx, yy), P("C12.1")])
(px, py) = P("U1.5"); (y1x, y1y) = P("Y1.1"); YR = 48.35
T("HSE_IN", "F", W, [(px, py), (px, YR + 0.5), (px + 0.5, YR), (y1x - 0.7, YR), (y1x, YR - 0.7), (y1x, y1y)], miter_=False)
T("HSE_IN", "F", W, [(y1x, y1y), P("C11.1")])
# ---- +3V3A: pin 13 north to its via (the escape plan's stagger column), down the back into C9 (right under the pin),
# then along the filter row C9 -> C10 -> FB1
(px, py) = P("U1.13"); v = (px, 47.95)
T("+3V3A", "F", W, [(px, py), v]); VIA("+3V3A", *v, d=0.45, h=0.25)
T("+3V3A", "B", W, [v, P("C9.1")]); T("+3V3A", "B", 0.3, [P("C9.1"), P("C10.1"), P("FB1.2")])
m = MAZE((78.0, 33.0, 96.0, 61.0))
AUTOROUTE(m, "VCAP1", (*P("U1.30"), "F"), (*P("C7.1"), "B"), w=0.3, via_cost=20.0)
# ---- BB_PSYNC (PC3, pin 11) -> R25 (its pull-down) -> TPS63021 PS/SYNC: north on the front between the GND pin's
# fanout and the charger pair, over to the back (the buck-boost's front is its power pour), up to R25
AUTOROUTE(m, "BB_PSYNC", (*P("U1.11"), "F"), (*P("R25.1"), "F"), via_cost=30.0)
T("BB_PSYNC", "F", W, [P("R25.1"), P("U10.13")])
