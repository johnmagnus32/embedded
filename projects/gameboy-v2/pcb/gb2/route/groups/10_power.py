# Power chain, top edge: USB-C -> charger -> Q1 soft switch -> SYS -> buck-boost (+3V3) and LCD boost; BAT -> J8.
# Power class: 0.8 mm trunks, necked only where a fine-pitch pin forces it; 0.6/0.3 vias, two per layer change on
# the SYS trunk (~1.5 A peak). Ground returns are the In1 plane (fanned out in a later group), so nothing here
# draws GND copper.
#
# The one crossing: J7's VBUS arrives from the north-west and the charger's OUT leaves to the south-west (Q1), so
# VBUS (<= 0.5 A, ILIM) takes the back and SYS_SRC stays on the front, straight from C37 to Q1.

# ---- VBUS: J7's two VBUS pads bridged on the back (CC1/CC2 drop straight to R15/R16 between them on the front),
# then straight east along the back under SYS_SRC / R19 to a via beside C36 -> U9 IN. TP1 sits on it.
vb = "VBUS"
v0, v1, v2 = (92.5, 40.05), (97.65, 39.95), (106.5, 43.15)
T(vb, "F", 0.4, [P("J7.B4A9"), (92.85, 39.55), (92.5, 39.9), v0])
T(vb, "F", 0.4, [P("J7.A4B9"), v1])
VIA(vb, *v0); VIA(vb, *v1)
T(vb, "B", 0.8, [v0, (97.65, 40.05)])
T(vb, "B", 0.8, [v1, (97.65, 41.4), (98.45, 42.2), (105.55, 42.2), v2])
VIA(vb, *v2)
T(vb, "F", 0.6, [v2, P("C36.1")])
T(vb, "F", 0.3, [P("C36.1"), (106.5, 41.95), (106.75, 41.7), P("U9.13")])   # neck: U9.12/14 are 0.5 mm away

# ---- SYS_SRC (charger OUT): U9.10/11 -> C37.1 -> south-west to Q1 S and R30
ss = "SYS_SRC"
T(ss, "F", 0.75, [P("C37.1"), (105.8, 39.5)])
T(ss, "F", 0.8, [P("C37.1"), (104.37, 40.58), (103.4, 41.55), (100.2, 41.55), (99.7, 42.05), P("Q1.2")])
T(ss, "F", 0.5, [(101.4, 41.55), P("R30.2")])
# EN2's 0R tie (R17.2): over the back from above R17 to a via north of C37.1
e1, e2 = (108.4, 34.3), (104.37, 38.0)
T(ss, "F", 0.3, [P("R17.2"), e1]); VIA(ss, *e1, d=0.45, h=0.25)
T(ss, "B", 0.3, [e1, (104.9, 34.3), (104.37, 34.83), e2]); VIA(ss, *e2, d=0.45, h=0.25)
T(ss, "F", 0.4, [e2, P("C37.1")])

# ---- ILIM: U9.12 -> via -> R19 on the back; R19.2 / C37.2 share one GND via
T("CHG_ILIM", "F", 0.2, [P("U9.12"), (105.6, 40.25), (105.32, 40.53), (105.32, 40.95)]); VIA("CHG_ILIM", 105.32, 40.95, d=0.45, h=0.25)
T("CHG_ILIM", "B", 0.25, [(105.32, 40.95), P("R19.1")])
VIA("GND", 102.37, 40.55)
T("GND", "F", 0.5, [P("C37.2"), (102.37, 40.55)])
T("GND", "B", 0.5, [(102.37, 40.55), (102.62, 40.3), P("R19.2")])

# ---- Q1 gate ring: G - R30 (pull-up to S) - C51 (Miller to D) - R31 (from the power switch)
qg = "/charger/Q1_GATE"
T(qg, "F", 0.3, [P("Q1.1"), P("R30.1"), P("C51.1"), P("R31.2")])

