"""params.py - the gb3 enclosure's single source of numbers (CONTRACT.md).

  1. stack inputs, each an Input (a float carrying .src, its source)
  2. variant flags from the environment (GB3_FRONT, GB3_BACK, GB3_CELL, GB3_LCD_PLATE, GB3_PROCESS)
  3. every derived z (nothing below is typed twice: it all follows from 1 + 2)
  4. the console outline (analytic rounded rectangle) and the frame helpers P() / face() / prism() / cyl() / box() / Rect
  5. named positions read from build/board.json: holes, switches, LEDs, edge features, CN1 / P1 / FPC1, the panel,
     cell, LRA, speaker, the display / touch flex paths
  6. the shared contracts front.py / back.py / controls.py / checks.py build against (keys, edge keys, shoulders,
     bosses + seam screws, panel pocket + ledge, fold bay, openings, cell bay, speaker, LRA)
  7. region_table() and the asserts against board.json z_design (run on import)

Frame (gb2's): board mm, +X right, +Y UP = KiCad's Y flipped, P(x, y) = (x, -y); +Z toward the screen; board bottom
face z 0, top face z BOARD_T (F = screen side). Every position below is in this enclosure frame. A KiCad (x, y) from
the brief goes through P(); a KiCad rectangle through Rect.k(x0, y0, x1, y1).

Importing prints the stack and the per-region thickness table (GB3_QUIET=1 silences it), then asserts the board data
against the stack: an AssertionError lists every failure.
"""
import json, math, os
from typing import NamedTuple
from build123d import Box, Cylinder, Face, Plane, Pos, RectangleRounded, Vector, Wire, extrude

HERE = os.path.dirname(os.path.abspath(__file__))
BOARD_JSON = os.path.join(HERE, "build", "board.json")
BJ = json.load(open(BOARD_JSON))            # written by export_board.py (KiCad python, on a temp copy of gb3.kicad_pcb)
EPS = 1e-6


# ================================================================ 1. stack inputs
class Input(float):
    """a stack input: a float with its source in .src (arithmetic on it gives plain floats)."""
    def __new__(cls, value, src):
        o = super().__new__(cls, value)
        o.src = src
        return o

    def __repr__(self):
        return f"{float(self):g}"


INPUTS = {}                                  # name -> Input (printed by stack_report())


def _in(name, value, src):
    INPUTS[name] = Input(value, src)
    return INPUTS[name]


# board + P1 (the panel-to-board gap)
BOARD_T = _in("board_t", 1.6, "JLC04161H-7628 (route/PLAN.md l.22); JLC tol +/-10% (1.44-1.76)")
P1_MAX = _in("p1_max", 2.1, "C9160.pdf p1: 2.0 +/-0.1 closed, 3.6 lid open")
P1_AIR = _in("p1_air", 0.2, "design: air over P1 (panel + thin reports)")
# panel ATM0500D27-CT
MODULE_NOM = _in("module_nom", 4.25, 'ATM0500D27-CT.pdf p4 item 8 + p17 "*4.25+/-0.3 (Total thickness)"')
MODULE_TOL = _in("module_tol", 0.3, "ATM0500D27-CT.pdf p17")
LCM_T = _in("lcm_t", 3.0, 'ATM0500D27-CT.pdf p17 "*3.0+/-0.20 (LCM)"')
DST_T = _in("dst_t", 0.175, "ATM0500D27-CT.pdf p6: 3M 9495 tape, touch panel to LCM")
SENSOR_T = _in("sensor_t", 0.425, "ATM0500D27-CT.pdf p6: ITO film sensor")
GLASS_T = _in("glass_t", 0.7, "ATM0500D27-CT.pdf p6: cover glass (soda-lime, chem. strengthened)")
LCM_OUTLINE_TOL = _in("lcm_outline_tol", 0.3, "ATM0500D27-CT.pdf p17: 120.7 +/-0.3 x 75.8 +/-0.3")
POCKET_CLR = _in("pocket_clr", 0.2, "design: module pocket over the LCM max outline, per side (121.4 x 76.5)")
PANEL_TAPE = _in("panel_tape", 0.15, "design: die-cut acrylic tape, LCM back on the ledge (panel report 0.15-0.2)")
LEDGE_T = _in("ledge_t", 1.0, "design: ledge under the LCM back edge, z 2.75-3.75 (thin report option b)")
LEDGE_W = _in("ledge_w", 1.0, "design: ledge reach under the LCM edge (synth/scripts/ledge.py)")
LEDGE_BL_X1 = _in("ledge_bl_x1", 81.0, "synth/scripts/ledge.py: bottom-left segment ends x 81.0 (display flex >= 83.4)")
RIM = _in("rim", 0.1, "design: face 0.1 above the thickest glass (flush front)")
LIP = _in("lip", 0.8, "design: MJF lip over the glass black border (thin report)")
GASKET = _in("gasket", 0.1, "design: compressed gasket/tape under the lip (panel report)")
LIP_WINDOW_MARGIN = _in("lip_window_margin", 0.95, "panel report: lip window >= active area + 0.95 per side")
# cell column
CELL_AIR = _in("cell_air", 0.2, "design: stack tolerance above the swollen cell")
CELL_TAPE = _in("cell_tape", 0.15, "design: removable double-sided tape under the cell")
FLOOR_FR4 = _in("floor_fr4", 1.0, "design: 1.0 FR4 battery plate (JLCPCB 1.0 +/-10%); 0.4 mm at 20 N (thin report)")
FLOOR_PR = _in("floor_pr", 1.2, "design: printed floor, 6 x 0.2 layers")
SS_T = _in("ssplate", 0.5, "design: 0.5 SUS304 plate between LCM and cell (gb2 REVB_DESIGN.md s3.1)")
SS_AIR = _in("ssplate_air", 0.3, "design: LCM to plate (plate ~0.25 mm at 20 N over ~40 x 64)")
SS_FILM = _in("ssplate_film", 0.05, "design: insulating film plate-to-PCM (gb2 REVB_DESIGN.md s3.1)")
# board column
CLR_B = _in("clr_b", 0.3, "design: B-part to floor = 0.16 board tol + 0.14 air")
CLR_F = _in("clr_f", 0.3, "design: F-part to the front plate's inner face (CN1 gets 0.35)")
USBC_T = _in("usbc", 3.36, "C165948.pdf p1: 3.26, <5 mm class +/-0.10 (deepest B part: sets the body back)")
PLATE = _in("plate", 1.2, "design: front plate (MJF/FDM)")
PLATE_CN1 = _in("plate_cn1", 1.0, "design: plate thinned over CN1 (top z 7.2: 0.35 air)")
# shell
WALL_MJF = _in("wall_mjf", 1.5, "thin report: JLC3DP MJF PA12 walls 1.5 (1.2-2.0)")
WALL_FDM = _in("wall_fdm", 1.67, "thin report: FDM 4 perimeters 0.45 + 3 x 0.407 = 1.67")
R_FRONT = _in("r_front", 2.0, "gb2 DESIGN.md: R2 front outer fillet")
R_BACK = _in("r_back", 3.0, "gb2 DESIGN.md: R3 back outer fillet")
OUTLINE_R = _in("outline_r", float(os.environ.get("GB3_CORNER_R", "20.0")),
                "user 2026-10-15: Switch-like R20 (was 7.1); the board's corners are R18 since its step 8, concentric "
                "(GB3_CORNER_R: a styling study)")
