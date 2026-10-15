#!/usr/bin/env python3
"""plot.py — zoomed routing view of gb2b: pads (highlighted nets in colour, the rest grey), tracks per layer, vias,
board edge, and the ratsnest (the DRC's unconnected pairs), as a PNG for review.

Run:  <kicad python> route/plot.py <x0> <y0> <x1> <y1> [--nets N1,N2,...] [--drc drc.json] [-o out.png]
       (board frame mm; default board gb2b.kicad_pcb next to this folder; --drc runs kicad-cli if omitted)
"""
import pcbnew, os, sys, json, subprocess, argparse, colorsys, html, math

HERE = os.path.dirname(os.path.abspath(__file__)); BOARD = os.path.join(HERE, "..", "gb2b.kicad_pcb")
CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
TO = pcbnew.ToMM
LAYER_COL = {pcbnew.F_Cu: "#d03030", pcbnew.B_Cu: "#3050d0", pcbnew.In1_Cu: "#30a030", pcbnew.In2_Cu: "#c08020"}

ap = argparse.ArgumentParser()
ap.add_argument("box", nargs=4, type=float); ap.add_argument("--nets", default=""); ap.add_argument("--drc")
ap.add_argument("-o", default="/tmp/gb2b-plot.png")
ap.add_argument("--channels", action="store_true", help="overlay route/channels.py"); ap.add_argument("--escape", action="store_true", help="overlay route/mcu_escape.py"); ap.add_argument("--no-tracks", action="store_true"); ap.add_argument("--board", default=BOARD); ap.add_argument("--px", type=float, default=40)
a = ap.parse_args()
x0, y0, x1, y1 = a.box; S = a.px                                      # px per mm
side = max(x1 - x0, y1 - y0); x1, y1 = x0 + side, y0 + side           # qlmanage thumbnails are square: pad the window
hl = [n for n in a.nets.split(",") if n]
col = {n: "#%02x%02x%02x" % tuple(int(255 * c) for c in colorsys.hsv_to_rgb(i / max(1, len(hl)), 0.85, 0.85)) for i, n in enumerate(hl)}
B = pcbnew.LoadBoard(a.board)
if not a.drc:
    a.drc = "/tmp/gb2b-plot-drc.json"
    subprocess.run([CLI, "pcb", "drc", "--format", "json", "-o", a.drc, a.board], capture_output=True)
drc = json.load(open(a.drc))
X = lambda x: (x - x0) * S; Y = lambda y: (y - y0) * S
inwin = lambda x, y, m=2: x0 - m <= x <= x1 + m and y0 - m <= y <= y1 + m
o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{(x1-x0)*S:.0f}" height="{(y1-y0)*S:.0f}"><rect width="100%" height="100%" fill="#101010"/>']

def poly(ps, fill, op=1.0, stroke="none"):
    for k in range(ps.OutlineCount()):
        ol = ps.Outline(k); pts = " ".join(f"{X(TO(ol.CPoint(i).x)):.1f},{Y(TO(ol.CPoint(i).y)):.1f}" for i in range(ol.PointCount()))
        o.append(f'<polygon points="{pts}" fill="{fill}" fill-opacity="{op}" stroke="{stroke}" stroke-width="1"/>')

ps = pcbnew.SHAPE_POLY_SET(); B.GetBoardPolygonOutlines(ps, True)
for k in range(ps.OutlineCount()):
    ol = ps.Outline(k); pts = " ".join(f"{X(TO(ol.CPoint(i).x)):.1f},{Y(TO(ol.CPoint(i).y)):.1f}" for i in range(ol.PointCount()))
    o.append(f'<polygon points="{pts}" fill="#1c241c" stroke="#e0e000" stroke-width="2"/>')
    for h in range(ol.PointCount() and ps.HoleCount(k)):
        hl_ = ps.Hole(k, h); pts = " ".join(f"{X(TO(hl_.CPoint(i).x)):.1f},{Y(TO(hl_.CPoint(i).y)):.1f}" for i in range(hl_.PointCount()))
        o.append(f'<polygon points="{pts}" fill="#101010" stroke="#e0e000" stroke-width="2"/>')
for z in B.Zones():
    if z.GetIsRuleArea():
        bb = z.GetBoundingBox()
        if inwin(TO(bb.GetCenter().x), TO(bb.GetCenter().y), 30): poly(z.Outline(), "#ff00ff", 0.08, "#ff00ff")
