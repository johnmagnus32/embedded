"""front_screen.py — the front shell's screen opening + lens pocket + land frame, the LED light-pipe hole + tube, the
speaker grille + locating ring (DESIGN.md section 3). API (front_features.py): adds() / cuts() / keepouts(), lists of
solids in the assembly frame; plus lens_outline() and export_lens() for the laser-cut lens and its mask.

Run it for the self-test: .venv/bin/python front_screen.py"""
import math
from build123d import (Pos, Rot, RectangleRounded, SlotCenterToCenter, Compound, ExportDXF, ExportSVG, Unit)
from params import (Z_FRONT, Z_PLATE_IN, OPENING, PANEL, FPC_BULGE, LENS, LENS_POCKET, LAND_Z0, FRAME_W, MASK_WINDOW,
                    LED, SPEAKER_AT, GRILLE_D, SPK_RING, KEYS, MECH, CLR, prism, cyl, face)

EPS = 0.5                    # cutters overshoot the faces they go through
FUSE = 0.2                   # adds reach this far into the plate so they fuse with it
OPENING_Z0 = 14.0            # the opening column (keepout) starts here: the module's front face (14.17) and up

# ---------------------------------------------------------------- helpers
def rrect(cx, cy, w, h, r):
    """rounded-rect face at z 0."""
    return Pos(cx, cy) * RectangleRounded(w, h, r)

def rr_of(d):
    """rounded rect from a {x0, x1, y0, y1, r} dict."""
    return rrect((d["x0"] + d["x1"]) / 2, (d["y0"] + d["y1"]) / 2, d["x1"] - d["x0"], d["y1"] - d["y0"], d["r"])

def pocket_face(grow=0.0):
    """lens pocket outline (lens + clr per side, corner r + clr), grown by 'grow' more."""
    c = LENS_POCKET["clr"] + grow
    return rrect(LENS["cx"], LENS["cy"], LENS["w"] + 2 * c, LENS["h"] + 2 * c, LENS["r"] + c)

# ---------------------------------------------------------------- 1. screen
POCKET_FLOOR = Z_FRONT - LENS_POCKET["depth"]                     # 19.27

# OPENING's R1.0 corners pass the panel's sharp corners at only 0.15, so the cut also takes the panel + FPC fold
# + CLR (corner r CLR): that only adds four small corner reliefs, everywhere else OPENING is the larger.
RELIEF = {"x0": PANEL["x0"] - CLR, "x1": PANEL["x1"] + FPC_BULGE + CLR, "y0": PANEL["y0"] - CLR,
          "y1": PANEL["y1"] + CLR, "r": CLR}

def opening_prism(z0, z1):
    """the screen opening (OPENING + the corner reliefs) as a column z0..z1."""
    return prism(rr_of(OPENING), z0, z1) + prism(rr_of(RELIEF), z0, z1)

def land_frame():
    """frame hanging from the plate to LAND_Z0: (pocket outline + FRAME_W) minus the opening; its top is the land."""
    return (prism(pocket_face(FRAME_W), LAND_Z0, Z_PLATE_IN + FUSE) - opening_prism(LAND_Z0 - 1, Z_FRONT + 1)
            - pocket_cut())                                       # (stays out of its own pocket keepout)

def opening_cut():
    return opening_prism(OPENING_Z0, Z_FRONT + EPS)

def pocket_cut():
    return prism(pocket_face(), POCKET_FLOOR, Z_FRONT + EPS)

def lens_outline():
    """(lens face, mask window face), flat at z 0, assembly XY (mm)."""
    return (rrect(LENS["cx"], LENS["cy"], LENS["w"], LENS["h"], LENS["r"]),
            rrect(MASK_WINDOW["cx"], MASK_WINDOW["cy"], MASK_WINDOW["w"], MASK_WINDOW["h"], MASK_WINDOW["r"]))

