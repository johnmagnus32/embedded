"""gb2 rev B - the CONSOLIDATED architecture model behind 01..05 PNGs and ARCHITECTURE.md.

Read-only on every KiCad / enclosure file. Merges the three studies in this folder:
  ME  (me_stacks.py / me_draw.py): architecture R = panel hung from the cover lens in the front shell, nothing under it,
       cell on its own battery plate in a back-shell bay with a door.
  EE  (ee_plan.json, variant T): the floor plan for a board with no parts under the panel (MCU + FPGA on the back,
       J8 next to the charger, USB DFU, GND/GND inner layers or 6 layers).
  teardowns (teardown_figs.py): archetype A (GBA, DS Lite, Analogue Pocket, Miyoo Mini).

Frames: XY = KiCad board mm (+Y DOWN), z = 0 at the board's bottom face, +z toward the viewer (front).
Run it to print the stack totals and the zone collision check:
    ../../enclosure/.venv/bin/python arch_model.py
"""
import json, os
from shapely.geometry import Polygon, LineString, Point, box as sbox

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
BJ = json.load(open(os.path.join(ROOT, "enclosure", "build", "board.json")))
EE = json.load(open(os.path.join(HERE, "ee_plan.json")))

# ------------------------------------------------------------------------------------------------ z stacks
# Each option: board thickness t, the layers above the board top at the screen centre (bottom -> top), the layers
# below the board bottom in the battery bay (board side first), and the grip back (U12 1.7 + 0.3 + 1.6 floor).
# Sources per layer in SRC; every total matches me_stacks.py / REVB_DESIGN.md §3.1 / the mechanical journal.
SRC = {
    "MI": "MI0283QT-11 datasheet P.5: 4.00 thick excl. tape, tol +/-0.20 (note 11), CN4190 tape 0.115",
    "params": "enclosure/params.py:20 PLATE 1.6, :34 LENS_T 1.5 (cast acrylic)",
    "DESIGN": "enclosure/DESIGN.md:18-21 (Z_BACK -3.6 = U12 1.7 + 0.3 + 1.6 floor), :60-61 (0.5 worst lens gap)",
    "REVB": "pcb/REVB_DESIGN.md §3.1 z stack, §3.5 door 1.2 + 0.85 swell (assumed 10 %)",
    "PKCELL": "PKCELL LP-803860 spec p.5: 62 x 38.5 x 8.5 max",
    "ME": "me_stacks.py / me_numbers.py (architecture R)",
    "SR5": "enclosure/STRUCTURAL_REVIEW.md #5 (1.1 mm glass, 8.6x stiffer than 1.5 acrylic)",
    "cut": "mechanical journal (journal_mechanical.txt:9): cut-out 17.05 vs taped 18.17",
}


def Ly(name, t, kind, src):
    return dict(name=name, t=t, kind=kind, src=src)


OPTIONS = {
    "T": dict(
        title="TAPED: panel taped flat onto the board", short="Taped", t=1.0,
        front=[Ly("conductive tape CN4190", 0.115, "tape", "MI"), Ly("display panel", 4.0, "display", "MI"),
               Ly("air gap (0.7 nom, 0.5 worst)", 0.7, "air", "DESIGN"), Ly("acrylic lens", 1.5, "lens", "params")],
        bay=[Ly("gap", 0.3, "air", "REVB"), Ly("cell 8.5 max", 8.5, "cell", "PKCELL"),
             Ly("swell space filled with foam", 0.85, "foam", "REVB"), Ly("door", 1.2, "door", "REVB")],
        cell=(65.6, 104.1), bay_x=(62.8, 120.7)),
    "F": dict(
        title="FLOATING: panel held by the front shell 2.3 mm above the board (parts under it)", short="Floating", t=1.6,
        front=[Ly("parts under the panel (<= 2.0) + 0.3", 2.3, "air", "ME"), Ly("display panel", 4.0, "display", "MI"),
               Ly("air gap", 0.7, "air", "DESIGN"), Ly("acrylic lens", 1.5, "lens", "params")],
        bay=[Ly("gap", 0.3, "air", "REVB"), Ly("cell 8.5 max", 8.5, "cell", "PKCELL"),
             Ly("swell space filled with foam", 0.85, "foam", "REVB"), Ly("door", 1.2, "door", "REVB")],
        cell=(76.0, 114.5), bay_x=(62.8, 123.8)),
    "C": dict(
        title="CUT-OUT: cell sits in a hole in the board, right behind the panel", short="Cut-out", t=1.0,
        front=[Ly("conductive tape", 0.115, "tape", "MI"), Ly("display panel", 4.0, "display", "MI"),
               Ly("air gap", 0.7, "air", "DESIGN"), Ly("acrylic lens", 1.5, "lens", "params")],
        # in the window: the cell's front face sits 0.30 behind the panel's back glass
        bay=None, cell=(65.6, 104.1), bay_x=(62.8, 106.0)),
    "R": dict(
        title="RECOMMENDED: panel held by the front shell, nothing under it; cell on a plate in a back-shell bay",
        short="Recommended", t=1.2,
        front=[Ly("clearance over a no-parts zone", 0.3, "air", "ME"),
               Ly("panel's own tape, liner left on", 0.115, "tape", "MI"), Ly("display panel", 4.0, "display", "MI"),
               Ly("black foam gasket = air gap", 0.3, "gasket", "ME"), Ly("glass cover lens", 1.1, "lens", "SR5")],
        bay=[Ly("gap", 0.2, "air", "ME"), Ly("battery plate 0.5 stainless", 0.5, "plate", "ME"),
             Ly("stretch-release tape", 0.15, "tape", "ME"), Ly("cell 8.5 max", 8.5, "cell", "PKCELL"),
             Ly("FREE swell gap (no foam)", 0.85, "swell", "REVB"), Ly("door", 1.2, "door", "REVB")],
        cell=(65.0, 103.5), bay_x=(62.8, 120.7)),
}
GRIP_BACK = [Ly("back parts (U12 1.7)", 1.7, "parts", "DESIGN"), Ly("clearance", 0.3, "air", "DESIGN"),
             Ly("back floor", 1.6, "back", "DESIGN")]
