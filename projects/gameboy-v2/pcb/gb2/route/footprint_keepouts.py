#!/usr/bin/env python3
"""footprint_keepouts.py — copper keep-outs that come from part datasheets, not from the mechanics (idempotent).

  ALPS SKRT side switches (SW10-13, SKRTLBE010): the land pattern marks a 2.0 x 1.2 mm "prohibited area of copper
  tracks" between the two mounting lugs (datasheet: soldering-pad drawing, viewed from the mounting side). One F.Cu
  rule area per switch: no tracks, no vias, no pour.

Run once after placement (re-running replaces them):  <kicad python> route/footprint_keepouts.py [board]
"""
import pcbnew, os, sys, math
HERE = os.path.dirname(os.path.abspath(__file__))
path = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "..", "gb2.kicad_pcb")
B = pcbnew.LoadBoard(path); MM, TO = pcbnew.FromMM, pcbnew.ToMM
PREFIX = "datasheet keep-out: "
SKRT = [(-1.0, 0.38), (1.0, 0.38), (1.0, 1.58), (-1.0, 1.58)]      # footprint frame: between the lugs (pads 4 / 5)
# read everything first and remove the old keep-outs last: B.Remove() breaks the SWIG typing of later calls (place.py)
parts = [(f.GetReference(), math.radians(-f.GetOrientationDegrees()), f.GetPosition().x, f.GetPosition().y)
         for f in B.GetFootprints() if f.GetReference() in ("SW10", "SW11", "SW12", "SW13")]    # the four SKRTLBE010s
old = [z for z in B.Zones() if z.GetIsRuleArea() and z.GetZoneName().startswith(PREFIX)]   # removed last
n = 0
for ref, a, ox, oy in parts:
    z = pcbnew.ZONE(B); z.SetIsRuleArea(True); z.SetLayer(pcbnew.F_Cu); z.SetZoneName(PREFIX + ref + " (SKRT lug gap)")
    z.SetDoNotAllowTracks(True); z.SetDoNotAllowVias(True); z.SetDoNotAllowZoneFills(True)
    z.SetDoNotAllowPads(False); z.SetDoNotAllowFootprints(False)
    ol = z.Outline(); ol.NewOutline()
    for x, y in SKRT:
        ol.Append(int(ox + MM(x * math.cos(a) - y * math.sin(a))), int(oy + MM(x * math.sin(a) + y * math.cos(a))))
    B.Add(z); n += 1
for z in old: B.Remove(z)
pcbnew.SaveBoard(path, B); print(f"footprint keep-outs: {n}")
