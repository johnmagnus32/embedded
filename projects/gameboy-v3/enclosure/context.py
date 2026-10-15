"""context.py - the bought / board-side bodies the enclosure fits round, and the core keep-outs (CONTRACT.md).

context()  -> {name: (shape, colour)}   every body below, in the assembly frame (cached; colour = a CSS name / hex)
keepouts() -> {name: solid}             core keep-outs: no module may put material in them (checks.py tests it):
                                        fold bay, cell swell, usb_c plug, jack plug, microsd card, reset pin,
                                        cell leads
groups()   -> {name: group}             exploded-view group of each context body (render / viewer only)

Bodies (names are the dict keys):
  board                       board solid from board.outline_yup, H1-H5 drilled (D2.2)
  F:<ref> / B:<ref>           193 part envelopes: model hull prisms over z_design (F up from the board top, B down
                              from its bottom face), coloured by side. Left out: H1-H5 + TP1-TP12 (no body), and the
                              parts drawn below from better data (USBC1, Audio1, U4, FPC1, LRA1, SPK1)
  usb_c, jack, microsd        from board.json edge_features + the datasheets (NOT the KiCad 3D models: USBC1 and
                              Audio1 sit 1.5-2.2 off there); microsd card (latched) = the card's 1.30 past the socket
  FPC1 as placed              red: its slot faces the board's top edge, the touch tail cannot reach it
  FPC1 corrected              green: rotated 180 deg about (118.4, 73.6) KiCad, lid edge y ~77.08 (pre-fab fix)
  panel LCM / panel touch / panel glass / panel active area     ATM0500D27-CT: LCM 3.0, DST + ITO sensor 0.6, cover
                              glass 0.7 (the black-print border), the 108 x 64.8 active area tinted
  display flex, touch flex    0.3 strips swept along params.FLEX (the panel report's fold model), widths by s; the
                              touch flex includes its FT7311 + steel area under the board (<= 1.45, chip up)
  cell                        params.CELL (on its floor, or hanging from the SUS plate)
  lcd plate                   only with GB3_LCD_PLATE=1: the 0.5 SUS304 plate between LCM and cell
  lra, speaker, speaker foam  LRA on the floor; speaker back-firing on its foam ring on the floor

`.venv/bin/python context.py` builds them and prints a summary."""
import functools, math
from build123d import Axis, Box, Compound, Cylinder, Pos, Rot
import params as P

COLOURS = {
    "board": "#1f6f3f", "F": "#8fa8c8", "B": "#c8a07a",
    "usb_c": "#c0c0c0", "jack": "#3a3a3a", "microsd": "#a8a8a8", "microsd card (latched)": "#2b4d8f",
    "FPC1 as placed": "#d62728", "FPC1 corrected": "#2ca02c",
    "panel LCM": "#4a4a4a", "panel touch": "#9cc3e6", "panel glass": "#151515", "panel active area": "#2f5fa8",
    "display flex": "#d39a2c", "touch flex": "#e0b84a",
    "cell": "#f08a24", "lcd plate": "#b0b4b8", "lra": "#7b3fa0", "speaker": "#d4af37", "speaker foam": "#555555",
}
SPECIAL_REFS = {"USBC1", "Audio1", "U4", "FPC1", "LRA1", "SPK1"}      # drawn from better data than the hull prisms


def colour(name):
    if name[:2] in ("F:", "B:"):
        return COLOURS[name[0]]
    return COLOURS.get(name, "#888888")


# ---------------------------------------------------------------- board + parts
def board_solid():
    b = P.prism(P.face(P.BJ["board"]["outline_yup"], board_frame=False), P.Z_BOARD_BOT, P.BOARD_TOP)
    return b - [P.cyl(*h["at"], h["d"], -0.1, P.BOARD_TOP + 0.1) for h in P.HOLES.values()]


def part_envelopes():
    """{'F:<ref>' / 'B:<ref>': hull prism over z_design}."""
    out = {}
    for p in P.BJ["parts"]:
        z0, z1 = p["z_design"]
        pts = p.get("hull") or p.get("courtyard")
        if p["ref"] in SPECIAL_REFS or z1 - z0 < 1e-6 or not pts:
            continue
        out[f"{'F' if p['side'] == 'top' else 'B'}:{p['ref']}"] = P.prism(P.face(pts), z0, z1)
    return out