PLATE_T = 1.6          # front face plate (params.py:20)
CELL_T, SWELL = 8.5, 0.85


def z_front(o):
    O = OPTIONS[o]
    return round(O["t"] + sum(l["t"] for l in O["front"]), 3)


def z_back_bay(o):
    O = OPTIONS[o]
    if o == "C":   # cell front 0.3 behind the panel glass back (z t + 0.115), then foam 0.85 + door 1.2
        cell_front = O["t"] + 0.115 - 0.3
        return round(cell_front - CELL_T - SWELL - 1.2, 3)
    return round(-sum(l["t"] for l in O["bay"]), 3)


Z_BACK_GRIP = -3.6


def plate_sag(E=193e3, t=0.5, b=40.0, F=20.0):
    """0.5 mm 304 battery plate under the 20 N back press of STRUCTURAL_REVIEW #7, spread by the cell over its face.
    Roark 7th ed. Table 11.4 case 1a, y = alpha q b^4 / (E t^3) (as me_numbers.py): all 4 edges simply supported,
    a/b = 63/40 -> alpha 0.0890. The drawn plate rests on 3 edges only (left + top bay-wall ledges, the rib); with its
    bottom edge (y 103.6) free, the free edge bends at most like a simply supported 40 mm beam strip, 5*12/384."""
    q = F / (38.5 * 62.0)
    return {k: a * q * b ** 4 / (E * t ** 3) for k, a in (("4 edges supported", 0.0890),
                                                          ("bottom edge free (40 mm strip bound)", 5 * 12 / 384))}


def totals(o):
    return dict(grip=round(z_front(o) - Z_BACK_GRIP, 2), bump=round(z_front(o) - z_back_bay(o), 2),
                front=z_front(o), back=z_back_bay(o))


# ------------------------------------------------------------------------------------------------ XY geometry
SIL = Polygon(BJ["mech"]["silhouette"])                       # the device's outer outline
PANEL = (63.25, 45.90, 132.55, 96.10)                         # MI0283QT-11 module (REVB §3.2)
AA = (66.45, 49.40, 124.05, 92.60)                            # LCD active area 57.6 x 43.2
FOLD = (132.55, 48.05, 133.55, 93.95)                         # factory FPC fold bulge (<= 1.0)
POCKET = (62.95, 45.60, 133.85, 96.40)                        # front-shell panel pocket = panel + fold + 0.3
FPC_OPEN = (118.25, 47.55, 133.05, 94.45)                     # through-opening in the board (REVB §3.6)
LENS = (54.15, 43.0, 136.35, 99.0)                            # 82.2 x 56 glass lens (ME)
MASK_WIN = (66.05, 49.0, 124.45, 93.0)                        # mask window = AA + 0.4
CELL_R = (65.0, 40.0, 103.5, 102.0)                           # cell, portrait, PCM end + leads at the TOP (EE)
PLATE_R = (63.5, 38.2, 104.6, 103.6)                          # 0.5 stainless plate (ME)
RIB_R = (103.6, 38.2, 104.6, 103.6)                           # rib between cell and the service compartment
BAY = (62.8, 37.0, 120.7, 108.0)                              # bay = door opening, inner (REVB §3.5)
LEAD_PATH = (66.0, 37.4, 104.9, 39.9)                         # leads: PCM end (top) -> notch in the rib -> J8
J3_R = (106.23, 55.7, 111.98, 86.3)                           # AFC07 top-contact, back (ME: origin x ~107.80)
FPC_SWEEP = (111.98, 57.25, 118.25, 84.75)                    # tail + stiffener lying on the back (REVB §3.7)
ANT_KO = (25.2, 38.6, 34.9, 52.4)                             # BT830 antenna: no copper, any layer
U12 = (28.06, 46.41, 37.75, 60.09)
SPK = (151.45, 85.1, 27.0)
LRA = (28.5, 82.0, 8.0)

