# 60_east -- U1's east side: the console, the north-east risers to U17 / U6 (A1 plain swap), the E1 escapes and the IMU
# (step-3 escape + trials s4 ne / imu).
#
# Console (UART0, PE2/PE3): U1.33 / U1.35 east to the 22R series R46 / R45 at U1's NE corner, on to TP2 / TP1.
# NE1 risers (B): PWR_BTN_3V3 (U1.31 PE13, Power 0.2) up the N4 lane x 71.6 (0.18 mm from C3.1), then x 73.0; PSHOLD
#   (U1.36) x 77.2; BL_CTRL (U1.37 PE10 / PWM4) x 77.6. They fan out in NE2 (B only): PWR_BTN_3V3 -> R40.1 (the /PB_OUT
#   pull-up) -> U17.8, PSHOLD -> U17.4, BL_CTRL -> R51.1 (100k pull-down) -> U6.2 (TPS61165 CTRL) under U6's body.
#   U17's and U6's local links first (VREF cap, PWR_EN / /PB vias, Rset Kelvin, COMP).
# E1: TOUCH_RST / TOUCH_INT east to the touch FPC1 (pins 1 / 2 since the pre-fab rotation), EXP_INT east to R29.1 (pull-up) and U16.1 (PCA9555
#   INT); SPK_EN east to x 101 (group 80 takes it on through M1 / T1 to R14 in the right grip).
# IMU (P2 + A7, B only, 0 vias): U7's 14 pads, C48 / C49 / R13 / R41 / R42 and their plane vias, then the ribbon that
#   peels off the bottom of E1, westmost first.
W = 0.15

# ---- console
T("/soc/UART0_RX", "B", W, [P("U1.33"), (73.2, P("U1.33")[1]), P("R46.1")])
T("/soc/UART0_TX", "B", W, [P("U1.35"), (P("R45.1")[0], P("U1.35")[1]), P("R45.1")])
T("/soc/UART0_RX_PAD", "B", W, [P("R46.2"), (P("R46.2")[0], 66.95), P("TP2.1")])
T("/soc/UART0_TX_PAD", "B", W, [P("R45.2"), (74.85, 66.4 + (P("TP1.1")[0] - 74.85)), P("TP1.1")])

# ---- NE1 risers and the E1 escapes
T("PWR_BTN_3V3", "B", 0.2, [(P("U1.31")[0], 68.69), (70.3, 67.75), (71.6, 67.75), (71.6, 61.6), (73.0, 60.2), (73.0, 52.0)])
T("PSHOLD", "B", W, [P("U1.36"), (77.2, P("U1.36")[1]), (77.2, 52.0)])
T("BL_CTRL", "B", W, [P("U1.37"), (77.6, P("U1.37")[1]), (77.6, 52.0)])
for pin, n in ((38, "TOUCH_RST"), (39, "TOUCH_INT"), (40, "EXP_INT"), (41, "SPK_EN")):
    T(n, "B", W, [P("U1.%d" % pin), (101.0, P("U1.%d" % pin)[1])])

