# gb3 signal channels: the reserved corridors every route group respects (see route/PLAN.md "Channels").
# Each: name -> (layer, polygon in board mm, what runs there). Layer "F" / "B", or "*" for a through-via row.
# plot.py --channels draws them over the board; the first word of a name is the channel's ID, and mcu_escape.py's
# `ch` field uses those IDs.
#
# Sources (step 3, integrated 2026-10-14, fixed after the first review the same day): the step-2 placement review's hard
# corridors, renamed and merged by the channels draft; the display group's D1/D2/D4/D5 (replacing the step-2 crossbar
# R1/R2/R3, which could not be built); the integration's NE1/NE2, E2 and T0; the fix pass's T1b (the B neck), A1c/A1f
# (the audio core, routed before the button groups) and the VIA_SPOTS of every plane / EP / signal via the combined trial
# board placed (fix/trial/s5). Netlist and placement as proposed in route/PLAN.md "Decisions" (A1-A8, O1, P1-P4, M1).
#
# SOFT: wide bands that routes weave through; plane vias may land in them (decoupling beats a lane). Every other
# channel is a choke point: fanout keeps its plane vias out (a through via blocks both layers), and so do test
# points. Route groups place their own signal vias in a channel where PLAN.md says so (mcu_escape.VIAS lists them).
# OWNERS: parts whose pads/courtyard may sit inside a hard channel because the channel serves them; anything else
#   inside a hard channel is an error (chancheck, to be ported with fanout.py).
# NO_PARTS: placement reservations that copper may cross (cable landings) - not channels.
# VIA_SPOTS: reserved FOR particular vias (keep parts and foreign copper out).