def _stadium_y(cx, cz, w, h, y0, y1):
    """stadium section (w along X, h along Z, round ends) swept along Y over y0..y1 (enclosure)."""
    L, ym, r = abs(y1 - y0), (y0 + y1) / 2, h / 2
    s = Pos(cx, ym, cz) * Box(w - h, L, h)
    for sx in (-1, 1):
        s = s + Pos(cx + sx * (w / 2 - r), ym, cz) * Rot(90, 0, 0) * Cylinder(r, L)
    return s


def _cyl_y(x, z, d, y0, y1):
    return Pos(x, (y0 + y1) / 2, z) * Rot(90, 0, 0) * Cylinder(d / 2, abs(y1 - y0))


def usb_c():
    """TYPE-C-31-M-12 shell 8.94 x 3.36 (design), mouth at edge_features.mouth_y, 7.35 long (C165948)."""
    u = P.USBC
    return _stadium_y(u["x"], (u["z"][0] + u["z"][1]) / 2, u["shell_w"], u["shell_h"], u["mouth_y"],
                      u["mouth_y"] - u["shell_l"])


def jack():
    """PJ-327C: body 6.0 W x 11.6 L x 5.15 (design) on the board top, D5.0 x 2.5 barrel on axis z 4.1 to the tip,
    + its solder-leg footprint (hull, 0.5 high)."""
    j = P.JACK
    y_base = j["tip_y"] + j["barrel_l"]                   # barrel base = body front (enclosure y, inward is +y)
    w, L = j["body"]
    body = P.box(j["x"], y_base + L / 2, w, L, P.BOARD_TOP, j["z_top"])
    legs = P.prism(P.face(P.PARTS["Audio1"]["hull"]), P.BOARD_TOP, P.BOARD_TOP + 0.5) & \
        P.box(j["x"], y_base + L / 2, 20, L, P.BOARD_TOP - 1, P.BOARD_TOP + 1)
    return body + legs + _cyl_y(j["x"], j["axis_z"], j["barrel_d"], j["tip_y"], y_base)


def microsd():
    return P.prism(P.face(P.PARTS["U4"]["hull"]), *P.MICROSD["z"])


def microsd_card():
    m = P.MICROSD
    return P.box(m["x"], m["face_y"] + m["out_latched"] / 2, m["card_w"], m["out_latched"], *m["card_z"])


def fpc1(which):
    return P.prism(P.face(P.FPC1[which], board_frame=False), *P.FPC1["z"])


# ---------------------------------------------------------------- panel
def panel_bodies():
    pn = P.PANEL
    glass = pn["glass"].prism(*pn["z_glass"]) - pn["active"].prism(pn["z_glass"][0] - 0.1, pn["z_glass"][1] + 0.1)
    return {"panel LCM": pn["lcm"].prism(*pn["z_lcm"]), "panel touch": pn["sensor"].prism(*pn["z_touch"]),
            "panel glass": glass, "panel active area": pn["active"].prism(*pn["z_glass"])}


# ---------------------------------------------------------------- flexes
def strip(path, s0, s1, xk, t):
    """a t-thick strip along path (KiCad y, z) over s0..s1, spanning KiCad x xk = (x0, x1)."""
    left, right = [], []
    for s in path.samples(s0, s1):
        y, z, ty, tz = path.at(s)
        ny, nz = -tz, ty                                  # normal: tangent turned +90 deg in the (y, z) plane
        left.append((-(y + ny * t / 2), z + nz * t / 2))  # enclosure y = -KiCad y
        right.append((-(y - ny * t / 2), z - nz * t / 2))
    return P.yz_plate(left + right[::-1], *xk)


def _fuse(pieces):
    s = pieces[0].fuse(*pieces[1:]) if len(pieces) > 1 else pieces[0]
    return s.solids()[0] if len(s.solids()) == 1 else s