# ---- BAT: U9.2/3 -> C38.1 -> south-east to J8; TP2 by a back stub
bt = "BAT"
T(bt, "F", 0.75, [(109.2, 39.5), P("C38.1")])
T(bt, "F", 0.8, [P("C38.1"), (110.63, 41.2), (113.0, 43.6), (120.2, 43.6), (125.2, 48.6), P("J8.1")])
VIA(bt, 116.0, 43.6)
T(bt, "B", 0.3, [(116.0, 43.6), P("TP2.1")])
# battery-sense tap (uA): from C38.1 up through a via, west along the board's top edge on the back (over EN2's
# tie), then under J7 as the northmost of the charger-signal bundle (route/channels.py "N") and down to R26
bs = (110.63, 38.1)
T(bt, "F", 0.25, [P("C38.1"), bs]); VIA(bt, *bs, d=0.45, h=0.25)
T(bt, "B", 0.15, [bs, (110.63, 34.2), (109.93, 33.5), (104.0, 33.5), (103.4, 34.1), (103.4, 34.9), (87.5, 34.9),
                  (86.6, 35.8), (86.6, 43.5), P("R26.1")])

# ---- SYS: Q1 D -> two vias -> back, past TP3, west under the J7 / crystal gap -> two vias up at C41
sy = "SYS"
s1a, s1b, s2a, s2b = (96.2, 43.4), (96.2, 44.3), (88.6, 37.55), (88.6, 38.45)
T(sy, "F", 0.8, [P("Q1.3"), s1a, s1b])
for v in (s1a, s1b, s2a, s2b): VIA(sy, *v)
T(sy, "B", 0.6, [s1a, (95.5, 42.7), P("TP3.1")])
T(sy, "B", 0.8, [s1b, s1a, (95.6, 42.8), (90.4, 42.8), (88.6, 41.0), s2b, s2a])
T(sy, "F", 0.8, [P("C41.1"), s2a, s2b])
# amp feed (route/channels.py "A"): from Q1's back-side vias east along the back at y 45.2 (between VBUS and the
# east signal channel E1, under the BAT trunk), through J4's pin 19/20 gap (J4's ends are closed by the standoff /
# screw keep-outs; its pins carry nothing; 0.5 mm neck in the 0.94 mm gap), over J1/J2 and down the right edge to
# the amp's bulk cap. C51.2 (Miller cap, drain side) taps it on the way.
c5 = (101.4, 48.15)
T(sy, "B", 0.8, [s1b, (97.1, 45.2), (129.5, 45.2), (132.44, 48.14)])
T(sy, "B", 0.5, [(132.44, 48.14), (134.8, 48.14)])            # neck between J4.19 / J4.20
T(sy, "B", 0.6, [(134.8, 48.14), (137.39, 45.55), (149.0, 45.55), (153.5, 50.05), (153.5, 54.9)])
T(sy, "F", 0.3, [P("C51.2"), c5]); VIA(sy, *c5, d=0.45, h=0.25)
T(sy, "B", 0.3, [c5, (101.4, 45.2)])
for v in ((153.5, 54.05), (153.5, 54.9)): VIA(sy, *v)
T(sy, "F", 0.8, [(153.5, 54.05), (153.5, 54.9), P("C30.1"), (150.8, 54.9), P("C29.1")])
T(sy, "F", 0.5, [P("C29.1"), (150.5, 53.5)])                  # U6.7/8 (PVDD)
VIA(sy, 147.6, 51.75, d=0.45, h=0.25)                           # U6.2 (VDD) sits between I2S SD and GND
T(sy, "F", 0.25, [P("U6.2"), (147.6, 51.75)])
T(sy, "B", 0.3, [(147.6, 51.75), (147.6, 45.55)])          # VDD pin: mA
# buck-boost input: C41.1 = C40.1 -> U10.10/11/12
T(sy, "F", 1.0, [P("C41.1"), P("C40.1")])
T(sy, "F", 0.6, [(85.75, 37.25), (83.65, 37.25)])
T(sy, "F", 0.3, [(83.65, 36.5), (83.65, 37.5)])
# north line along the top edge: C41 -> U10.1 / C39 (VINA) -> LCD boost (L1, C27, U5)
T(sy, "F", 0.8, [P("C41.1"), (87.4, 33.7), (71.4, 33.7), (71.4, 34.2)])
T(sy, "F", 0.3, [P("U10.1"), (80.55, 33.7)])
T(sy, "F", 0.4, [P("C39.1"), (79.25, 33.7)])
T(sy, "F", 0.6, [(71.6, 37.0), (72.7, 38.1), P("C27.1")])
T(sy, "F", 0.5, [(72.7, 39.3), (71.2, 39.3)])
T(sy, "F", 0.4, [(71.2, 39.3), (71.2, 40.25)])

