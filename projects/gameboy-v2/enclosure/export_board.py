#!/usr/bin/env python3
"""export_board.py — the bridge from the KiCad board to the enclosure CAD: writes build/board.json.

Run with KiCad's python (it needs pcbnew, and draw_mech for the mechanical references):
    /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 export_board.py

board.json (board mm: +X right, +Y DOWN as in KiCad; the enclosure script flips Y):
  board     outline (outer polygon), holes (the shell-screw and 2090-standoff holes, with names), thickness
  mech      GBA silhouette (the shell's OUTER surface), shell wall, board inset, 2090 module rects + heights,
            battery / speaker / LRA envelopes (from draw_mech.py)
  parts     every footprint: ref, value, side, position, rotation, courtyard polygon and body height (height read
            from its VRML 3D model, 0.1 inch units; a few through-hole parts from their datasheets)
"""
import os, sys, json, re, math
HERE = os.path.dirname(os.path.abspath(__file__))
BRD = os.path.join(HERE, "..", "pcb", "gb2")
sys.path.insert(0, BRD)
import pcbnew
import draw_mech as M

TO = pcbnew.ToMM
B = pcbnew.LoadBoard(os.path.join(BRD, "gb2.kicad_pcb"))
CAD = os.path.join(HERE, "..", "..", "..", "cad")

# ---- heights that don't come from a VRML model (datasheets)
HEIGHT = {"J3": 8.5, "J4": 8.5,                 # HX PM2.54-1x20P ZC insulator (mech_ref.json, LCSC C41417332)
          "J1": 8.5}                            # 1x5 2.54 mm male header: 2.5 body + ~6 pin above the board
DEFAULT_H = {"C": 0.9, "R": 0.5, "FB": 0.9, "L": 2.0, "D": 1.0, "Q": 1.1, "U": 1.0, "TP": 0.05, "SW": 1.5, "J": 3.0, "Y": 0.8}

def wrl_height(fp):
    """body height (mm) above the board from the footprint's VRML model, or None."""
    for m in fp.Models():
        path = m.m_Filename.replace("${KIPRJMOD}", BRD)
        if not os.path.isabs(path):
            for base in (os.path.join(CAD), BRD):
                if os.path.exists(os.path.join(base, path)): path = os.path.join(base, path); break
        if not os.path.exists(path) or not path.lower().endswith(".wrl"): continue
        t = open(path, errors="ignore").read()
        z = [float(v) for blk in re.findall(r"point\s*\[([^\]]*)\]", t) for v in re.split(r"[\s,]+", blk.strip()) if v][2::3]
        if z: return (max(z) * m.m_Scale.z + m.m_Offset.z / 2.54) * 2.54
    return None

def poly_pts(ps, k=0):
    o = ps.COutline(k); return [(round(TO(o.CPoint(i).x), 4), round(TO(o.CPoint(i).y), 4)) for i in range(o.PointCount())]

# ---- board
ps = pcbnew.SHAPE_POLY_SET(); B.GetBoardPolygonOutlines(ps, True)
S = M.stretch(); gx, gy = M.GBA_ORIGIN
names = {(round(x, 1), round(y, 1)): (n, d) for n, x, y, d, _ in M.board_holes(S)}
holes = []
for h in range(ps.HoleCount(0)):
    c = ps.CHole(0, h); pts = [(TO(c.CPoint(i).x), TO(c.CPoint(i).y)) for i in range(c.PointCount())]
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]; cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    n, d = min(names.items(), key=lambda kv: math.hypot(kv[0][0] - cx, kv[0][1] - cy))[1]
    holes.append({"name": n, "x": round(cx, 3), "y": round(cy, 3), "d": round(max(xs) - min(xs), 3)})
board = {"outline": poly_pts(ps), "holes": holes, "thickness": TO(B.GetDesignSettings().GetBoardThickness())}

# ---- mechanical references (draw_mech: GBA frame + GBA_ORIGIN = board mm)
pos = M.fit(S)
mech = {
    "silhouette": [(round(x + gx, 4), round(y + gy, 4)) for x, y in M.silhouette(S)],
    "shell_wall": M.SHELL_WALL, "board_inset": M.BOARD_INSET, "gba_depth": 24.5,
    "module": {"rects": [(a + gx, b + gy, c + gx, d + gy) for a, b, c, d in M.module_rects(S)],
               "pcb_bottom_above_board": 11.0, "glass_top_above_board": 16.7,
               "note": "2090 on 8.5 mm sockets; module PCB 1.57; panel + glass 4.1 (mech_ref.json / draw_mech depth note)"},
    "battery": {"x": pos["battery"][0] + gx, "y": pos["battery"][1] + gy, "w": M.BATTERY[0], "h": M.BATTERY[1], "t": 8.5,
                "note": "Adafruit 2011 / PKCELL LP-803860, sits in the board cut-out under the 2090"},
    "speaker": {"x": pos["speaker"][0] + gx, "y": pos["speaker"][1] + gy, "d": M.SPEAKER[0], "t": 4.9},
    "lra": {"x": M.LRA_AT[0] + gx, "y": M.LRA_AT[1] + gy, "d": M.LRA[0], "t": 3.2},
}

# ---- parts
parts = []
for fp in sorted(B.GetFootprints(), key=lambda f: f.GetReference()):
    ref = fp.GetReference(); back = fp.IsFlipped()
    cy = fp.GetCourtyard(pcbnew.B_CrtYd if back else pcbnew.F_CrtYd)
    if cy.OutlineCount(): crt = poly_pts(cy)
    else:
        bb = fp.GetBoundingBox(False); crt = [(TO(bb.GetLeft()), TO(bb.GetTop())), (TO(bb.GetRight()), TO(bb.GetTop())),
                                              (TO(bb.GetRight()), TO(bb.GetBottom())), (TO(bb.GetLeft()), TO(bb.GetBottom()))]
    h = HEIGHT.get(ref) or wrl_height(fp) or DEFAULT_H.get(re.sub(r"\d", "", ref), 1.0)
    q = fp.GetPosition()
    parts.append({"ref": ref, "value": fp.GetValue(), "side": "bottom" if back else "top",
                  "x": round(TO(q.x), 4), "y": round(TO(q.y), 4), "rot": fp.GetOrientationDegrees(),
                  "courtyard": crt, "height": round(h, 3), "tht": bool(fp.GetAttributes() & pcbnew.FP_THROUGH_HOLE)})

os.makedirs(os.path.join(HERE, "build"), exist_ok=True)
out = os.path.join(HERE, "build", "board.json")
json.dump({"source": "projects/gameboy-v2/pcb/gb2/gb2.kicad_pcb", "board": board, "mech": mech, "parts": parts},
          open(out, "w"), indent=1)
tall = sorted(parts, key=lambda p: -p["height"])[:6]
print(f"board.json: {len(parts)} parts, {len(holes)} holes; tallest: " + ", ".join(f"{p['ref']} {p['height']:.1f}" for p in tall))
