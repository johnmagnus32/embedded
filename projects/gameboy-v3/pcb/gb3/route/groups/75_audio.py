# 75_audio -- the headphone path: U19 PCM5102A DAC -> anti-imaging R-C -> coupling caps -> U20 TPA6132A2 -> the jack
# Audio1, the DAC / amp control lines from U1 (S1), and the jack-detect (trial s4 audio + the second review's items).
# Routed before groups 80 / 85 so the button lanes go round it; route/channels.py's A1c / A1f follow this result.
#
# Order (second review): U19's and U20's local caps first, then the signal chain, then the long legs, so the maze cannot
#   box the charge-pump / LDO pins in (the maze-first trial did: CAPP / CAPM / VNEG).
# U19 locals (PCM5102A p.34 layout: CAPP / CAPM flying cap, VNEG and LDOO caps at their pins): CAPP -> C81.1, CAPM ->
#   C81.2, VNEG -> C82.1, LDOO -> C77.1, DVDD / AVDD / CPVDD (+3V3) to C74 / C75 (+ C79 bulk) / C76; C74.2's GND linked
#   to C78.2 (its thermal starved between the CAPP / VNEG lanes). I2S in: the 33R series R32 / R34 / R33 (group 70's
#   DAC branch) -> BCK / DIN / LRCK at U19.13-15.
# U20 locals (TPA6132A2 p.18): the charge-pump flying cap C87 (CPP / CPN), HPVDD -> C89, HPVSS -> C88, VDD -> C91; the
#   GND pins into the exposed pad (group 05's 2 x 2 vias).
# Chain (A6: amp channel R carries LEFT): U19 OUTL -> R36 -> C84 shunt + C86 -> U20.4 INR-; OUTR -> R35 -> C83 + C85 ->
#   U20.1 INL-. HP_L (U20.5 OUTR) up a via at U20's NW corner and SW on F to the tip pad; HP_R (U20.16 OUTL) SW on B,
#   west along y 105.75, down x 35.95 to a via beside the ring pad; SGND (U20.15) parallel outside HP_R to a via above
#   the sleeve pad (TPA6132A2: SGND to the jack's sleeve).
# Control (S1, west-bound; static): DAC_XSMT (U1.85) west along y 92.9, then y 92.17 between R33 and C77 into U19.17, R44's
#   10k pull-down branched off; both pass U1's bulk caps C37 / C38; HP_EN (U1.86) hops on F over DAC_XSMT's lane, then
#   to R37 (pull-down) and U20.13. JACK_DET: group 70's F end in W4 -> F round the audio block -> a via at R49.2; JACK_SW: the jack's
#   tip-switch pad -> a via -> R49.1.
W, WA = 0.15, 0.2

# ---- U19 locals
T("/dac/PCM_CAPP", "B", WA, [P("U19.2"), (40.6, P("U19.2")[1]), (40.6, 95.5), (40.2, 95.9)])
T("/dac/PCM_CAPM", "B", WA, [P("U19.4"), (41.1, P("U19.4")[1]), (39.95, 93.33)])        # 0.16 past C82.1's corner
T("/dac/PCM_VNEG", "B", WA, [P("U19.5"), (P("C82.1")[0], P("U19.5")[1])])
T("/dac/PCM_LDOO", "B", WA, [P("U19.18"), (P("C77.1")[0], P("U19.18")[1])])
T("+3V3", "B", WA, [P("U19.1"), (P("U19.1")[0], P("C74.1")[1])])                      # DVDD -> C74
T("GND", "B", 0.3, [P("C74.2"), (P("C78.2")[0] - 0.8, P("C74.2")[1]), P("C78.2")])     # C74.2 -> C78.2
T("+3V3", "B", WA, [P("U19.8"), (P("C75.1")[0], P("U19.8")[1])])                      # AVDD -> C75
T("+3V3", "B", 0.3, [P("C75.1"), (P("C75.1")[0], 87.73), P("C79.1")])                 # -> C79 bulk
T("+3V3", "B", WA, [P("U19.20"), (49.1, P("U19.20")[1]), (49.8, 94.83)])              # CPVDD -> C76
T("/dac/DAC_BCK", "B", W, [P("R32.2"), (48.38, P("U19.13")[1]), P("U19.13")])
T("/dac/DAC_DIN", "B", W, [P("R34.2"), (49.36, P("U19.14")[1]), P("U19.14")])
T("/dac/DAC_LRCK", "B", W, [P("R33.2"), (49.39, P("U19.15")[1]), P("U19.15")])

# CPGND (U19.3) between the flying-cap lanes: its own GND via under the body, east of the pin; DAC_OUTR runs under the
# body at x 44.2, clear of it, and out past C78 / C74 to the south chain (R35)
T("GND", "B", WA, [P("U19.3"), (43.4, P("U19.3")[1])]); VIA("GND", 43.4, P("U19.3")[1])
T("/dac/DAC_OUTR", "B", WA, [P("U19.7"), (43.9, P("U19.7")[1]), (44.2, P("U19.7")[1] + 0.3), (44.2, 93.6), (42.9, 94.9),
                             (42.9, 95.95), (42.5, 96.35), (41.25, 96.35), (40.6, 97.0), (40.6, 101.45), (40.45, 101.6),
                             (38.1, 101.6), P("R35.1")])

