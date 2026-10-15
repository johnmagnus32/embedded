#!/usr/bin/env python3
"""ipc_check.py — current-capacity check of gb3's power copper (ported from gb2's; PLAN "Power budget").

IPC-2221 curve fit (the conservative basis IPC-2152 refines): I = k * dT^0.44 * A^0.725, A in mil^2, k = 0.048
outer / 0.024 inner, dT 10 C. Outer copper 1 oz (35 um); via barrels with JLC's 18 um hole plating, treated as inner
conductors (0.68 A for a 0.3 mm drill, 0.84 A for 0.4). Per power net and region (a track takes the first region its
midpoint lies in; each region carries its own worst-case current): the narrowest run per layer, the necks (narrow
segments under NECK mm: heat spreads out of a short neck, so they run cooler than the curve says), the branches
narrower than the net's BRANCH width (pin stubs, taps, Kelvin and test-point spurs: listed, not held to the load),
the layer changes the PLAN sized, and the drop over each trunk.

Run:  <kicad python> route/ipc_check.py [board]
"""
import pcbnew, os, sys, math
HERE = os.path.dirname(os.path.abspath(__file__))
TO = pcbnew.ToMM
DT = 10.0                     # allowed temperature rise, C
T_OUT, PLATING = 0.035, 0.018 # mm
NECK = 2.0                    # mm: shorter narrow segments are necks
RHO = 1.72e-8                 # ohm m
def R(x0, y0, x1, y1): return (x0, y0, x1, y1)
# net -> [(label, [boxes] or None for the rest of the net, worst-case A)]; PLAN "Power budget" (SYS: 2.7 A at R27,
# 2.2 A west, U12's input 1.87 A, U6 0.33 A, U9 <= 0.5 A peak; the inductor nets at their RMS, the peaks are the
# switchers' current limits)
LOAD = {
    "+5V":     [("D4 clamp / TP4 spurs", [R(117.5, 42.5, 119.5, 46.0), R(146.5, 40.8, 147.5, 47.5)], 0.05),
                ("VBUS -> C55 -> U10.13", None, 1.45)],
    "SYS_SRC": [("R26 / R18 tap", [R(158.5, 35.5, 160.2, 36.6)], 0.01), ("U10 -> C56 -> R27.1", None, 2.7)],
    "SYS":     [("INA226 Kelvin taps", [R(169.6, 36.0, 173.5, 39.8)], 0.01),
                ("GAIN_SLOT strap (U9.2)", [R(183.9, 74.7, 187.2, 79.95)], 0.01),
                ("U12 VINA / U17 VCC feeds", [R(134.3, 38.0, 136.8, 39.0), R(95.5, 38.7, 96.5, 39.4)], 0.01),
                ("R27.2 -> T2's corner", [R(168.0, 39.5, 169.6, 50.0)], 2.7),
                ("T2 west to the U12 branch", [R(140.0, 49.6, 169.6, 51.7)], 2.2),
                ("U12 branch -> C60 / C61 -> U12.10/11", [R(136.5, 41.0, 141.2, 50.2)], 1.87),
                ("east to U9 (C53 / C54)", [R(169.6, 46.5, 190.0, 83.0)], 0.5),
                ("TP6 stub", [R(149.0, 46.8, 150.1, 50.2)], 0.01),
                ("T3 west to U6 / U17, VINA", None, 0.5)],
    "BAT":     [("TP5 / gauge spurs", [R(151.5, 38.2, 153.4, 47.6), R(183.5, 38.4, 186.2, 40.5)], 0.01),
                ("CN1 -> C57 -> U10", None, 2.7)],
    "+0V9":    [("L1 -> C34 -> W3 -> U1", None, 0.8)],
    "LDOB":    [("U1.30 -> N5 -> U1.48/49", None, 0.4)],
    "LDOA":    [("the LDOA ring", None, 0.085)],
    "/core-buck/CORE_SW": [("U2.3 -> L1", None, 1.0)],
    "/buck-boost/BB_L1":  [("U12 -> L3", None, 1.9)],
    "/buck-boost/BB_L2":  [("L3 -> U12", None, 1.9)],
    "/backlight/BL_SW":   [("U6.3 -> L2 / D3", None, 1.44)],
    "BL_LED_P": [("D3 -> C45 -> the panel", None, 0.04)],
    "BL_LED_N": [("the panel -> R38", None, 0.04)],
    "/amp/SPK_P": [("U9.9 -> J3.1", None, 0.45)],
    "/amp/SPK_N": [("U9.10 -> J3.2", None, 0.45)],
}
BRANCH = {"LDOB": 0.15, "LDOA": 0.15, "BL_LED_P": 0.15, "BL_LED_N": 0.15, "+0V9": 0.5}   # default 0.4 mm
# layer changes the PLAN sized: (label, net, A, via spots)
VIAS = [("BAT drops at C57 (firmware cap 1.8 A; 2.7 A all-at-once)", "BAT", 1.8, [(152.7, 37.0), (153.6, 37.0), (154.5, 37.0)]),
        ("+5V at the VBUS pads", "+5V", 1.45, [(115.6, 39.35), (120.4, 39.35)]),
        ("+5V drops at C55 / U10.13", "+5V", 1.45, [(158.75, 41.05), (158.75, 41.95)])]