for z in B.Zones():                                                   # outer-layer pour fills (planes are inner)
    if z.GetIsRuleArea(): continue
    for L, colr in ((pcbnew.F_Cu, "#ff5050"), (pcbnew.B_Cu, "#5070ff")):
        if z.IsOnLayer(L):
            bb = z.GetBoundingBox()
            if TO(bb.GetRight()) < x0 or TO(bb.GetLeft()) > x1 or TO(bb.GetBottom()) < y0 or TO(bb.GetTop()) > y1: continue
            poly(z.GetFilledPolysList(L), colr, 0.28, colr)
for fp in B.GetFootprints():                                          # courtyards
    for side, L in ((0, pcbnew.F_CrtYd), (1, pcbnew.B_CrtYd)):
        c = fp.GetCourtyard(L)
        if c.OutlineCount():
            bb = c.BBox()
            if inwin(TO(bb.GetCenter().x), TO(bb.GetCenter().y), 10):
                for k in range(c.OutlineCount()):
                    ol = c.Outline(k); pts = " ".join(f"{X(TO(ol.CPoint(i).x)):.1f},{Y(TO(ol.CPoint(i).y)):.1f}" for i in range(ol.PointCount()))
                    o.append(f'<polygon points="{pts}" fill="none" stroke="{"#606060" if side == 0 else "#304878"}" stroke-width="1" stroke-dasharray="{"" if side == 0 else "4,3"}"/>')
labels = []
for fp in B.GetFootprints():
    p0 = fp.GetPosition()
    if not inwin(TO(p0.x), TO(p0.y), 6): continue
    back = fp.IsFlipped()
    for p in fp.Pads():
        n = p.GetNetname()
        for L in ((pcbnew.B_Cu,) if back and not p.HasHole() else (pcbnew.F_Cu,)):
            fill = col.get(n, "#7a7a7a" if not back else "#4a5a7a")
            sh = p.GetEffectivePolygon(L, pcbnew.ERROR_INSIDE)
            poly(sh, fill, 0.55 if back else 0.9, "#ffffff" if n in col else "none")
    labels.append((TO(p0.x), TO(p0.y), fp.GetReference(), back))
for t in ([] if a.no_tracks else B.GetTracks()):
    if t.Type() == pcbnew.PCB_VIA_T:
        p = t.GetPosition(); r = TO(t.GetWidth(pcbnew.F_Cu)) / 2
        o.append(f'<circle cx="{X(TO(p.x)):.1f}" cy="{Y(TO(p.y)):.1f}" r="{r*S:.1f}" fill="{col.get(t.GetNetname(), "#c0c0c0")}" stroke="#000" stroke-width="1"/>'
                 f'<circle cx="{X(TO(p.x)):.1f}" cy="{Y(TO(p.y)):.1f}" r="{TO(t.GetDrill())/2*S:.1f}" fill="#000"/>')
    else:
        s, e = t.GetStart(), t.GetEnd()
        c = LAYER_COL.get(t.GetLayer(), "#fff")
        o.append(f'<line x1="{X(TO(s.x)):.1f}" y1="{Y(TO(s.y)):.1f}" x2="{X(TO(e.x)):.1f}" y2="{Y(TO(e.y)):.1f}" stroke="{c}" stroke-opacity="0.8" stroke-width="{TO(t.GetWidth())*S:.1f}" stroke-linecap="round"/>')
if a.channels:
    sys.path.insert(0, HERE); from channels import CHANNELS
    for i, (name, (lay, pts, what)) in enumerate(CHANNELS.items()):
        c = "#%02x%02x%02x" % tuple(int(255 * v) for v in colorsys.hsv_to_rgb(i / len(CHANNELS), 0.7, 1.0))
        pp = " ".join(f"{X(x):.1f},{Y(y):.1f}" for x, y in pts)
        o.append(f'<polygon points="{pp}" fill="{c}" fill-opacity="0.22" stroke="{c}" stroke-width="2" stroke-dasharray="{"" if lay == "F" else "8,4"}"/>')
        cx = sum(x for x, _ in pts) / len(pts); cy = sum(y for _, y in pts) / len(pts)
        o.append(f'<text x="{X(cx):.1f}" y="{Y(cy):.1f}" fill="{c}" font-size="{max(11, S*0.9):.0f}" font-family="Helvetica" font-weight="bold" text-anchor="middle" stroke="#000" stroke-width="3" paint-order="stroke">{html.escape(name.split()[0])} ({lay})</text>')