def flexes():
    d, t = P.FLEX["display"], P.FLEX["touch"]
    disp = _fuse([strip(d["path"], s0, min(s1, d["path"].s), xk, d["t"]) for s0, s1, xk in d["widths"]])
    y0, y1 = t["ic_y"]                                    # the IC area replaces the strip over s ic_s
    ic = P.box((t["ic_x"][0] + t["ic_x"][1]) / 2, -(y0 + y1) / 2, t["ic_x"][1] - t["ic_x"][0], y1 - y0,
               P.Z_BOARD_BOT - t["ic_t"], P.Z_BOARD_BOT)
    touch = _fuse([strip(t["path"], s0, min(s1, t["path"].s), xk, t["t"]) for s0, s1, xk in t["widths"]] + [ic])
    return {"display flex": disp, "touch flex": touch}


# ---------------------------------------------------------------- cell + transducers
def cell():
    return P.CELL["rect"].prism(*P.CELL["z"])


def lcd_plate():
    return P.CELL["buffer"].prism(*P.SS_Z)


def lra():
    return P.cyl(*P.LRA["at"], P.LRA["d"], *P.LRA["z"])


def speaker():
    s = P.SPEAKER
    return P.cyl(*s["at"], s["lip_d"], *s["lip_z"]) + P.cyl(*s["at"], s["d"], s["lip_z"][1], s["z"][1])


def speaker_foam():
    s = P.SPEAKER
    return P.cyl(*s["at"], s["lip_d"], *s["foam_z"]) - P.cyl(*s["at"], s["grille_d"] + 0.2, s["foam_z"][0] - 0.1,
                                                              s["foam_z"][1] + 0.1)


# ---------------------------------------------------------------- the dict
@functools.lru_cache(maxsize=None)
def _bodies():
    out = {"board": board_solid()}
    out.update(part_envelopes())
    out.update({"usb_c": usb_c(), "jack": jack(), "microsd": microsd(), "microsd card (latched)": microsd_card(),
                "FPC1 as placed": fpc1("placed"), "FPC1 corrected": fpc1("corrected")})
    out.update(panel_bodies())
    out.update(flexes())
    out["cell"] = cell()
    if P.LCD_PLATE:
        out["lcd plate"] = lcd_plate()
    out.update({"lra": lra(), "speaker": speaker(), "speaker foam": speaker_foam()})
    for n, s in out.items():
        s.label = n
    return out


def context():
    """{name: (shape, colour)} of every context body."""
    return {n: (s, colour(n)) for n, s in _bodies().items()}


def groups():
    """exploded-view group per context body: 'board' (board + parts + flexes), 'panel', 'back-mounted'."""
    g = {}
    for n in _bodies():
        g[n] = "panel" if n.startswith("panel") or n == "lcd plate" else \
            "back-mounted" if n in ("cell", "lra", "speaker", "speaker foam") else "board"
    return g


# ---------------------------------------------------------------- core keep-outs
@functools.lru_cache(maxsize=None)
def _keepouts():
    u, j, m, r = P.USBC, P.JACK, P.MICROSD, P.RESET
    far = P.OUTLINE.y1 + 10.0                                       # 10 mm beyond the top wall (enclosure y)
    rw, rh, _ = u["relief"]
    ko = {
        # FOLD_BAY: no ribs or bosses; the plate over it keeps its inner face at or above Z_FOLD_SKIN_IN
        "fold bay": P.FOLD_BAY["rect"].prism(*P.FOLD_BAY["z"]),
        # the cell's swell allowance (+ the air to the LCM back when it swells toward the panel)
        "cell swell": P.CELL["rect"].prism(P.CELL["swell_z"][0], P.CELL["swell_z"][1] +
                                           (0.0 if P.LCD_PLATE else P.CELL_AIR)),
        # USB-C plug: the overmold (relief w x h) may come to face_max off the receptacle face, the tongue to the face
        "usb_c plug": P.box(u["x"], (u["mouth_y"] + u["face_max"] + far) / 2, rw, far - u["mouth_y"] - u["face_max"],
                            u["axis_z"] - rh / 2, u["axis_z"] + rh / 2) +
        P.box(u["x"], (u["mouth_y"] + far) / 2, u["opening"][0], far - u["mouth_y"], u["axis_z"] - u["opening"][1] / 2,
              u["axis_z"] + u["opening"][1] / 2),
        # 3.5 mm plug: D3.6 into the barrel, its shoulder (D6.0) seats on the barrel tip
        "jack plug": _cyl_y(j["x"], j["axis_z"], j["plug_d"], j["tip_y"] + j["barrel_l"], j["tip_y"] - 10) +
        _cyl_y(j["x"], j["axis_z"], j["shoulder_d"], j["tip_y"], j["tip_y"] - 10),
        # microSD card path: latched .. ejected and the finger beyond (card + 0.25 / 0.15 per side)
        "microsd card": P.box(m["x"], (m["face_y"] + far) / 2, m["card_w"] + 0.5, far - m["face_y"],
                              m["card_z"][0] - 0.15, m["card_z"][1] + 0.15),
        # reset: the pin from below the back face up to the B-side keystake (SW1 presses +z)
        "reset pin": P.cyl(*r["at"], r["pin_d"], P.Z_BACK_MIN - 3.0, r["keystake_z"]),
        # the cell leads: plug in CN1, over the board edge, down the notch corner outside the panel, along the
        # cell's top end gap from the PCM exit (cell centre +/- LEAD_EXIT_TOL) - a lead gap in any fence there
        "cell leads": _cell_leads(),
    }
    for n, s in ko.items():
        s.label = n
    return ko


