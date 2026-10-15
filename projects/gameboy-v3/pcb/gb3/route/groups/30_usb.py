# 30_usb -- the USB 2.0 pair end to end, the receptacle's CC pins, and CC_SENSE (step-3 escape + trial s4 usb).
#
# Pair (90 ohm: 0.275 / 0.20 mm on F over In1 GND; 0.20 mm escapes on B): U1.115 D+ / U1.114 D- west on B (W2) to two
#   vias, F riser north (D+ x 50.95, D- x 51.425, 0.475 c-c), 45 deg turn into the band, east along y 47.0 (D+, outer) /
#   47.475 (D-) under the panel to D4. D- drops first (west of C93) to D4.6, D+ continues east of C93 to D4.4: with A4
#   (USBLC6 I/O1 = D-, I/O2 = D+) the order at D4 needs no crossing. D4's north pins run straight to the receptacle's
#   B7 (D-) / B6 (D+); A6 joins D+ on B, A7 jumps over A6 on F (the receptacle's own interleave: 2 x 0.5/0.3 vias). D-
#   has 4 vias, D+ 2 (DRU <= 4); in-pair skew 1.30 mm (DRU 4 mm), uncoupled 14.37 mm (DRU 16) on the trial board.
# CC1 / CC2 -> Rd R52 / R53 (5.1k) -> R56 / R57 (200k) into CC_SENSE (step 1c's charge-current sense on GPADC0).
# CC_SENSE (U1.101 = GPADC0): group 20's island via -> C94.1 on F -> W along W4 x 56.1 north -> E at y 52.4 -> B hop down
#   x 64.8 under the I2S lanes and the band -> F east along y 45.4 (north of the band, south of T0) -> B drops to R56.2
#   (x 113.9) and R57.2 (x 122.1), either side of the pair's B legs.
W, WR = 0.275, 0.2

# ---- U1 end: B escape (W2) -> vias -> F riser
T("USB_D+", "B", WR, [P("U1.115"), (51.15, P("U1.115")[1]), (50.95, 75.95)]); VIA("USB_D+", 50.95, 75.95)
T("USB_D-", "B", WR, [P("U1.114"), (52.15, P("U1.114")[1]), (52.15, 76.3)]); VIA("USB_D-", 52.15, 76.3)
# D+ straight north; D- one 45 deg leg (not parallel to D+, so no out-of-range gap) into 0.475 c-c north of D+'s via
T("USB_D+", "F", W, [(50.95, 75.95), (50.95, 48.0), (51.95, 47.0), (118.95, 47.0)])
T("USB_D-", "F", W, [(52.15, 76.3), (51.425, 75.575), (51.425, 48.197), (52.147, 47.475), (116.6, 47.475), (117.05, 47.925), (117.05, 48.2)])

# ---- D4 end: D- drops first (west of C93), D+ continues east of C93
VIA("USB_D-", 117.05, 48.2); VIA("USB_D+", 118.95, 47.0)
T("USB_D-", "B", W, [(117.05, 48.2), P("D4.6")]); T("USB_D+", "B", W, [(118.95, 47.0), P("D4.4")])
VIA("GND", 118.0, 48.0); T("GND", "B", 0.25, [P("C93.2"), (118.0, 48.0)])          # C93's GND, south of the pocket
VIA("GND", 117.60, 42.8); T("GND", "B", 0.25, [P("D4.2"), (118.0, 42.15), (117.6, 42.55), (117.6, 42.8)])   # USBLC6 GND under the body

# ---- D4 north pins -> receptacle (A6/B6 D+, A7/B7 D- interleave): B7 / B6 straight, A6 on B into D+, A7 jumps on F
T("USB_D-", "B", WR, [P("USBC1.B7"), (117.25, 40.55), (117.05, 40.75), P("D4.1")])
T("USB_D+", "B", WR, [P("USBC1.B6"), (118.75, 40.55), (118.95, 40.75), P("D4.3")])
T("USB_D+", "B", WR, [P("USBC1.A6"), (117.75, 39.90), (118.40, 40.55), (118.75, 40.55)])
VIA("USB_D-", 118.25, 39.50, d=0.5, h=0.3); T("USB_D-", "B", WR, [P("USBC1.A7"), (118.25, 39.50)])
T("USB_D-", "F", WR, [(118.25, 39.50), (117.25, 40.50)]); VIA("USB_D-", 117.25, 40.50, d=0.5, h=0.3)

# ---- CC_SENSE
T("USBC_CC_SENSE", "F", 0.15, [P("C94.1"), (56.95, P("C94.1")[1]), (56.1, 80.95), (56.1, 53.1), (56.8, 52.4), (64.8, 52.4)])
VIA("USBC_CC_SENSE", 64.8, 52.4)
T("USBC_CC_SENSE", "B", 0.15, [(64.8, 52.4), (64.8, 45.4)]); VIA("USBC_CC_SENSE", 64.8, 45.4)
T("USBC_CC_SENSE", "F", 0.15, [(64.8, 45.4), (122.1, 45.4)])
for r in ("R56.2", "R57.2"):
    x = P(r)[0]; VIA("USBC_CC_SENSE", x, 45.4); T("USBC_CC_SENSE", "B", 0.15, [(x, 45.4), P(r)])

# ---- CC1 / CC2 -> Rd -> the sense dividers (B)
m = MAZE((44.0, 36.0, 126.0, 84.0))
AUTOROUTE(m, "/usb-c/USBC_CC1", "USBC1.A5", "R52.1", layers="B"); AUTOROUTE(m, "/usb-c/USBC_CC1", "R52.1", "R56.1", layers="B")
AUTOROUTE(m, "/usb-c/USBC_CC2", "USBC1.B5", "R53.1", layers="B"); AUTOROUTE(m, "/usb-c/USBC_CC2", "R53.1", "R57.1", layers="B")