# ---- buck-boost switch nodes: pins -> L2, short and wide
T("/buck-boost/BB_L1", "F", 0.8, [(83.45, 38.25), (83.45, 39.0), (84.05, 39.6), P("L2.1")])
T("/buck-boost/BB_L2", "F", 0.8, [(80.55, 38.25), (80.55, 39.0), (79.95, 39.6), P("L2.2")])

# ---- +3V3 out: U10.4/5 -> C42/C43/C44 bar -> four plane vias (the regulator feeds the whole In2 plane through
# these; two inside TP5); FB 0R from the cap bar
p3 = "+3V3"
T(p3, "F", 0.6, [(80.55, 37.25), (78.25, 37.25)])
T(p3, "F", 1.0, [P("C42.1"), P("C44.1")])
for x in (75.78, 77.2):
    VIA(p3, x, 36.35); T(p3, "F", 0.6, [(x, 36.35), (x, 37.3)])
for x, y in ((74.55, 36.05), (76.5, 35.55)):               # above C44 / C43 (the second in TP5's pad, like 75.78)
    VIA(p3, x, y); T(p3, "F", 0.6, [(x, y), (x, 37.3)])
T(p3, "F", 0.4, [P("R24.2"), (78.0, 37.3)])
T("/buck-boost/BB_FB", "F", 0.25, [P("U10.3"), (79.9, 36.5), P("R24.1")])

# ---- LCD boost: SW node L1 - D3 - U5.1 short and wide; feedback divider tight to U5.3
T("BST_SW", "F", 1.0, [P("L1.2"), P("D3.2")])
T("BST_SW", "F", 0.5, [P("U5.1"), (68.65, 37.0)])
T("BST_FB", "F", 0.25, [P("R9.2"), P("R10.1"), (67.85, 40.25), P("U5.3")])

# ---- +5V_LCD: D3 -> C28 -> west between the top-left screw and standoff keep-outs (0.95 mm apart: the 0.4 mm
# trace keeps >= 0.28 mm off both), down the west side of J3 (the display bus fans out on J3's east side, and the
# left-grip corridor crosses this on the back) to C26 / J3.6 and C25 / J3.2. Sense for R9 off the output cap.
lv = "+5V_LCD"
T(lv, "F", 0.8, [P("D3.1"), P("C28.1")])
t6 = (63.0, 37.9)
T(lv, "F", 0.4, [P("C28.1"), t6]); VIA(lv, *t6, d=0.45, h=0.25)
T(lv, "B", 0.4, [t6, P("TP6.1")])
T(lv, "F", 0.25, [t6, (64.3, 39.2), P("R9.1")])
T(lv, "F", 0.4, [P("C28.1"), (60.65, 38.95), (53.9, 38.95), (53.9, 44.0), (55.0, 45.1), (55.0, 82.43)])   # into C26.1
T(lv, "F", 0.4, [P("C26.1"), P("J3.6")])
T(lv, "F", 0.4, [(55.65, 82.43), (55.65, 92.0), (55.2, 92.45)])                 # C25 / J3.2 branch off the C26-J3.6 link
T(lv, "F", 0.4, [P("C25.1"), P("J3.2")])
