# 10_power -- DRAFT, step 3 fix: the top-strip power trunks only (BAT, +5V, SYS_SRC, SYS), drawn after 05_switcher_gnd's
# via field so they route around it. The rest of group 10 (U2 core buck loop and the +0V9 trunk, U6's boost loop, the
# charger / buck-boost pin locals) is step 4; the strip's pin locals are trial-routed after these trunks in
# scripts/trials/strip_locals.py (stage s4). Trial: ~/.cache/gb3-step3/fix/trial (stage s2), DRC with the proposed DRU.
#
# BAT (F, 1.2 mm = 2.7 A at 10 degC rise, IPC-2221): CN1.1 -> north to y 40.1 -> west south of H2's keep-out and U13's EP
#   vias -> NW to the board's top edge (y 32.6-33.8) -> west over the charger -> down at x 153.6 to three 0.8/0.4 vias
#   (2.5 A) north of C57 -> C57.1 -> U10.2/3. Along the top edge it leaves F free between y 34 and +5V for the charger's
#   north-side hops (EN2, PGOOD, LED). CN1 itself is rated 2 A per contact (JST PH, C295747.pdf p.1) = the cell's 1C.
# +5V (F, 0.8 mm): VBUS pads -> one 0.8/0.4 via each -> F join at y 38.3 (north of the receptacle's D- jumper) -> T1's
#   north lane y 40.7 -> jog to y 38.45 over U12 (its EP grid and V1 at y 39.5-41.0, COUT vias at y <= 37.3) -> back to
#   y 40.7 (0.3 mm south of U10's EP vias) -> two 0.8/0.4 vias east of C55 -> C55.1 -> U10.13. D4.5/C93 via under D4.
# SYS_SRC (B, 1.2 mm): U10.10/11 -> C56.1 -> north at x 160.6 (R26.1, R18.2 tap) -> east along the top edge -> R27.1.
# SYS (B): R27.2 -> T2 at y 50.6 (2.0 mm, 2.2 A) -> U12 branch up x 140.55 (0.75) -> C60/C61 -> U12.10/11; VINA spur up
#   x 134.0 (0.6) -> C59/U12.1; T3 west at y 49.3 (0.6, 0.33 A) -> L2.1 -> C46.1 -> U6.1 and the U17 feed (B only) ->
#   C68.1; east at y 47.5 through the B neck and down the grip at x 186.8 -> C53/C54 -> U9.7/8. Kelvin taps to U15.
def at(key, dx, dy): x, y = P(key); return (x + dx, y + dy)
_T = T
def T(net, layer, w, pts): _T(net, layer, w, pts, miter_=False)     # every corner here is drawn explicitly

# ---- BAT
T("BAT", "F", 1.2, [P("CN1.1"), (185.7, 40.1), (176.0, 40.1), (169.1, 33.2), (153.6, 33.2), (153.6, 37.0)])
T("BAT", "F", 1.2, [(152.7, 37.0), (154.5, 37.0)])
for x in (152.7, 153.6, 154.5): VIA("BAT", x, 37.0, d=0.8, h=0.4)
T("BAT", "B", 1.2, [(153.6, 37.0), (153.6, 37.9)])
T("BAT", "B", 0.6, [P("C57.1"), (154.05, 39.2), (155.4, 39.2)])        # covers U10.2 and U10.3
T("BAT", "B", 0.6, [(152.85, 38.45), (152.85, 45.7), (152.08, 46.47), P("TP5.1")])
VIA("BAT", 185.60, 40.00)                                          # gauge VDD + its decap, from CN1.1's north end
T("BAT", "B", 0.6, [(185.60, 40.00), P("C65.1")])
T("BAT", "B", 0.25, [(185.60, 38.85), P("U13.3")])

# ---- +5V
for k in ("USBC1.A4B9", "USBC1.B4A9"):
    x, y = P(k); VIA("+5V", x, y + 1.16, d=0.8, h=0.4); T("+5V", "B", 0.6, [(x, y), (x, y + 1.16)])
xa, xb = P("USBC1.A4B9")[0], P("USBC1.B4A9")[0]; yv = P("USBC1.A4B9")[1] + 1.16
T("+5V", "F", 0.8, [(xa, yv), (xa, 38.3), (xb, 38.3), (xb, yv)])
T("+5V", "F", 0.8, [(xb, yv), (xb, 40.0), (121.1, 40.7), (133.4, 40.7), (135.65, 38.45), (142.6, 38.45), (144.85, 40.7), (158.75, 40.7), (158.75, 41.95)])
VIA("+5V", 158.75, 41.05, d=0.8, h=0.4); VIA("+5V", 158.75, 41.95, d=0.8, h=0.4)
T("+5V", "B", 0.6, [(158.75, 41.05), (158.75, 41.95), (158.4, 42.3), P("C55.1"), (157.65, 41.55)])
T("+5V", "B", 0.25, [(157.65, 41.55), P("U10.13")])
VIA("+5V", 147.0, 40.9); T("+5V", "B", 0.6, [(147.0, 40.9), P("TP4.1")])          # +5V test pad
VIA("+5V", 118.45, 42.8)                                           # D4.5 / C93 (USBLC6 VBUS clamp + its 100 nF), under
T("+5V", "F", 0.6, [(118.45, 42.8), (119.4, 41.85), (119.4, 38.3)])   # D4's body beside D4.2's GND via (117.60, 42.80)
T("+5V", "B", 0.4, [P("D4.5"), (118.0, 43.5), (118.45, 43.05), (118.45, 42.8)])
T("+5V", "B", 0.6, [P("D4.5"), P("C93.1")])