def export_lens(path_prefix):
    """<prefix>.dxf + <prefix>.svg: lens outline (layer LENS) + mask window (layer MASK), mm, viewed from the front
    (the mask goes on the lens back: mirror it if the mask is cut as a separate film laid face-up)."""
    lens, win = lens_outline()
    for exp, ext in ((ExportDXF(unit=Unit.MM), "dxf"), (ExportSVG(unit=Unit.MM), "svg")):
        exp.add_layer("LENS"); exp.add_layer("MASK")
        exp.add_shape(lens.edges(), layer="LENS")
        exp.add_shape(win.edges(), layer="MASK")
        exp.write(f"{path_prefix}.{ext}")
    return f"{path_prefix}.dxf", f"{path_prefix}.svg"

# ---------------------------------------------------------------- 2. LED light pipe
LED_WALL = 1.0
LED_ROD_D = LED["d"]                                              # Ø2.5 acrylic rod
LED_STOP_Z = LED["stop_z"]   # (2.4, 3.2): the stop ring 0.8 tall (DESIGN.md 6 min wall), 0.24 over D1 (top 2.16);
                             # the rod is 20.9 - 3.2 = 17.7 long
LX, LY = LED["at"]
LED_OD = LED["tube_id"] + 2 * LED_WALL                            # 4.7

def led_tube():
    """tube (ID tube_id) from the plate down to the stop, closed by the stop ring (ID stop_id) over stop_z."""
    z0, z1 = LED_STOP_Z
    return (cyl(LX, LY, LED_OD, z0, Z_PLATE_IN + FUSE) - cyl(LX, LY, LED["tube_id"], z1, Z_FRONT + 1)
            - cyl(LX, LY, LED["stop_id"], z0 - 1, z1 + 1))

def led_cut():
    return cyl(LX, LY, LED["hole_d"], Z_PLATE_IN - FUSE - 0.1, Z_FRONT + EPS)   # through the plate only

# ---------------------------------------------------------------- 3. speaker grille + ring
SX, SY = SPEAKER_AT
SLOT_W = 1.2                 # choice: >= 1.0 (GBA ~1.0), 3 FDM lines' worth of bar either side
SLOT_PITCH = 2.95            # GBA pitch -> bars 1.75 (>= 1.2)
SLOT_N = 7                   # GBA has 5; the 7-slot variant (exterior notes) gives ~23 % open area
SLOT_TILT = 13.71            # deg, right end higher (GBA)
SLOT_LEN = 16.7              # GBA slot length outside; the outer two are clipped to the circle
GRILLE_R = GRILLE_D / 2

def slots():
    """[(centre, centre-to-centre length)] of each slot: GBA length, clipped so its stadium stays in Ø GRILLE_D."""
    a = math.radians(SLOT_TILT); n = (-math.sin(a), math.cos(a))  # across the slots
    out = []
    for i in range(SLOT_N):
        d = (i - (SLOT_N - 1) / 2) * SLOT_PITCH
        half = math.sqrt((GRILLE_R - SLOT_W / 2) ** 2 - d * d)    # centreline half-length to the circle
        out.append(((SX + n[0] * d, SY + n[1] * d), min(2 * half, SLOT_LEN - SLOT_W)))
    return out

def grille_cut():
    return Compound([prism(Pos(c[0], c[1]) * Rot(0, 0, SLOT_TILT) * SlotCenterToCenter(L, SLOT_W),
                           Z_PLATE_IN - FUSE - 0.1, Z_FRONT + EPS) for c, L in slots()])

SPK_R = MECH["speaker"]["d"] / 2
RING_IR = SPK_RING["id"] / 2
RING_OR = RING_IR + SPK_RING["wall"]
RING_Z0 = Z_PLATE_IN - SPK_RING["depth"]                          # 17.3
B = KEYS["B"]
B_NOTCH_D = B["guide_id"] + 2 * CLR                               # B's bore + 0.3 per side
B_TUBE_CLR = 0.5             # rev 2.1: the ring notch clears B's whole guide tube (bore + GUIDE_WALL) by this. Notched
                             # only round the bore, the ring's radial end met B's speaker relief at a shallow angle
                             # and left a 0.025 sliver of wall (checks (i))

