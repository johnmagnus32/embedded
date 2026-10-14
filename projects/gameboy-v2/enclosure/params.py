"""params.py — the gb2 enclosure's shared constants, frame helpers and context geometry (see DESIGN.md).

Frame: board mm, +X right, +Y up (KiCad's Y flipped), Z up; board bottom z = 0, top z = T.
Every module imports from here; numbers come from DESIGN.md (2026-10-13 research) unless marked choice.
"""
import json, os, math
from build123d import Box, Compound, Cylinder, Face, Pos, Wire, Vector, extrude

HERE = os.path.dirname(os.path.abspath(__file__))
D = json.load(open(os.path.join(HERE, "build", "board.json")))
MECH = D["mech"]

# ---------------------------------------------------------------- global parameters
T = D["board"]["thickness"]          # 1.6
Z_FRONT = 20.9                       # front outer face: total 24.5 = the GBA
Z_BACK = -3.6                        # back outer face
SPLIT = T                            # parting plane = board top
WALL = MECH["shell_wall"]            # 2.0
FLOOR = 1.6                          # back floor; inner face z = Z_BACK + FLOOR = -2.0
PLATE = 1.6                          # front face plate; inner face z = Z_FRONT - PLATE = 19.3
Z_FLOOR_IN = Z_BACK + FLOOR
Z_PLATE_IN = Z_FRONT - PLATE
R_OUT_FRONT, R_OUT_BACK = 2.0, 3.0   # outer perimeter fillets
CLR = 0.3                            # general print clearance
CLR_SLIDE = 0.2                      # sliding fits (plungers in guides, caps in notches)
GLASS_TOP = 18.27                    # 2090 cover glass (nominal; worst ~18.9) - re-measure the real module
GLASS_WORST = 18.9

# hardware
INSERT_HOLE_D, INSERT_DEPTH = 3.2, 4.0          # M2 x 3 brass heat-set insert
BOSS_D = 6.0
SCREW_CLEAR_D = 2.5                              # M2 clearance (FDM)
CBORE_D, CBORE_DEPTH = 4.2, 2.0                  # DIN 912 M2 head 3.8 x 2.0
LENS_T = 1.5                                     # cast acrylic

# ---------------------------------------------------------------- frame helpers
flip = lambda pts: [(x, -y) for x, y in pts]
P = lambda x, y: (x, -y)                         # board (KiCad) point -> enclosure XY

def face(pts, board_frame=True):
    """a planar face at z = 0 from a polygon (board frame by default: Y gets flipped)."""
    pts = flip(pts) if board_frame else list(pts)
    if pts[0] == pts[-1]: pts = pts[:-1]
    return Face(Wire.make_polygon([Vector(x, y, 0) for x, y in pts], close=True))

def prism(f, z0, z1):
    """extrude face f (at z 0) to span z0..z1 (always +Z, whatever the face's winding)."""
    return Pos(0, 0, z0) * extrude(f, amount=z1 - z0, dir=(0, 0, 1))

def cyl(x, y, d, z0, z1):
    return Pos(x, y, (z0 + z1) / 2) * Cylinder(d / 2, z1 - z0)

def box(cx, cy, w, h, z0, z1):
    return Pos(cx, cy, (z0 + z1) / 2) * Box(w, h, z1 - z0)

SILHOUETTE = face(MECH["silhouette"])

# ---------------------------------------------------------------- parts by reference
PARTS = {p["ref"]: p for p in D["parts"]}
def part_xy(ref): p = PARTS[ref]; return P(p["x"], p["y"])

SHELL_SCREWS = [P(s["x"], s["y"]) for s in MECH["shell_screws"]]
STANDOFFS = [P(h["x"], h["y"]) for h in D["board"]["holes"] if h["name"].startswith("2090 standoff")]

# ---------------------------------------------------------------- context geometry (for fit checks / the viewer)
def board_solid():
    b = prism(face(D["board"]["outline"]), 0, T)
    for h in D["board"]["holes"]:
        b -= cyl(h["x"], -h["y"], h["d"], -0.1, T + 0.1)
    return b

def part_envelopes():
    """component bodies: courtyard x height, up from the top face / down from the bottom face."""
    top, bot = [], []
    for p in D["parts"]:
        if p["height"] < 0.1: continue
        try: f = face(p["courtyard"])
        except Exception: continue
        (top if p["side"] == "top" else bot).append(prism(f, T, T + p["height"]) if p["side"] == "top"
                                                   else prism(f, -p["height"], 0))
    return Compound(top), Compound(bot)