# transducers
SPK_T = _in("spk_t", 5.1, "ces-2704-088l050.pdf p2: 4.9 +/-0.2")
SPK_LIP_H = _in("spk_lip_h", 0.4, "CES-2704 STEP: front lip D23.4 stands 0.4 proud of the 4.5 frame")
SPK_FOAM = _in("spk_foam", 0.3, "design: foam ring between the back-firing lip and the floor")
SPK_CUP_ID = _in("spk_cup_id", 27.4, "design: D27 +/-0.2 + 0.2 radial (gb2 SPK_ID)")
SPK_GRILLE_D = _in("spk_grille_d", 22.0, "design: grille inside the D23.4 lip (gb2 GRILLE_D)")
LRA_T = _in("lra_t", 3.53, "VG0832022D.step measured (3.25 + 0.28 adhesive)")
# flexes (panel report, fold_model.py; estimates marked)
DISP_EXIT_DZ = _in("disp_exit_dz", 1.76, "ATM0500D27 p16 side view: display flex leaves 1.76 +/-0.3 above the LCM back")
TOUCH_EXIT_BELOW = _in("touch_exit_below", 0.70, "ATM0500D27 p5/p16: touch flex leaves 0.70 below the glass top")
FLEX_T = _in("flex_t", 0.3, "ATM0500D27 p17: FPC 0.3 +/-0.05 (context strips)")
FLEX_S0 = _in("flex_s0", 0.3, "fold_model.py: straight exit before each first bend (est)")
R_TOUCH = _in("r_touch", 1.0, "fold_model.py: touch flex bend centreline radius (est; no datasheet value)")
P1_SLOT_DZ = _in("p1_slot_dz", 0.8, "fold_model.py: P1 slot height over the board (est +/-0.3)")
P1_INSERT = _in("p1_insert", 4.5, "fold_model.py: P1 outermost edge -> FPC end (1.4 grip + ~3.1 in; est +/-0.7)")
HDGC_INSERT = _in("hdgc_insert", 4.1, "fold_model.py: FPC1 lid lip face -> FFC end (0.9 + 2.75 + 0.45; est +/-0.4)")
FLEX_GAP = _in("flex_gap", 0.1, "fold_model.py: air between the display loop and the touch U-turn")
DISP_TAIL = _in("disp_tail", 53.2, "ATM0500D27 p17: display tail *53.2 +/-0.5 from the LCM edge")
TOUCH_TAIL = _in("touch_tail", 53.4, "ATM0500D27 p6: touch tail *53.4 +/-0.5 from the glass edge")
TOUCH_IC_T = _in("touch_ic_t", 1.45, "ATM0500D27 p6: 'MAX 1.45 (IC+FPC+Stiffener)', chip toward the board")
FOLD_SKIN_RULE = _in("fold_skin_rule", 0.55, "panel report: bezel inner face over the fold bay >= glass_max - 0.55")
FOLD_BAY_Z0 = _in("fold_bay_z0", -1.6, "panel report: fold-bay keep-out from z -1.6")

# cells (GB3_CELL): t = max thickness as delivered, swell = allowance, w x l = max plan (w along x, l along y)
CELLS = {
    "LP803860": dict(t=_in("cell_t", 8.5, "LiIon2000mAh37V.pdf p5: max 62 x 38.5 x 8.5"),
                     swell=_in("swell", 0.85, "design 10% of 8.5 = the gb2 rev B brief (REVB_DESIGN.md s3.1); "
                                              "BU-301a 8-10%/500 cycles"),
                     w=38.5, l=62.0, label="PKCELL LP-803860 (Adafruit 2011) 2000 mAh, 1900 min, 1C"),
    "LP503759JU": dict(t=_in("jauch_t", 5.4, "Jauch LP503759JU datasheet p1: 5.4 max delivered, 5.8 after cycling, "
                                             "38 x 62, 1300 mAh min, 2C"),
                       swell=_in("jauch_swell", 0.4, "Jauch LP503759JU p1: 5.8 - 5.4"),
                       w=38.0, l=62.0, label="Jauch LP503759JU 1300 mAh min, 2C, bare leads"),
    "ADA258": dict(t=_in("ada258_t", 5.3, "C101 LP-503562 p5: 5.0 +/-0.3 x 35 x 62, 1140 mAh min, 1C (p2)"),
                   swell=_in("ada258_swell", 0.53, "design: 10% of 5.3 (no maker figure)"),
                   w=35.3, l=62.3, label="Adafruit 258 / PKCELL LP-503562 1200 mAh, 1C, JST-PH fitted"),
}


# ================================================================ 2. variant flags
def _flag(name, default, allowed):
    v = os.environ.get(name, default).strip()
    for a in allowed:                                   # case-insensitive match, canonical spelling kept
        if v.lower() == a.lower():
            return a
    raise ValueError(f"{name}={v!r}: expected one of {', '.join(allowed)}")


FRONT = _flag("GB3_FRONT", "flush", ("flush", "lip"))
BACK = _flag("GB3_BACK", "flat", ("flat", "hump"))
CELL_NAME = _flag("GB3_CELL", "LP803860", tuple(CELLS))
LCD_PLATE = _flag("GB3_LCD_PLATE", "0", ("0", "1")) == "1"
PROCESS = _flag("GB3_PROCESS", "mjf", ("mjf", "fdm"))
FLAGS = {"FRONT": FRONT, "BACK": BACK, "CELL": CELL_NAME, "LCD_PLATE": LCD_PLATE, "PROCESS": PROCESS}
VARIANT = f"FRONT={FRONT} BACK={BACK} CELL={CELL_NAME} LCD_PLATE={int(LCD_PLATE)} PROCESS={PROCESS}"
DEFAULT_FLAGS = {"FRONT": "flush", "BACK": "flat", "CELL": "LP803860", "LCD_PLATE": False, "PROCESS": "mjf"}
# "" for the baseline, else the non-default flags (e.g. "front-lip_cell-lp503759ju"): enclosure.py writes a variant's
# exports and renders to out/variants/<slug>/ and build/variants/<slug>/, so the baseline's out/ is never overwritten
VARIANT_SLUG = "_".join(f"{k.lower()}-{str(int(v) if isinstance(v, bool) else v).lower()}"
                        for k, v in FLAGS.items() if v != DEFAULT_FLAGS[k])
if "GB3_CORNER_R" in os.environ:                       # the corner-radius study writes beside the flag variants
    VARIANT_SLUG = "_".join(x for x in (VARIANT_SLUG, "corner-r" + os.environ["GB3_CORNER_R"]) if x)
WALL = WALL_MJF if PROCESS == "mjf" else WALL_FDM


# ================================================================ 3. derived z (all from 1 + 2)
Z_BOARD_BOT = 0.0
BOARD_TOP = float(BOARD_T)                       # 1.60
SPLIT = BOARD_TOP                                # parting plane = board top: the board's top face is the datum
Z_LCM_BACK = BOARD_TOP + P1_MAX + P1_AIR         # 3.90: the lowest the LCM back can be (P1 + air)
Z_GLASS_NOM = Z_LCM_BACK + MODULE_NOM            # 8.15: datasheet total on that back
Z_GLASS_MAX = Z_GLASS_NOM + MODULE_TOL           # 8.45: the thickest module
Z_FACE = Z_GLASS_MAX + (RIM if FRONT == "flush" else GASKET + LIP)    # 8.55 flush / 9.35 lip
Z_PLATE_IN = Z_FACE - PLATE                      # 7.35: front plate inner face
Z_PLATE_IN_CN1 = Z_FACE - PLATE_CN1              # 7.55: plate thinned over CN1
LEDGE_Z1 = Z_LCM_BACK - PANEL_TAPE               # 3.75: ledge top (the LCM back sits on it through the tape)
LEDGE_Z0 = LEDGE_Z1 - LEDGE_T                    # 2.75
PANEL_PARTS_T = LCM_T + DST_T + SENSOR_T + GLASS_T   # 4.30: the parts sum (the datasheet total is 4.25 +/-0.3)
# context panel: flush - its LCM back on the ledge tape; lip - its glass top under the gasket (LCM back floats)
Z_PANEL0 = Z_LCM_BACK if FRONT == "flush" else Z_GLASS_MAX - PANEL_PARTS_T
Z_PANEL_LCM = (Z_PANEL0, Z_PANEL0 + LCM_T)
Z_PANEL_TOUCH = (Z_PANEL_LCM[1], Z_PANEL_LCM[1] + DST_T + SENSOR_T)
Z_PANEL_GLASS = (Z_PANEL_TOUCH[1], Z_PANEL_TOUCH[1] + GLASS_T)
Z_FOLD_SKIN_IN = Z_GLASS_MAX - FOLD_SKIN_RULE    # 7.90: the plate's inner face over the fold bay is at or above it

# the board-limited back: the deepest B part (USB-C) + clearance + printed floor
Z_BACK_BOARD = -(USBC_T + CLR_B + FLOOR_PR)      # -4.86

