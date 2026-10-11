#!/usr/bin/env python3
"""ipc_check.py — current-capacity check of gb2's power copper (IPC-2221 curve fit, the conservative basis IPC-2152
refines): I = k * dT^0.44 * A^0.725, A in mil^2, k = 0.048 outer / 0.024 inner. Outer copper 1 oz (35 um), via
barrels 25 um plating (treated as inner conductors). For every power net: its narrowest track per layer (necks
under NECK mm long are reported separately - heat spreads out of a short neck, so they run cooler than the curve
says), the smallest via drill and how many vias it has, and the worst-case drop over its longest single run.

Run:  <kicad python> route/ipc_check.py [board]
"""
import pcbnew, os, sys, math
HERE = os.path.dirname(os.path.abspath(__file__))
TO = pcbnew.ToMM
DT = 10.0                     # allowed temperature rise, C
T_OUT, PLATING = 0.035, 0.025 # mm
NECK = 2.0                    # mm: shorter narrow segments are necks
BRANCH = 0.4                  # mm: power-net copper narrower than this is a signal-current branch (EN tie, sense tap,
                              # Miller tie, VINA / VDD pins, test-point stub) - listed for review, not held to the trunk load
RHO = 1.72e-8                 # ohm m
# worst-case current per net (A): see route/PLAN.md "Power budget"
LOAD = {"VBUS": 0.5, "SYS_SRC": 1.5, "SYS": 1.5, "BAT": 1.5, "/buck-boost/BB_L1": 1.8, "/buck-boost/BB_L2": 1.8,
        "+3V3": 0.6, "+5V_LCD": 0.15, "/lcd-boost/BST_SW": 0.6, "+1V2": 0.1}
AMP_FEED = 1.0                # the SYS branch to the amp alone

def amps(width_mm, thick_mm, k):
    a = width_mm * thick_mm / (0.0254 ** 2)
    return k * DT ** 0.44 * a ** 0.725

def via_amps(drill_mm):
    return amps(math.pi * drill_mm, PLATING, 0.024)

B = pcbnew.LoadBoard(os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "..", "gb2.kicad_pcb"))
print(f"{'net':20s} {'I(A)':>5s}  {'layer':5s} {'min w':>6s} {'I@min':>6s}  {'necks (w x len)':24s} {'vias':>4s} {'drill':>5s} {'I/via':>5s}")
bad, branches = [], []
for net, I in LOAD.items():
    tr = [t for t in B.GetTracks() if t.GetNetname() == net and t.Type() != pcbnew.PCB_VIA_T]
    vs = [t for t in B.GetTracks() if t.GetNetname() == net and t.Type() == pcbnew.PCB_VIA_T]
    for L in (pcbnew.F_Cu, pcbnew.B_Cu):
        if net == "SYS" and L == pcbnew.B_Cu:      # the back-side SYS east of Q1 is the amp feed alone
            amp_side = [t for t in tr if t.GetLayer() == L and TO(min(t.GetStart().x, t.GetEnd().x)) > 96.5]
            w_amp = min((TO(t.GetWidth()) for t in amp_side if TO(t.GetWidth()) >= BRANCH), default=0)
            if w_amp:
                c_amp = amps(w_amp, T_OUT, 0.048)
                print(f"{'SYS (amp feed)':20s} {AMP_FEED:5.2f}  {'B.Cu':5s} {w_amp:6.2f} {c_amp:6.2f}{'  << UNDER' if c_amp < AMP_FEED else ''}")
                if c_amp < AMP_FEED: bad.append("SYS amp feed")
            tr = [t for t in tr if t not in amp_side]
        allsegs = [(TO(t.GetWidth()), TO(t.GetLength())) for t in tr if t.GetLayer() == L]
        segs = [s for s in allsegs if s[0] >= BRANCH]
        thin = [s for s in allsegs if s[0] < BRANCH]
        if thin: branches.append((net, B.GetLayerName(L), len(thin), sum(l for _, l in thin), min(w for w, _ in thin)))
        if not segs: continue
        runs = [s for s in segs if s[1] >= NECK] or segs
        wmin = min(w for w, _ in runs); cap = amps(wmin, T_OUT, 0.048)
        necks = sorted({(w, round(l, 1)) for w, l in segs if l < NECK and w < wmin})
        neck_s = " ".join(f"{w:.2f}x{l:.1f}" for w, l in necks[:3]) + (" ..." if len(necks) > 3 else "")
        dmin = min((TO(v.GetDrill()) for v in vs), default=0)
        flag = "" if cap >= I else "  << UNDER"
        if cap < I: bad.append(net)
        print(f"{net.split('/')[-1]:20s} {I:5.2f}  {B.GetLayerName(L):5s} {wmin:6.2f} {cap:6.2f}  {neck_s:24s} {len(vs):4d} {dmin:5.2f} {via_amps(dmin) if dmin else 0:5.2f}{flag}")
# vias per layer change on the main trunks (current / per-via capacity)
print("\nthin branches (< %.1f mm; must carry signal current only):" % BRANCH)
for net, lay, n, l, w in branches:
    print(f"  {net.split('/')[-1]:10s} {lay:5s} {n:3d} segments, {l:5.1f} mm, narrowest {w:.2f} mm")
print("\nlayer changes on the trunks (vias in parallel vs the current through them):")
for name, net, I, pts in (("Q1 drain -> back (SYS)", "SYS", 1.5, [(96.2, 43.4), (96.2, 44.3)]),
                          ("back -> buck-boost input (SYS)", "SYS", 1.2, [(88.6, 37.55), (88.6, 38.45)]),
                          ("amp feed up at C30 (SYS)", "SYS", AMP_FEED, [(153.5, 54.05), (153.5, 54.9)]),
                          ("J7 -> back (VBUS, each side)", "VBUS", 0.5, [(97.65, 39.95)]),
                          ("back -> C36 (VBUS)", "VBUS", 0.5, [(106.5, 43.15)])):
    vs = [v for v in B.GetTracks() if v.Type() == pcbnew.PCB_VIA_T and v.GetNetname() == net and
          any(abs(TO(v.GetPosition().x) - x) < 0.05 and abs(TO(v.GetPosition().y) - y) < 0.05 for x, y in pts)]
    cap = sum(via_amps(TO(v.GetDrill())) for v in vs)
    print(f"  {name:34s} {len(vs)} via(s), {cap:.2f} A for {I:.2f} A{'  << UNDER' if cap < I else ''}")
    if cap < I: bad.append(name)
# drop over the long SYS feed to the amp (sum L/w of its back-side run)
amp = [t for t in B.GetTracks() if t.GetNetname() == "SYS" and t.Type() != pcbnew.PCB_VIA_T and t.GetLayer() == pcbnew.B_Cu
       and TO(min(t.GetStart().x, t.GetEnd().x)) > 96.5]
sq = sum(TO(t.GetLength()) / TO(t.GetWidth()) for t in amp)
r = RHO / (T_OUT * 1e-3) * sq
print(f"\namp feed (back, Q1 -> C30): {sum(TO(t.GetLength()) for t in amp):.0f} mm, {sq:.0f} squares = {r * 1e3:.0f} mOhm -> {r * AMP_FEED * 1e3:.0f} mV at {AMP_FEED:g} A peak")
print("\nRESULT:", "all within the 10 C curve" if not bad else "under-sized: " + ", ".join(bad))