def b_bore():
    """B's guide bore column (contract geometry: guide_id, plate down to GUIDE_BOTTOM_Z)."""
    from params import GUIDE_BOTTOM_Z
    return cyl(B["at"][0], B["at"][1], B["guide_id"], GUIDE_BOTTOM_Z, Z_FRONT)

def ring_notch():
    """radial-ended sector of the ring where its outer edge comes within B_TUBE_CLR of B's guide tube (a plain round
    notch would leave slivers of wall: take the full wall, square-ended, instead)."""
    from params import GUIDE_WALL
    bx, by = B["at"]; dB = math.hypot(bx - SX, by - SY); rn = B["guide_id"] / 2 + GUIDE_WALL + B_TUBE_CLR
    half = math.acos((dB ** 2 + RING_OR ** 2 - rn ** 2) / (2 * dB * RING_OR))    # outer edge meets the notch circle
    a0 = math.atan2(by - SY, bx - SX); R = 2 * RING_OR
    pts = [(SX, SY)] + [(SX + R * math.cos(a0 + half * k / 4), SY + R * math.sin(a0 + half * k / 4)) for k in range(-4, 5)]
    return prism(face(pts, board_frame=False), RING_Z0 - 1, Z_FRONT + 1), math.degrees(2 * half)

def spk_ring():
    ring = cyl(SX, SY, 2 * RING_OR, RING_Z0, Z_PLATE_IN + FUSE) - cyl(SX, SY, 2 * RING_IR, RING_Z0 - 1, Z_FRONT + 1)
    return ring - ring_notch()[0] - cyl(B["at"][0], B["at"][1], B_NOTCH_D, RING_Z0 - 1, Z_FRONT + 1)

# ---------------------------------------------------------------- API
def adds():
    return [land_frame(), led_tube(), spk_ring()]

def cuts():
    return [opening_cut(), pocket_cut(), led_cut(), grille_cut()]

def keepouts():
    s = MECH["speaker"]
    spk = cyl(SX, SY, s["d"] + 2 * CLR, Z_PLATE_IN - 0.3 - s["t"] - CLR, Z_PLATE_IN) - spk_ring()   # (ring: 0.2, intended)
    return [opening_prism(OPENING_Z0, Z_FRONT),
            prism(pocket_face(), POCKET_FLOOR, Z_FRONT),
            cyl(LX, LY, LED_ROD_D, LED_STOP_Z[1], Z_FRONT),
            spk]

# ---------------------------------------------------------------- self-test
if __name__ == "__main__":
    import params as p, shell
    front, _ = shell.shells()
    f = front
    for s in adds(): f = f + s
    for s in cuts(): f = f - s
    print(f"front shell + front_screen: {len(f.solids())} solid(s), valid {f.is_valid}, {f.volume:.0f} mm3")
    top, bot = p.part_envelopes()
    mine = Compound(adds())
    ctx = {"board": p.board_solid(), "parts top": top, "parts bottom": bot, "module_2090": p.module_2090(),
           "battery": p.battery(), "speaker": p.speaker(), "lra": p.lra()}
    for n, c in ctx.items():
        print(f"  adds vs {n:12s} overlap {(mine & c).volume:.4f} mm3, gap {mine.distance_to(c):.3f}")
    print(f"  land frame to module_2090 {land_frame().distance_to(p.module_2090()):.4f} (>= 0.3)")
    print(f"  pocket floor z {pocket_cut().bounding_box().min.Z:.3f}, LED tube to D1/parts {led_tube().distance_to(top):.3f}")
    print(f"  ring to speaker {spk_ring().distance_to(p.speaker()):.3f}, to B bore {spk_ring().distance_to(b_bore()):.3f},"
          f" notch {ring_notch()[1]:.1f} deg, {len(spk_ring().solids())} solid")
    for i, k in enumerate(keepouts()):
        print(f"  keepout {i} vs front after {(k & f).volume:.4f}")
    print("  lens files:", export_lens("/tmp/gb2_lens"))