# the cell column (no PCB in it)
CELL_SPEC = CELLS[CELL_NAME]
CELL_T, CELL_SWELL = CELL_SPEC["t"], CELL_SPEC["swell"]
if LCD_PLATE:            # 0.5 SUS plate under the LCM, the cell taped to it, the swell gap toward the back
    SS_Z = (Z_LCM_BACK - SS_AIR - SS_T, Z_LCM_BACK - SS_AIR)                 # 3.10..3.60
    SS_FILM_Z = (SS_Z[0] - SS_FILM, SS_Z[0])                                # 3.05..3.10
    _cell_z1 = SS_FILM_Z[0] - CELL_TAPE                                     # 2.90: cell top, hanging
    _floor_need = _cell_z1 - CELL_T - CELL_SWELL                            # floor top under the swollen cell
else:                    # the cell on its floor, its swollen top CELL_AIR under the LCM
    SS_Z = SS_FILM_Z = None
    _cell_z1 = Z_LCM_BACK - CELL_AIR - CELL_SWELL                           # 2.85
    _floor_need = _cell_z1 - CELL_T - CELL_TAPE                             # -5.80
if _floor_need - FLOOR_PR >= Z_BACK_BOARD - EPS:      # a printed floor fits above the board-limited back: no FR4
    CELL_FLOOR = "printed"
    Z_BACK_CELL = _floor_need - FLOOR_PR             # what the cell column alone would need
    Z_BACK_BAY = Z_BACK_BOARD
    CELL_FLOOR_Z = (Z_BACK_BAY, Z_BACK_BAY + FLOOR_PR)
else:                                                # 1.0 FR4 plate closing a window in the back floor
    CELL_FLOOR = "fr4"
    Z_BACK_CELL = _floor_need - FLOOR_FR4            # -6.80
    Z_BACK_BAY = min(Z_BACK_BOARD, Z_BACK_CELL)
    CELL_FLOOR_Z = (Z_BACK_BAY, Z_BACK_BAY + FLOOR_FR4)
if LCD_PLATE:
    CELL_Z = (_cell_z1 - CELL_T, _cell_z1)                                   # hangs from the plate
    CELL_SWELL_Z = (CELL_Z[0] - CELL_SWELL, CELL_Z[0])                       # free swell gap under it
    CELL_TAPE_Z = (CELL_Z[1], SS_FILM_Z[0])
else:
    CELL_Z = (CELL_FLOOR_Z[1] + CELL_TAPE, CELL_FLOOR_Z[1] + CELL_TAPE + CELL_T)   # rests on its floor (tape)
    CELL_SWELL_Z = (CELL_Z[1], CELL_Z[1] + CELL_SWELL)
    CELL_TAPE_Z = (CELL_FLOOR_Z[1], CELL_Z[0])

# the back: flat everywhere at the bay level, or (hump) the board-limited body + a hump over the cell bay only
HUMP = BACK == "hump" and Z_BACK_BAY < Z_BACK_BOARD - EPS
Z_BACK = Z_BACK_BOARD if HUMP else Z_BACK_BAY    # the back face outside the hump: -6.80 flat / -4.86 hump
Z_BACK_HUMP = Z_BACK_BAY if HUMP else None       # the hump's back face (None: no hump)
Z_BACK_MIN = min(Z_BACK, Z_BACK_BAY)             # the lowest outer face
Z_FLOOR_IN = Z_BACK + FLOOR_PR                   # printed floor inner face outside the hump: -5.60 flat
Z_FLOOR_IN_HUMP = Z_BACK_HUMP + FLOOR_PR if HUMP else None
TOTAL = Z_FACE - Z_BACK_MIN                      # 15.35
for _n in [n for n in list(globals()) if n.startswith(("Z_", "LEDGE_Z", "SS_Z", "SS_FILM_Z", "CELL_Z", "CELL_SWELL_Z",
                                                         "CELL_TAPE_Z", "CELL_FLOOR_Z")) or n in ("TOTAL", "SPLIT")]:
    _v = globals()[_n]                                # strip float noise (-6.800000000000001) from the derived z
    if isinstance(_v, float):
        globals()[_n] = round(_v, 6)
    elif isinstance(_v, tuple):
        globals()[_n] = tuple(round(x, 6) for x in _v)


# ================================================================ 4. outline + frame helpers
def P(x, y):
    """KiCad board point -> enclosure XY (Y flipped)."""
    return (x, -y)


def flip(pts):
    return [(x, -y) for x, y in pts]


class Rect(NamedTuple):
    """axis-aligned rectangle in the ENCLOSURE frame (x0 < x1, y0 < y1). Rect.k() builds one from KiCad numbers."""
    x0: float
    y0: float
    x1: float
    y1: float

    @classmethod
    def k(cls, x0, y0, x1, y1):
        """from a KiCad rectangle (Y down; any corner order)."""
        return cls(min(x0, x1), -max(y0, y1), max(x0, x1), -min(y0, y1))

    @classmethod
    def centred(cls, cx, cy, w, h):
        return cls(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2)

    @property
    def cx(self): return (self.x0 + self.x1) / 2

    @property
    def cy(self): return (self.y0 + self.y1) / 2

    @property
    def w(self): return self.x1 - self.x0

    @property
    def h(self): return self.y1 - self.y0

    def kicad(self):
        """(x0, y0, x1, y1) in KiCad numbers."""
        return (self.x0, -self.y1, self.x1, -self.y0)

    def grow(self, d, dy=None):
        dy = d if dy is None else dy
        return Rect(self.x0 - d, self.y0 - dy, self.x1 + d, self.y1 + dy)

    def overlaps(self, o, pad=0.0):
        return self.x0 - pad < o.x1 and o.x0 - pad < self.x1 and self.y0 - pad < o.y1 and o.y0 - pad < self.y1

    def contains(self, x, y, pad=0.0):
        return self.x0 - pad <= x <= self.x1 + pad and self.y0 - pad <= y <= self.y1 + pad

    def face(self, r=0.0):
        """planar face at z 0 (corner radius r)."""
        return rrect_face(self.cx, self.cy, self.w, self.h, r)

    def prism(self, z0, z1, r=0.0):
        return prism(self.face(r), z0, z1)


def rrect_face(cx, cy, w, h, r=0.0):
    if r > 1e-6:
        return (Pos(cx, cy) * RectangleRounded(w, h, min(r, w / 2 - 1e-3, h / 2 - 1e-3))).faces()[0]
    return face([(cx - w / 2, cy - h / 2), (cx + w / 2, cy - h / 2), (cx + w / 2, cy + h / 2),
                 (cx - w / 2, cy + h / 2)], board_frame=False)


def face(pts, board_frame=True):
    """planar face at z 0 from a polygon (KiCad numbers by default: Y gets flipped)."""
    pts = flip(pts) if board_frame else list(pts)
    if pts[0] == pts[-1]:
        pts = pts[:-1]
    return Face(Wire.make_polygon([Vector(x, y, 0) for x, y in pts], close=True))


def prism(f, z0, z1):
    """extrude face f (at z 0) to span z0..z1 (always +Z, whatever the face's winding)."""
    return Pos(0, 0, z0) * extrude(f, amount=z1 - z0, dir=(0, 0, 1))


def cyl(x, y, d, z0, z1):
    """vertical cylinder, enclosure frame."""
    return Pos(x, y, (z0 + z1) / 2) * Cylinder(d / 2, z1 - z0)


def box(cx, cy, w, h, z0, z1):
    """axis-aligned box, enclosure frame."""
    return Pos(cx, cy, (z0 + z1) / 2) * Box(w, h, z1 - z0)


def yz_plate(pts_yz, x0, x1):
    """a solid from a (y, z) polygon (ENCLOSURE y) extruded along +X over x0..x1 (sections across the Y-Z plane)."""
    pl = Plane(origin=(x0, 0, 0), x_dir=(0, 1, 0), z_dir=(1, 0, 0))     # local x -> +Y, local y -> +Z
    f = face(pts_yz, board_frame=False)
    return pl.location * extrude(f, amount=x1 - x0, dir=(0, 0, 1))


OUTLINE = Rect.k(*BJ["mech"]["console_outline"]["bbox"])        # x 20..214.7, KiCad y 30..119.8 (194.7 x 89.8)