def _cell_leads():
    """plug in CN1; over the board from its rear to the notch corner; the riser in the corner of the cell's end gap
    outside the panel; along the end gap from the PCM exit; minus the board (the leads wrap round its edge)."""
    cn, cl, buf, pn = P.CN1, P.CELL, P.CELL["buffer"], P.PANEL["lcm"]
    z0, z1 = cn["plug_z"]
    end_gap_y = (cl["rect"].y1, buf.y1)                                  # between the cell's top end and the notch edge
    plug = cn["plug"].prism(z0, z1)
    over = P.Rect(pn.x1, end_gap_y[0], cn["plug"].x1, cn["plug"].y0 + 0.01).prism(P.BOARD_TOP, z1)
    zf = max(cl["z"][0], P.floor_in_at(cl["rect"].cx, (end_gap_y[0] + end_gap_y[1]) / 2))   # on the printed floor
    riser = P.Rect(pn.x1, end_gap_y[0], buf.x1, end_gap_y[1]).prism(zf, z1)
    gap = P.Rect(cl["rect"].cx - P.LEAD_EXIT_TOL, end_gap_y[0], buf.x1, end_gap_y[1]).prism(zf, cl["z"][1])
    k = plug.fuse(over, riser, gap).clean() - board_solid()
    return k.solids()[0] if len(k.solids()) == 1 else k


def keepouts():
    """{name: solid}: the core keep-outs (fold bay, cell swell, plug / card / pin paths)."""
    return dict(_keepouts())


if __name__ == "__main__":
    import time
    t0 = time.time()
    ctx = context()
    print(f"{len(ctx)} context bodies in {time.time() - t0:.1f} s")
    for n, (s, c) in ctx.items():
        if n[:2] in ("F:", "B:"):
            continue
        bb = s.bounding_box()
        print(f"  {n:24s} {len(s.solids())} solid(s) valid {s.is_valid}  x {bb.min.X:7.2f}..{bb.max.X:7.2f}  "
              f"KiCad y {-bb.max.Y:7.2f}..{-bb.min.Y:7.2f}  z {bb.min.Z:+6.2f}..{bb.max.Z:+6.2f}")
    nf = sum(1 for n in ctx if n.startswith("F:"))
    nb = sum(1 for n in ctx if n.startswith("B:"))
    print(f"  part envelopes: {nf} F + {nb} B = {nf + nb} (+ usb_c, jack, microsd, FPC1, lra, speaker = "
          f"{nf + nb + 6} parts)")
    for n, s in keepouts().items():
        bb = s.bounding_box()
        print(f"  keepout {n:14s} z {bb.min.Z:+6.2f}..{bb.max.Z:+6.2f}")
    print(f"touch U-turn at KiCad y {P.FLEX['touch']['uturn_y']:.2f}, display loop out to {P.FLEX['display']['loop_out_y']:.2f}"
          f", touch tail end y {P.FLEX['touch']['end_y']:.2f} (FPC1 corrected lid {P.FPC1['lid_y_k']:.2f})")
    print(f"{time.time() - t0:.1f} s")
