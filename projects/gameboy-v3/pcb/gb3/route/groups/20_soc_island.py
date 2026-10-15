# 20_soc_island -- U1's inside: every supply / analog pin to its via and its decap on F (step-2 viafit2 plan, the
# t113-breakout way: decaps on F directly under U1, the B ring left to the signal fan-out), the exposed pad's 4 x 4 GND
# vias, the In2 +0V9 island (Z2), the LDOA ring, and the small locals that sit inside U1's pin ring.
#
# Data: groups/soc/island.py (generated from ~/.cache/gb3-step2p/final/iter/final/viafit2_* + the step-3 escape: the
# RESET via 0.5/0.3 moved 0.05 north so its In2 antipad clears HOSC_IN, RTC_IN entering X2.1 from below at x 68.15).
# The plan's straight stubs that were off the 45 deg grid are redrawn on it, with their vias re-fitted (moved
# <= 0.33 mm) and U1.28 sharing U1.26's LDOA via.
#   LDOB lane N5 (F, x 70.125): U1.30's via -> U1.48's via, the LDOB pins tied on F between the decap columns.
#   LDOA ring (B): the T113's LDOA pins (20, 26/28, 50, 65, 107) each have a via and a decap; the ring ties the five
#     clusters on B inside the via ring (PLAN Z2), north C23 -> C42, east down to U1.50, south under the EP to U1.107,
#     and U1.65's via at the SE corner. LDOA is the 1.8 V internal regulator's output (VCC-PLL / VCC-RTC side): a few mA.
#   Z2: In2 +0V9 island under the core (58.6-70.6 x 73.6-82.5 + fingers) on all five +0V9 vias; U1's +3V3 vias land on
#     the +3V3 plane around it.
import importlib.util, os
_spec = importlib.util.spec_from_file_location("soc_island", os.path.join(os.path.dirname(os.path.abspath(__file__)), "soc", "island.py"))
D = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(D)
POWER = {"+3V3", "AVCC", "HPVCC", "+0V9", "LDOA", "LDOB", "VRA1", "VRA2", "GND"}
W = lambda net: 0.2 if net in POWER else 0.15

for x, y, net, pin, d in D.VIAS: VIA(net, x, y, d=d)
for net, layer, pts in D.STUBS: T(net, layer, W(net), pts)

# ---- exposed pad: 4 x 4 GND vias at 1.1 mm (U1 EP 5.5 x 5.5)
for x in (62.85, 63.95, 65.05, 66.15):
    for y in (75.10, 76.20, 77.30, 78.40): VIA("GND", x, y)

# ---- N5: the LDOB lane on F between the decap columns, U1.30's via -> U1.48's via
v30, v48 = (70.1, 70.75), (70.325, 76.55)
T("LDOB", "F", 0.2, [v30, (70.125, v30[1] + 0.3), (70.125, v48[1]), v48])

# ---- decap GND vias inside the pin ring (C9, C10, C17): F stub from the GND pad to its via
for (x, y), c in (((70.90, 74.35), "C9.2"), ((70.75, 81.10), "C17.2")):
    VIA("GND", x, y); T("GND", "F", 0.2, [P(c), (x, y)])
VIA("GND", 72.275, 69.70)                             # C10's (its stub is in soc/island.py: re-fitted onto the 45 deg grid)

# ---- locals on U1's east and south sides
T("/soc/DDR_ZQ", "B", 0.15, [P("U1.47"), (P("R5.1")[0] - 0.3, P("U1.47")[1]), P("R5.1")])      # 240R ZQ, B 0.7 mm east
VIA("GND", P("R5.2")[0] + 0.85, P("R5.2")[1]); T("GND", "B", 0.2, [P("R5.2"), (P("R5.2")[0] + 0.85, P("R5.2")[1])])
T("LDOB", "B", 0.2, [P("U1.49"), (P("TP10.1")[0], P("U1.49")[1])])                                 # TP10 (LDOB test pad)
T("GND", "B", 0.2, [P("U1.91"), (P("U1.91")[0], 85.85)]); VIA("GND", P("U1.91")[0], 85.85)          # AGND: inward is the VRA vias
T("LDOA", "B", 0.2, [P("U1.107"), (P("TP9.1")[0], P("U1.107")[1])])                                # TP9 (LDOA test pad)

# ---- the three bulk caps beside the decap rows (F): LDOB C35 (U1.49's via), +0V9 C39 (U1.81's), AVCC C92 (U1.89's)
T("LDOB", "F", 0.2, [(70.875, 76.75), (71.125, P("C35.1")[1]), P("C35.1")])
T("+0V9", "F", 0.2, [(64.3, 83.25), (64.3, 83.7), (P("C39.1")[0], 84.4)])
T("AVCC", "F", 0.2, [(61.325, 82.725), (61.325, 84.0)])

# ---- Z2: the In2 +0V9 island under the core
ZONE("+0V9", "In2.Cu", [(58.6, 73.6), (70.6, 73.6), (70.6, 77.6), (71.4, 77.6), (71.4, 78.6), (70.6, 78.6), (70.6, 82.5),
                        (64.65, 82.5), (64.65, 83.65), (63.95, 83.65), (63.95, 82.5), (58.6, 82.5), (58.6, 75.9), (57.7, 75.9),
                        (57.7, 74.5), (58.6, 74.5)], "In2 +0V9 island: U1 core (balls 46/51/81/116/117)")

# ---- LDOA ring (B, inside the via ring): the five LDOA clusters, maze-routed on B only
m = MAZE((55.5, 67.0, 74.0, 86.5))
LDOA = {20: (65.9, 70.25), 26: (68.6, 71.0), 50: (70.175, 78.025), 65: (71.15, 83.45), 107: (58.0, 78.95)}
for a, b in ((20, 26), (26, 50), (50, 65), (50, 107)):
    AUTOROUTE(m, "LDOA", (*LDOA[a], "B"), (*LDOA[b], "B"), w=0.2, layers="B")