KEY_POSTS = {"SW2": (39.95, 56.09), "SW3": (39.95, 72.09), "SW4": (31.95, 64.09), "SW5": (47.95, 64.09),
             "SW8": (49.49, 83.82), "SW9": (49.44, 91.82), "SW6": (156.88, 61.92), "SW7": (143.94, 66.25)}
POST_D = 4.0
SHELL_SCREWS = {"S1": (57.39, 35.47), "S2": (133.19, 35.45), "S3": (29.24, 91.91), "S4": (140.5, 100.5),
                "S5": (165.5, 73.5)}        # S4 off-board; S5 = STRUCTURAL_REVIEW #3
BOARD_SCREWS = {"B1": (55.0, 101.5), "B2": (126.5, 103.0)}   # board -> front shell (ME, proposed)
DOOR_SCREWS = {"D1": (63.9, 105.0), "D2": (118.9, 104.6)}    # door (ME, proposed)

# EE variant T zones/blocks, adapted to R (cell 0.6 left, J3 at ME's x, test pads clear of the plate)
def _ee(side=None):
    T = EE["T"]
    return T


FRONT_ZONES = [  # (label, box, category)
    ("POWER IN\nUSB-C, charger,\nbuck-boost, LCD boost", (60.5, 32.6, 112.9, 45.4), "power"),
    ("CONTROLS LEFT\nD-pad, Start/Select, L,\nI/O expander, haptics + LRA", (22.6, 33.5, 62.9, 106.0), "hmi"),
    ("AUDIO + RIGHT KEYS\namp, speaker, A/B, Vol, R", (136.5, 34.5, 167.2, 104.0), "audio"),
    ("FLASH +\nMOTION", (113.2, 33.5, 129.0, 43.0), "core"),
]
FRONT_BLOCKS = [
    ("NOR", (120.4, 33.8, 128.6, 42.6), "core"), ("IMU", (113.4, 35.0, 119.4, 42.0), "core"),
    ("ESD", (88.0, 41.5, 92.5, 44.6), "power"),
]
BACK_BLOCKS = [  # (label, box, category, note)
    ("MCU\nSTM32F411", (136.0, 38.5, 158.0, 58.5), "core", "U1 + decoupling ring, crystal, I2C pull-ups"),
    ("FPGA\niCE40", (113.5, 33.2, 128.5, 47.0), "core", "U2 + ring; sits over the bay wall line (walls stop at z -2.0)"),
    ("LDO", (129.0, 39.0, 133.4, 46.8), "core", "+1V2"),
    ("J8", (104.9, 37.3, 112.9, 45.0), "battery", "cell connector, right behind charger U9"),
    ("BL", (104.9, 47.0, 112.2, 55.2), "display", "backlight driver"),
    ("J3", J3_R, "display", "50-pin FPC connector, top contact"),
    ("C", (104.9, 86.7, 110.6, 88.8), "display", "C52/C53"),
    ("SWD", (137.5, 59.0, 142.0, 63.4), "debug", "Tag-Connect TC2030-NL pads, bring-up only"),
    ("TEST PADS (door open)", (70.0, 104.2, 92.0, 106.6), "debug", "rail pads"),
    ("RF", U12, "rf", "BT830 U12 (unchanged)"),
]
CAT_COL = {"core": "#5b8def", "power": "#f08a5d", "hmi": "#9b8ce0", "audio": "#e98bb0", "display": "#f2c14e",
           "battery": "#5fb36b", "debug": "#e06666", "rf": "#3cb4a0"}