def outline_face(inset=0.0):
    """the console outline at z 0, offset inward by inset (corner radius OUTLINE_R - inset)."""
    return rrect_face(OUTLINE.cx, OUTLINE.cy, OUTLINE.w - 2 * inset, OUTLINE.h - 2 * inset,
                      max(OUTLINE_R - inset, 0.01))


WALL_IN = OUTLINE.grow(-WALL)                    # the walls' inner faces (straight runs)


# ================================================================ 5. board.json named positions
PARTS = {p["ref"]: p for p in BJ["parts"]}
SWITCH_JSON = {s["ref"]: s for s in BJ["switches"]}
EDGE_JSON = BJ["edge_features"]
MECH = BJ["mech"]


def part_rect(ref):
    """a part's model-hull (or courtyard) bounding box, enclosure frame."""
    pts = PARTS[ref].get("hull") or PARTS[ref]["courtyard"]
    return Rect.k(min(q[0] for q in pts), min(q[1] for q in pts), max(q[0] for q in pts), max(q[1] for q in pts))


def part_outline(ref):
    """a part's model hull (or courtyard) as an enclosure-frame polygon."""
    return flip(PARTS[ref].get("hull") or PARTS[ref]["courtyard"])


# mounting holes H1-H5 (NPTH 2.2, r 2.75 copper keep-out F+B)
HOLES = {h["name"]: {"at": P(h["x"], h["y"]), "kicad": (h["x"], h["y"]), "d": h["d"], "keepout_r": h["keepout_r"]}
         for h in BJ["board"]["holes"]}


def _switch(s):
    out = {"ref": s["ref"], "role": s["role"], "value": s["value"], "side": s["side"], "kind": s["kind"],
           "kicad": (s["x"], s["y"]), "travel": s["travel"], "src": s["src"]}
    if "tip_xy" in s:                                   # SKRT side push: tip, outward direction, axis height
        out.update(at=P(*s["tip_xy"]), tip=P(*s["tip_xy"]), out=(s["out_dir"][0], -s["out_dir"][1]),
                   axis_z=s["axis_z_ds"], plunger_d=s["plunger_d"], plunger_out=s["plunger_out_of_body"])
    else:                                               # TS-1187A top push: keystake centre + top
        out.update(at=P(*s["actuator_xy"]), top_z=s["top_z_ds"], top_z_max=s["top_z_ds_max"],
                   actuator_d=s["actuator_d"], press=(s["press"][0], -s["press"][1], s["press"][2]))
    return out


SWITCHES = {r: _switch(s) for r, s in SWITCH_JSON.items()}       # SW1 (reset, B side) .. SW16
LEDS = {r: {"at": P(PARTS[r]["x"], PARTS[r]["y"]), "rect": part_rect(r), "top_z": PARTS[r]["z_design"][1]}
        for r in ("U5", "U14")}                                   # 0603 LEDs on F, under the top-edge bezel

# panel ATM0500D27-CT (LCM outline from the board's User.2 rect = place.py's screen outline)
_pm = MECH["panel"]
_lcm = Rect.k(*_pm["rect"])                                       # x 57..177.7, KiCad y 37..112.8
_ps = {"active": (63.42, 40.32, 171.42, 105.12), "view": (63.0, 39.9, 171.8, 105.5)}   # panel_stack.json (p16 / p5)
PANEL = {
    "lcm": _lcm,
    "glass": Rect.centred(_lcm.cx, _lcm.cy, 120.1, 75.4),          # p6: 120.1 x 75.4, assumed centred (+/-0.3)
    "sensor": Rect.centred(_lcm.cx, _lcm.cy, 119.1, 74.2),         # p6: ITO sensor 119.1 x 74.2
    "active": Rect.k(*_ps["active"]),                             # 108.0 x 64.8 (2.18 toward the top edge)
    "view": Rect.k(*_ps["view"]),                                 # cover-glass black-print opening 108.8 x 65.6
    "z_lcm": Z_PANEL_LCM, "z_touch": Z_PANEL_TOUCH, "z_glass": Z_PANEL_GLASS,
    "max": Rect.centred(_lcm.cx, _lcm.cy, _lcm.w + LCM_OUTLINE_TOL, _lcm.h + LCM_OUTLINE_TOL),   # 121.0 x 76.1
    "src": _pm["src"],
}

# cell: the LP-803860's drawn outline (place.py BAT_OUTLINE); a smaller cell keeps its top-right corner (PCM + lead
# end toward CN1), shrinking toward -x / -y (enclosure)
_cr = Rect.k(*MECH["cell"]["rect"])                              # x 141.2..179.7, KiCad y 54.3..116.3
CELL = {
    "name": CELL_NAME, "label": CELL_SPEC["label"], "t": CELL_T, "swell": CELL_SWELL,
    "rect": Rect(_cr.x1 - CELL_SPEC["w"], _cr.y1 - CELL_SPEC["l"], _cr.x1, _cr.y1),
    "buffer": Rect.k(*MECH["cell"]["buffer"]),                     # the notch gaps: 2.0 round the LP-803860
    "z": CELL_Z, "swell_z": CELL_SWELL_Z, "tape_z": CELL_TAPE_Z,
    "floor": CELL_FLOOR, "floor_z": CELL_FLOOR_Z,                  # "fr4": a bought plate in a floor window (back.py)
    "fence_t_max": 1.2, "fence_clr": 0.4,                          # fences <= 1.2 in the 2 mm notch gaps, 0.4 off it
    "leads": MECH["cell"]["leads"],
}
_bk = CELL["buffer"].kicad()                                     # KiCad 139.2, 52.3, 181.7, 117.8
HUMP_RECT = Rect.k(_bk[0] - WALL, _bk[1] - WALL, _bk[2] + WALL, OUTLINE.kicad()[3])   # KiCad 137.7..183.2 x 50.8..119.8
HUMP_INNER = HUMP_RECT.grow(-WALL) if HUMP else None


def back_at(x, y):
    """outer back face z under an enclosure point."""
    if HUMP and HUMP_RECT.contains(x, y):
        return Z_BACK_HUMP
    return Z_BACK


def floor_in_at(x, y):
    """printed floor inner face z under an enclosure point (bare shell; the cell bay window is back.py's)."""
    if HUMP and HUMP_INNER.contains(x, y):
        return Z_FLOOR_IN_HUMP
    return Z_FLOOR_IN


_lra = MECH["lra"]
LRA = {"at": P(_lra["x"], _lra["y"]), "d": _lra["d"], "t": LRA_T, "ring_id": _lra["d"] + 0.4}
LRA["z"] = (round(floor_in_at(*LRA["at"]), 6), round(floor_in_at(*LRA["at"]) + LRA_T, 6))   # glued on the floor

_spk = MECH["speaker"]
SPEAKER = {"at": P(_spk["x"], _spk["y"]), "d": _spk["d"], "lip_d": _spk["front_lip_d"], "t": SPK_T,
           "lip_h": SPK_LIP_H, "foam": SPK_FOAM, "cup_id": SPK_CUP_ID, "grille_d": SPK_GRILLE_D}
_sz0 = floor_in_at(*SPEAKER["at"]) + SPK_FOAM               # back-firing: lip down on the foam ring on the floor
SPEAKER.update(foam_z=(round(_sz0 - SPK_FOAM, 6), round(_sz0, 6)), lip_z=(round(_sz0, 6), round(_sz0 + SPK_LIP_H, 6)),
               z=(round(_sz0, 6), round(_sz0 + SPK_T, 6)))

# connectors
CN1 = {"rect": part_rect("CN1"), "top_z": PARTS["CN1"]["z_design"][1], "plate_in_z": Z_PLATE_IN_CN1,
       "mouth_y": -BJ["connectors"]["CN1"]["mouth_face_y"], "src": BJ["connectors"]["CN1"]["src"]}
# the cell's PHR-2 plug mated in CN1 and the lead route (cell report: PCM end -> east in the 2 mm end gap -> up in the
# x 177.7-181.7 corner outside the panel -> over the board edge -> into the plug from the south). Estimates.
PLUG_W = _in("phr2_w", 5.8, "C295747.pdf p2: PHR-2 housing 5.8 wide")
PLUG_OUT = _in("phr2_out", 2.5, "cell report: mated PHR-2 sticks out of CN1's mouth to y ~52.0-52.5")
PLUG_T = _in("phr2_t", 4.5, "C295747.pdf p2: PHR-2 4.5 thick; wires leave at z ~4.35 +/-0.5 (cell report)")
PLUG_Z = _in("phr2_z", 4.35, "cell report: lead exit height at the mated plug (est +/-0.5)")
LEAD_EXIT_TOL = _in("lead_exit_tol", 8.0, "LiIon2000mAh37V.pdf p5: leads leave the PCM end 'near its middle' (+/-8)")
CN1["plug"] = Rect(CN1["rect"].cx - PLUG_W / 2, CN1["mouth_y"] - PLUG_OUT, CN1["rect"].cx + PLUG_W / 2,
                   CN1["mouth_y"])
