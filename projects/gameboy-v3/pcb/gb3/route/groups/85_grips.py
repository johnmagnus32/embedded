# 85_grips -- the left grip (U16 port 1 -> the D-pad, START, L1), the right grip's U9 locals, U8's haptics locals and
# the BT830's LDO cap (PLAN section 6 and the group table). Group 80 already drew the right grip's switch drops.
#
# Left grip (all F after one via each; the switches are front parts):
#   U16 end: B stubs west into the pocket between SPK_EN's B lane (x 103.0, then y 84.0) and the pin row, a via
#     column at x 104.5 (0.8 pitch), F legs SW (0.57 mm apart, clear of every other via) over SPK_EN and S2's B
#     charger lanes, then west along S2 at y 94.4 .. 95.9 (south of HP_EN's F hop).
#   The corridor: north at x 48.3 .. 49.8 between the D-pad's east switch and W4 (I2S DAC branch, the USB riser),
#     past the core buck's GND vias (C33 / C34: (46.8 / 47.6, 80.15), (48.3, 78.35)). Each lane peels off west where its
#     switch is (the west lane first): south switch y 81.05, west switch through the D-pad's centre to its east pad,
#     east switch y 73.05, north switch y 65.05, START NW to y 48.15, L1 on past the SD via row (x 50.1) and NW to
#     SW15.2 from below.
#   The order N -> S at U16's west pins must equal that peel-off order (planar, one via per lane): U16.18 .. 13 =
#     SW15, SW10, the north, east, west and south switches. The nets are read from U16's pads.
# Right grip: U9 (MAX98357A) SPK_P / SPK_N 0.25 mm out of the 0.5-pitch pins, 0.4 mm to J3; SD_MODE R14.1 -> U9.4;
#   GAIN_SLOT's strap to SYS.
# Haptics: U8 (DRV2605L) REG -> C50, OUT+ / OUT- -> J2 (0.3 mm), IN/TRIG's GND tie (boxed in for fanout).
# BT830: VREG18 (U18.10) -> C70, clear of the module's body (no copper under it).
W = 0.15

# ---- left grip
U16 = B.FindFootprintByReference("U16")
def unet(pin): return U16.FindPadByNumber(str(pin)).GetNetname()
VX, VY0, VP = 104.5, 78.95, 0.8                 # the pocket's via column
SY0, CX0 = 94.4, 49.8                           # S2's north lane (y) and the corridor's east lane (x), 0.3 pitch
LG = [  # U16 pin, switch pad, the lane north of the corridor's turn -> the pad
    (18, "SW15.2", lambda cx: [(cx, 53.0), (50.1, 52.7), (50.1, 49.11), (41.99, 41.0), P("SW15.2")]),
    (17, "SW10.2", lambda cx: [(cx, 52.0), (45.65, 48.15), P("SW10.2")]),
    (16, "SW4.2",  lambda cx: [(cx, 65.05), P("SW4.2")]),
    (15, "SW3.2",  lambda cx: [(cx, 73.05), P("SW3.2")]),
    (14, "SW2.2",  lambda cx: [(cx, 79.25), (39.0, 79.25), (32.8, 73.05), P("SW2.2")]),
    (13, "SW5.2",  lambda cx: [(cx, 81.05), P("SW5.2")]),
]
for k, (pin, sw, tail) in enumerate(LG):
    n = unet(pin)
    ref, num = sw.split("."); sn = B.FindFootprintByReference(ref).FindPadByNumber(num).GetNetname()
    if sn != n: raise SystemExit(f"85_grips: U16.{pin} is {n} but {sw} is {sn}: U16's port-1 order no longer matches the grip")
    px, py = P("U16.%d" % pin)
    vy = round(VY0 + VP * k, 4)
    T(n, "B", W, [(px, py), (round(VX + abs(vy - py), 4), py), (VX, vy)]); VIA(n, VX, vy)
    sy, cx = round(SY0 + 0.3 * k, 4), round(CX0 - 0.3 * k, 4)
    T(n, "F", W, [(VX, vy), (round(VX + vy - sy, 4), sy), (cx, sy)] + tail(cx))

# ---- right grip: U9 locals
T("/amp/SPK_N", "B", 0.25, [P("U9.10"), (185.0, 83.4)])
T("/amp/SPK_N", "B", 0.4, [(185.0, 83.4), (185.0, 84.9), P("J3.2")])
T("/amp/SPK_P", "B", 0.25, [P("U9.9"), (185.5, 83.2), (185.9, 83.6)])     # out to 0.9 from SPK_N before widening
T("/amp/SPK_P", "B", 0.4, [(185.9, 83.6), (187.7, 85.4), P("J3.1")])
T("/amp/SPK_SD", "B", W, [P("R14.1"), (185.0, 79.05), (185.5, 79.55), P("U9.4")])
# GAIN_SLOT -> SYS (6 dB): 0.25 mm (DRU: the 'pins: U9 GAIN_SLOT strap' area), north between DOUT's B stub and R14
# (group 80 puts DOUT's drop via west of the strap and brings DOUT into U9.1 from the NW), past SPK_EN's via, east
# into SYS's B lane; 0.2 mm (HighCurrent) clear of everything
T("SYS", "B", 0.25, [P("U9.2"), (184.5, 79.45), (184.22, 79.17), (184.22, 75.1), (186.8, 75.1)])

# ---- haptics: U8 locals
T("/haptics/HAP_REG", "B", 0.25, [P("U8.1"), P("C50.1")])
T("GND", "B", W, [P("U8.4"), (116.6, P("U8.4")[1]), (116.25, P("U8.4")[1] - 0.35), (115.8, P("U8.4")[1] - 0.35)])   # IN/TRIG tied low
VIA("GND", 115.8, P("U8.4")[1] - 0.35)
T("/haptics/LRA_P", "B", 0.3, [P("U8.7"), (123.2, 58.7), (124.45, 57.45), P("J2.1")])
T("/haptics/LRA_N", "B", 0.3, [P("U8.9"), (123.2, 59.7), (124.45, 60.95), P("J2.2")])

# ---- BT830 LDO cap
T("/bluetooth/BT_VREG18", "B", 0.25, [P("C70.1"), (28.35, 43.05), P("U18.10")])