def board_revb():
    """Rev B outline (REVB_DESIGN.md §3.6): silhouette inset 2.5, minus rev A's speaker notch, minus the FPC opening."""
    inset = SIL.buffer(-2.5, join_style=2)
    a = Polygon(BJ["board"]["outline"]).buffer(0)
    notch = inset.difference(a).intersection(sbox(136.4, 60.0, 200.0, 120.0))
    return inset.difference(notch.buffer(0.01)).difference(sbox(*FPC_OPEN))


def span_at(poly, y=None, x=None):
    """list of (a, b) intervals where a horizontal (y=) or vertical (x=) line crosses poly."""
    ln = LineString([(0, y), (400, y)]) if y is not None else LineString([(x, 0), (x, 400)])
    g = poly.intersection(ln)
    segs = getattr(g, "geoms", [g])
    k = 0 if y is not None else 1
    out = sorted((min(s.coords[0][k], s.coords[-1][k]), max(s.coords[0][k], s.coords[-1][k])) for s in segs
                 if not s.is_empty)
    return out


def part(ref):
    return next(p for p in BJ["parts"] if p["ref"] == ref)


def bbox(ref):
    c = part(ref)["courtyard"]
    xs = [p[0] for p in c]; ys = [p[1] for p in c]
    return min(xs), min(ys), max(xs), max(ys)


# ------------------------------------------------------------------------------------------------ checks
def check(back_blocks=None, front_blocks=None, parts=True, quiet=False):
    """Collision check of the consolidated R floor plan. Prints every conflict; returns the count.
    back_blocks / front_blocks default to BACK_BLOCKS / FRONT_BLOCKS; selftest() passes deliberately bad ones."""
    back_blocks = BACK_BLOCKS if back_blocks is None else back_blocks
    front_blocks = FRONT_BLOCKS if front_blocks is None else front_blocks
    say = (lambda s: None) if quiet else print
    board = board_revb()
    bad = 0
    keep_b = {"cell zone": sbox(65.1, 39.5, 104.6, 102.5), "battery plate (0.2 off the board)": sbox(*PLATE_R),
              "plate rib": sbox(*RIB_R),
              "FPC sweep": sbox(*FPC_SWEEP), "FPC opening": sbox(*FPC_OPEN).buffer(0.5),
              "antenna KO": sbox(*ANT_KO)}
    for n, (x, y) in KEY_POSTS.items():
        keep_b["post " + n] = Point(x, y).buffer(2.5)
    for n, (x, y) in {**SHELL_SCREWS, **BOARD_SCREWS}.items():
        keep_b["screw " + n] = Point(x, y).buffer(3.0)
    for n, (x, y) in DOOR_SCREWS.items():
        keep_b["door screw " + n] = Point(x, y).buffer(2.0)
    for lab, b, cat, _ in back_blocks:
        g = sbox(*b)
        if cat == "rf":
            continue
        if not board.buffer(0.01).contains(g):
            say(f"  BACK {lab!r:24} leaves the board outline"); bad += 1
        for kn, kg in keep_b.items():
            if g.intersection(kg).area > 0.01 and not (lab == "J3" and kn == "FPC sweep"):
                say(f"  BACK {lab!r:24} hits {kn} ({g.intersection(kg).area:.1f} mm2)"); bad += 1
    keep_f = {"panel pocket (no parts)": sbox(*POCKET), "antenna KO": sbox(*ANT_KO),
              "D-pad pivot": Point(39.95, 64.09).buffer(5.3)}
    for n, (x, y) in {**SHELL_SCREWS, **BOARD_SCREWS}.items():
        keep_f["screw " + n] = Point(x, y).buffer(3.0)
    for lab, b, cat in front_blocks:
        g = sbox(*b)
        for kn, kg in keep_f.items():
            if g.intersection(kg).area > 0.01:
                say(f"  FRONT {lab!r:23} hits {kn}"); bad += 1
    # rev A front parts that stay: anything inside the panel pocket must have moved
    if parts:
        for p in BJ["parts"]:
            if p["side"] != "top" or p["height"] < 0.1:
                continue
            g = Polygon(p["courtyard"]).buffer(0)
            if g.intersection(sbox(*POCKET)).area > 0.01 and p["ref"] not in MOVED_FRONT:
                say(f"  rev A front part {p['ref']} still under the panel"); bad += 1
    return bad


