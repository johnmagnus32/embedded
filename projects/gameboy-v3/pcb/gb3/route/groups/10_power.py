# 10_power -- the power trunks (BAT, +5V, SYS_SRC, SYS), drawn after 05_switcher_gnd's via field so they route around it,
# then the strip's pin locals (trial s4 strip_locals), the U2 core buck and its +0V9 trunk to U1, and U6's boost loop.
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

# ---- the strip's pin locals (trial s4 strip_locals): U10 (BQ24073: EN2 and the charge LED take an F hop north of BAT's
#      F, PGOOD runs west on B under BAT's vias to its T1 drop, TS / ISET / TMR / ILIM on B), U12 (TPS63021: BB_L1 / BB_L2
#      to L3, FB, PS/SYNC; EN's via below the pin), U15 / U13 (INA226 / MAX17048: I2C1 between them south of H2's
#      keep-out, ALRT, the gauge GND pins), U14 (charge LED, F)
W = 0.15
VIA("/charger/CHG_EN2", 156.15, 36.7); T("/charger/CHG_EN2", "B", W, [P("U10.5"), (156.15, 36.7)])
T("/charger/CHG_EN2", "F", W, [(156.15, 36.7), (157.5, 35.35), (158.05, 35.35)]); VIA("/charger/CHG_EN2", 158.05, 35.35)
T("/charger/CHG_EN2", "B", W, [(158.05, 35.35), (158.6, 34.8), P("R18.1")])
VIA("+3V3", 158.05, 34.4); T("+3V3", "B", 0.2, [P("R28.2"), (157.53, 34.4), (158.05, 34.4)])
T("CHG_PGOOD", "B", W, [P("U10.7"), (P("U10.7")[0], P("R28.1")[1] + 0.3), P("R28.1")])
T("CHG_PGOOD", "B", W, [P("R28.1"), (148.5, 36.1), (148.5, 42.4)]); VIA("CHG_PGOOD", 148.5, 42.4)
VIA("/charger/CHG_LED_K", 158.85, 37.75); T("/charger/CHG_LED_K", "B", W, [P("U10.9"), (158.82, 38.31), (158.85, 37.75)])
T("/charger/CHG_LED_K", "F", W, [(158.85, 37.75), (158.85, 35.9), (159.75, 35.0), P("U14.1")])
T("/charger/CHG_LED_A", "F", W, [P("U14.2"), (161.25, 35.35), (161.85, 35.95)]); VIA("/charger/CHG_LED_A", 161.85, 35.95)
T("/charger/CHG_LED_A", "B", W, [(161.85, 35.95), (162.5, 35.3), P("R26.2")])
VIA("/charger/CHG_TS", 154.6, 48.0)
T("/buck-boost/BB_L2", "B", 0.25, [P("U12.6"), (139.0, 38.55), (139.5, 38.55), P("U12.7")])
T("/buck-boost/BB_L2", "B", 0.8, [(139.45, 38.75), (143.6, 38.75)])
T("/buck-boost/BB_L1", "B", 0.25, [P("U12.9"), (139.0, 41.85), (139.5, 41.85), P("U12.8")])
T("/buck-boost/BB_L1", "B", 0.8, [(139.45, 41.65), (142.0, 41.65), (142.6, 42.25), (143.6, 42.25)])
VIA("PWR_EN", 137.5, 42.45); T("PWR_EN", "B", W, [P("U12.12"), (137.5, 42.45)])
for pin, (dx, dy) in (("6", (0.5, 0)), ("1", (-0.5, 0)), ("4", (-0.5, 0))):
    T("GND", "B", 0.2, [P("U13." + pin), (P("U13." + pin)[0] + dx, P("U13." + pin)[1] + dy)])
VIA("+3V3", 180.45, 38.95); T("+3V3", "B", 0.2, [P("R43.2"), (180.45, 38.95)])   # north of BAT's F, south of SCL's stub
# I2C1 / GAUGE_ALRT drops (T1 is south of the pins): SDA stays one B link U15.4 -> U13.8; SCL is two B stubs joined on F
# (vias A/B), so SDA's drop (via S) comes up between them without crossing SCL; GAUGE drops at R43.1 (via G)
T("I2C1_SDA", "B", W, [P("U15.4"), (178.75, 37.5), (179.25, 38.0), (182.0, 38.0), (182.15, 37.85), P("U13.8")])
T("I2C1_SCL", "B", W, [P("U15.5"), (176.86, 38.35), (177.9, 38.35), (177.9, 41.25)]); VIA("I2C1_SCL", 177.9, 41.25)
T("I2C1_SCL", "B", W, [P("U13.7"), (179.6, 38.35), (179.4, 38.55), (179.4, 41.25)]); VIA("I2C1_SCL", 179.4, 41.25)
T("I2C1_SCL", "F", W, [(177.9, 41.25), (179.4, 41.25)])
T("I2C1_SDA", "B", W, [(178.65, 41.8), (178.65, 37.5)]); VIA("I2C1_SDA", 178.65, 41.8)
T("GAUGE_ALRT", "B", W, [(182.3, 42.35), (182.3, 40.95), P("R43.1")]); VIA("GAUGE_ALRT", 182.3, 42.35)
T("+3V3", "B", 0.2, [P("U15.6"), (P("U15.6")[0], P("C66.1")[1] - (P("U15.6")[0] - P("C66.1")[0])), P("C66.1")])
m = MAZE((126.0, 32.4, 190.0, 53.0), limit=3000000)
for net, a, b in (("/charger/CHG_TS", "U10.1", (154.6, 48.0, "B")), ("/charger/CHG_ISET", "U10.16", "R15.1"),
                  ("/charger/CHG_TMR", "U10.14", "R47.1"), ("/charger/CHG_ILIM", "U10.12", "R16.1"),
                  ("/charger/CHG_ILIM", "R16.1", "R54.1"), ("/charger/CHG_ILIM_SW", "R54.2", "Q1.3"), ("CHG_FAST", "Q1.1", "R55.1")):
    AUTOROUTE(m, net, a, b, w=W, layers="B")