# ---- NE2: U17 / U6 locals, then the risers' fan-out (trial s4 ne)
# ---- U17 locals (B): VREF cap, the two signals that leave U17 on F (/PB, PWR_EN), GND
T("/power-ctrl/STM_VREF", "B", 0.2, [(94.6, 37.4), (95.0, 37.75), (95.55, 37.95), (96.22, 37.95)])
T("PWR_EN", "B", 0.15, [(97.98, 37.45), (98.85, 37.45)]); VIA("PWR_EN", 98.85, 37.45)
T("/power-ctrl/PWR_BTN", "B", 0.15, [(96.22, 36.45), (96.22, 35.6)]); VIA("/power-ctrl/PWR_BTN", 96.22, 35.6)
T("GND", "B", 0.2, [(97.98, 38.95), (98.85, 38.95)]); VIA("GND", 98.85, 38.95)
# pre-fab (fab review fpic-1 / fpic-2, 2026-10-15): #SR tied to /PB and C_SRD to C97 (on F), both through 0.5 / 0.3 vias in
# the 0.86 mm channel between U17's pad rows (B is full west of the pins: PSHOLD wraps pin 5, VREF / SYS box pin 2). SR's F
# leg joins /PB's F run on its diagonal. PSHOLD's 100k pull-up R58 sits between the PWR_BTN_3V3 lane and the PSHOLD run
# (its +3V3 pad gets a plane via in fanout.py); TP13 hangs off the PSHOLD diagonal on a 45 deg stub.
T("/power-ctrl/PWR_BTN", "B", 0.15, [P("U17.2"), (97.1, 38.45)]); VIA("/power-ctrl/PWR_BTN", 97.1, 38.45, d=0.5, h=0.3)
T("/power-ctrl/PWR_BTN", "F", 0.15, [(97.1, 38.45), (97.6, 38.45), (98.1, 37.95), (98.1, 35.9)])
T("/power-ctrl/STM_CSRD", "B", 0.15, [P("U17.5"), (97.1, 36.95)]); VIA("/power-ctrl/STM_CSRD", 97.1, 36.95, d=0.5, h=0.3)
T("/power-ctrl/STM_CSRD", "F", 0.15, [(97.1, 36.95), P("C97.1")])
T("PSHOLD", "B", W, [P("R58.1"), (P("R58.1")[0], 36.65)])
T("PSHOLD", "B", W, [P("TP13.1"), (88.025, 41.025)])
# ---- U6 locals (B): FB -> Rset, Rset GND Kelvin to U6.4 under the SOT-23 (west of BL_CTRL's lane, BOM #33), COMP
T("BL_LED_N", "B", 0.25, [(102.35, 53.45), (102.40, 53.15), (102.40, 51.83)])
T("GND", "B", 0.2, [(102.40, 50.97), (102.95, 50.97), (103.25, 51.27), (103.25, 55.05), (102.95, 55.35), (102.35, 55.35)])
T("/backlight/BL_COMP", "B", 0.2, [(102.35, 54.40), (101.20, 54.40), (100.60, 55.00), (100.32, 55.00)])
m = MAZE((66.0, 32.6, 112.0, 88.0))
# ---- NE corridor west to east (RESET, the west lane, is group 25's): PWR_BTN_3V3 (x 73.0) and PSHOLD (x 77.2) as one
#      parallel 45 deg pair (the trial's maze paths, fixed: a fresh maze run boxes PSHOLD in), BL_CTRL (x 77.6) east
T("PWR_BTN_3V3", "B", 0.2, [(73.0, 52.0), (89.95, 35.05), (95.95, 35.05), (96.0, 35.0), (97.75, 35.0), P("R40.1")])
T("PWR_BTN_3V3", "B", 0.2, [P("R40.1"), (98.4, 35.05), (98.8, 35.45), (98.8, 36.6), (98.45, 36.95), P("U17.8")])   # from the east (pre-fab: the channel is SR / CSRD's)
T("PSHOLD", "B", W, [(77.2, 52.0), (77.2, 51.85), (92.4, 36.65), (95.15, 36.65), (95.5, 37.0), (95.5, 37.25), (95.7, 37.45), P("U17.4")])
AUTOROUTE(m, "BL_CTRL", (77.6, 52.0, "B"), "R51.1", w=0.15, layers="B", via_cost=40, through=[(100.75, 50.8, "B")])
AUTOROUTE(m, "BL_CTRL", "R51.1", "U6.2", w=0.15, layers="B", via_cost=40, through=[(103.85, 50.8, "B"), (103.85, 54.40, "B")])   # from the pad (2nd review)

# ---- E1 east (B): the touch FPC and the expander's INT. The three turn NE in step (TOUCH_RST at x 101.0, TOUCH_INT
#      101.4, EXP_INT 101.8: 0.57 mm c-c on the diagonals). Both touch lanes pass NORTH of FPC1's pad row (RST at y
#      68.9, INT at 69.2) and turn south into pins 1 / 2, so pads 3-6 (SDA, SCL, GND, +3V3) keep their north side for
#      their own vias. EXP_INT runs north of U16's decaps C43 / C67 (y 71.6) so their GND
#      pads keep both thermal spokes, then drops into R29.1 and on to U16.1 west of FPC1's tab. SPK_EN stays the south lane (group 80).
T("TOUCH_RST", "B", W, [(101.0, P("U1.38")[1]), (104.65, 68.9), (120.3, 68.9), (121.0, 69.6), P("FPC1.1")])
T("TOUCH_INT", "B", W, [(101.0, P("U1.39")[1]), (101.4, P("U1.39")[1]), (105.15, 69.2), (119.5, 69.2), (120.0, 69.7), P("FPC1.2")])
T("EXP_INT", "B", W, [(101.0, P("U1.40")[1]), (101.8, P("U1.40")[1]), (103.55, 71.6), (110.9, 71.6), P("R29.1")])
T("EXP_INT", "B", W, [P("R29.1"), (111.6, 72.55), (112.05, 73.0), (112.05, 75.43), P("U16.1")])   # west of FPC1's tab (pad 7)

