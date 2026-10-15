#!/usr/bin/env python3
"""silk.py — silkscreen cleanup on gb2b (idempotent; run after placement changes).

  * tact switches (TS-1187A): the refdes sat on the body outline's top edge (and the inside is the actuator
    ring) -> 0.8 mm text just above the outline
  * side switches / labels within the edge clearance -> moved inboard
  * J7 (USB-C) overhangs the board edge on purpose: its silk outline + refdes go to F.Fab (they'd print off-board)
  * U2 (iCE40, QFN with exposed pad): refdes to F.Fab (no clear spot that isn't the board cut-out or the EP)
  * D1: the pin-1 dot sat on its own pad; the cathode bar marks polarity, so the dot goes to F.Fab

Run:  <kicad python> route/silk.py [board]
"""
import pcbnew, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
path = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "..", "gb2b.kicad_pcb")
B = pcbnew.LoadBoard(path); MM = pcbnew.FromMM
V = lambda x, y: pcbnew.VECTOR2I(int(round(MM(x))), int(round(MM(y))))
fp = B.FindFootprintByReference
for r in ("SW2", "SW3", "SW4", "SW5", "SW6", "SW7", "SW8", "SW9"):
    f = fp(r); t = f.Reference(); c = f.GetPosition()
    t.SetTextSize(V(0.8, 0.8)); t.SetTextThickness(MM(0.12)); t.SetTextAngleDegrees(0)
    t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_CENTER); t.SetVertJustify(pcbnew.GR_TEXT_V_ALIGN_CENTER); t.SetPosition(pcbnew.VECTOR2I(c.x, c.y - MM(3.6)))
for r, (x, y) in {"SW10": (159.6, 54.5), "SW11": (160.6, 66.0), "SW12": (37.7, 41.6), "SW13": (152.8, 41.6), "R29": (59.6, 65.6)}.items():
    t = fp(r).Reference(); t.SetPosition(V(x, y)); t.SetTextAngleDegrees(90 if r == "R29" else 0)
for r in ("J7", "U2"):
    fp(r).Reference().SetLayer(pcbnew.F_Fab)
for g in fp("J7").GraphicalItems():
    if g.GetLayer() == pcbnew.F_SilkS: g.SetLayer(pcbnew.F_Fab)
for g in fp("D1").GraphicalItems():
    if g.GetLayer() == pcbnew.F_SilkS and g.GetClass() == "PCB_SHAPE" and g.GetShape() == pcbnew.SHAPE_T_CIRCLE: g.SetLayer(pcbnew.F_Fab)
pcbnew.SaveBoard(path, B)
print("silk: tidied")
