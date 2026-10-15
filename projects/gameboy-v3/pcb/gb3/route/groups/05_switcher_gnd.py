# 05_switcher_gnd -- the plane connections every later group must route around (step-3 fix: these used to come after
# group 10, so the trunks were drawn over them). Runs FIRST. route/channels.py VIA_SPOTS reserves every via below.
#
# 1. In2 GND islands under the back-side switch nodes and the 24 MHz crystal (Z1). B.Cu's neighbour plane is In2 (+3V3),
#    so a back-side switch node couples into +3V3; TPS61165 p.21 "always use a ground plane under the switching
#    regulator", FP6161 checklist #4/#5. Each island keeps its fill only through the GND vias below + pour.py's island
#    stitching. X1's island runs 0.21 mm past U1's pin tips (y 68.6): beyond the tips the HOSC lines are over GND at all
#    288 samples (refcheck on fix/trial/s5).
# 2. Exposed-pad via grids, 0.6/0.3 (U1's 4 x 4 is group 20's): U12 TPS63021 2 x 4 + V1 (PGND, DS Fig 31), U10 BQ24073 5
#    (TI land pattern note 5: vias under paste plugged or tented), U13 MAX17048 2, U9 MAX98357A 2 x 2, U20 TPA6132A2 2 x 2.
# 3. Switcher capacitor GND vias (two per CIN/COUT GND pad where they fit) and U12's COUT +3V3 vias (4, all at y <= 37.0
#    so +5V's F jog at y 38.05-38.85 clears them), the charger/gauge/INA226 decap and programming-resistor GND vias.
# Geometry: pad-relative where a part owns the via (P()), so small moves carry the vias.
def at(key, dx, dy): x, y = P(key); return (x + dx, y + dy)
def V(net, xy, d=0.6, h=0.3): VIA(net, xy[0], xy[1], d=d, h=h)

# ---- 1. In2 GND islands (priority 2 over the +3V3 plane)
ZONE("GND", "In2.Cu", [(39.6, 69.3), (49.3, 69.3), (49.3, 79.55), (39.6, 79.55)], "In2 GND island: U2 core buck")
ZONE("GND", "In2.Cu", [(101.4, 49.4), (111.0, 49.4), (111.0, 63.2), (101.4, 63.2)], "In2 GND island: U6 backlight boost")
ZONE("GND", "In2.Cu", [(136.2, 37.3), (146.6, 37.3), (146.6, 46.6), (136.2, 46.6)], "In2 GND island: U12 buck-boost")
ZONE("GND", "In2.Cu", [(66.2, 62.6), (71.4, 62.6), (71.4, 68.6), (66.2, 68.6)], "In2 GND island: X1 24 MHz (past U1's pin tips)")
ZONE("GND", "In2.Cu", [(112.0, 38.0), (124.0, 38.0), (124.0, 48.0), (112.0, 48.0)], "In2 GND island: USB-C / D4 (optional)")

# ---- 2. exposed pads
ep = P("U12.15")                                                   # TPS63021 EP 4.4 x 1.58: 2 x 4 at 1.0 / 0.8 mm
for dx in (-1.5, -0.5, 0.5, 1.5):
    for dy in (-0.4, 0.4): V("GND", (ep[0] + dx, ep[1] + dy))
T("GND", "B", 0.8, [(ep[0] + 2.2, ep[1]), (ep[0] + 3.85, ep[1])])    # EP -> V1 PGND strap (U12-L3 gap; 0.31 to L3.2)
for dx, dy in ((3.0, -0.4), (3.0, 0.4), (3.85, 0.0)): V("GND", (ep[0] + dx, ep[1] + dy))   # V1
T("GND", "B", 0.24, [P("U12.2"), (P("U12.2")[0], ep[1] - 0.6)])     # GND (control) -> PGND at one point, by the GND pin
ep = P("U10.17")                                                   # BQ24073 EP 1.7 x 1.7: 5 (centre + 4 at +-0.5)
for dx, dy in ((0, 0), (-0.5, -0.5), (0.5, -0.5), (-0.5, 0.5), (0.5, 0.5)): V("GND", (ep[0] + dx, ep[1] + dy))
for pin, (dx, dy) in (("4", (0.55, 0)), ("6", (0, 0.55)), ("8", (0, 0.55)), ("15", (0, -0.55))):   # GND pins into the EP
    T("GND", "B", 0.24, [P("U10." + pin), (P("U10." + pin)[0] + dx, P("U10." + pin)[1] + dy)])
ep = P("U13.9")                                                    # MAX17048 EP 0.8 x 1.2: 2
for dy in (-0.3, 0.3): V("GND", (ep[0], ep[1] + dy))
for key in ("U9.17", "U20.17"):                                    # MAX98357A / TPA6132A2 EP: 2 x 2 at 0.7 mm
    ep = P(key)
    for dx in (-0.35, 0.35):
        for dy in (-0.35, 0.35): V("GND", (ep[0] + dx, ep[1] + dy))