def R(x0, y0, x1, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


CHANNELS = {
    # ---------------------------------------------------------------- U1 north side
    "N1 SD bus":            ("B", [(46.8, 46.9), (59.0, 46.9), (61.5, 48.2), (61.1, 58.0), (61.1, 63.5), (63.0, 65.0), (63.0, 68.6),
                                   (59.5, 68.6), (57.6, 62.0), (54.4, 56.0), (48.4, 56.0), (46.8, 49.0)],
                             "SD_CD (PG10 = U1.5, A2) + SD_CLK/CMD/D0-D3 (U1.7-12): one planar B fan N then NW (explicit, diagonals "
                             "x-y = c) to U4, lanes x 48.65-60.45 under D1/D2 (SP3004 remap A8) with every ESD tap and pull-up "
                             "(R6-R10, R48) on B; 0 signal vias, skew 3.77 mm (trial s4 sd). Plane vias: VIA_SPOTS V13"),
    "N2 NOR fan-in":        ("B", R(63.2, 57.8, 66.1, 68.6),
                             "NOR_* from U1.14-19 straight north to U3; IO3 and one of CS/IO0 take one via pair each at U3 "
                             "(pin order vs the W25Q pads: 6 crossings; DRU <= 2 vias)"),
    "N3 X1 HOSC":           ("B", [(66.2, 62.6), (71.4, 62.6), (71.4, 67.6), (67.4, 67.6), (67.4, 68.6), (66.2, 68.6)],
                             "HOSC_IN/OUT U1.22/23 -> X1/C3/C4 only, 0 vias (DRU), B only; the In2 GND island under it runs 0.21 mm "
                             "past U1's pin tips (y 68.6; tips at 68.39) so neither HOSC line crosses an island edge (HOSC_IN's y 67.35 leg is 1.25 mm "
                             "inside; 288 of 288 samples beyond the tips over the island, fix/work/meas/ref_s5.json). RESET's U1.27 via "
                             "(69.40,67.95, 0.5/0.3) sits in the island just outside this channel. Partial GND guard, not a ring: X1.2-X1.4 linked under the crystal, the two stitch vias "
                             "(66.80,63.20)/(69.80,63.20) north; it cannot close on the east (C3 is 0.08 mm from N4)"),
    "N4 PWR_BTN riser":     ("B", R(71.4, 60.5, 72.3, 68.3),
                             "PWR_BTN_3V3 (PE13 = U1.31, A1 plain swap: a static line beside C3.1/HOSC_IN instead of the backlight "
                             "PWM) north at x 71.6, 0.2 mm (Power class, 0.18 to C3.1), over the In2 gap east of the X1 island, then "
                             "45 deg to x 73.0 at y 61.6-60.2 into NE1"),
    "K1 no tracks over X1": ("F", R(66.9, 64.3, 70.6, 67.15),
                             "F.Cu rule area (route/footprint_keepouts.py): no tracks over the 24 MHz crystal; the GND pour and "
                             "the crystal island's GND stitching vias are allowed"),
    "K2 no pattern X2":     ("F", R(67.2, 68.4, 67.9, 69.8),
                             "F.Cu rule area between X2's pads: no tracks, vias or pour (SC-20S, C97607 p.1 'no pattern under')"),
    "N5 LDOB lane":         ("F", R(69.95, 70.4, 70.30, 76.9),
                             "LDOB (0.4 A) from the U1.30 via (70.10,70.75) to the U1.48/49 vias, under U1 in the F island"),
    "NE1 NE risers":        ("B", [(72.25, 51.5), (78.1, 51.5), (78.1, 69.8), (76.6, 69.8), (76.6, 61.5), (72.25, 61.5)],
                             "west to east: RESET (x 72.45), PWR_BTN_3V3 (x 73.0, from N4), PSHOLD (U1.36 = PE11, unchanged pin, "
                             "x 77.2) and BL_CTRL (U1.37 = PE10 PWM4, x 77.6) between TP1 and TP3; 0.40-0.55 mm pitch past C7 / "
                             "U3's east pads"),
    "NE2 NE fan":           ("B", R(71.9, 33.0, 101.7, 51.5),
                             "the NE1 lines fan out after SYS (T3 -> C68.1/U17.1, group 10): RESET -> SW1.1 (SW1.1-SW1.2 tied: "
                             "the TS-1187A's pads 1/2 are one contact), PWR_BTN_3V3 -> R40.1 -> U17.8, PSHOLD -> U17.4, BL_CTRL "
                             "east at y 50.8 -> R51.1, then down x 103.85 into U6.2 from under the SOT-23 (no via in the U6-L2 "
                             "gap). PWR_EN and U17.6's /PB leave U17 on F (vias (98.85,37.45), (96.2,35.6)) into T0. Trial s4 "
                             "ne: all B, 0 extra vias (RESET 23.2, PWR_BTN_3V3 34.7, PSHOLD 25.9, BL_CTRL 32.8 mm)"),
    # ---------------------------------------------------------------- U1 east side and the display corridor
    "E1 U1 east escapes":   ("B", R(72.9, 69.8, 101.5, 75.6),
                             "U1.33-45 east: UART0 -> R46/R45 (local), PSHOLD/BL_CTRL turn north into NE1 at x 77.2/77.6, "
                             "TOUCH_RST/INT -> FPC1, EXP_INT -> R29/U16.1, SPK_EN -> M1 (east lanes y 72.55-73.75), and the IMU "
                             "ribbon I2C2_SDA/SCL + IMU_INT2/INT1 (y 74.15-75.35, south lanes) east to E2"),
    "E2 IMU lane":          ("B", R(92.6, 75.6, 101.6, 85.5),
                             "the IMU ribbon peels off the bottom of E1 (westmost first) south to U7 at (96.6,80.2) rot 90: "
                             "INT1 round U7's west side to U7.4, INT2 into U7.9, SCL/SDA down its east side to U7.13/14; "
                             "R41/R42/R13 tap on the way. 0 ribbon vias (needs PE0<->PE1, A7). All 14 U7 pads, C48 (rot 90, "
                             "94.4/77.3), C49 (94.4/81.45), R41 (rot 270) and the 9 plane vias (VIA_SPOTS V14) routed (trial s4 imu)"),
    "D1 display rows":      ("B", [(72.90, 77.75), (89.35, 77.75), (89.35, 76.30), (92.60, 76.30), (92.60, 78.10), (89.35, 79.25),
                                   (86.33, 83.40), (82.25, 87.00), (76.85, 90.60), (66.00, 90.60), (66.00, 85.10), (72.90, 85.10)],
                             "group 40: the 21 B rows (0.6 mm, U1 pin order N->S: DISP VSYNC HSYNC B2-7 G2-5 | S-row wrap G6 G7 "
                             "R3 R2 R4-7) and their vias (block staircase: red SW, green, blue, DISP/VSYNC/HSYNC NE), PANEL_DISP "
                             "on to R39; the DE via at (66.30,86.30)"),
    "D2 display F bundle":  ("F", [(73.05, 85.85), (73.05, 81.51), (86.60, 67.26), (86.60, 66.10), (100.80, 66.10), (100.80, 70.55),
                                   (79.67, 91.70), (66.00, 91.70), (66.00, 85.85)],
                             "group 40: F columns (P1 pad order W->E) from the vias, one parallel 45 deg NE jog, straight into P1's "
                             "pads from the south; DCLK (0 vias, from R50) and DE on F round the SE of the staircase. In1 GND "
                             "under all of it; no other F copper inside"),
    "D4 P1 backlight feed": ("B", [(83.10, 60.60), (96.70, 60.60), (96.70, 53.10), (102.40, 53.10), (102.40, 53.80), (97.30, 53.80),
                                   (97.30, 61.60), (101.30, 61.60), (102.20, 60.80), (103.30, 60.80), (103.30, 62.85), (83.10, 62.85)],
                             "group 40: BL_LED_N (U6.6 -> x 97.0 -> y 61.0 -> via (83.45,63.30) -> P1.1) and BL_LED_P (C45.1 -> "
                             "TP11 -> via (84.45,62.50) -> P1.2), 0.25 mm; everything else keeps 0.6 mm from TP11 (bare pad, up "
                             "to 39 V: DRU 'TP11 isolation')"),
    "D5 P1 plane-via row":  ("*", R(84.60, 63.35, 101.85, 64.40),
                             "fanout's spots for P1's plane pads at y 63.75: +3V3 pad 4; GND 5/6, 13/14, 29, 36; GND 21/22 at "
                             "(94.20,64.00) (0.65 mm from TP11); GND pad 3 takes its via south at (84.95,66.95) under the body"),
    "S1 PB2-7 south":       ("B", R(61.8, 85.2, 65.5, 92.0),
                             "U1.79-86 south: CHG_FAST, CHG_PGOOD (PB6, O1), GAUGE_ALRT east-bound; BT_EN_RST (PB4, O1), "
                             "DAC_XSMT, HP_EN west-bound - planar with O1 (else BT_EN_RST takes a via pair); then E/W below y 92"),
    # ---------------------------------------------------------------- U1 west side
    "W1 UART3 north-west":  ("B", R(50.0, 66.0, 56.1, 74.7),
                             "UART3 x4 west from U1.118-121, north at x 50.2-51.4 (0.4 pitch); I2S1 x3 + JACK_DET (PG14 = U1.126, "
                             "A3) west from U1.124-127 to the W4 staircase vias at x 53.25-55.5, y 70.95-72.45"),
    "W2 USB escape":        ("B", R(50.3, 75.6, 56.1, 77.9),
                             "USB_D+/- from U1.115/114 west, coupled 0.2/0.2 mm, to the pair vias D+ (50.95,75.95) and D- "
                             "(52.15,76.30), south of the +0V9 trunk (y 74.85-75.45)"),
    "W3 west turn strip":   ("B", R(49.3, 66.0, 52.7, 82.0),
                             "where U1's west signals turn (UART3 north, the USB vias); parts and fanout vias stay out; the +0V9 "
                             "trunk C34 -> U1.116/117 crosses it at y 74.85-75.45"),
    "W4 I2S T":             ("F", [(52.6, 64.3), (57.9, 64.3), (57.9, 67.0), (57.3, 67.0), (57.3, 92.0), (52.6, 92.0)],
                             "F from the staircase vias: I2S1 x3 north (U9 branch) into H1 and south (DAC branch) to vias at x "
                             "~52.5, y 88-91 (R32-34); JACK_DET south with the DAC branch; CC_SENSE north at x 56.1 from C94 "
                             "to y 53.1, east at y 52.4 to its first hop (64.8,52.4); the I2C1 risers x 56.75/57.5 from their "
                             "vias (56.75,67.9)/(57.5,67.3)"),
    "USB band":             ("F", [(50.3, 77.9), (52.5, 77.9), (52.5, 50.2), (123.0, 50.2), (123.0, 46.4), (50.3, 46.4)],
                             "USB_D+/- 90 ohm on F over In1: riser x 50.95/51.425, east along y 47.0/47.475 to the D4-end vias "
                             "D- (117.05,48.2) / D+ (118.95,47.0), B legs to D4.6/D4.4, receptacle jumper via pair at (118.25,39.5)"
                             "/(117.25,40.5) for A7; 4 vias on D-, 2 on D+; uncoupled 14.37 mm, skew 1.30 mm (DRU 16 / 4 mm). "
                             "Nothing else on F crosses it (CC_SENSE hops under on B at x 64.8)"),
    "W5 UART3 to U18":      ("B", [(50.0, 66.0), (51.8, 66.0), (51.8, 58.0), (35.0, 58.0), (35.0, 46.5), (30.2, 46.5), (30.2, 56.0),
                                   (50.0, 56.0)],
                             "UART3 north from W1 at x 50.2-51.4, west along y 56-58 through the vacated core-buck zone, north "
                             "into U18's pins 4-7 (x 30.8-33.2); RX/RTS/TX arrive reversed: 2 via pairs at the U18 end"),
    # ---------------------------------------------------------------- long-haul F bands (soft)
    "H1 F highway":         ("F", R(52.6, 50.6, 123.4, 64.3),
                             "east-west on F over In1 between the USB band and P1, N->S as they leave W4: I2S1 DOUT/BCLK/LRCK "
                             "(U9 branch), GND guard, I2C1 SCL/SDA; they turn east at y >= 51.5 (the SD via row V13 is at y 50.85) "
                             "and pass P1 north of y 61.9 (the D4/D5 vias). CC_SENSE left H1: it runs north of the USB band"),
    "M1 east lane":         ("F", R(106.8, 50.6, 127.0, 83.6),
                             "north-south on F east of P1 / the display landing: right-grip buttons from U16's east pins, I2C1 "
                             "to U16/FPC1/U8, SPK_EN up from E1, charger and gauge lines from S2, into T1"),
    "S2 south band":        ("F", R(57.5, 92.0, 127.0, 110.5),
                             "east-west on F south of the RGB field: left-grip buttons to U16's west pins, CHG_FAST/CHG_PGOOD/"
                             "GAUGE_ALRT east from S1, DAC_XSMT/HP_EN west to the audio block"),
    "T0 U17 to strip":      ("F", R(97.5, 41.2, 123.5, 44.0),
                             "PWR_EN (U17.9 -> U12.12's via (137.5,42.45)) and /PB (U17.6 -> SW14) east on F from U17's vias, "
                             "south of +5V (y 40.7) and north of CC_SENSE (y 45.4); they enter T1 at y 42.45 / 41.65 (T1's "
                             "west-end order in PLAN.md section 5)"),
    # ---------------------------------------------------------------- top strip, the only left<->right link
    "T1 strip F highway":   ("F", R(123.5, 40.0, 182.0, 51.8),
                             "the only left<->right link on F, over In1, above the B power cluster: 20 lanes at 0.30 pitch weave "
                             "fixed bands between the reserved vias (V3-V12). x 124 N->S: /PB (B hop at x 124.6-127.6 under the "
                             "north lanes), PWR_EN, CHG_PGOOD, I2C1 SCL/SDA, GAUGE_ALRT, R1, VOL+, VOL-, (/PB), B, X, SELECT, Y, "
                             "A, CHG_FAST, SPK_EN, I2S DOUT/BCLK/LRCK; CHG_TS joins at its via (154.6,48.0). +5V and BAT keep "
                             "the north edge. The buttons drop to B in V12's staircase; I2C1/GAUGE at V11; the south five leave "
                             "through T1n. Trial s4 strip_lanes: explicit, DRC-clean"),
    "T1n F neck":           ("F", [(179.5, 50.3), (181.4, 50.3), (184.0, 52.9), (184.0, 83.0), (182.2, 83.0), (182.2, 53.2),
                                   (181.0, 51.9), (179.5, 51.9)],
                             "the 2.50 mm F neck between CN1.4's SW corner (182.6,50.23) and the cell cutout's copper limit "
                             "(arc centre (179.7,54.3), r 2.5), then the cutout wall: CHG_TS, SPK_EN, I2S DOUT/BCLK/LRCK as a 45 "
                             "deg octagon r 3.85-2.65 about the arc's centre (1.08 mm spare to CN1.4: 3 more lanes), down x "
                             "183.55-182.35 to their drops at R17 (CHG_TS), R14 (SPK_EN) and U9 (DOUT/BCLK/LRCK); no vias in the neck"),
    "T1b B neck":           ("B", R(174.3, 42.7, 188.2, 45.9),
                             "the 9 right-grip lanes (R1, VOL+, VOL-, /PB, B, X, SELECT, Y, A at y 42.925-45.325, 0.30 pitch) from "
                             "V12's staircase east through the 10.8 mm B neck between R43 (y <= 41.03) and SYS's east branch "
                             "(y >= 47.2): 1.8 mm spare on each side. East of it they fan on B (columns x 187.9-206.3) to vias "
                             "beside the switch pads (VIA_SPOTS V20)"),
    "T2 strip power band":  ("B", R(127.5, 48.8, 181.7, 51.8),
                             "SYS from R27.2 west at y 50.6 to the U12 branch (0.75 mm, up x 140.55 east of C60) and on into T3, "
                             "2.0 mm / 2.2 A; nothing else: +5V and CHG_TS would cross it on B (interleaved pads), so they ride F"),
    "T3 SYS trunk west":    ("B", R(103.0, 46.5, 127.5, 51.8),
                             "SYS from the strip west to U6/L2 (0.33 A) and on to U17/C68 (uA); C46 and C93 sit in it"),
    "P2 U6 boost return":   ("B", R(101.9, 55.6, 103.0, 59.4),
                             "C45 GND -> U6.4 GND, the backlight boost's output loop (TPS61165 p.21); nothing else"),
    "A1 audio quiet zone":  ("B", R(30.0, 84.0, 56.0, 112.0),
                             "no switcher or digital-bus routing (PCM5102A p.34, TPA6132A2 p.18): audio nets, I2S/XSMT/HP_EN/"
                             "JACK_DET entering from the NE, jack lines; BT_EN_RST passes it on its way to U18"),
    "A1c audio core":       ("B", R(35.5, 86.6, 49.6, 113.8),
                             "group 75 routes it before 80/85 (trial s4 audio): U19 OUTL/OUTR -> R36/R35 (A6, 0 vias), the R-C "
                             "chains and AMP_INL/R into U20.4/U20.1, HP_R B west at y 105.75 and down x 35.95 to its via, SGND "
                             "(U20.15) down x 42.15 to the sleeve via; only the audio nets and their GND vias"),
    "A1f HP_L F":           ("F", R(40.4, 99.8, 43.3, 108.9),
                             "HP_L from its via at U20's NW corner (42.9,100.15) on F down x 42.9 and SW into the jack's tip pad"),
}

SOFT = {"NE2 NE fan", "D5 P1 plane-via row", "W5 UART3 to U18", "H1 F highway", "M1 east lane", "S2 south band",
        "T0 U17 to strip", "T1 strip F highway", "T3 SYS trunk west", "A1 audio quiet zone"}

OWNERS = {
    "N1 SD bus":            {"U1", "U4", "D1", "D2", "R6", "R7", "R8", "R9", "R10", "R48", "C11", "C40"},
    "N2 NOR fan-in":        {"U1", "U3"},
    "N3 X1 HOSC":           {"U1", "X1", "C3", "C4"},
    "N4 PWR_BTN riser":     {"U1"},
    "K1 no tracks over X1": set(),
    "K2 no pattern X2":     {"X2"},
    "N5 LDOB lane":         set(),
    "NE1 NE risers":        {"TP1", "TP3"},
    "E1 U1 east escapes":   {"U1", "R45", "R46"},
    "E2 IMU lane":          {"U7", "R13", "R41", "R42", "C48", "C49"},
    "D1 display rows":      {"U1", "R39"},
    "D2 display F bundle":  {"P1"},
    "D4 P1 backlight feed": {"P1", "TP11", "C45", "U6"},
    "S1 PB2-7 south":       {"U1"},
    "W1 UART3 north-west":  {"U1"},
    "W2 USB escape":        {"U1"},
    "W3 west turn strip":   {"U1"},
    "W4 I2S T":             set(),
    "USB band":             set(),
    "T1n F neck":           set(),
    "T2 strip power band":  set(),
    "P2 U6 boost return":   {"U6", "C45"},
    "T1b B neck":           set(),
    "A1c audio core":       {"U19", "U20", "R35", "R36", "R37", "R44", "R49", "C74", "C75", "C76", "C77", "C78", "C79", "C80",
                             "C81", "C82", "C83", "C84", "C85", "C86", "C87", "C88", "C89", "C90", "C91", "R32", "R33", "R34", "Audio1"},
    "A1f HP_L F":           {"Audio1"},
}

NO_PARTS = {
    "display flex landing": ("F", R(83.4, 65.1, 103.9, 83.9), "folded display tail (neck); only P1, its target"),
    "display flex wide":    ("F", R(83.4, 83.9, 132.4, 112.8), "display tail component area (driver IC 1.2 mm, ATM0500D27 drawing)"),
    "touch flex landing":   ("B", R(115.0, 70.6, 122.0, 112.8), "folded touch tail on the back; FPC1 is its target"),
}

# VIA_SPOTS: one tight box per via cluster (via radius + 0.15); the exact vias are in VIA_POINTS. Generated from the
# combined trial board (fix/trial/s5, every group and trial together); a name with /k is the k-th cluster of a set.
VIA_SPOTS = {
    "V1 U12 PGND vias":              ("*", R(140.55, 39.35, 142.30, 41.05), "TPS63021 PGND vias in the U12-L3 gap (DS Fig 31), strapped to the EP (group 05)"),
    "V2 USB-C via band /1":          ("*", R(115.05, 38.80, 116.15, 39.90), "VBUS vias under A4B9/B4A9 (F join), D4.5's +5V via, D4.2's GND via, the A7 jumper pair"),
    "V2 USB-C via band /2":          ("*", R(116.85, 39.10, 118.65, 40.90), "VBUS vias under A4B9/B4A9 (F join), D4.5's +5V via, D4.2's GND via, the A7 jumper pair"),
    "V2 USB-C via band /3":          ("*", R(117.15, 42.35, 118.90, 43.25), "VBUS vias under A4B9/B4A9 (F join), D4.5's +5V via, D4.2's GND via, the A7 jumper pair"),
    "V2 USB-C via band /4":          ("*", R(119.85, 38.80, 120.95, 39.90), "VBUS vias under A4B9/B4A9 (F join), D4.5's +5V via, D4.2's GND via, the A7 jumper pair"),
    "V3 U12 EP":                     ("*", R(136.05, 39.35, 139.95, 41.05), "TPS63021 thermal pad: 2x4 GND vias (group 05, before the trunks)"),
    "V4 U12 COUT /1":                ("*", R(135.10, 33.00, 141.00, 33.90), "C62-C64: pad-2 GND vias at y 33.45, +3V3 pairs at x 137.02 / 139.155 into In2"),
    "V4 U12 COUT /2":                ("*", R(136.57, 35.85, 137.47, 37.45), "C62-C64: pad-2 GND vias at y 33.45, +3V3 pairs at x 137.02 / 139.155 into In2"),
    "V4 U12 COUT /3":                ("*", R(138.71, 35.85, 139.60, 37.45), "C62-C64: pad-2 GND vias at y 33.45, +3V3 pairs at x 137.02 / 139.155 into In2"),
    "V5 U12 input caps /1":          ("*", R(131.70, 38.95, 132.60, 39.85), "C59.2, R23.2, C61.2, C60.2 GND vias"),
    "V5 U12 input caps /2":          ("*", R(134.65, 45.25, 139.90, 47.40), "C59.2, R23.2, C61.2, C60.2 GND vias"),
    "V6 U10 EP + C57 /1":            ("*", R(149.60, 37.65, 150.50, 39.25), "BQ24073 thermal pad: 5 GND vias (M11 heat), C57.2's pair"),
    "V6 U10 EP + C57 /2":            ("*", R(155.95, 38.25, 157.85, 40.15), "BQ24073 thermal pad: 5 GND vias (M11 heat), C57.2's pair"),
    "V7 U10 GND vias /1":            ("*", R(155.05, 44.30, 155.95, 45.20), "R15/R47 (shared), C55, Q1/R55 (shared) GND vias at y 44.6-44.75 (T1's lanes pass at 43.85 / 45.3), R16.2, C56.2"),
    "V7 U10 GND vias /2":            ("*", R(157.20, 44.25, 158.10, 45.15), "R15/R47 (shared), C55, Q1/R55 (shared) GND vias at y 44.6-44.75 (T1's lanes pass at 43.85 / 45.3), R16.2, C56.2"),
    "V7 U10 GND vias /3":            ("*", R(159.10, 44.18, 160.00, 45.08), "R15/R47 (shared), C55, Q1/R55 (shared) GND vias at y 44.6-44.75 (T1's lanes pass at 43.85 / 45.3), R16.2, C56.2"),
    "V7 U10 GND vias /4":            ("*", R(161.95, 40.75, 162.85, 41.65), "R15/R47 (shared), C55, Q1/R55 (shared) GND vias at y 44.6-44.75 (T1's lanes pass at 43.85 / 45.3), R16.2, C56.2"),
    "V7 U10 GND vias /5":            ("*", R(163.00, 38.10, 163.90, 39.70), "R15/R47 (shared), C55, Q1/R55 (shared) GND vias at y 44.6-44.75 (T1's lanes pass at 43.85 / 45.3), R16.2, C56.2"),
    "V8 BAT drops /1":               ("*", R(152.15, 36.45, 155.05, 37.55), "BAT F trunk -> B: 3 x 0.8/0.4 at C57.1, the gauge tap at C65.1"),
    "V8 BAT drops /2":               ("*", R(185.15, 39.55, 186.05, 40.45), "BAT F trunk -> B: 3 x 0.8/0.4 at C57.1, the gauge tap at C65.1"),
    "V9 +5V drops /1":               ("*", R(146.55, 40.45, 147.45, 41.35), "+5V F -> B at TP4 and at C55/U10.13 (2 x 0.8/0.4)"),
    "V9 +5V drops /2":               ("*", R(158.20, 40.50, 159.30, 42.50), "+5V F -> B at TP4 and at C55/U10.13 (2 x 0.8/0.4)"),
    "V10 U15/U13/C44/C66/C65 /1":    ("*", R(171.45, 41.75, 172.35, 42.65), "C66/C44 GND (y 42.2) and +3V3 (172.85,39.8), U15's GND via, U13's EP pair, C65.2"),
    "V10 U15/U13/C44/C66/C65 /2":    ("*", R(172.40, 39.35, 173.30, 40.25), "C66/C44 GND (y 42.2) and +3V3 (172.85,39.8), U15's GND via, U13's EP pair, C65.2"),
    "V10 U15/U13/C44/C66/C65 /3":    ("*", R(173.35, 41.75, 174.25, 42.65), "C66/C44 GND (y 42.2) and +3V3 (172.85,39.8), U15's GND via, U13's EP pair, C65.2"),
    "V10 U15/U13/C44/C66/C65 /4":    ("*", R(176.85, 34.65, 177.75, 35.55), "C66/C44 GND (y 42.2) and +3V3 (172.85,39.8), U15's GND via, U13's EP pair, C65.2"),
    "V10 U15/U13/C44/C66/C65 /5":    ("*", R(182.85, 37.85, 183.75, 39.35), "C66/C44 GND (y 42.2) and +3V3 (172.85,39.8), U15's GND via, U13's EP pair, C65.2"),
    "V10 U15/U13/C44/C66/C65 /6":    ("*", R(185.15, 36.30, 186.05, 37.20), "C66/C44 GND (y 42.2) and +3V3 (172.85,39.8), U15's GND via, U13's EP pair, C65.2"),
    "V11 I2C1 / GAUGE drops /1":     ("*", R(177.45, 40.80, 179.85, 42.25), "SCL A/B (two B stubs joined on F), SDA between them, GAUGE at R43.1, R43.2's +3V3 via north of BAT's F"),
    "V11 I2C1 / GAUGE drops /2":     ("*", R(180.00, 38.50, 180.90, 39.40), "SCL A/B (two B stubs joined on F), SDA between them, GAUGE at R43.1, R43.2's +3V3 via north of BAT's F"),
    "V11 I2C1 / GAUGE drops /3":     ("*", R(181.85, 41.90, 182.75, 42.80), "SCL A/B (two B stubs joined on F), SDA between them, GAUGE at R43.1, R43.2's +3V3 via north of BAT's F"),
    "V12 B-neck staircase":          ("*", R(174.15, 42.70, 180.65, 46.00), "the 9 grip lanes F -> B: (174.6 + 0.7k, 43.15 + 0.3k)"),
    "V13 SD support row /1":         ("*", R(44.15, 46.46, 45.05, 47.36), "SP3004 VCC/GND, the pull-ups' +3V3, C11/C40, R48, U3.4 (row at y 50.85; H1 lanes turn at y >= 51.5)"),
    "V13 SD support row /2":         ("*", R(48.90, 50.40, 49.80, 51.30), "SP3004 VCC/GND, the pull-ups' +3V3, C11/C40, R48, U3.4 (row at y 50.85; H1 lanes turn at y >= 51.5)"),
    "V13 SD support row /3":         ("*", R(52.40, 50.40, 54.15, 51.30), "SP3004 VCC/GND, the pull-ups' +3V3, C11/C40, R48, U3.4 (row at y 50.85; H1 lanes turn at y >= 51.5)"),
    "V13 SD support row /4":         ("*", R(54.90, 50.40, 57.23, 51.30), "SP3004 VCC/GND, the pull-ups' +3V3, C11/C40, R48, U3.4 (row at y 50.85; H1 lanes turn at y >= 51.5)"),
    "V13 SD support row /5":         ("*", R(58.90, 54.10, 59.80, 55.00), "SP3004 VCC/GND, the pull-ups' +3V3, C11/C40, R48, U3.4 (row at y 50.85; H1 lanes turn at y >= 51.5)"),
    "V13 SD support row /6":         ("*", R(59.35, 50.40, 60.25, 51.30), "SP3004 VCC/GND, the pull-ups' +3V3, C11/C40, R48, U3.4 (row at y 50.85; H1 lanes turn at y >= 51.5)"),
    "V13 SD support row /7":         ("*", R(61.35, 53.15, 62.65, 55.45), "SP3004 VCC/GND, the pull-ups' +3V3, C11/C40, R48, U3.4 (row at y 50.85; H1 lanes turn at y >= 51.5)"),
    "V13 SD support row /8":         ("*", R(62.30, 50.40, 63.20, 51.30), "SP3004 VCC/GND, the pull-ups' +3V3, C11/C40, R48, U3.4 (row at y 50.85; H1 lanes turn at y >= 51.5)"),
    "V14 IMU pocket /1":             ("*", R(93.10, 81.70, 94.00, 82.60), "U7 / C48 / C49 / R13 / R41 / R42 plane vias (+3V3 x4, GND x5)"),
    "V14 IMU pocket /2":             ("*", R(93.95, 78.85, 95.50, 80.30), "U7 / C48 / C49 / R13 / R41 / R42 plane vias (+3V3 x4, GND x5)"),
    "V14 IMU pocket /3":             ("*", R(94.80, 75.85, 95.70, 76.75), "U7 / C48 / C49 / R13 / R41 / R42 plane vias (+3V3 x4, GND x5)"),
    "V14 IMU pocket /4":             ("*", R(95.40, 85.40, 96.30, 86.30), "U7 / C48 / C49 / R13 / R41 / R42 plane vias (+3V3 x4, GND x5)"),
    "V14 IMU pocket /5":             ("*", R(96.40, 82.00, 98.30, 83.15), "U7 / C48 / C49 / R13 / R41 / R42 plane vias (+3V3 x4, GND x5)"),
    "V14 IMU pocket /6":             ("*", R(97.55, 78.10, 98.45, 79.00), "U7 / C48 / C49 / R13 / R41 / R42 plane vias (+3V3 x4, GND x5)"),
    "V14 IMU pocket /7":             ("*", R(101.50, 80.25, 102.40, 81.15), "U7 / C48 / C49 / R13 / R41 / R42 plane vias (+3V3 x4, GND x5)"),
    "V15 X1 stitch /1":              ("*", R(66.35, 62.75, 67.25, 63.65), "the X1 island's GND stitch vias north of the crystal"),
    "V15 X1 stitch /2":              ("*", R(69.35, 62.75, 70.25, 63.65), "the X1 island's GND stitch vias north of the crystal"),
    "V16 U2 core buck /1":           ("*", R(46.35, 79.70, 48.05, 80.60), "FP6161 GND plate vias (C33.2/C34.2)"),
    "V16 U2 core buck /2":           ("*", R(47.85, 77.90, 48.75, 78.80), "FP6161 GND plate vias (C33.2/C34.2)"),
    "V17 U6 boost /1":               ("*", R(97.85, 51.85, 98.75, 52.75), "TPS61165 GND vias: C46.2, C45.2 (output loop), R51.2, C47.2, U6.4"),
    "V17 U6 boost /2":               ("*", R(99.03, 55.55, 99.93, 56.45), "TPS61165 GND vias: C46.2, C45.2 (output loop), R51.2, C47.2, U6.4"),
    "V17 U6 boost /3":               ("*", R(100.75, 55.50, 101.65, 56.40), "TPS61165 GND vias: C46.2, C45.2 (output loop), R51.2, C47.2, U6.4"),
    "V17 U6 boost /4":               ("*", R(103.45, 59.05, 104.35, 59.95), "TPS61165 GND vias: C46.2, C45.2 (output loop), R51.2, C47.2, U6.4"),
    "V17 U6 boost /5":               ("*", R(104.65, 48.75, 105.55, 49.65), "TPS61165 GND vias: C46.2, C45.2 (output loop), R51.2, C47.2, U6.4"),
    "V18 U9 / U20 EP /1":            ("*", R(43.60, 100.80, 45.20, 102.40), "2x2 thermal-pad vias each"),
    "V18 U9 / U20 EP /2":            ("*", R(183.95, 80.60, 185.55, 82.20), "2x2 thermal-pad vias each"),
    "V19 audio core /1":             ("*", R(35.50, 111.67, 36.40, 112.57), "C84.2 / C83.2 GND, HP_L / HP_R jack vias, SGND's sleeve via"),
    "V19 audio core /2":             ("*", R(37.95, 104.45, 38.85, 105.35), "C84.2 / C83.2 GND, HP_L / HP_R jack vias, SGND's sleeve via"),
    "V19 audio core /3":             ("*", R(38.95, 96.80, 39.85, 97.70), "C84.2 / C83.2 GND, HP_L / HP_R jack vias, SGND's sleeve via"),
    "V19 audio core /4":             ("*", R(41.70, 112.95, 42.60, 113.85), "C84.2 / C83.2 GND, HP_L / HP_R jack vias, SGND's sleeve via"),
    "V19 audio core /5":             ("*", R(42.45, 99.70, 43.35, 100.60), "C84.2 / C83.2 GND, HP_L / HP_R jack vias, SGND's sleeve via"),
    "V20 grip drops /1":             ("*", R(182.10, 82.15, 183.00, 83.05), "B -> F beside each switch pad; the T1n lanes' drops at R17 / R14 / U9"),
    "V20 grip drops /2":             ("*", R(182.45, 78.50, 184.45, 80.05), "B -> F beside each switch pad; the T1n lanes' drops at R17 / R14 / U9"),
    "V20 grip drops /3":             ("*", R(183.65, 59.95, 184.55, 60.85), "B -> F beside each switch pad; the T1n lanes' drops at R17 / R14 / U9"),
    "V20 grip drops /4":             ("*", R(184.55, 75.55, 185.45, 76.45), "B -> F beside each switch pad; the T1n lanes' drops at R17 / R14 / U9"),
    "V20 grip drops /5":             ("*", R(187.45, 71.45, 188.35, 72.35), "B -> F beside each switch pad; the T1n lanes' drops at R17 / R14 / U9"),
    "V20 grip drops /6":             ("*", R(192.54, 40.15, 193.44, 41.05), "B -> F beside each switch pad; the T1n lanes' drops at R17 / R14 / U9"),
    "V20 grip drops /7":             ("*", R(193.95, 79.45, 194.85, 80.35), "B -> F beside each switch pad; the T1n lanes' drops at R17 / R14 / U9"),
    "V20 grip drops /8":             ("*", R(194.75, 46.55, 195.65, 47.45), "B -> F beside each switch pad; the T1n lanes' drops at R17 / R14 / U9"),
    "V20 grip drops /9":             ("*", R(195.45, 63.50, 196.35, 64.40), "B -> F beside each switch pad; the T1n lanes' drops at R17 / R14 / U9"),
    "V20 grip drops /10":            ("*", R(202.75, 71.35, 203.65, 72.25), "B -> F beside each switch pad; the T1n lanes' drops at R17 / R14 / U9"),
    "V20 grip drops /11":            ("*", R(204.45, 61.54, 205.35, 62.44), "B -> F beside each switch pad; the T1n lanes' drops at R17 / R14 / U9"),
    "V20 grip drops /12":            ("*", R(205.15, 53.54, 206.05, 54.44), "B -> F beside each switch pad; the T1n lanes' drops at R17 / R14 / U9"),
    "V20 grip drops /13":            ("*", R(205.85, 45.54, 206.75, 46.44), "B -> F beside each switch pad; the T1n lanes' drops at R17 / R14 / U9"),
    "V21 strip lane drops /1":       ("*", R(124.15, 41.20, 125.05, 42.10), "/PB's hop pair, PWR_EN at U12.12, CHG_PGOOD, CHG_TS at U10.1, CHG_FAST at R55.1"),
    "V21 strip lane drops /2":       ("*", R(127.15, 46.50, 128.05, 47.40), "/PB's hop pair, PWR_EN at U12.12, CHG_PGOOD, CHG_TS at U10.1, CHG_FAST at R55.1"),
    "V21 strip lane drops /3":       ("*", R(137.05, 42.00, 137.95, 42.90), "/PB's hop pair, PWR_EN at U12.12, CHG_PGOOD, CHG_TS at U10.1, CHG_FAST at R55.1"),
    "V21 strip lane drops /4":       ("*", R(148.05, 41.95, 148.95, 42.85), "/PB's hop pair, PWR_EN at U12.12, CHG_PGOOD, CHG_TS at U10.1, CHG_FAST at R55.1"),
    "V21 strip lane drops /5":       ("*", R(154.15, 47.55, 155.05, 48.45), "/PB's hop pair, PWR_EN at U12.12, CHG_PGOOD, CHG_TS at U10.1, CHG_FAST at R55.1"),
    "V21 strip lane drops /6":       ("*", R(162.15, 48.15, 163.05, 49.05), "/PB's hop pair, PWR_EN at U12.12, CHG_PGOOD, CHG_TS at U10.1, CHG_FAST at R55.1"),
    "V22 NOR hops /1":               ("*", R(62.45, 62.45, 63.35, 63.35), "NOR_IO0 / NOR_IO3 F hop pairs at U3, C7.2's GND via"),
    "V22 NOR hops /2":               ("*", R(64.05, 58.55, 64.95, 59.45), "NOR_IO0 / NOR_IO3 F hop pairs at U3, C7.2's GND via"),
    "V22 NOR hops /3":               ("*", R(66.35, 62.75, 67.25, 63.65), "NOR_IO0 / NOR_IO3 F hop pairs at U3, C7.2's GND via"),
    "V22 NOR hops /4":               ("*", R(68.25, 54.35, 69.15, 55.25), "NOR_IO0 / NOR_IO3 F hop pairs at U3, C7.2's GND via"),
    "V22 NOR hops /5":               ("*", R(68.75, 56.15, 69.65, 57.05), "NOR_IO0 / NOR_IO3 F hop pairs at U3, C7.2's GND via"),
    "V22 NOR hops /6":               ("*", R(69.35, 62.75, 70.25, 63.65), "NOR_IO0 / NOR_IO3 F hop pairs at U3, C7.2's GND via"),
    "V22 NOR hops /7":               ("*", R(70.55, 60.40, 71.45, 61.30), "NOR_IO0 / NOR_IO3 F hop pairs at U3, C7.2's GND via"),
    "V23 CC_SENSE hop /1":           ("*", R(64.35, 44.95, 65.25, 45.85), "CC_SENSE's B hop under the I2S lanes (x 64.8, y 52.4 -> 45.4)"),
    "V23 CC_SENSE hop /2":           ("*", R(64.35, 51.95, 65.25, 52.85), "CC_SENSE's B hop under the I2S lanes (x 64.8, y 52.4 -> 45.4)"),
    "V24 CC_SENSE drops /1":         ("*", R(113.45, 44.95, 114.35, 45.85), "CC_SENSE F -> B to R56.2 (x 113.9) and R57.2 (x 122.1), either side of the USB pair's B legs"),
    "V24 CC_SENSE drops /2":         ("*", R(121.65, 44.95, 122.55, 45.85), "CC_SENSE F -> B to R56.2 (x 113.9) and R57.2 (x 122.1), either side of the USB pair's B legs"),
    "V25 USB D4 end":                ("*", R(116.60, 46.55, 119.40, 48.65), "the pair's D4-end vias D- (117.05,48.2), D+ (118.95,47.0), C93.2's GND via"),
    "V26 U17 locals /1":             ("*", R(95.75, 35.15, 96.65, 36.05), "U17's /PB and PWR_EN F exits into T0, its GND via"),
    "V26 U17 locals /2":             ("*", R(98.40, 37.00, 99.30, 39.45), "U17's /PB and PWR_EN F exits into T0, its GND via"),
    "V27 U10 north hops /1":         ("*", R(155.70, 36.25, 156.60, 37.15), "CHG_EN2's F hop to R18, the LED K/A hops to U14, R28.2's +3V3"),
    "V27 U10 north hops /2":         ("*", R(157.60, 33.95, 158.50, 35.80), "CHG_EN2's F hop to R18, the LED K/A hops to U14, R28.2's +3V3"),
    "V27 U10 north hops /3":         ("*", R(158.40, 37.30, 159.30, 38.20), "CHG_EN2's F hop to R18, the LED K/A hops to U14, R28.2's +3V3"),
    "V27 U10 north hops /4":         ("*", R(161.40, 35.50, 162.30, 36.40), "CHG_EN2's F hop to R18, the LED K/A hops to U14, R28.2's +3V3"),
}

VIA_POINTS = {   # the vias each spot reserves, as placed on fix/trial/s5 (x, y, diameter, net)
    "V1 U12 PGND vias":              [(141.850, 40.200, 0.6, "GND"), (141.000, 40.600, 0.6, "GND"), (141.000, 39.800, 0.6, "GND")],
    "V2 USB-C via band /1":          [(115.600, 39.350, 0.8, "+5V")],
    "V2 USB-C via band /2":          [(118.250, 39.500, 0.5, "USB_D-"), (117.250, 40.500, 0.5, "USB_D-")],
    "V2 USB-C via band /3":          [(118.450, 42.800, 0.6, "+5V"), (117.600, 42.800, 0.6, "GND")],
    "V2 USB-C via band /4":          [(120.400, 39.350, 0.8, "+5V")],
    "V3 U12 EP":                     [(139.500, 40.600, 0.6, "GND"), (139.500, 39.800, 0.6, "GND"), (138.500, 40.600, 0.6, "GND"), (138.500, 39.800, 0.6, "GND"), (137.500, 40.600, 0.6, "GND"), (137.500, 39.800, 0.6, "GND"), (136.500, 40.600, 0.6, "GND"), (136.500, 39.800, 0.6, "GND")],
    "V4 U12 COUT /1":                [(140.550, 33.450, 0.6, "GND"), (139.850, 33.450, 0.6, "GND"), (138.450, 33.450, 0.6, "GND"), (137.750, 33.450, 0.6, "GND"), (136.250, 33.450, 0.6, "GND"), (135.550, 33.450, 0.6, "GND")],
    "V4 U12 COUT /2":                [(137.020, 37.000, 0.6, "+3V3"), (137.020, 36.300, 0.6, "+3V3")],
    "V4 U12 COUT /3":                [(139.155, 37.000, 0.6, "+3V3"), (139.155, 36.300, 0.6, "+3V3")],
    "V5 U12 input caps /1":          [(132.150, 39.400, 0.6, "GND")],
    "V5 U12 input caps /2":          [(139.450, 46.950, 0.6, "GND"), (138.050, 45.700, 0.6, "GND"), (136.600, 46.950, 0.6, "GND"), (135.100, 45.950, 0.6, "GND")],
    "V6 U10 EP + C57 /1":            [(150.050, 38.800, 0.6, "GND"), (150.050, 38.100, 0.6, "GND")],
    "V6 U10 EP + C57 /2":            [(157.400, 39.700, 0.6, "GND"), (157.400, 38.700, 0.6, "GND"), (156.900, 39.200, 0.6, "GND"), (156.400, 39.700, 0.6, "GND"), (156.400, 38.700, 0.6, "GND")],
    "V7 U10 GND vias /1":            [(155.500, 44.750, 0.6, "GND")],
    "V7 U10 GND vias /2":            [(157.650, 44.700, 0.6, "GND")],
    "V7 U10 GND vias /3":            [(159.550, 44.629, 0.6, "GND")],
    "V7 U10 GND vias /4":            [(162.400, 41.200, 0.6, "GND")],
    "V7 U10 GND vias /5":            [(163.450, 39.250, 0.6, "GND"), (163.450, 38.550, 0.6, "GND")],
    "V8 BAT drops /1":               [(154.500, 37.000, 0.8, "BAT"), (153.600, 37.000, 0.8, "BAT"), (152.700, 37.000, 0.8, "BAT")],
    "V8 BAT drops /2":               [(185.600, 40.000, 0.6, "BAT")],
    "V9 +5V drops /1":               [(147.000, 40.900, 0.6, "+5V")],
    "V9 +5V drops /2":               [(158.750, 41.950, 0.8, "+5V"), (158.750, 41.050, 0.8, "+5V")],
    "V10 U15/U13/C44/C66/C65 /1":    [(171.900, 42.200, 0.6, "GND")],
    "V10 U15/U13/C44/C66/C65 /2":    [(172.850, 39.800, 0.6, "+3V3")],
    "V10 U15/U13/C44/C66/C65 /3":    [(173.800, 42.200, 0.6, "GND")],
    "V10 U15/U13/C44/C66/C65 /4":    [(177.300, 35.100, 0.6, "GND")],
    "V10 U15/U13/C44/C66/C65 /5":    [(183.300, 38.900, 0.6, "GND"), (183.300, 38.300, 0.6, "GND")],
    "V10 U15/U13/C44/C66/C65 /6":    [(185.600, 36.750, 0.6, "GND")],
    "V11 I2C1 / GAUGE drops /1":     [(179.400, 41.250, 0.6, "I2C1_SCL"), (178.650, 41.800, 0.6, "I2C1_SDA"), (177.900, 41.250, 0.6, "I2C1_SCL")],
    "V11 I2C1 / GAUGE drops /2":     [(180.450, 38.950, 0.6, "+3V3")],
    "V11 I2C1 / GAUGE drops /3":     [(182.300, 42.350, 0.6, "GAUGE_ALRT")],
    "V12 B-neck staircase":          [(180.200, 45.550, 0.6, "BTN_A"), (179.500, 45.250, 0.6, "BTN_Y"), (178.800, 44.950, 0.6, "BTN_SELECT"), (178.100, 44.650, 0.6, "BTN_X"), (177.400, 44.350, 0.6, "BTN_B"), (176.700, 44.050, 0.6, "/power-ctrl/PWR_BTN"), (176.000, 43.750, 0.6, "BTN_VOLDOWN"), (175.300, 43.450, 0.6, "BTN_VOLUP"), (174.600, 43.150, 0.6, "BTN_R1")],
    "V13 SD support row /1":         [(44.600, 46.910, 0.6, "+3V3")],
    "V13 SD support row /2":         [(49.350, 50.850, 0.6, "+3V3")],
    "V13 SD support row /3":         [(53.700, 50.850, 0.6, "+3V3"), (52.850, 50.850, 0.6, "GND")],
    "V13 SD support row /4":         [(56.775, 50.850, 0.6, "+3V3"), (55.350, 50.850, 0.6, "+3V3")],
    "V13 SD support row /5":         [(59.350, 54.550, 0.6, "+3V3")],
    "V13 SD support row /6":         [(59.800, 50.850, 0.6, "GND")],
    "V13 SD support row /7":         [(62.200, 53.600, 0.6, "GND"), (61.800, 55.000, 0.6, "GND")],
    "V13 SD support row /8":         [(62.750, 50.850, 0.6, "+3V3")],
    "V14 IMU pocket /1":             [(93.550, 82.150, 0.6, "GND")],
    "V14 IMU pocket /2":             [(95.050, 79.850, 0.6, "GND"), (94.400, 79.300, 0.6, "+3V3")],
    "V14 IMU pocket /3":             [(95.250, 76.300, 0.6, "GND")],
    "V14 IMU pocket /4":             [(95.850, 85.850, 0.6, "GND")],
    "V14 IMU pocket /5":             [(97.850, 82.450, 0.6, "GND"), (96.850, 82.700, 0.6, "GND")],
    "V14 IMU pocket /6":             [(98.000, 78.550, 0.6, "+3V3")],
    "V14 IMU pocket /7":             [(101.950, 80.700, 0.6, "+3V3")],
    "V15 X1 stitch /1":              [(66.800, 63.200, 0.6, "GND")],
    "V15 X1 stitch /2":              [(69.800, 63.200, 0.6, "GND")],
    "V16 U2 core buck /1":           [(47.600, 80.150, 0.6, "GND"), (46.800, 80.150, 0.6, "GND")],
    "V16 U2 core buck /2":           [(48.300, 78.350, 0.6, "GND")],
    "V17 U6 boost /1":               [(98.300, 52.300, 0.6, "GND")],
    "V17 U6 boost /2":               [(99.480, 56.000, 0.6, "GND")],
    "V17 U6 boost /3":               [(101.200, 55.950, 0.6, "GND")],
    "V17 U6 boost /4":               [(103.900, 59.500, 0.6, "GND")],
    "V17 U6 boost /5":               [(105.100, 49.200, 0.6, "GND")],
    "V18 U9 / U20 EP /1":            [(44.750, 101.950, 0.6, "GND"), (44.750, 101.250, 0.6, "GND"), (44.050, 101.950, 0.6, "GND"), (44.050, 101.250, 0.6, "GND")],
    "V18 U9 / U20 EP /2":            [(185.100, 81.750, 0.6, "GND"), (185.100, 81.050, 0.6, "GND"), (184.400, 81.750, 0.6, "GND"), (184.400, 81.050, 0.6, "GND")],
    "V19 audio core /1":             [(35.950, 112.120, 0.6, "HP_R")],
    "V19 audio core /2":             [(38.400, 104.900, 0.6, "GND")],
    "V19 audio core /3":             [(39.400, 97.250, 0.6, "GND")],
    "V19 audio core /4":             [(42.150, 113.400, 0.6, "GND")],
    "V19 audio core /5":             [(42.900, 100.150, 0.6, "HP_L")],
    "V20 grip drops /1":             [(182.550, 82.600, 0.6, "I2S1_LRCK")],
    "V20 grip drops /2":             [(184.000, 78.950, 0.6, "I2S1_DOUT"), (182.900, 79.600, 0.6, "I2S1_BCLK")],
    "V20 grip drops /3":             [(184.100, 60.400, 0.6, "/charger/CHG_TS")],
    "V20 grip drops /4":             [(185.000, 76.000, 0.6, "SPK_EN")],
    "V20 grip drops /5":             [(187.900, 71.900, 0.6, "BTN_A")],
    "V20 grip drops /6":             [(192.990, 40.600, 0.6, "BTN_R1")],
    "V20 grip drops /7":             [(194.400, 79.900, 0.6, "BTN_Y")],
    "V20 grip drops /8":             [(195.200, 47.000, 0.6, "BTN_SELECT")],
    "V20 grip drops /9":             [(195.900, 63.950, 0.6, "BTN_X")],
    "V20 grip drops /10":            [(203.200, 71.800, 0.6, "BTN_B")],
    "V20 grip drops /11":            [(204.900, 61.990, 0.6, "/power-ctrl/PWR_BTN")],
    "V20 grip drops /12":            [(205.600, 53.990, 0.6, "BTN_VOLDOWN")],
    "V20 grip drops /13":            [(206.300, 45.990, 0.6, "BTN_VOLUP")],
    "V21 strip lane drops /1":       [(124.600, 41.650, 0.6, "/power-ctrl/PWR_BTN")],
    "V21 strip lane drops /2":       [(127.600, 46.950, 0.6, "/power-ctrl/PWR_BTN")],
    "V21 strip lane drops /3":       [(137.500, 42.450, 0.6, "PWR_EN")],
    "V21 strip lane drops /4":       [(148.500, 42.400, 0.6, "CHG_PGOOD")],
    "V21 strip lane drops /5":       [(154.600, 48.000, 0.6, "/charger/CHG_TS")],
    "V21 strip lane drops /6":       [(162.600, 48.600, 0.6, "CHG_FAST")],
    "V22 NOR hops /1":               [(62.900, 62.900, 0.6, "NOR_IO3")],
    "V22 NOR hops /2":               [(64.500, 59.000, 0.6, "NOR_IO0")],
    "V22 NOR hops /3":               [(66.800, 63.200, 0.6, "GND")],
    "V22 NOR hops /4":               [(68.700, 54.800, 0.6, "NOR_IO0")],
    "V22 NOR hops /5":               [(69.200, 56.600, 0.6, "NOR_IO3")],
    "V22 NOR hops /6":               [(69.800, 63.200, 0.6, "GND")],
    "V22 NOR hops /7":               [(71.000, 60.850, 0.6, "GND")],
    "V23 CC_SENSE hop /1":           [(64.800, 45.400, 0.6, "USBC_CC_SENSE")],
    "V23 CC_SENSE hop /2":           [(64.800, 52.400, 0.6, "USBC_CC_SENSE")],
    "V24 CC_SENSE drops /1":         [(113.900, 45.400, 0.6, "USBC_CC_SENSE")],
    "V24 CC_SENSE drops /2":         [(122.100, 45.400, 0.6, "USBC_CC_SENSE")],
    "V25 USB D4 end":                [(118.950, 47.000, 0.6, "USB_D+"), (118.000, 48.000, 0.6, "GND"), (117.050, 48.200, 0.6, "USB_D-")],
    "V26 U17 locals /1":             [(96.200, 35.600, 0.6, "/power-ctrl/PWR_BTN")],
    "V26 U17 locals /2":             [(98.850, 39.000, 0.6, "GND"), (98.850, 37.450, 0.6, "PWR_EN")],
    "V27 U10 north hops /1":         [(156.150, 36.700, 0.6, "/charger/CHG_EN2")],
    "V27 U10 north hops /2":         [(158.050, 35.350, 0.6, "/charger/CHG_EN2"), (158.050, 34.400, 0.6, "+3V3")],
    "V27 U10 north hops /3":         [(158.850, 37.750, 0.6, "/charger/CHG_LED_K")],
    "V27 U10 north hops /4":         [(161.850, 35.950, 0.6, "/charger/CHG_LED_A")],
}