def module_2090():
    """the 2090 module: PCB at z 12.6-14.17 over its rects, panel + glass up to GLASS_TOP over the panel outline."""
    m = MECH["module"]; out = []
    for x0, y0, x1, y1 in m["rects"]:
        out.append(box((x0 + x1) / 2, -(y0 + y1) / 2, x1 - x0, y1 - y0, 12.6, 14.17))
    out.append(box(95.25 + 0.16, -71.0, 69.2, 50.0, 14.27, GLASS_TOP))     # panel 69.2 x 50 (centre +0.16 toward JP2)
    out.append(box(76.85, -78.75, 12.3, 27.9, 9.95, 12.6))                   # back parts over the cell (orig rev SOIC-20W)
    for x in (57.15, 133.35):                                                # header pins + solder through the module PCB
        out.append(box(x, -71.0, 2.54, 51.3, 14.17, 16.0))
    for x, y in STANDOFFS:                                                   # M2.5 pan-head screws on top of the module
        out.append(cyl(x, y, 5.0, 14.17, 16.4))
    return Compound(out)

def battery():
    b = MECH["battery"]
    return box(b["x"], -b["y"], b["w"], b["h"], Z_FLOOR_IN, Z_FLOOR_IN + b["t"])       # rests on the floor

def speaker():
    s = MECH["speaker"]
    return cyl(s["x"], -s["y"], s["d"], Z_PLATE_IN - 0.3 - s["t"], Z_PLATE_IN - 0.3)   # sealed to the plate (0.3 foam)

def lra():
    l = MECH["lra"]
    return cyl(l["x"], -l["y"], l["d"], Z_PLATE_IN - l["t"], Z_PLATE_IN)               # glued under the plate

# ================================================================ shared contracts (rev 2, 2026-10-14)
# Every module builds against these numbers; DESIGN.md explains them. Rev 2 folds in the controls agent's findings
# (caps go in head-first from inside, D-pad binding, edge-cap neck, board position tolerance).

# ---------------------------------------------------------------- face keys + D-pad
# TS-1187A keystake top z 3.10, travel 0.2 +-0.1, hard stop 0.3 (its stainless cover, z 2.80). Each cap: a head in a
# face hole (KEY_HOLE_CLR), KEY_PROUD above the face; a flange under the plate retains it; one piece, so it goes in
# head-first from inside: its guide tube's bore passes the flange (GUIDE_ID) along its whole length, and a guide ring on
# the stem (OD = the flange's, over FIN_Z) rides that bore so the stem stays straight. (rev 2.1: a full ring, not four
# fins - B's bore is open on the speaker side and a fin could drop into the gap.)
KEYSTAKE_Z = 3.10
KEY_REST_GAP = 0.20          # face keys: tip above the keystake at rest. The plate->board stack (shells, bosses, cap) holds
                             # about +-0.2, so 0.2 never pre-presses; worst case 0.4 of free play before the switch