VIA("CHG_FAST", 162.6, 48.6); T("CHG_FAST", "B", W, [P("R55.1"), (162.2, 48.6), (162.6, 48.6)])   # its T1 drop, north of T2
for net, a, b in (("/buck-boost/BB_PSYNC", "U12.13", "R23.1"), ("/buck-boost/BB_FB", "U12.3", "R22.1"), ("GAUGE_ALRT", "U13.5", "R43.1")):
    AUTOROUTE(m, net, a, b, w=W, layers="B")

# ---- the power LED (U5, F, in its window) from its series R12 (B): one via beside R12.2
VIA("/power-led/PWR_LED_A", 126.3, 36.3); T("/power-led/PWR_LED_A", "B", W, [P("R12.2"), (126.3, 36.3)])
T("/power-led/PWR_LED_A", "F", W, [(126.3, 36.3), (126.3, 35.05), P("U5.2")])

# ---- U2 core buck (FP6161, behind SW3; FP6161 p.10-11: SW node and the CIN/COUT loops short, FB away from SW)
T("/core-buck/CORE_SW", "B", 0.6, [P("U2.3"), P("L1.1")])                         # SW node, 3.15 mm
T("+3V3", "B", 0.6, [P("U2.4"), (P("U2.4")[0], 78.7), P("C33.1")])               # VIN -> CIN
T("+0V9", "B", 1.0, [P("L1.2"), P("C34.1")])                                      # L1 -> COUT
T("+0V9", "B", 0.3, [P("L1.2"), P("TP8.1")])                                      # TP8 (+0V9 test pad)
T("/core-buck/CORE_RUN", "B", W, [P("U2.1"), (P("C2.1")[0], P("U2.1")[1]), (P("C2.1")[0], P("R4.2")[1]), P("R4.2")])
T("/core-buck/CORE_FB", "B", W, [P("U2.5"), P("R2.1")]); T("/core-buck/CORE_FB", "B", W, [P("U2.5"), P("R3.2")])
T("/core-buck/CORE_FB", "B", W, [P("R3.2"), (P("R3.2")[0], 76.33), P("C1.2")])
# divider top (R3.1) and the feed-forward cap (C1.1) Kelvin-sensed at L1 / TP8 (0.2 mm, the Power minimum): west of
# the RUN / FB parts, north over L1
T("+0V9", "B", 0.2, [P("C1.1"), (P("R3.1")[0], P("C1.1")[1] + (P("C1.1")[0] - P("R3.1")[0])), P("R3.1")])
T("+0V9", "B", 0.2, [P("R3.1"), (38.3, P("R3.1")[1]), (38.3, 70.0), (39.1, 69.2), (47.45, 69.2), (48.25, 68.4)])   # x 38.3: R4.1 / C2.2 keep room for their plane vias
# +0V9 trunk (W3, 0.6 mm, ~4.8 mV at 0.8 A): COUT -> U1.116 / U1.117's outer ends (the island takes them inward)
T("+0V9", "B", 0.6, [(48.6, 75.15), (55.9, 75.15)])
T("+0V9", "B", 0.2, [(56.44, 74.95), (55.9, 75.15)]); T("+0V9", "B", 0.2, [(56.44, 75.35), (55.9, 75.15)])

# ---- U6 boost loop (TPS61165 p.21: SW node and the D3 -> C45 -> U6.4 output loop small)
T("/backlight/BL_SW", "B", 0.8, [P("U6.3"), (106.8, P("U6.3")[1])])              # U6.3 -> L2.2
T("/backlight/BL_SW", "B", 0.8, [P("U6.3"), (P("U6.3")[0], P("D3.2")[1] - 0.3), P("D3.2")])   # -> D3 anode
T("BL_LED_P", "B", 0.6, [P("D3.1"), (103.0, P("D3.1")[1]), P("C45.1")])          # D3 cathode -> COUT
T("GND", "B", 0.6, [P("C45.2"), (P("U6.4")[0], 59.4), (P("U6.4")[0], 55.65)])    # COUT GND -> U6.4 up channel P2