CN1["plug_z"] = (PLUG_Z - PLUG_T / 2, PLUG_Z + PLUG_T / 2)
P1 = {"rect": part_rect("P1"), "top_z": PARTS["P1"]["z_design"][1]}
# FPC1 (touch FFC, B side): the board's step 7 (dd7c895) rotated it 180 deg to (118.5, 72.8) so its slot faces +y KiCad and
# the touch tail, which wraps the notch edge, reaches it (lid edge y 76.28: the furthest south its west tab clears U16.1).
# A board without the fix (rot 0, slot facing -y) still gets the corrected body for comparison.
FPC1_FIXED = abs(PARTS["FPC1"]["rot"] % 360 - 180) < 1
FPC1_KEY = "FPC1" if FPC1_FIXED else "FPC1 corrected"           # the context body the tail plugs into
FPC1_OLD_ORIGIN_K = (PARTS["FPC1"]["x"], PARTS["FPC1"]["y"])
FPC1_NEW_ORIGIN_K = (118.5, 72.8)                                # step 7


def fpc1_corrected_k(pts):
    """KiCad points of FPC1 as placed -> the corrected placement (rot 180 about the new origin; as placed once fixed)."""
    if FPC1_FIXED:
        return [tuple(p) for p in pts]
    (ox, oy), (nx, ny) = FPC1_OLD_ORIGIN_K, FPC1_NEW_ORIGIN_K
    return [(nx - (x - ox), ny - (y - oy)) for x, y in pts]


FPC1 = {"placed": part_outline("FPC1"), "corrected": flip(fpc1_corrected_k(PARTS["FPC1"]["hull"])),
        "z": tuple(PARTS["FPC1"]["z_design"])}
FPC1["lid_y_k"] = max(y for _, y in fpc1_corrected_k(PARTS["FPC1"]["hull"]))     # 76.28 (step 7): slot faces +y KiCad

# edge features (re-registered models + datasheets; NOT the KiCad 3D view, which has USBC1 / Audio1 1.5-2.2 off)
_u, _j, _m = EDGE_JSON["usb_c"], EDGE_JSON["jack"], EDGE_JSON["microsd"]
USBC = {"x": _u["x_centre"], "mouth_y": -_u["mouth_y"], "z": tuple(_u["z"]), "axis_z": _u["axis_z"],
        "shell_w": _u["shell_w"], "shell_h": _u["z"][1] - _u["z"][0], "shell_l": 7.35, "opening": tuple(_u["opening"]),
        "relief": (13.0, 7.0, 0.3),          # overmold relief w x h x depth (USB Type-C: exterior <= 0.40 off the face)
        "face_max": 0.40, "src": _u["src"]}
JACK = {"x": _j["x_axis"], "tip_y": -_j["barrel_tip_y"], "axis_z": _j["axis_z"], "barrel_d": _j["boss_d"],
        "barrel_l": 2.5, "body": (6.0, 11.6), "hole_d": 5.4, "plug_d": 3.6, "shoulder_d": 6.0,
        "z_top": PARTS["Audio1"]["z_design"][1], "src": _j["src"]}
MICROSD = {"x": _m["x_centre"], "face_y": -_m["slot_face_y"], "z": tuple(_m["z"]), "card_w": 11.0, "card_t": 1.0,
           "out_latched": 1.30, "out_ejected": 5.30, "src": _m["src"]}
MICROSD["card_z"] = (MICROSD["z"][0] / 2 - MICROSD["card_t"] / 2, MICROSD["z"][0] / 2 + MICROSD["card_t"] / 2)  # est
RESET = {"switch": "SW1", "at": SWITCHES["SW1"]["at"], "keystake_z": SWITCHES["SW1"]["top_z"], "pin_d": 1.0,
         "hole_d": 1.5}                    # pushed with a SIM tool / paperclip (<= 1.0) through the back floor


# ---------------------------------------------------------------- flex paths (the panel report's fold model)
class Path:
    """a centreline in the (KiCad y, z) plane: line / arc segments, parametrised by length s from the panel edge."""

    def __init__(self):
        self.segs = []                                   # (kind, s0, s1, data)
        self.s = 0.0

    def line(self, p0, p1):
        L = math.dist(p0, p1)
        self.segs.append(("line", self.s, self.s + L, (p0, p1)))
        self.s += L
        return self

    def arc(self, c, r, a0, a1):
        """angles in deg from the +y axis toward +z; sweeps a0 -> a1."""
        L = abs(math.radians(a1 - a0)) * r
        self.segs.append(("arc", self.s, self.s + L, (c, r, a0, a1)))
        self.s += L
        return self

    def at(self, s):
        """(y, z, ty, tz) at s (clamped)."""
        for kind, s0, s1, d in self.segs:
            if s <= s1 + 1e-9 or (kind, s0, s1, d) == self.segs[-1]:
                f = 0.0 if s1 == s0 else min(max((s - s0) / (s1 - s0), 0.0), 1.0)
                if kind == "line":
                    (y0, z0), (y1, z1) = d
                    L = math.dist(d[0], d[1])
                    return (y0 + (y1 - y0) * f, z0 + (z1 - z0) * f, (y1 - y0) / L, (z1 - z0) / L)
                (cy, cz), r, a0, a1 = d
                a = math.radians(a0 + (a1 - a0) * f)
                sg = 1 if a1 > a0 else -1
                return (cy + r * math.cos(a), cz + r * math.sin(a), -math.sin(a) * sg, math.cos(a) * sg)
        raise ValueError(s)

    def samples(self, s0, s1, step_deg=6.0):
        """s values covering [s0, s1]: the ends, segment joints, and arc points every step_deg."""
        ss = {s0, s1}
        for kind, a, b, d in self.segs:
            for v in (a, b):
                if s0 < v < s1:
                    ss.add(v)
            if kind == "arc":
                n = max(1, int(abs(d[3] - d[2]) / step_deg))
                for i in range(1, n):
                    v = a + (b - a) * i / n
                    if s0 < v < s1:
                        ss.add(v)
        return sorted(ss)