DPAD_REST_GAP = 0.10         # the D-pad rests on its own pivot leg (board-referenced): only the part's own +-0.1 counts
KEY_TIP_D = 2.4              # flat stem tip (A/B), wider than the Ø2.0 keystake so +-0.2 of drift stays on it
KEY_PROUD = 1.8              # cap top above the face (GBA 1.9-2.1)
KEY_HOLE_CLR = 0.2           # radial, head in its face hole
FLANGE_T, FLANGE_LIP = 0.8, 1.0     # retention flange under the plate: thickness, overhang past the face hole
GUIDE_BOTTOM_Z = 7.0         # guide tubes hang from the plate inner face down to here (parts below are <= 4.95 tall)
GUIDE_WALL = 1.0
FIN_Z = (8.0, 11.0)          # stem guide ring: z range at rest (still inside the tube at full press)
def _sw(ref): return part_xy(ref)
KEYS = {
    "A":      {"shape": "round", "d": 9.4, "stem_d": 4.0, "tip_d": KEY_TIP_D, "at": _sw("SW6"), "contacts": [_sw("SW6")]},
    "B":      {"shape": "round", "d": 9.4, "stem_d": 4.0, "tip_d": KEY_TIP_D, "at": _sw("SW7"), "contacts": [_sw("SW7")]},
    "START":  {"shape": "round", "d": 4.0, "stem_d": 2.6, "tip_d": 2.6, "at": _sw("SW8"), "contacts": [_sw("SW8")]},
    "SELECT": {"shape": "round", "d": 4.0, "stem_d": 2.6, "tip_d": 2.6, "at": _sw("SW9"), "contacts": [_sw("SW9")]},
    # one cross: arms reach 9.4 from the centre, 6.6 wide; four legs (leg_d, R2.5 domed tips) onto SW2-5 (8.000 from
    # the centre) and a centre pivot leg on the board's top soldermask (z T; plain GND pour, no pads within 5.3 mm),
    # centred by a printed washer (controls.dpad_washer) that the four switch bodies locate.
    # hole_clr 1.3 (rev 2.1): the cross sways 0.97 axial / 1.36 diagonal at the face when a leg takes its switch to the
    # 0.3 stop, and sits up to 0.285 off centre (washer play 0.085 + washer to switches 0.1 + board 0.1). The hole is
    # the sharp-jointed cross offset (front_keys.dpad_hole_face); the flange follows (hole + FLANGE_LIP).
    "DPAD":   {"shape": "cross", "arm": 9.4, "w": 6.6, "leg_d": 2.6, "pivot_d": 3.0, "hole_clr": 1.3,
               "at": P(39.95, 64.09), "contacts": [_sw("SW2"), _sw("SW3"), _sw("SW4"), _sw("SW5")]},
}
for _k in KEYS.values():
    if _k["shape"] == "round":
        _k["flange_d"] = _k["d"] + 2 * (KEY_HOLE_CLR + FLANGE_LIP)     # 11.8 / 6.4
        _k["guide_id"] = _k["flange_d"] + 2 * CLR_SLIDE                 # 12.2 / 6.8: the flange passes the bore
        _k["fin_od"] = _k["flange_d"]

# ---------------------------------------------------------------- edge keys (ALPS SKRTLBE010: Vol+, Vol-, L, R)
# Plunger axis z 3.3, Ø2, travel 0.2; the tip sits 2.82 inside the outer surface (0.82 short of the wall's inner face).
# The cap rides a notch in the front shell's wall; its lip sits in a recess in the wall's inner face (so the cap's
# section behind the wall is >= 0.8). The board is located to the front shell by BOARD_LOCATORS, so the rest gap holds
# about +-0.2; EDGE_PROUD leaves enough push to trip at the largest gap.
EDGE_AXIS_Z = 3.3
EDGE_TIP_INSET = 2.82
EDGE_REST_GAP = 0.15
EDGE_PROUD = 0.6             # cap face out of the outer surface along 'dir'
EDGE_NOTCH = (4.0, 3.6)      # wall notch per key: width along the wall x height (z SPLIT..SPLIT+3.6, axis at 3.3)
EDGE_LIP_RECESS = {"depth": 0.3, "w": 4.8, "z1": 6.8}   # pocket in the wall's inner face round each notch, z SPLIT..z1
def _edge(ref, outer_xy):
    sx, sy = part_xy(ref); ox, oy = outer_xy
    L = math.hypot(ox - sx, oy - sy); u = ((ox - sx) / L, (oy - sy) / L)              # outward unit vector
    return {"switch": ref, "outer": outer_xy, "dir": u, "tip": (ox - u[0] * EDGE_TIP_INSET, oy - u[1] * EDGE_TIP_INSET)}
EDGE_KEYS = {
    "VOL_UP":   _edge("SW10", (167.97, -53.98)),
    "VOL_DOWN": _edge("SW11", (168.94, -61.98)),
    "L":        _edge("SW12", (37.13, -33.61)),
    "R":        _edge("SW13", (153.33, -33.60)),
}

# Power slide (MSK12C02-HB, SW1): lever 1.3 x 1.0 at z 1.8-2.8, travel 1.6 along the 13.36 deg bottom-left wall,
# tip 0.97 short of the wall's inner face. Captive printed slider in a slot through the wall.
POWER_SLOT = {"at": (41.96, -103.76), "len": 4.0, "h": 2.2, "z0": 1.6, "angle_deg": -13.36, "travel": 1.6}

# USB-C (J7): mouth centre x 95.25, z 3.25; 9.5 x 3.8 stadium through the top wall + 13 x 7 overmold relief 1.1 deep
USBC = {"x": 95.25, "z": 3.25, "w": 9.5, "h": 3.8, "relief_w": 13.0, "relief_h": 7.0, "relief_d": 1.1}