# ---- U20 locals
T("/headphone-amp/TPA_CPP", "B", WA, [P("U20.11"), (P("C87.1")[0], P("U20.11")[1])])
T("/headphone-amp/TPA_CPN", "B", WA, [P("U20.9"), (P("C87.2")[0], P("U20.9")[1])])
T("/headphone-amp/TPA_HPVDD", "B", WA, [P("U20.12"), (46.6, P("U20.12")[1]), (47.475, 103.225)])
T("/headphone-amp/TPA_HPVSS", "B", WA, [P("U20.8"), (P("U20.8")[0], 99.35), (45.6, 98.9)])
T("+3V3", "B", WA, [P("U20.14"), P("C91.1")])
for pin, end in (("2", (43.6, None)), ("3", (43.6, None)), ("10", (45.2, None))):      # GND pins into the exposed pad
    T("GND", "B", WA, [P("U20." + pin), (end[0], P("U20." + pin)[1])])
for pin in ("6", "7"):
    T("GND", "B", WA, [P("U20." + pin), (P("U20." + pin)[0], 100.8)])

# ---- the chain (trial s4 audio)
T("DAC_OUTL_F", "B", WA, [P("R36.2"), P("C86.1")]); T("DAC_OUTL_F", "B", WA, [P("C84.1"), (39.4, 100.85)])
T("DAC_OUTR_F", "B", WA, [P("R35.2"), P("C85.1")]); T("DAC_OUTR_F", "B", WA, [P("C83.1"), (39.4, 102.35)])
T("/headphone-amp/AMP_INL", "B", WA, [P("C86.2"), P("U20.4")])
T("/headphone-amp/AMP_INR", "B", WA, [P("C85.2"), P("U20.1")])
VIA("GND", 39.4, 97.25); T("GND", "B", 0.3, [P("C84.2"), (39.4, 97.25)])
VIA("GND", 38.4, 104.9); T("GND", "B", 0.3, [P("C83.2"), (38.4, 104.9)])
T("HP_L", "B", 0.25, [P("U20.5"), (43.65, 100.15), (42.9, 100.15)]); VIA("HP_L", 42.9, 100.15)
T("HP_L", "F", 0.25, [(42.9, 100.15), (P("Audio1.2")[0] - 0.5, P("Audio1.2")[1])])   # straight into pad 2 (step 8: jack +2.5 x)
T("HP_R", "B", 0.25, [P("U20.16"), (43.65, 103.5), (41.4, 105.75), (38.45, 105.75), (38.45, 112.12)]); VIA("HP_R", 38.45, 112.12)
T("HP_R", "F", 0.3, [(38.45, 112.12), P("Audio1.3")])                         # step 8: the jack +2.5 x (the via east of pad 3)
T("GND", "B", W, [P("U20.15"), (44.15, 103.5), (42.15, 105.5), (42.15, 113.4)]); VIA("GND", 42.15, 113.4)
T("GND", "F", 0.25, [(42.15, 113.4), (42.15, P("Audio1.1")[1]), P("Audio1.1")])

# ---- control lines out of S1 (west-bound)
# DAC_XSMT: south past U1's bulk caps C37 / C38, west along y 92.9, up between R33 and C77 into U19.17; R44 off the corner
T("DAC_XSMT", "B", W, [(P("U1.85")[0], 84.81), (P("U1.85")[0], 92.9), (53.33, 92.9), (52.6, 92.17), (49.0, 92.17), P("U19.17")])
T("DAC_XSMT", "B", W, [(53.33, 92.9), (53.33, P("R44.1")[1]), P("R44.1")])
# HP_EN: west north of C37 / C38 (y 88.3), then an F hop over DAC_XSMT's lane west of C37
T("HP_EN", "B", W, [(P("U1.86")[0], 84.81), (P("U1.86")[0], 88.3), (57.4, 88.3), (57.4, 92.15)]); VIA("HP_EN", 57.4, 92.15)
T("HP_EN", "F", W, [(57.4, 92.15), (57.4, 93.65)]); VIA("HP_EN", 57.4, 93.65)

# ---- the long legs (maze; DAC_OUTR is drawn above)
m = MAZE((30.5, 76.0, 66.0, 117.3), limit=3000000)
AUTOROUTE(m, "/dac/DAC_OUTL", "U19.6", "R36.1", w=WA, layers="B")      # DAC OUTL -> the north chain (A6)
AUTOROUTE(m, "HP_EN", (57.4, 93.65, "B"), "R37.1", w=W, layers="B")
AUTOROUTE(m, "HP_EN", "R37.1", "U20.13", w=W, layers="B")
AUTOROUTE(m, "/jack/JACK_SW", "Audio1.4", "R49.1", w=W, layers="FB", via_cost=20)
AUTOROUTE(m, "JACK_DET", (53.85, 77.85, "B"), "R49.2", w=W, layers="FB", via_cost=20)   # from group 70's hop via