def amps(width_mm, thick_mm, k):
    a = width_mm * thick_mm / (0.0254 ** 2)
    return k * DT ** 0.44 * a ** 0.725
def via_amps(drill_mm):
    return amps(math.pi * drill_mm, PLATING, 0.024)
def region(net, t):
    m = ((TO(t.GetStart().x) + TO(t.GetEnd().x)) / 2, (TO(t.GetStart().y) + TO(t.GetEnd().y)) / 2)
    for label, boxes, I in LOAD[net]:
        if boxes is None or any(x0 <= m[0] <= x1 and y0 <= m[1] <= y1 for x0, y0, x1, y1 in boxes): return label, I

B = pcbnew.LoadBoard(os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "..", "gb3.kicad_pcb"))
tracks = [t for t in B.GetTracks() if t.Type() != pcbnew.PCB_VIA_T]
vias = [t for t in B.GetTracks() if t.Type() == pcbnew.PCB_VIA_T]
print(f"{'net / region':52s} {'I(A)':>5s}  {'layer':5s} {'min w':>6s} {'I@min':>6s}  {'necks (w x len)':22s} {'drop':>7s}")
bad, branches = [], []
for net in LOAD:
    tr = [t for t in tracks if t.GetNetname() == net]
    if not tr: print(f"{net:52s}  (no copper)"); continue
    br = BRANCH.get(net, 0.4)
    groups = {}
    for t in tr:
        label, I = region(net, t)
        groups.setdefault((label, I, t.GetLayer()), []).append(t)
    for (label, I, L), ts in sorted(groups.items(), key=lambda kv: (-kv[0][1], kv[0][0], kv[0][2])):
        segs = [(TO(t.GetWidth()), TO(t.GetLength())) for t in ts]
        main = [s for s in segs if s[0] >= br]; thin = [s for s in segs if s[0] < br]
        if thin: branches.append((net, label, B.GetLayerName(L), len(thin), sum(l for _, l in thin), min(w for w, _ in thin), I))
        if not main or I < 0.02: continue
        runs = [s for s in main if s[1] >= NECK]
        short = not runs                                  # only necks here (the run is on the other layer / in pads)
        runs = runs or main
        wmin = min(w for w, _ in runs); cap = amps(wmin, T_OUT, 0.048)
        necks = sorted({(w, round(l, 1)) for w, l in main if l < NECK and w < wmin})
        neck_s = " ".join(f"{w:.2f}x{l:.1f}" for w, l in necks[:3]) + (" ..." if len(necks) > 3 else "")
        drop = RHO / (T_OUT * 1e-3) * sum(l / w for w, l in main) * I        # V, over this region's copper in series
        flag = "" if cap >= I else ("  (necks only, %.1f mm max: run cooler)" % max(l for _, l in main)) if short else "  << UNDER"
        if cap < I and not short: bad.append(f"{net} ({label})")
        name = f"{net.split('/')[-1]}: {label}"
        print(f"{name[:52]:52s} {I:5.2f}  {B.GetLayerName(L):5s} {wmin:6.2f} {cap:6.2f}  {neck_s:22s} {drop * 1e3:5.0f}mV{flag}")
print("\nbranches (narrower than the net's trunk width; pin stubs and taps):")
for net, label, lay, n, l, w, I in branches:
    note = "signal-current spurs" if I < 0.3 else "pin necks" if l / n < 1.2 else "check"
    print(f"  {net.split('/')[-1]:10s} {label[:34]:34s} {lay:5s} {n:3d} segments, {l:5.1f} mm, narrowest {w:.2f} mm  ({note})")
print("\nlayer changes (vias in parallel vs the current through them):")
for name, net, I, pts in VIAS:
    vs = [v for v in vias if v.GetNetname() == net and
          any(abs(TO(v.GetPosition().x) - x) < 0.05 and abs(TO(v.GetPosition().y) - y) < 0.05 for x, y in pts)]
    cap = sum(via_amps(TO(v.GetDrill())) for v in vs)
    print(f"  {name:58s} {len(vs)} via(s), {cap:.2f} A for {I:.2f} A{'  << UNDER' if cap < I else ''}")
    if cap < I: bad.append(name)
for net, area, I in (("+3V3", R(134.0, 33.0, 148.0, 38.2), 1.4), ("+0V9", R(57.0, 72.0, 72.0, 86.0), 0.8)):
    vs = [v for v in vias if v.GetNetname() == net and TO(v.GetPosition().x) >= area[0] and TO(v.GetPosition().x) <= area[2]
          and TO(v.GetPosition().y) >= area[1] and TO(v.GetPosition().y) <= area[3]]
    cap = sum(via_amps(TO(v.GetDrill())) for v in vs)
    what = "U12's COUT -> In2" if net == "+3V3" else "the +0V9 caps -> the Z2 island"
    print(f"  {net} {what:53s} {len(vs)} via(s), {cap:.2f} A for {I:.2f} A{'  << UNDER' if cap < I else ''}")
    if cap < I: bad.append(f"{net} {what}")
print("\nRESULT:", "all within the 10 C curve" if not bad else "under-sized: " + "; ".join(bad))
sys.stdout.flush(); os._exit(0)