# ---------------------------------------------------------------- board location (rev 2)
# Six fingers hang from the front shell's wall below the split (z FINGER z0..SPLIT) into the 0.5 gap between the
# board edge and the wall line, FINGER gap off the board edge, so the board (and with it every switch) sits within
# about +-0.1 of the front shell that carries the caps. The back shell's wall has a pocket behind each finger.
# 'at' = the board-edge point, 'n' = the outward normal there (both enclosure frame); edge -> outer surface 2.500.
BOARD_LOCATORS = {
    "left_upper":  {"at": (24.683, -57.010), "n": (-0.993, 0.120)},
    "left_lower":  {"at": (22.623, -80.000), "n": (-1.000, 0.016)},
    "right":       {"at": (164.850, -48.994), "n": (0.993, 0.120)},
    "top_left":    {"at": (75.000, -32.500), "n": (0.000, 1.000)},
    "top_right":   {"at": (112.000, -32.501), "n": (0.000, 1.000)},
    "bottom_left": {"at": (51.513, -104.268), "n": (-0.370, -0.929)},
}
FINGER = {"w": 3.0, "gap": 0.1, "z0": 0.3, "lead_in": 0.3,       # width along the edge, gap to the board edge, bottom z,
          "pocket_depth": 0.7, "pocket_clr": 0.2}                 # 45 deg lead-in at the tip; the finger's outer face is
                                                                  # pocket_depth into the wall line, and the back wall's
                                                                  # pocket pocket_clr beyond it (0.9 deep, 1.1 wall left)

# ---------------------------------------------------------------- screen (rev 2: the research's option C')
# No printed plastic over the glass: the plate is cut through over the panel + FPC fold (OPENING); a big clear lens
# with a black mask on its back (leaving MASK_WINDOW over the pixels) sits in a pocket flush with the face, on a land
# outside the opening. The land is the top of a frame that hangs from the plate down to LAND_Z0, clear of the module's
# header pins and standoff screw heads (module_2090).
PANEL = {"x0": 60.81, "x1": 130.01, "y0": -96.0, "y1": -46.0}      # 2090 panel + glass (module_2090)
FPC_BULGE = 1.0                                                   # the FPC fold, past the panel's right edge
OPENING = {"x0": PANEL["x0"] - 0.4, "x1": PANEL["x1"] + FPC_BULGE + 0.5, "y0": PANEL["y0"] - 0.4,
           "y1": PANEL["y1"] + 0.4, "r": 1.0}                     # x 60.41..131.51, y -96.4..-45.6
TAPE_T = 0.13                                                     # acrylic transfer tape (3M 468MP class)
LENS = {"cx": 95.25, "cy": -71.0, "w": 77.5, "h": 56.0, "r": 2.0, "t": LENS_T}   # x 56.5..134.0, y -99.0..-43.0
LENS_POCKET = {"clr": 0.2, "depth": LENS_T + TAPE_T}              # lens + 0.2 per side; floor z 19.27, lens bottom 19.4
LAND_Z0 = 17.0                                                    # frame bottom (module screw heads <= 16.4, pins 16.0)
FRAME_W = 1.5                                                     # frame beyond the pocket outline
MASK_WINDOW = {"cx": 92.61, "cy": -71.0, "w": 60.6, "h": 46.2, "r": 1.5}         # over the active area (on the lens)
SCREEN_WINDOW = MASK_WINDOW                                       # (rev 1 name)

# ---------------------------------------------------------------- other face features
LED = {"at": part_xy("D1"), "d": 2.5, "hole_d": 2.6, "tube_id": 2.7, "stop_id": 2.0, "stop_z": (2.4, 3.2)}
# Ø2.5 x 17.7 clear acrylic rod light pipe: in from the face, rests on the tube's bottom stop (z 3.2; 0.8 tall for the
# min wall, 0.24 over D1's top 2.16), glued flush
SPEAKER_AT = P(MECH["speaker"]["x"], MECH["speaker"]["y"])
GRILLE_D = 22.0                                                   # slots inside this circle
SPK_RING = {"id": MECH["speaker"]["d"] + 0.4, "wall": 1.0, "depth": 2.0}   # locates the speaker under the plate; notched
                                                                  # where it would come within 0.3 of B's guide bore
J1_HATCH = {"cx": 123.6, "cy": -37.0, "w": 13.4, "h": 3.4}
