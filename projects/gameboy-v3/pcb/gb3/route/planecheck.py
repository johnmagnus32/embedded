#!/usr/bin/env python3
"""planecheck.py — return-path check of gb3's reference planes (ported from gb2's; run after fanout + pour).

Every signal track is sampled every 0.1 mm and checked against the plane it references (PLAN "Stackup and layer
rules"): F.Cu tracks against In1 (GND), B.Cu tracks against In2 (+3V3, its GND islands, the +0V9 island). Reported:
  * per track, the longest stretch with no plane copper under it (a via antipad crossing ~0.5-0.7 mm is normal; a
    slot is longer); the RGB field's merged antipads are the plan's measured exception (RGB_DCLK, 1.1 mm)
  * every B.Cu segment that crosses an In2 island edge (its samples see two references); only DC or slow lines may
    (PLAN: SYS, +0V9, PWR_BTN_3V3, BL_CTRL, BL_LED_P / BL_LED_N, CC_SENSE at its via; + the DC rails and static
    lines), the rest are flagged; nets on two references in separate segments are listed for information

Run:  <kicad python> route/planecheck.py [board] [--min 1.0]
"""
import pcbnew, os, sys, math
HERE = os.path.dirname(os.path.abspath(__file__)); TO = pcbnew.ToMM
args = [a for a in sys.argv[1:] if not a.startswith("--")]
MIN = float(sys.argv[sys.argv.index("--min") + 1]) if "--min" in sys.argv else 1.0
B = pcbnew.LoadBoard(os.path.abspath(args[0]) if args else os.path.join(HERE, "..", "gb3.kicad_pcb"))
pcbnew.ZONE_FILLER(B).Fill(B.Zones())
PLANE_NETS = {"GND", "+3V3", "+0V9"}
SLOW_OK = {"SYS", "+0V9", "PWR_BTN_3V3", "BL_CTRL", "/backlight/BL_LED_P", "/backlight/BL_LED_N", "BL_LED_P", "BL_LED_N",
           "USBC_CC_SENSE",                                          # the PLAN's list
           "+5V", "BAT", "SYS_SRC", "LDOA", "LDOB", "PWR_EN", "/soc/RESET",   # DC rails and static lines
           "/buck-boost/BB_FB"}          # the TPS63021 divider tap: DC, stepping from its GND island onto its own +3V3 output
def fill(layer, nets):
    out = []
    for z in B.Zones():
        if z.GetIsRuleArea() or not z.IsOnLayer(layer) or z.GetNetname() not in nets: continue
        out.append(((z.GetNetname(), z.GetZoneName() or "plane"), z.GetFilledPolysList(layer), z.GetAssignedPriority()))
    return sorted(out, key=lambda r: -r[2])                    # islands (priority 2) before the plane they sit in
REF = {pcbnew.F_Cu: fill(pcbnew.In1_Cu, {"GND"}), pcbnew.B_Cu: fill(pcbnew.In2_Cu, {"+3V3", "GND", "+0V9"})}
def under(layer, x, y):
    v = pcbnew.VECTOR2I(int(pcbnew.FromMM(x)), int(pcbnew.FromMM(y)))
    for n, ps, _ in REF[layer]:
        if ps.Contains(v): return n
    return None
gaps, per, cross = [], {}, []
for t in B.GetTracks():
    if t.Type() == pcbnew.PCB_VIA_T or t.GetLayer() not in REF or t.GetNetname() in PLANE_NETS: continue
    a, b = t.GetStart(), t.GetEnd(); ax, ay, bx, by = TO(a.x), TO(a.y), TO(b.x), TO(b.y)
    L = math.hypot(bx - ax, by - ay); n = max(1, int(L / 0.1))
    run = best = 0.0; where = None; seen = {}
    for k in range(n + 1):
        x, y = ax + (bx - ax) * k / n, ay + (by - ay) * k / n
        p = under(t.GetLayer(), x, y)
        if p:
            run = 0.0; seen.setdefault(p, (round(x, 2), round(y, 2)))
            if t.GetLayer() == pcbnew.B_Cu: per.setdefault(t.GetNetname(), {}).setdefault(p, (round(x, 2), round(y, 2)))
        else:
            run += L / n
            if run > best: best, where = run, (x, y)
    if best >= MIN: gaps.append((best, t.GetNetname(), B.GetLayerName(t.GetLayer()), where))
    if t.GetLayer() == pcbnew.B_Cu and len(seen) > 1: cross.append((t.GetNetname(), seen))
gaps.sort(reverse=True)
print(f"plane gaps >= {MIN} mm under signal tracks: {len(gaps)}")
for g, n, l, w in gaps[:40]: print(f"  {g:5.2f} mm  {l:5s} {n:28s} near ({w[0]:.2f},{w[1]:.2f})")
bad = sorted({n for n, _ in cross if n not in SLOW_OK})
print(f"B.Cu segments crossing an In2 island edge: {len(cross)} on {len({n for n, _ in cross})} nets "
      f"({len(bad)} not on the DC / slow list)")
done = set()
for n, seen in cross:
    if n in done: continue
    done.add(n)
    print(f"  {'FLAG ' if n in bad else 'ok   '}{n:28s} " + "; ".join(f"{z[1][:30]} at {xy}" for z, xy in seen.items()))
mixed = sorted(n for n, s in per.items() if len(s) > 1 and n not in done)
print(f"B.Cu nets on two In2 references in separate segments (no crossing; info): {', '.join(mixed) or 'none'}")
sys.stdout.flush(); os._exit(0)
