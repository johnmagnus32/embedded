#!/usr/bin/env python3
"""footprint_keepouts.py — gb3's copper keep-outs that come from part datasheets and the crystal plan (idempotent).

  X2 SC-20S 32.768 kHz (F): "Please make sure that there is no pattern under SC-20S" (C97607.pdf p.1). One F.Cu rule
      area between the two pads (pads end at x 67.15 / start at 67.95; body 2.05 x 1.2 +0.1): no tracks, vias, pour.
  X1 24 MHz (over it on F): no tracks over the crystal (step-2 SoC survey; T113-S3 DS 5.9.1 keep the oscillator loop
      short and alone). Vias stay allowed (the crystal's In2 GND island and B guard ring stitch to In1 here; no foreign
      via goes there by plan) and so does the F GND pour (In1 GND sits 0.2 mm under it either way).
  U18 BT830-SA antenna (all copper): "no copper in the antenna keep-out area on any layers" (EZ-DS-BT830 p.27); the
      p.25 land pattern hatches x >= 7.08 / y <= 6.30 from the module's pin-1 corner and leaves it open past the
      module's antenna-side edge and its far end (wavy break lines), Figure 18 runs it to the PCB edges. On gb3 that is
      x <= 35.30, y <= 38.92 out to the NW board edge; the rule area keeps 0.8 mm east and 0.53 mm south of it and
      stops 0.15 mm short of U18.14 (the old y 39.72 edge put the pad 0.12 mm inside: the 5th DRC error).
  SW12-SW16 ALPS SKRT (F): "Prohibited area of copper tracks", 2.0 x 1.2 mm between the mounting lugs (C127481.pdf
      p.2; same footprint frame as gb2's SKRT switches).
  M2 mounting holes H1-H5 (F+B): r 2.75 mm copper keep-outs (5.5 mm, the step-2 board survey's figure) round the
      hole spots, so no track or via is planned under a screw head or boss.
  U4 A-MicroTF-1.85A (B): two hatched "PROHIBITION AREA"s in the recommended PCB layout (C22467599.pdf p.2), mapped
      from the drawing with the socket centre line at x 56.40 (pegs -4.95 / +3.05, #1 at +2.25) and +0.3 mm margin.

Run once after placement (re-running replaces them):  <kicad python> route/footprint_keepouts.py [board]
(step 3, 2026-10-14: the routing plan's datasheet keep-outs, the X1 area, the M2 hole keep-outs and the 'pins: <ref>' DRC areas)
"""
import pcbnew, os, sys, math
HERE = os.path.dirname(os.path.abspath(__file__))
path = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "..", "gb3.kicad_pcb")
# M2 mounting holes: placed here as board-only footprints, with their copper keep-outs below (one list for both).
# Scheme (user-approved 2026-10-14): screws clamp, shell fingers locate; each grip is clamped at its ends, the right
# grip (it hangs off the 20 mm strip above the cell notch) at three points. The display module and the notch rule out
# the middle. Spots from a free-area scan (parts, pads, keep-outs, channels, via spots; edge web >= 3.9 mm):
#   H1 left grip, outer edge between U18 and the D-pad: 23.6 mm from the BT830 antenna (EZ-DS-BT830 12.1.1: "keep all
#      mounting hardware ... clear", metal < 20 mm degrades it), so nothing nearer the top-left corner
#   H2 right grip, top inner corner (step 3); H3 right grip, top outer corner by SW16; H4 under the ABXY corner
#   H5 left grip, bottom-left corner, 11 mm from the jack (plug force)
# (the first H1 at (102.0, 36.0) was dropped at step 3: its M2 head sat ~0.9 mm under the display module, y 37.0)
HOLES = [("H1", 26.5, 58.0), ("H2", 180.5, 35.0), ("H3", 204.0, 37.5), ("H4", 208.0, 83.5), ("H5", 26.25, 110.75)]
HOLE_LIB = os.path.join(HERE, "..", "..", "..", "..", "..", "cad", "symbols", "manual.pretty")
_io = pcbnew.PCB_IO_MGR.FindPlugin(pcbnew.PCB_IO_MGR.KICAD_SEXP)      # footprints load BEFORE LoadBoard (KiCad 10 bug)
_hole_fps = {ref: _io.FootprintLoad(HOLE_LIB, "MountingHole_2.2mm_M2") for ref, _, _ in HOLES}
B = pcbnew.LoadBoard(path); MM, TO = pcbnew.FromMM, pcbnew.ToMM
PREFIX = "datasheet keep-out: "
SKRT = [(-1.0, 0.38), (1.0, 0.38), (1.0, 1.58), (-1.0, 1.58)]      # footprint frame, between lugs 4 / 5
ALL = [pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.In2_Cu, pcbnew.B_Cu]
FIXED = [  # (name, layers, board-frame polygon, allow pour[, allow vias])
    ("X2 (SC-20S: no pattern under the crystal)", [pcbnew.F_Cu], [(67.20, 68.40), (67.90, 68.40), (67.90, 69.80), (67.20, 69.80)], False),
    ("X1 (no tracks over the 24 MHz crystal)", [pcbnew.F_Cu], [(66.90, 64.30), (70.60, 64.30), (70.60, 67.15), (66.90, 67.15)], True, True),
    ("U18 antenna (BT830-SA, all layers)", ALL, [(21.5, 30.5), (36.1, 30.5), (36.1, 39.45), (21.5, 39.45)], False),
    ("U4 centre bar (A-MicroTF-1.85A)", [pcbnew.B_Cu], [(50.0, 40.2), (59.6, 40.2), (59.6, 42.8), (50.0, 42.8)], False),
    ("U4 card-detect side bar (A-MicroTF-1.85A)", [pcbnew.B_Cu], [(48.3, 40.9), (49.7, 40.9), (49.7, 45.4), (48.3, 45.4)], False),
]
# M2 mounting holes (step-3 default, decision M1): NPTH 2.2 mm (KiCad MountingHole_2.2mm_M2), both outer layers copper-free
# within r 2.75 mm (step-2 board survey's 5.5 mm keep-out: M2 pan head 4.4 mm + margin). H2 (180.5, 35.0): 1.5 mm north
# of the (180.5, ~36.5) default, which put U13's pins 2.05 mm from the screw axis; BAT passes south of it at y 39.5-40.7.
for ref, cx, cy in HOLES:
    FIXED.append(("%s M2 mounting hole (r 2.75 mm, F+B)" % ref, [pcbnew.F_Cu, pcbnew.B_Cu],
                  [(cx + 2.75 * math.cos(k * math.pi / 16), cy + 2.75 * math.sin(k * math.pi / 16)) for k in range(32)], False))