# ---- U12 VOUT -> COUT (C64/C62/C63) -> the four +3V3 plane vias between the caps (05)
T("+3V3", "B", 0.5, [(138.25, 38.40), (138.25, 37.50)])
T("+3V3", "B", 0.6, [P("C64.1"), P("C63.1")])

# ---- SYS_SRC
T("SYS_SRC", "B", 0.25, [P("U10.11"), (158.9, 39.45)]); T("SYS_SRC", "B", 0.25, [P("U10.10"), (158.9, 38.95)])
T("SYS_SRC", "B", 0.8, [(158.9, 39.2), (160.2, 39.2)])
T("SYS_SRC", "B", 0.25, [P("U10.10"), P("U10.11")])
T("SYS_SRC", "B", 1.2, [P("C56.1"), (160.6, 38.5), (160.6, 33.6), (168.8, 33.6), P("R27.1")])
T("SYS_SRC", "B", 0.6, [P("R18.2"), (160.6, P("R18.2")[1])])
T("SYS_SRC", "B", 0.25, [(169.80, 34.67), (171.13, 36.00), (173.00, 36.00)])   # INA226 IN+ Kelvin (pin 10)

# ---- SYS
T("SYS", "B", 1.2, [P("R27.2"), (168.8, 47.5), (168.8, 50.6)])
T("SYS", "B", 2.0, [(168.8, 50.6), (140.55, 50.6)])
T("SYS", "B", 0.25, [(169.80, 39.53), (171.10, 38.23), (171.10, 36.50), (173.00, 36.50)])  # INA226 IN-/VBUS Kelvin
T("SYS", "B", 0.25, [(171.10, 37.00), (173.00, 37.00)])
T("SYS", "B", 0.75, [(140.55, 50.6), (140.55, 43.7), P("C60.1")])          # 1.94 A IPC; 0.39 to C60.2, 0.25 to TP7
T("SYS", "B", 0.8, [P("C60.1"), P("C61.1")])
T("SYS", "B", 0.6, [P("C60.1"), (138.5, 43.1)]); T("SYS", "B", 0.25, [(138.5, 43.1), P("U12.10")])
T("SYS", "B", 0.25, [P("U12.10"), P("U12.11")])
T("SYS", "B", 0.6, [(140.55, 50.6), (134.0, 50.6), (134.0, 40.0), (134.5, 39.5), P("C59.1")])
T("SYS", "B", 0.6, [P("C59.1"), (134.5, 38.65)]); T("SYS", "B", 0.25, [(134.5, 38.65), (136.5, 38.65), P("U12.1")])   # VINA, north of the EP
T("SYS", "B", 0.6, [(149.54, 50.6), P("TP6.1")])
T("SYS", "B", 0.6, [(134.0, 50.6), (132.7, 49.3), (109.1, 49.3), (108.6, 49.8), P("L2.1")])
T("SYS", "B", 0.6, [P("L2.1"), (105.4, 52.5), (105.1, 52.2), P("C46.1")])
T("SYS", "B", 0.6, [P("C46.1"), (105.1, 53.1), P("U6.1")])
T("SYS", "B", 0.6, [(108.9, 49.3), (99.4, 39.8), (95.2, 39.8), P("C68.1")])       # U17 feed (the B-only maze path of the ne trial)
T("SYS", "B", 0.6, [P("C68.1"), (95.8, 39.2)]); T("SYS", "B", 0.25, [(95.8, 39.2), P("U17.1")])
T("SYS", "B", 0.6, [(168.8, 47.5), (184.6, 47.5), (186.8, 49.7), (186.8, 79.25), P("C53.1")])   # east: B neck -> U9
T("SYS", "B", 0.6, [P("C53.1"), (187.65, 81.7), P("C54.1")])
T("SYS", "B", 0.6, [P("C54.1"), (186.85, 82.0)]); T("SYS", "B", 0.25, [(186.85, 82.0), (186.0, 82.0)])
T("SYS", "B", 0.25, [P("U9.8"), P("U9.7")])