def _flex_paths():
    lcm, glass = PANEL["lcm"], PANEL["glass"]
    ly1, gy1 = -lcm.y0, -glass.y0                        # KiCad y of the panel's / glass's bottom edge (112.8 / 112.6)
    t = FLEX_T
    # display (40P) flex: 180 deg loop through the cable notch, back under the panel at P1's slot height into P1
    p1_front = -P1["rect"].y0                            # 70.78: P1's entry face (KiCad y)
    z_exit, z_slot = Z_PANEL0 + DISP_EXIT_DZ, BOARD_TOP + P1_SLOT_DZ
    r = (z_exit - z_slot) / 2
    need = DISP_TAIL - (ly1 - (p1_front - P1_INSERT))   # length the loop has to take up
    extra = max(0.0, need - (2 * FLEX_S0 + math.pi * r))
    yc, zc = ly1 + FLEX_S0 + extra / 2, (z_exit + z_slot) / 2
    disp = Path().line((ly1, z_exit), (yc, z_exit)).arc((yc, zc), r, 90, -90).line((yc, z_slot), (p1_front, z_slot))
    disp_out = yc + r + t / 2                            # the loop's outer face (KiCad y)
    # touch (6P) flex: from between glass and sensor, round the board's bottom edge (U-turn outside the display loop),
    # flat under the board's B face to the corrected FPC1 (lid edge FPC1["lid_y_k"])
    z_e = Z_PANEL_GLASS[1] - TOUCH_EXIT_BELOW - t / 2
    z_u = Z_BOARD_BOT - t / 2                            # strip on the board's bottom face
    R = R_TOUCH
    y_end = FPC1["lid_y_k"] - HDGC_INSERT                 # FFC end inside the corrected FPC1
    fixed = math.pi * R + (z_e - z_u - 2 * R) + (gy1 - y_end)
    y_b = gy1 + (TOUCH_TAIL - fixed) / 2                 # the U-turn takes the slack (bows toward the bottom edge)
    y_v = y_b + R
    touch = (Path().line((gy1, z_e), (y_b, z_e)).arc((y_b, z_e - R), R, 90, 0)
             .line((y_v, z_e - R), (y_v, z_u + R)).arc((y_b, z_u + R), R, 0, -90).line((y_b, z_u), (y_end, z_u)))
    ic_s = (16.2, 31.9)                                  # p6: FT7311 + steel area, s from the glass edge
    return {
        "display": {"path": disp, "t": t, "loop_out_y": disp_out, "loop_r": r, "exit_z": z_exit, "slot_z": z_slot,
                    "widths": [(0.0, 10.5, (102.3, 132.4)), (10.5, 34.43, (83.4, 132.4)),
                               (34.43, disp.s, (83.4, 103.9))]},          # KiCad x ranges by s (panel report)
        "touch": {"path": touch, "t": t, "uturn_y": y_v, "exit_z": z_e, "end_y": y_end, "ic_s": ic_s,
                  "ic_x": (104.7, 125.4), "ic_t": TOUCH_IC_T,
                  "ic_y": (touch.at(ic_s[1])[0], touch.at(ic_s[0])[0]),
                  "widths": [(0.0, 4.0, (98.1, 131.1)), (4.0, ic_s[0], (108.6, 120.5)),
                             (ic_s[1], touch.s - HDGC_INSERT, (114.9, 121.9))],   # the last HDGC_INSERT is in FPC1
                  "fits": y_v - t / 2 >= disp_out + FLEX_GAP - EPS, "min_bottom_z": z_u - t / 2},
    }


FLEX = _flex_paths()


# ================================================================ 6. shared contracts
# ---------------------------------------------------------------- face keys: front.py cuts the holes (+ keeps the
# flange zone clear), controls.py builds the caps. TS-1187A: keystake top z 3.10, travel 0.2 +/-0.1, hard stop at
# 0.3 (its stainless cover, z 2.80). Caps go in head-first from inside (gb2 style): head in the face hole KEY_PROUD
# over the face, a flange under the plate retains it, the stem ends rest_gap over the keystake.
KEYSTAKE_Z = SWITCHES["SW2"]["top_z"]                # 3.10
TS_STOP = 0.3
KEY_REST_GAP = 0.15
DPAD_REST_GAP = 0.10                                 # the D-pad rests on its own pivot (board-referenced)
KEY_PROUD = 1.4                                      # cap top over the face: caps 6.7 long (z 3.25..9.95 flush)
KEY_HOLE_CLR = 0.2                                   # radial, head in its face hole
FLANGE_T, FLANGE_LIP = 0.8, 0.8                      # flange under the plate: thickness, overhang past the face hole
KEY_SWEEP_CLR = 0.3                                  # front.py keeps this clear round each flange, plate to keystake
FLANGE_Z = (round(Z_PLATE_IN - FLANGE_T, 6), Z_PLATE_IN)


def _r6(v):
    return round(v, 6) + 0.0 if isinstance(v, float) else tuple(_r6(x) for x in v) if isinstance(v, tuple) else v


def _round_key(ref, d):
    s = SWITCHES[ref]
    hole_d = d + 2 * KEY_HOLE_CLR
    flange_d = hole_d + 2 * FLANGE_LIP
    return {k: _r6(v) for k, v in {
        "shape": "round", "switch": ref, "role": s["role"], "at": s["at"], "contacts": [s["at"]], "d": d,
        "hole_d": hole_d, "flange_d": flange_d, "flange_z": FLANGE_Z, "rest_gap": KEY_REST_GAP,
        "tip_z": KEYSTAKE_Z + KEY_REST_GAP, "top_z": Z_FACE + KEY_PROUD, "clear_d": flange_d + 2 * KEY_SWEEP_CLR}.items()}


_dpad_sw = [SWITCHES[r]["at"] for r in ("SW2", "SW3", "SW4", "SW5")]       # left, right, up, down
KEYS = {
    "A": _round_key("SW7", 8.0), "B": _round_key("SW9", 8.0), "X": _round_key("SW8", 8.0),
    "Y": _round_key("SW6", 8.0), "START": _round_key("SW10", 5.0), "SELECT": _round_key("SW11", 5.0),
    # one cross: arms reach `arm` from the centre, `w` wide; four legs onto SW2-5 (8.0 from the centre), a centre
    # pivot leg resting on the board's top face (F side free within 4.7 of the centre). hole_clr: gb2 thin-stack 1.05
    # with the face 14.8 over the pivot, scaled to gb3's 6.95 (sway ~0.34 axial / 0.48 diagonal + 0.29 off-centre)
    "DPAD": {"shape": "cross", "switch": "SW2-SW5", "role": "D-pad",
             "at": (sum(p[0] for p in _dpad_sw) / 4, sum(p[1] for p in _dpad_sw) / 4), "contacts": _dpad_sw,
             "arm": 9.4, "w": 6.6, "corner_r": 0.5, "hole_clr": 0.7, "flange_lip": FLANGE_LIP, "flange_z": FLANGE_Z,
             "pivot_d": 3.0, "pivot_z": BOARD_TOP, "rest_gap": DPAD_REST_GAP,
             "tip_z": _r6(KEYSTAKE_Z + DPAD_REST_GAP), "top_z": _r6(Z_FACE + KEY_PROUD)},
}


def _cross_pts(arm, w, grow=0.0):
    """the D-pad cross outline (enclosure frame, about its centre), grown by `grow` (sharp-jointed offset)."""
    a, h = arm + grow, w / 2 + grow
    return [(h, h), (a, h), (a, -h), (h, -h), (h, -a), (-h, -a), (-h, -h), (-a, -h), (-a, h), (-h, h), (-h, a),
            (h, a)]


def key_hole_face(name):
    """the face hole outline of a key at z 0 (front.py cuts it through the plate)."""
    k = KEYS[name]
    if k["shape"] == "round":
        return Pos(*k["at"]) * Face(Wire.make_circle(k["hole_d"] / 2))
    cx, cy = k["at"]
    return face([(cx + x, cy + y) for x, y in _cross_pts(k["arm"], k["w"], k["hole_clr"])], board_frame=False)


def key_flange_face(name):
    """the flange outline at z 0 (controls.py's flange; front.py keeps KEY_SWEEP_CLR clear round it)."""
    k = KEYS[name]
    if k["shape"] == "round":
        return Pos(*k["at"]) * Face(Wire.make_circle(k["flange_d"] / 2))
    cx, cy = k["at"]
    return face([(cx + x, cy + y) for x, y in _cross_pts(k["arm"], k["w"], k["hole_clr"] + k["flange_lip"])],
                board_frame=False)


# ---------------------------------------------------------------- edge keys + shoulders: front.py cuts the notches
# (open at the split, through the wall), controls.py builds the caps. ALPS SKRTLBE010: plunger D2 on an axis 1.7 over
# the board (z 3.3), travel 0.2, 1.6 N.
EDGE_AXIS_Z = SWITCHES["SW12"]["axis_z"]             # 3.3
EDGE_REST_GAP = 0.15                                 # cap contact face to the plunger tip at rest
EDGE_PROUD = 0.5                                     # cap face out of the outer surface
EDGE_NOTCH_Z = (SPLIT, SPLIT + 3.6)                  # notch height: open at the split, axis 3.3 inside it


def _edge(ref, notch_w):
    s = SWITCHES[ref]
    (tx, ty), (ux, uy) = s["tip"], s["out"]
    if abs(ux) > abs(uy):                            # right wall (x = OUTLINE.x1)
        d = (OUTLINE.x1 - tx) / ux
    else:                                            # top wall (y = OUTLINE.y1)
        d = (OUTLINE.y1 - ty) / uy
    return {k: _r6(v) for k, v in {
        "switch": ref, "role": s["role"], "tip": (tx, ty), "dir": (ux, uy), "inset": d,
        "outer": (tx + ux * d, ty + uy * d), "inner": (tx + ux * (d - WALL), ty + uy * (d - WALL)),
        "contact": (tx + ux * EDGE_REST_GAP, ty + uy * EDGE_REST_GAP), "axis_z": EDGE_AXIS_Z,
        "notch_w": notch_w, "notch_z": EDGE_NOTCH_Z, "rest_gap": EDGE_REST_GAP, "proud": EDGE_PROUD}.items()}


