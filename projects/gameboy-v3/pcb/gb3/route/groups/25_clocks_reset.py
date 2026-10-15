# 25_clocks_reset -- the 24 MHz crystal (N3), the 32.768 kHz RTC crystal and the reset line (step-3 escape + trial s4 ne).
#
# HOSC (B, 0 vias, DRU): U1.22 HOSC_OUT straight north into X1.3, on to C4.1; U1.23 HOSC_IN north, east along y 67.35
#   (0.18 mm south of X1.2) into X1.1, on to C3.1. Both stay inside N3, over group 05's In2 GND island (it runs 0.21 mm
#   past U1's pin tips). X1 GND guard on B: C4.2 and X1.4 to the island's two stitch vias, X1.2 -> X1.4 under the
#   crystal, C3.2 -> X1.4. It cannot close on the east: C3 sits 0.08 mm from the N4 lane (PWR_BTN_3V3, group 60).
# RTC (F): group 20 lands RTC_IN / RTC_OUT on X2.1 / X2.2 from below; here each pad's load cap, outward on F (K2, the
#   SC-20S "no pattern" area, is between the crystal's pads).
# RESET (0.2 mm): U1.27's 0.5/0.3 island via (group 20) -> R11.1 (pull-up to LDOA) -> C31.1 on F -> F riser x 72.45
#   north (NE1) -> via (72.45, 58.5) -> B north -> SW1.1 (the reset button; SW1.1/SW1.2 are one contact, jumper pad group).
W = 0.15

# ---- HOSC (N3)
T("HOSC_OUT", "B", W, [(P("U1.22")[0], 68.69), (P("U1.22")[0], 65.2), (67.3, 65.2)])
T("HOSC_OUT", "B", W, [(P("C4.1")[0], 64.6), P("C4.1")])
T("HOSC_IN", "B", W, [(P("U1.23")[0], 68.69), (P("U1.23")[0], 67.35), (P("X1.1")[0], 67.35), (P("X1.1")[0], 66.9)])
T("HOSC_IN", "B", W, [(70.3, P("C3.1")[1]), P("C3.1")])
# X1 GND guard (B)
T("GND", "B", 0.2, [P("C4.2"), (66.97, P("C4.2")[1]), (66.80, 63.20)])
T("GND", "B", 0.2, [(69.80, 64.6), (69.80, 63.20)])
T("GND", "B", 0.2, [(68.27, 65.95), (68.77, 65.45), (69.15, 65.45)])
T("GND", "B", 0.2, [P("C3.2"), (70.55, P("C3.2")[1]), (70.40, 65.53)])

# ---- RTC load caps (F)
T("RTC_IN", "F", W, [P("X2.1"), P("C5.1")])
T("RTC_OUT", "F", W, [P("X2.2"), P("C6.1")])

# ---- RESET
T("/soc/RESET", "F", 0.2, [P("R11.1"), (72.10, P("R11.1")[1]), P("C31.1")])
T("/soc/RESET", "F", 0.2, [(72.5, 67.2), (72.45, 66.9), (72.45, 58.5)]); VIA("/soc/RESET", 72.45, 58.5)
T("/soc/RESET", "B", 0.2, [(72.45, 58.5), (72.45, 52.0)])
T("/soc/RESET", "B", 0.2, [(72.45, 52.0), (72.45, 44.1), P("SW1.1")])    # NE2's west lane: north, then 45 deg to SW1.1