# ---- IMU (trial s4 imu)
# ---- supply (explicit): pocket A (INT1|INT2): one +3V3 via shared by C48/C49, GND vias north of C48 and south of C49
VIA("+3V3", 94.40, 79.30)
T("+3V3", "B", 0.2, [(94.40, 78.00), (94.40, 80.75)])                     # C48.1 - via - C49.1
T("+3V3", "B", 0.2, [(95.85, 79.03), (95.85, 78.45), (95.40, 78.00), (94.40, 78.00)])   # U7.8 VDD -> C48.1
T("+3V3", "B", 0.2, [(95.67, 80.70), (94.40, 80.70)])                     # U7.5 VDDIO -> C49.1 (pad 80.35-81.15)
VIA("GND", 95.25, 76.30); T("GND", "B", 0.2, [(94.40, 76.60), (94.95, 76.60), (95.25, 76.30)])   # C48.2 (x+y >= 171.33: east of RGB_DE on F)
VIA("GND", 93.55, 82.15); T("GND", "B", 0.2, [(94.40, 82.15), (93.55, 82.15)])   # C49.2 (west of the pad)
# U7.6/7 GND: strap, then under the body to U7.3; S row strap 1-3; two GND vias south (free arc)
# U7.6/7 GND: their own via at the pads' west ends (no copper under the LGA body)
VIA("GND", 95.05, 79.85); T("GND", "B", 0.2, [(95.67, 79.70), (95.20, 79.70), (95.05, 79.85)])
T("GND", "B", 0.2, [(95.67, 80.20), (95.40, 80.20), (95.05, 79.85)])
# S row GND (1-3): stubs out of the pads' outer ends, strap outside the body, two vias (free arc)
for x in (96.35, 96.85, 97.35): T("GND", "B", 0.2, [(x, 81.37), (x, 81.95)])
T("GND", "B", 0.2, [(96.35, 81.95), (97.35, 81.95)])
VIA("GND", 96.85, 82.70); T("GND", "B", 0.2, [(96.85, 81.95), (96.85, 82.70)])
VIA("GND", 97.85, 82.45); T("GND", "B", 0.2, [(97.35, 81.95), (97.85, 82.45)])
# pocket B (INT2|SCL): R41.2 and CS share a +3V3 via
VIA("+3V3", 98.00, 78.55)
T("+3V3", "B", 0.2, [(97.50, 77.65), (97.85, 78.00), (98.00, 78.25), (98.00, 78.55)])
T("+3V3", "B", 0.2, [(97.53, 79.70), (97.80, 79.70), (98.00, 79.50), (98.00, 78.55)])
VIA("GND", 95.85, 85.85); T("GND", "B", 0.2, [(95.85, 84.95), (95.85, 85.85)])  # R13.2
VIA("+3V3", 101.95, 80.70); T("+3V3", "B", 0.2, [(101.05, 80.70), (101.95, 80.70)])  # R42.2
m = MAZE((70.0, 66.0, 106.0, 90.0))
# ---- ribbon (peels off E1's bottom, westmost first)
AUTOROUTE(m, "IMU_INT1", "U1.45", "R13.1", w=0.15, layers="B", through=[(92.9, 75.9, "B"), (92.9, 83.45, "B")])
AUTOROUTE(m, "IMU_INT1", "R13.1", "U7.4", w=0.15, layers="B")
AUTOROUTE(m, "IMU_INT2", "U1.44", "U7.9", w=0.15, layers="B", through=[(95.3, 74.95, "B"), (96.35, 76.4, "B")])
AUTOROUTE(m, "I2C2_SCL", "U1.43", "U7.13", w=0.15, layers="B", through=[(98.6, 74.55, "B"), (98.6, 79.6, "B")])
AUTOROUTE(m, "I2C2_SCL", "R41.1", (97.5, 74.55, "B"), w=0.15, layers="B")
AUTOROUTE(m, "I2C2_SDA", "U1.42", "R42.1", w=0.15, layers="B", through=[(99.15, 74.15, "B"), (99.15, 79.9, "B")])
AUTOROUTE(m, "I2C2_SDA", "R42.1", "U7.14", w=0.15, layers="B")