EDGE_KEYS = {"VOL_UP": _edge("SW12", 4.0), "VOL_DOWN": _edge("SW13", 4.0), "POWER": _edge("SW14", 4.0)}
SHOULDERS = {"L": _edge("SW15", 12.0), "R": _edge("SW16", 12.0)}      # tips 6.2 inside the outline: long-stem / hinged

# ---------------------------------------------------------------- bosses: front.py builds the front bosses (tips at
# SPLIT = the board datum, heat-set insert holes), back.py the back bosses + counterbored clearance holes. On-board
# (H1-H5): the back boss stops BACK_BOSS_GAP under the board's bottom face, so the screw pulls the board onto the
# front datum. Off-board seam screws (S1 on the bottom edge, S2 on the right edge above the speaker): the back boss rises to SPLIT and meets
# the front boss tip (shell to shell).
BOSS_D = 5.5                                         # fits the r 2.75 copper keep-outs
# front.py merges each front boss into a wall with a BOSS_D-wide block (gusset) when its gap to the wall's inner face
# is under BOSS_MERGE_MAX: the nearest wall, unless BOSS_MERGE_DIR names another (outward unit, enclosure frame). H3
# goes into the RIGHT wall, which leaves the top wall free from H2 to the corner for the R shoulder's yoke.
# controls.py routes the shoulder yokes round the blocks from the same two names.
BOSS_MERGE_MAX = 4.0
BOSS_MERGE_DIR = {"H3": (1.0, 0.0)}
INSERT = {"hole_d": 3.2, "depth": 4.0, "label": "M2 x 3 brass heat-set (CNC Kitchen class)"}
SCREW = {"d": 2.0, "clear_d": 2.4, "head_d": 3.8, "head_h": 2.0, "cbore_d": 4.2, "label": "DIN 912 M2"}
BACK_BOSS_GAP = 0.2
SEAM_SCREWS_K = {"S1": (136.0, 116.3),               # between the fold bay (x <= 133) and the cell buffer (x >= 139.2)
                 "S2": (211.4, 91.5)}                # right edge above the speaker, 0.6 off its cup's wall and 1.0 off
                                                     # the board (the R20 corner put the old spot, 209.6 / 115.6, outside)
BOSSES = {}
for _n, _h in HOLES.items():
    BOSSES[_n] = {"at": _h["at"], "on_board": True, "front_z": (SPLIT, Z_PLATE_IN),
                  "back_top_z": Z_BOARD_BOT - BACK_BOSS_GAP}
for _n, _k in SEAM_SCREWS_K.items():
    BOSSES[_n] = {"at": P(*_k), "on_board": False, "front_z": (SPLIT, Z_PLATE_IN), "back_top_z": SPLIT}

# ---------------------------------------------------------------- panel pocket, ledge, fold bay, CN1 (front.py)
POCKET = PANEL["max"].grow(POCKET_CLR)               # 121.4 x 76.5 through the face, centred on the LCM
LIP_WINDOW = PANEL["active"].grow(LIP_WINDOW_MARGIN) if FRONT == "lip" else None   # ~109.9 x 66.7 (lip only)
_pk = POCKET.kicad()
LEDGE = {
    "z": (LEDGE_Z0, LEDGE_Z1), "w": LEDGE_W,
    # the ledge strips inside the pocket, pocket edge -> LEDGE_W under the LCM edge (none over the cell or the fold)
    "segments": {"top": Rect.k(_pk[0], _pk[1], _pk[2], -PANEL["lcm"].y1 + LEDGE_W),
                 "left": Rect.k(_pk[0], _pk[1], PANEL["lcm"].x0 + LEDGE_W, _pk[3]),
                 "bottom_left": Rect.k(_pk[0], -PANEL["lcm"].y0 - LEDGE_W, LEDGE_BL_X1, _pk[3])},
    "relief": {r: {"rect": part_rect(r), "top_z": PARTS[r]["z_design"][1]} for r in ("C20", "C94")},
}
FOLD_BAY = {"rect": Rect.k(84.5, 112.6, 133.0, 117.5), "z": (FOLD_BAY_Z0, Z_FOLD_SKIN_IN),
            "skin_max": Z_FACE - Z_FOLD_SKIN_IN}    # no ribs / bosses in it; the plate over it <= skin_max thick


# ================================================================ 7. regions, report, asserts
REGIONS = [  # (name, KiCad rects, kind) - synth/stack.json regions; "centre" leaves out the USB-C corner
    ("cell bay", [(139.2, 52.3, 181.7, 117.8)], "cell"),
    ("usb-c corner", [(108.0, 30.0, 127.5, 47.0)], "board"),
    ("top strip", [(127.5, 30.0, 181.7, 52.3)], "board"),
    ("centre under panel", [(57.0, 30.0, 108.0, 119.8), (108.0, 47.0, 127.5, 119.8)], "board"),
    ("left grip", [(20.0, 30.0, 57.0, 100.0)], "board"),
    ("jack corner", [(20.0, 100.0, 57.0, 119.8)], "board"),
    ("right grip", [(181.7, 30.0, 214.7, 87.8)], "board"),
    ("speaker cut-out", [(181.7, 87.8, 214.7, 119.8)], "speaker"),
    ("fold bay", [(80.3, 111.0, 133.0, 117.8)], "fold"),
    ("LRA strip", [(127.5, 52.3, 139.2, 111.0)], "lra"),
]
FREE_PARTS = {r for r, p in PARTS.items() if p.get("z_free")}           # LRA1, SPK1: in cut-outs, z is ours


def _poly_rect_overlap(pts, r):
    """convex polygon (KiCad pts) vs KiCad rect (x0, y0, x1, y1): separating-axis test."""
    rx = [(r[0], r[1]), (r[2], r[1]), (r[2], r[3]), (r[0], r[3])]
    for poly in (pts, rx):
        n = len(poly)
        for i in range(n):
            (ax, ay), (bx, by) = poly[i], poly[(i + 1) % n]
            nx, ny = ay - by, bx - ax
            p1 = [nx * x + ny * y for x, y in pts]
            p2 = [nx * x + ny * y for x, y in rx]
            if max(p1) <= min(p2) + 1e-9 or max(p2) <= min(p1) + 1e-9:
                return False
    return True


def _body_parts(side):
    return [p for p in BJ["parts"] if p["side"] == side and p["ref"] not in FREE_PARTS
            and p["z_design"][1] - p["z_design"][0] > 1e-6 and (p.get("hull") or p.get("courtyard"))]


def _floor_under(p):
    """the highest printed-floor inner face under a part's hull (hump: body floor unless wholly over the hump)."""
    pts = flip(p.get("hull") or p["courtyard"])
    return max(floor_in_at(x, y) for x, y in pts)