if a.escape:
    sys.path.insert(0, HERE); from mcu_escape import ESCAPE
    U1 = B.FindFootprintByReference("U1"); pads = {p.GetNumber(): p for p in U1.Pads()}
    CH = {"W1": "#ff5050", "E1": "#50a0ff", "E2": "#40e0ff", "hub->W1": "#ffa040", "hub->S": "#ffe040", "N": "#d070ff",
          "P": "#60ff60", "W2b": "#ff60c0", "crystal": "#ffffff", "local": "#a0a0a0", "plane": "#606060"}
    DIR = {"N": (0, -1), "S": (0, 1), "E": (1, 0), "W": (-1, 0)}
    for pin, (net, side, lay, via, ch, note) in ESCAPE.items():
        if ch == "plane": continue
        p = pads[str(pin)]; q = p.GetPosition(); px, py = TO(q.x), TO(q.y); dx, dy = DIR[side]
        tip = (px + dx * 1.4, py + dy * 1.4); c = CH.get(ch, "#a0a0a0")
        pts = [(px, py), tip] + ([via] if via and math.hypot(via[0] - tip[0], via[1] - tip[1]) < 8 else [])
        o.append('<polyline points="' + " ".join(f"{X(x):.1f},{Y(y):.1f}" for x, y in pts) + f'" fill="none" stroke="{c}" stroke-width="3" stroke-dasharray="{"" if lay == "F" else "7,3"}"/>')
        if via and math.hypot(via[0] - tip[0], via[1] - tip[1]) < 8:
            o.append(f'<circle cx="{X(via[0]):.1f}" cy="{Y(via[1]):.1f}" r="{0.225*S:.1f}" fill="none" stroke="{c}" stroke-width="2"/>')
        lx, ly = px + dx * 2.6, py + dy * 2.6
        anchor = "start" if side == "E" else "end" if side == "W" else "middle"
        rot = f' transform="rotate(-90 {X(lx):.1f} {Y(ly):.1f})"' if side in ("N", "S") else ""
        o.append(f'<text x="{X(lx):.1f}" y="{Y(ly):.1f}" fill="{c}" font-size="{S*0.38:.0f}" font-family="Helvetica" text-anchor="{"start" if side == "N" else "end" if side == "S" else anchor}" dominant-baseline="middle"{rot} stroke="#000" stroke-width="2" paint-order="stroke">{html.escape(net.split("/")[-1])} {ch}</text>')
for u in drc.get("unconnected_items", []):
    it = u["items"]; (ax, ay), (bx, by) = (it[0]["pos"]["x"], it[0]["pos"]["y"]), (it[1]["pos"]["x"], it[1]["pos"]["y"])
    n = it[0]["description"].split("[")[1].split("]")[0] if "[" in it[0]["description"] else ""
    if not (inwin(ax, ay) or inwin(bx, by)): continue
    c = col.get(n, "#808080"); w = 2.5 if n in col else 1
    if hl and n not in col: c, w = "#505050", 0.8
    o.append(f'<line x1="{X(ax):.1f}" y1="{Y(ay):.1f}" x2="{X(bx):.1f}" y2="{Y(by):.1f}" stroke="{c}" stroke-width="{w}" stroke-dasharray="6,3"/>')
for x, y, r, back in labels:
    o.append(f'<text x="{X(x):.1f}" y="{Y(y):.1f}" fill="{"#a0c0ff" if back else "#ffffff"}" font-size="{S*0.7:.0f}" font-family="Helvetica" text-anchor="middle" dominant-baseline="middle" stroke="#000" stroke-width="3" paint-order="stroke">{html.escape(r)}</text>')
for i, n in enumerate(hl):
    o.append(f'<text x="8" y="{20 + 18*i}" fill="{col[n]}" font-size="16" font-family="Helvetica" stroke="#000" stroke-width="3" paint-order="stroke">{html.escape(n)}</text>')
step = 5                                                                # mm grid ticks
for gx in range(int(x0 // step + 1) * step, int(x1) + 1, step):
    o.append(f'<line x1="{X(gx)}" y1="0" x2="{X(gx)}" y2="8" stroke="#888"/><text x="{X(gx)+2}" y="18" fill="#888" font-size="11">{gx}</text>')
for gy in range(int(y0 // step + 1) * step, int(y1) + 1, step):
    o.append(f'<line x1="0" y1="{Y(gy)}" x2="8" y2="{Y(gy)}" stroke="#888"/><text x="10" y="{Y(gy)+4}" fill="#888" font-size="11">{gy}</text>')
o.append("</svg>")
svg = os.path.splitext(a.o)[0] + ".svg"; open(svg, "w").write("\n".join(o))
d = os.path.dirname(os.path.abspath(a.o))
subprocess.run(["qlmanage", "-t", "-s", str(int(max(x1 - x0, y1 - y0) * S)), "-o", d, svg], capture_output=True)
os.replace(svg + ".png", a.o); print(a.o)