# ---- 3. switcher caps and the strip's small GND / plane vias
T("GND", "B", 1.0, [P("C33.2"), (48.25, 77.5), P("C34.2")], miter_=False)   # FP6161: CIN and COUT GND plates together,
for xy in ((48.30, 78.35), (47.60, 80.15), (46.80, 80.15)): V("GND", xy)    # then >= 3 vias at y >= 77.5 (off SW3's F pad)
V("GND", at("C46.2", 0.0, -1.0)); V("GND", (103.90, 59.50)); V("GND", at("R51.2", -0.95, 0.0))   # U6 boost: CIN, COUT, R51
V("GND", at("C47.2", 0.0, 1.0)); V("GND", at("U6.4", -1.15, 0.6))
T("GND", "B", 0.6, [P("C45.2"), (103.20, 59.50), (103.90, 59.50)], miter_=False)
V("GND", (66.80, 63.20)); V("GND", (69.80, 63.20))                   # X1 island / guard stitching (N3 owns them)
V("GND", at("C59.2", -0.95, 0.0))                                  # U12 VINA cap
V("GND", at("C61.2", -0.4, 1.25)); V("GND", at("C61.2", 1.05, 0.0)); V("GND", at("C60.2", 0.35, 1.25))   # CIN
for c in ("C64", "C62", "C63"):                                    # COUT GND pads (north), two each, y ~33.45
    for dx in (-0.35, 0.35): V("GND", at(c + ".2", dx, -1.35))
for x in (P("C64.1")[0] + 1.12, P("C62.1")[0] + 1.055):           # COUT +3V3 vias between the pads, y <= 37.0
    for y in (36.3, 37.0): V("+3V3", (x, y))
V("GND", at("R23.2", 0.0, 1.0))                                    # PS/SYNC strap's GND
V("GND", at("C57.2", -1.25, -0.35)); V("GND", at("C57.2", -1.25, 0.35))   # BAT cap
V("GND", at("C56.2", 1.25, -0.35)); V("GND", at("C56.2", 1.25, 0.35))     # SYS_SRC cap
V("GND", at("C55.2", 0.0, 1.0))                                    # +5V IN cap
V("GND", at("R15.2", -0.4, 1.05)); V("GND", at("R16.2", 0.95, 0.0))      # R47.2 / R55.2 share R15.2 / Q1.2 vias (B links)
V("GND", at("Q1.2", -1.1, 0.0))
T("GND", "B", 0.25, [P("U15.7"), (P("U15.7")[0] + 1.0, P("U15.7")[1]), (P("U15.7")[0] + 2.0, P("U15.2")[1]), P("U15.2")])   # INA226 GND 7 -> 2 under the MSOP (no EP)
T("GND", "B", 0.25, [P("U15.1"), P("U15.2")])
V("GND", (177.30, 35.10)); T("GND", "B", 0.25, [P("U15.1"), (177.30, P("U15.1")[1]), (177.30, 35.10)])
V("GND", at("C66.2", 0.0, 1.0)); V("GND", at("C44.2", 0.0, 1.0))  # INA226 decaps
V("+3V3", ((P("C66.1")[0] + P("C44.1")[0]) / 2, P("C66.1")[1]))
T("+3V3", "B", 0.25, [P("C66.1"), P("C44.1")])
V("GND", at("C65.2", 0.0, -1.05))                                  # gauge decap
# explicit pad -> via links for the small GND pads (the B pour's 0402/0603 thermal reliefs alone starved R15.2 in the trial)
def L(key, *pts, w=0.3): T("GND", "B", w, [P(key), *pts], miter_=False)
L("C59.2", (132.15, 39.4)); L("C61.2", (137.0, 46.55), (136.6, 46.95)); T("GND", "B", 0.3, [P("C61.2"), P("C60.2")], miter_=False)
L("C60.2", (139.1, 46.6), (139.45, 46.95)); L("R23.2", (135.1, 45.95))
for c in ("C64", "C62", "C63"):
    x, y = P(c + ".2"); L(c + ".2", (x, y - 1.35)); T("GND", "B", 0.3, [(x - 0.35, y - 1.35), (x + 0.35, y - 1.35)], miter_=False)
x, y = P("C57.2"); L("C57.2", (x - 1.25, y)); T("GND", "B", 0.3, [(x - 1.25, y - 0.35), (x - 1.25, y + 0.35)], miter_=False)
x, y = P("C56.2"); L("C56.2", (x + 1.25, y)); T("GND", "B", 0.3, [(x + 1.25, y - 0.35), (x + 1.25, y + 0.35)], miter_=False)
L("C55.2", at("C55.2", 0.0, 1.0)); L("R15.2", at("R15.2", 0.0, 0.65), at("R15.2", -0.4, 1.05))
L("R47.2", (156.20, P("R47.2")[1]), (155.50, 45.65), (155.50, 44.75))   # R47.2 -> R15.2's via, west of R47
L("R16.2", at("R16.2", 0.95, 0.0)); L("Q1.2", at("Q1.2", -1.1, 0.0))
L("R55.2", (159.60, P("R55.2")[1]), (159.60, 44.95), at("Q1.2", -1.1, 0.0))   # R55.2 -> Q1.2's via, west of Q1
L("C66.2", at("C66.2", 0.0, 1.0)); L("C44.2", at("C44.2", 0.0, 1.0)); L("C65.2", at("C65.2", 0.0, -1.05))
L("C46.2", at("C46.2", 0.0, -1.0)); L("C47.2", at("C47.2", 0.0, 1.0)); L("R51.2", at("R51.2", -0.95, 0.0))
L("U6.4", at("U6.4", -0.55, 0.0), at("U6.4", -1.15, 0.6))
L("C33.2", (46.85, 80.15), w=0.4); T("GND", "B", 0.4, [(46.8, 80.15), (47.6, 80.15)], miter_=False)
