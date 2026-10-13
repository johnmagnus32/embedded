#!/usr/bin/env python3
"""enclosure.py — gb2 enclosure, code-CAD (build123d). Everything is generated from build/board.json, which
export_board.py writes from the KiCad board, so the shell follows the board when the board changes.

Run (from this folder):   .venv/bin/python enclosure.py            -> out/*.step / *.3mf / *.stl (+ the live viewer)
View live: open this file in VS Code, open the OCP CAD Viewer (sidebar), then run it - show() pushes the model.

Frame: board mm, +X right, +Y up (KiCad's Y flipped), Z up; the board's bottom face is z = 0, its top z = T.

STATUS: setup / starter. `fit_frame()` is a placeholder back-shell tray (silhouette walls, floor, screw bosses) to
prove the pipeline end to end - the real front/back shells, keycaps, screen bezel and openings come next.
"""
import json, os, math
from build123d import (Box, Compound, Cylinder, Face, Location, Pos, Wire, Vector, export_step, export_stl, extrude,
                       offset, Mesher, Color)

HERE = os.path.dirname(os.path.abspath(__file__))
D = json.load(open(os.path.join(HERE, "build", "board.json")))
T = D["board"]["thickness"]
WALL = D["mech"]["shell_wall"]                     # 2.0 mm, the board sits BOARD_INSET inside the outer surface

# ---- print / fit parameters (tune here)
CLEAR = 0.3        # mm: shell to board / part clearance (FDM; ~0.15 for SLA)
FLOOR = 1.6        # mm: back-shell floor thickness
BOSS_D, PILOT_D = 5.0, 1.6     # mm: screw boss and its pilot hole (M2 self-tapping into plastic)

flip = lambda pts: [(x, -y) for x, y in pts]

def face(pts):
    pts = flip(pts)
    if pts[0] == pts[-1]: pts = pts[:-1]
    return Face(Wire.make_polygon([Vector(x, y, 0) for x, y in pts], close=True))

# ---------------------------------------------------------------- the board and what's on it
def board():
    b = extrude(face(D["board"]["outline"]), amount=T, dir=(0, 0, 1))
    for h in D["board"]["holes"]:
        b -= Pos(h["x"], -h["y"], T / 2) * Cylinder(h["d"] / 2, T + 0.2)
    return b

def parts():
    """component bodies as envelopes: courtyard x height, up from the top face or down from the bottom."""
    top, bot = [], []
    for p in D["parts"]:
        if p["height"] < 0.1: continue                                 # bare pads
        try: env = extrude(face(p["courtyard"]), amount=p["height"], dir=(0, 0, 1))
        except Exception: continue
        (top if p["side"] == "top" else bot).append(Pos(0, 0, T) * env if p["side"] == "top" else Pos(0, 0, -p["height"]) * env)
    return Compound(top), Compound(bot)

def mech():
    """the 2090 module, battery, speaker and LRA (from draw_mech's references), for fit checks."""
    m = D["mech"]; z0, z1 = T + m["module"]["pcb_bottom_above_board"] - T, m["module"]["glass_top_above_board"]
    mod = []
    for x0, y0, x1, y1 in m["module"]["rects"]:
        mod.append(Pos((x0 + x1) / 2, -(y0 + y1) / 2, T + (z0 + z1) / 2) * Box(x1 - x0, y1 - y0, z1 - z0))
    b = m["battery"]; cell_top = T + m["module"]["pcb_bottom_above_board"] - T - 0.5
    batt = Pos(b["x"], -b["y"], cell_top - b["t"] / 2) * Box(b["w"], b["h"], b["t"])
    s = m["speaker"]; spk = Pos(s["x"], -s["y"], T + s["t"] / 2) * Cylinder(s["d"] / 2, s["t"])
    l = m["lra"]; lra = Pos(l["x"], -l["y"], T + l["t"] / 2) * Cylinder(l["d"] / 2, l["t"])
    return Compound(mod), batt, spk, lra

# ---------------------------------------------------------------- placeholder: back-shell fit frame
def fit_frame(bottom_parts_h):
    """A back-shell tray: the GBA silhouette as the outer wall, hollow by WALL, a floor under the tallest back-side
    part, and screw bosses up to the board's shell-screw holes. A first print to check the board drops in."""
    sil = face(D["mech"]["silhouette"])
    z_floor = -(bottom_parts_h + CLEAR) - FLOOR                       # outer bottom of the tray
    depth = -z_floor + T                                              # walls up to the board's top face
    outer = Pos(0, 0, z_floor) * extrude(sil, amount=depth, dir=(0, 0, 1))
    inner = Pos(0, 0, z_floor + FLOOR) * extrude(offset(sil, amount=-WALL), amount=depth, dir=(0, 0, 1))
    tray = outer - inner
    for h in D["board"]["holes"]:
        if not h["name"].startswith("Shell screw"): continue
        x, y = h["x"], -h["y"]; hb = -(z_floor + FLOOR)               # floor top -> board bottom (z = 0)
        boss = Pos(x, y, z_floor + FLOOR + hb / 2) * Cylinder(BOSS_D / 2, hb)
        tray += boss
        tray -= Pos(x, y, z_floor + FLOOR / 2 + hb / 2) * Cylinder(PILOT_D / 2, hb + FLOOR + 0.2)
    return tray

if __name__ == "__main__":
    pcb = board(); top, bot = parts(); module, batt, spk, lra = mech()
    hb = max(p["height"] for p in D["parts"] if p["side"] == "bottom")
    frame = fit_frame(hb)
    out = os.path.join(HERE, "out"); os.makedirs(out, exist_ok=True)
    export_step(Compound([pcb, top, bot, module, batt, spk, lra]), os.path.join(out, "board_assembly.step"))
    export_step(frame, os.path.join(out, "fit_frame.step")); export_stl(frame, os.path.join(out, "fit_frame.stl"))
    m = Mesher(); m.add_shape(frame); m.write(os.path.join(out, "fit_frame.3mf"))
    print(f"fit frame: {frame.bounding_box().size.X:.1f} x {frame.bounding_box().size.Y:.1f} x "
          f"{frame.bounding_box().size.Z:.1f} mm -> out/fit_frame.3mf / .stl / .step; board assembly -> out/board_assembly.step")
    import socket
    with socket.socket() as sk:                                        # the OCP CAD Viewer listens on 3939
        sk.settimeout(0.3); viewer = sk.connect_ex(("127.0.0.1", 3939)) == 0
    if not viewer:
        print("viewer not running: open the OCP CAD Viewer in VS Code (sidebar), then rerun to see it live")
    else:
        from ocp_vscode import show
        pcb.color, top.color, bot.color = Color("darkgreen"), Color("gray"), Color("gray")
        module.color, batt.color, spk.color, lra.color, frame.color = (Color("royalblue"), Color("orange"),
                                                                       Color("gold"), Color("purple"), Color("ivory"))
        show(pcb, top, bot, module, batt, spk, lra, frame,
             names=["board", "parts (top)", "parts (bottom)", "2090 module", "battery", "speaker", "LRA", "fit frame"])
        print("sent to the OCP CAD Viewer")