parts = [(f.GetReference(), math.radians(-f.GetOrientationDegrees()), TO(f.GetPosition().x), TO(f.GetPosition().y))
         for f in B.GetFootprints() if f.GetReference() in ("SW12", "SW13", "SW14", "SW15", "SW16")]
# the step-2 board rule area is superseded by the datasheet one above; remove it with the old keep-outs (last:
# B.Remove() breaks the SWIG typing of later calls)
old = [z for z in B.Zones() if z.GetIsRuleArea() and (z.GetZoneName().startswith(PREFIX) or z.GetZoneName().startswith("pins: ") or z.GetZoneName() == "U18_ANT_KEEPOUT")]
def add(name, layers, pts, pour_ok=False, vias_ok=False):
    z = pcbnew.ZONE(B); z.SetIsRuleArea(True); z.SetZoneName(PREFIX + name)
    ls = pcbnew.LSET()
    for L in layers: ls.AddLayer(L)
    z.SetLayerSet(ls)
    z.SetDoNotAllowTracks(True); z.SetDoNotAllowVias(not vias_ok); z.SetDoNotAllowZoneFills(not pour_ok)
    z.SetDoNotAllowPads(False); z.SetDoNotAllowFootprints(False)
    ol = z.Outline(); ol.NewOutline()
    for x, y in pts: ol.Append(int(MM(x)), int(MM(y)))
    B.Add(z)