def region_table(show=True):
    """[(region, needs, model, margin, governed by)] for the current flags; prints it when show."""
    rows = []
    b_parts, f_parts = _body_parts("bottom"), _body_parts("top")
    for name, rects, kind in REGIONS:
        rk = rects[0]
        cx, cy = P((rk[0] + rk[2]) / 2, (rk[1] + rk[3]) / 2)
        if kind == "board":
            hit = [p for p in b_parts if any(_poly_rect_overlap(p.get("hull") or p["courtyard"], r) for r in rects)]
            deep = min(hit, key=lambda p: p["z_design"][0])
            need_back = deep["z_design"][0] - CLR_B - FLOOR_PR
            model_back = max(back_at(x, y) for x, y in flip(deep.get("hull") or deep["courtyard"]))
            top = [p for p in f_parts if any(_poly_rect_overlap(p.get("hull") or p["courtyard"], r) for r in rects)]
            tf = max(top, key=lambda p: p["z_design"][1]) if top else None
            gov = f"B {deep['ref']} {deep['value'][:14]} z {deep['z_design'][0]:.2f}"
            if tf is not None:
                plate_in = Z_PLATE_IN_CN1 if tf["ref"] == "CN1" else Z_PLATE_IN
                gov += f"; F {tf['ref']} top {tf['z_design'][1]:.2f} ({plate_in - tf['z_design'][1]:.2f} under the plate)"
        elif kind == "cell":
            need_back, model_back = Z_BACK_CELL, back_at(cx, cy)
            gov = (f"{CELL_NAME} {CELL_T:.2f} + swell {CELL_SWELL:.2f} + air {CELL_AIR:.2f} under the LCM back "
                   f"{Z_LCM_BACK:.2f}, {CELL_FLOOR} floor" + (", SUS plate" if LCD_PLATE else ""))
        elif kind == "speaker":
            need_back = Z_FACE - PLATE - CLR_B - SPK_T - SPK_FOAM - FLOOR_PR
            model_back = back_at(*SPEAKER["at"])
            gov = f"speaker {SPK_T:.2f} + foam {SPK_FOAM:.2f} (never governs)"
        elif kind == "fold":
            need_back = FLEX["touch"]["min_bottom_z"] - CLR_B - FLOOR_PR
            model_back = back_at(cx, cy)
            gov = (f"touch U-turn (bottom z {FLEX['touch']['min_bottom_z']:.2f}); plate inner >= {Z_FOLD_SKIN_IN:.2f}"
                   f" (skin <= {FOLD_BAY['skin_max']:.2f})")
        else:  # lra
            need_back = Z_LCM_BACK - CELL_AIR - LRA_T - FLOOR_PR
            model_back = back_at(*LRA["at"])
            gov = f"LRA {LRA_T:.2f} on the floor under the LCM (never governs)"
        need, model = Z_FACE - need_back, Z_FACE - model_back
        rows.append((name, rects, round(need, 2), round(model, 2), round(model - need, 2), gov))
    if show:
        print(f"gb3 enclosure [{VARIANT}]: face z {Z_FACE:+.2f}, back z {Z_BACK:+.2f}"
              + (f" (hump {Z_BACK_HUMP:+.2f})" if HUMP else "") + f", total {TOTAL:.2f} mm")
        print(f"  z: glass max {Z_GLASS_MAX:.2f} / nom {Z_GLASS_NOM:.2f} | LCM back {Z_LCM_BACK:.2f} (panel at "
              f"{Z_PANEL0:.2f}) | ledge {LEDGE_Z0:.2f}..{LEDGE_Z1:.2f} | plate inner {Z_PLATE_IN:.2f} (CN1 "
              f"{Z_PLATE_IN_CN1:.2f}) | split {SPLIT:.2f} | cell {CELL_Z[0]:.2f}..{CELL_Z[1]:.2f} (swell "
              f"{CELL_SWELL_Z[0]:.2f}..{CELL_SWELL_Z[1]:.2f}) | {CELL_FLOOR} floor {CELL_FLOOR_Z[0]:.2f}.."
              f"{CELL_FLOOR_Z[1]:.2f} | printed floor inner {Z_FLOOR_IN:.2f}" +
              (f" (hump {Z_FLOOR_IN_HUMP:.2f})" if HUMP else "") + (f" | SUS plate {SS_Z[0]:.2f}..{SS_Z[1]:.2f}"
                                                                     if LCD_PLATE else ""))
        print(f"  {'region':20s} {'KiCad x / y':24s} {'needs':>6s} {'model':>6s} {'margin':>6s}  governed by")
        for name, rects, need, model, margin, gov in rows:
            r = rects[0]
            print(f"  {name:20s} {f'{r[0]:g}-{r[2]:g} / {r[1]:g}-{r[3]:g}':24s} {need:6.2f} {model:6.2f} "
                  f"{margin:+6.2f}  {gov}")
    return rows


def stack_report():
    """every stack input with its source."""
    for k, v in INPUTS.items():
        print(f"  {k:18s} {float(v):7.3f}  {v.src}")


def _asserts():
    bad = []
    if abs(BJ["board"]["thickness"] - BOARD_T) > EPS:
        bad.append(f"board.json thickness {BJ['board']['thickness']} != board_t {BOARD_T}")
    for p in _body_parts("bottom"):              # (1) every B part >= CLR_B above the floor inner face under it
        g = p["z_design"][0] - _floor_under(p)
        if g < CLR_B - EPS:
            bad.append(f"B part {p['ref']} bottom z {p['z_design'][0]:.2f} is {g:.3f} over the floor (< {CLR_B})")
    p1 = PARTS["P1"]["z_design"][1]              # (2) P1 top + P1_AIR <= the LCM back
    if p1 + P1_AIR > Z_LCM_BACK + EPS:
        bad.append(f"P1 top {p1:.2f} + {P1_AIR} > LCM back {Z_LCM_BACK:.2f}")
    lcm = PANEL["lcm"]                           # ... and every other F part under the panel
    for p in _body_parts("top"):
        if p["ref"] != "P1" and _poly_rect_overlap(p.get("hull") or p["courtyard"], lcm.kicad()) and \
                p["z_design"][1] + P1_AIR > Z_LCM_BACK + EPS:
            bad.append(f"F part {p['ref']} top {p['z_design'][1]:.2f} under the panel + {P1_AIR} > {Z_LCM_BACK:.2f}")
    cn1 = PARTS["CN1"]["z_design"][1]            # (3) CN1 clears the thinned plate; every other F part the full plate
    if cn1 + CLR_F > Z_PLATE_IN_CN1 + EPS:
        bad.append(f"CN1 top {cn1:.2f} + {CLR_F} > the plate over it {Z_PLATE_IN_CN1:.2f}")
    for p in _body_parts("top"):
        if p["ref"] != "CN1" and p["z_design"][1] + CLR_F > Z_PLATE_IN + EPS:
            bad.append(f"F part {p['ref']} top {p['z_design'][1]:.2f} + {CLR_F} > plate inner {Z_PLATE_IN:.2f}")
    if LCD_PLATE:                                # (4) the swollen cell + air <= the LCM back (or under the plate)
        if SS_Z[1] + SS_AIR > Z_LCM_BACK + EPS:
            bad.append(f"SUS plate top {SS_Z[1]:.2f} + {SS_AIR} > LCM back {Z_LCM_BACK:.2f}")
        if CELL_SWELL_Z[0] < CELL_FLOOR_Z[1] - EPS:
            bad.append(f"swollen cell bottom {CELL_SWELL_Z[0]:.2f} below its floor {CELL_FLOOR_Z[1]:.2f}")
    elif CELL_SWELL_Z[1] + CELL_AIR > Z_LCM_BACK + EPS:
        bad.append(f"swollen cell top {CELL_SWELL_Z[1]:.2f} + {CELL_AIR} > LCM back {Z_LCM_BACK:.2f}")
    deep = min(p["z_design"][0] for p in _body_parts("bottom"))   # (5) the input that sets the body back is the board's
    if abs(-deep - USBC_T) > EPS:
        bad.append(f"deepest B part {-deep:.2f} != usbc input {USBC_T} (the body back follows the input)")
    j = PARTS["Audio1"]["z_design"]              # (6) edge features consistent with the parts
    if not (j[0] - EPS <= JACK["axis_z"] - JACK["barrel_d"] / 2 and JACK["axis_z"] + JACK["barrel_d"] / 2 <= j[1] + EPS):
        bad.append(f"jack barrel D{JACK['barrel_d']} on z {JACK['axis_z']} outside Audio1 z {j}")
    if not FLEX["touch"]["fits"]:                # (7) the touch U-turn outside the display loop
        bad.append(f"touch U-turn y {FLEX['touch']['uturn_y']:.2f} inside the display loop "
                   f"{FLEX['display']['loop_out_y']:.2f} + {FLEX_GAP}")
    if FLEX["touch"]["uturn_y"] + FLEX_T / 2 > -FOLD_BAY["rect"].y0 + EPS:
        bad.append(f"touch U-turn y {FLEX['touch']['uturn_y']:.2f} past the fold bay")
    if TOTAL < 0 or Z_BACK_MIN > Z_BACK_BOARD + EPS:
        bad.append(f"back {Z_BACK_MIN} above the board-limited back {Z_BACK_BOARD}")
    if bad:
        raise AssertionError("gb3 stack vs board.json:\n  " + "\n  ".join(bad))


if not os.environ.get("GB3_QUIET"):
    region_table()
_asserts()