def selftest():
    """Feed check() one deliberately bad block at a time; every one must be flagged. Returns (caught, tried)."""
    bad_back = [("on the cell", (80.0, 60.0, 85.0, 65.0), "core", ""),
                ("on the plate rib", (103.8, 70.0, 104.4, 72.0), "core", ""),
                ("on post SW3", (39.0, 71.0, 41.0, 73.0), "core", ""),
                ("on screw S1", (56.5, 35.0, 58.0, 36.5), "core", ""),
                ("on door screw D2", (118.0, 104.0, 119.5, 105.5), "core", ""),
                ("in the antenna keep-out", (26.0, 40.0, 28.0, 42.0), "core", ""),
                ("in the FPC opening", (120.0, 60.0, 122.0, 62.0), "core", ""),
                ("in the FPC sweep", (113.0, 60.0, 115.0, 62.0), "core", ""),
                ("off the board", (10.0, 60.0, 14.0, 64.0), "core", "")]
    bad_front = [("under the panel", (90.0, 70.0, 92.0, 72.0), "core"),
                 ("on the D-pad pivot", (39.0, 63.0, 41.0, 65.0), "core"),
                 ("on board screw B2", (126.0, 102.5, 127.0, 103.5), "core")]
    caught = sum(check([b], [], parts=False, quiet=True) > 0 for b in bad_back)
    caught += sum(check([], [f], parts=False, quiet=True) > 0 for f in bad_front)
    return caught, len(bad_back) + len(bad_front)


def floating_like_R():
    """Option F's 2.3 mm under-panel parts gap, but with R's 1.2 board, 1.1 glass + 0.3 gasket and R's battery bay:
    isolates what 'parts under the panel' costs by itself (F as studied also has a 1.6 board and 1.5 acrylic + 0.7)."""
    front = OPTIONS["R"]["t"] + 2.3 + 4.0 + 0.3 + 1.1
    return dict(grip=round(front - Z_BACK_GRIP, 2), bump=round(front - z_back_bay("R"), 2))


# rev A front parts that the EE plan moves (REVB_DESIGN.md §3.7 'Front, under the panel (22)' + J1/J3/J4 deleted)
MOVED_FRONT = set("C11 C24 C31 C32 C51 D2 J4 J8 R3 R5 R6 R7 R8 R12 R13 R14 R31 U1 U2 U4 U7 Y1 J1 J3".split())
# ... plus the front parts that collide with nothing but move with their block (REVB_DESIGN.md §6.1): C12 (Y1's load
# cap) goes to the MCU block, R29 (WR series resistor) stays at U2, C25/C26 go to the backlight block (decision 11).
FUNC_MOVERS = {"C12", "C25", "C26", "R29"}
# what is left on the front = REVB_DESIGN.md §6.2's 'unchanged placement' list (58 front parts of its 66)
STAY_FRONT = {p["ref"] for p in BJ["parts"] if p["side"] == "top"} - MOVED_FRONT - FUNC_MOVERS

if __name__ == "__main__":
    for o in OPTIONS:
        t = totals(o)
        print(f"{o}  {OPTIONS[o]['short']:12s} board {OPTIONS[o]['t']}  front z {t['front']:+.3f}  "
              f"bay back z {t['back']:+.3f}  grip {t['grip']:.2f}  bump {t['bump']:.2f}")
    # the datasheet measures from the antenna, which sits on the module over the keep-out; the module's own edge
    # (pcbnew bbox x <= 37.78, REVB_DESIGN.md §3.8; used by REVB §3.2 and me_numbers.py) is the conservative reference
    for name, rf in (("antenna keep-out edge (x 34.9)", ANT_KO[2]), ("U12 module edge (x 37.78)", 37.78)):
        print("RF distances from the %s: panel %.1f, plate %.1f, cell %.1f, FPGA %.1f, MCU %.1f"
              % (name, PANEL[0] - rf, PLATE_R[0] - rf, CELL_R[0] - rf, 113.5 - rf, 136.0 - rf))
    print("front parts that stay (REVB §6.2 list, front side):", len(STAY_FRONT))
    print("battery plate sag under 20 N, 0.2 gap to the board: " +
          ", ".join(f"{k} {v:.2f} mm" for k, v in plate_sag().items()))
    fl = floating_like_R()
    print("floating with R's board, lens/gasket and bay: grip %.2f, bump %.2f (R: %.2f / %.2f)"
          % (fl["grip"], fl["bump"], totals("R")["grip"], totals("R")["bump"]))
    print("board area %.0f mm2" % board_revb().area)
    n = check()
    print("collision check:", "0 conflicts" if n == 0 else f"{n} conflicts")
    c, k = selftest()
    print(f"self-test: {c} of {k} deliberately bad blocks caught")