n = 0
for name, layers, pts, *flags in FIXED: add(name, layers, pts, *flags); n += 1
for ref, a, ox, oy in parts:
    add(ref + " (SKRT lug gap)", [pcbnew.F_Cu],
        [(ox + x * math.cos(a) - y * math.sin(a), oy + x * math.sin(a) + y * math.cos(a)) for x, y in SKRT]); n += 1
# DRC-only areas ("pins: <ref>", no restrictions) round the pad fields of the parts whose HighCurrent pins need a neck-down:
# the EasyEDA courtyards hug the bodies, so intersectsCourtyard() misses the pin tips (U15's pins sit up to 1.4 mm outside
# its courtyard, U2/U6/D4's 1 mm). gb3.kicad_dru's neck-down rules test intersectsArea('pins: <ref>') as well.
PINS = ("U9", "U10", "U12", "U13", "U15", "U17", "U2", "U6", "D4", "USBC1")
for f in B.GetFootprints():
    if f.GetReference() not in PINS: continue
    xs, ys = [], []
    for p in f.Pads():
        if p.GetNetname() == "" or p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH: continue
        b = p.GetBoundingBox(); xs += [TO(b.GetLeft()), TO(b.GetRight())]; ys += [TO(b.GetTop()), TO(b.GetBottom())]
    x0, y0, x1, y1 = min(xs) - 0.3, min(ys) - 0.3, max(xs) + 0.3, max(ys) + 0.3
    z = pcbnew.ZONE(B); z.SetIsRuleArea(True); z.SetZoneName("pins: " + f.GetReference())
    ls = pcbnew.LSET(); ls.AddLayer(f.GetLayer()); z.SetLayerSet(ls)
    z.SetDoNotAllowTracks(False); z.SetDoNotAllowVias(False); z.SetDoNotAllowZoneFills(False)
    z.SetDoNotAllowPads(False); z.SetDoNotAllowFootprints(False)
    ol = z.Outline(); ol.NewOutline()
    for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)): ol.Append(int(MM(x)), int(MM(y)))
    B.Add(z); n += 1
# the hole footprints themselves (board-only, NPTH, out of the BOM / CPL); stale H<n> board-only parts go
ATTR = pcbnew.FP_BOARD_ONLY | pcbnew.FP_EXCLUDE_FROM_BOM | pcbnew.FP_EXCLUDE_FROM_POS_FILES
for ref, cx, cy in HOLES:
    f = B.FindFootprintByReference(ref)
    if f is None:
        f = _hole_fps[ref]; f.SetFPID(pcbnew.LIB_ID("manual", "MountingHole_2.2mm_M2")); f.SetReference(ref); B.Add(f)
    f.SetPosition(pcbnew.VECTOR2I(int(MM(cx)), int(MM(cy)))); f.SetAttributes(ATTR)
stale = [f for f in B.GetFootprints() if f.GetReference()[:1] == "H" and f.GetReference()[1:].isdigit()
         and f.GetAttributes() & pcbnew.FP_BOARD_ONLY and f.GetReference() not in {r for r, _, _ in HOLES}]
for z in old: B.Remove(z)
for f in stale: B.Remove(f)
pro = os.path.splitext(path)[0] + ".kicad_pro"
keep = open(pro, "rb").read() if os.path.exists(pro) else None
pcbnew.SaveBoard(path, B)
if keep is not None and open(pro, "rb").read() != keep: open(pro, "wb").write(keep)   # SaveBoard rewrites the project file
print(f"footprint keep-outs: {n}, holes: {len(HOLES)}"); sys.stdout.flush(); os._exit(0)
