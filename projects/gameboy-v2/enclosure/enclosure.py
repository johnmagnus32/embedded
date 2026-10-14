#!/usr/bin/env python3
"""enclosure.py — the gb2 enclosure, assembled (build123d). Everything comes from build/board.json (export_board.py
writes it from the KiCad board) through params.py, so the enclosure follows the board.

  front shell = back_openings.apply_front_openings(front_features.apply(bare front))
  back shell  = back_openings.apply_back_openings(back_openings.apply_back(bare back))
  controls    = controls.all_controls()  (A, B, START, SELECT, DPAD, DPAD_WASHER, VOL_UP, VOL_DOWN, L, R, POWER)
  J1 cover    = front_features.j1_cover()

Run (from this folder):  .venv/bin/python enclosure.py  ->  out/<part>.stl + .3mf (each printable part in its print
pose), out/assembly.step (every part at rest + board + module + battery + speaker + LRA), out/lens.dxf / .svg (the
laser-cut lens + mask), and the live OCP CAD Viewer if it is listening on 3939. checks.py runs the fit checks.

Frame: board mm, +X right, +Y up (KiCad's Y flipped), Z up; the board's bottom face is z = 0, its top z = 1.6.

Assembly (rev 2.1; checks.py (h) sweeps this order. DESIGN.md 2 still has the old one):
  1. Lens taped into its pocket. Front shell face-down on a spacer frame >= 2.5 tall round the keys (or foam with
     reliefs): the D-pad / A / B / Start / Select heads stand 1.8 proud and a flat bench would push their tips below
     the board top.
  2. D-pad, A, B, Start, Select in head-first from inside; edge caps and the power slider into their rim notches -
     hold the caps outward until the board is in (their plunger tips arrive 0.15 off), and set the slider to the same
     end as SW1's lever (both east = OFF): the fork has 0.25 play and no lead-in. Speaker into its ring, lead exit
     north (into the back cradle's gap at 90 deg); light-pipe rod (17.7 long) in from the face.
  3. Board: pivot washer taped on the GND pour round the D-pad centre (its rim drops between SW2-5); cell plugged
     into J8 BEFORE the 2090 goes on (J8 ends up under the 2090 PCB); 2090 on its standoffs. Cell taped into the back
     shell's fence. Board component-side down onto the boss tips between the six fingers; the leads run from the
     fence's lead gap over the board edge along the free J4 strip (x 128.25..132.1); speaker leads dressed south,
     under B's tube.
  4. Back shell folded over on the 100 mm cell leads (about 65 mm slack), 4 screws from the back.
Print notes: the front shell's lens land (z_print 1.63, a 2.5-3.9 cantilever over the pocket) needs support - sand
it flat, the pocket depth is exactly lens + tape. The keys' and D-pad's flanges overhang their heads by 1.2 / 2.3 in
the head-down pose: support under the flange seat (0.2 interface gap). Test-print the board fit first (FDM XY shrink
0.2-0.5 % is 0.3-0.7 over the 140 mm finger span; the fingers leave 0.1).
"""
import functools, os, socket, time
from build123d import Axis, Color, Compound, Mesher, Pos, Rot, export_step, export_stl
import params, shell, front_features, front_screen, back_openings, controls

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out")
VIEWER_PORT = 3939               # OCP CAD Viewer
MESH_TOL, MESH_ANG = 0.01, 0.1   # mm / rad: STL + 3MF tessellation (well under the 0.4 nozzle)
SHELL_ALPHA = 0.35               # viewer: shells semi-transparent


# ---------------------------------------------------------------- build
@functools.lru_cache(maxsize=None)
def build():
    """{name: solid} of every printed part at rest in the assembly frame: 'front', 'back', the eleven controls (incl.
    the D-pad's pivot washer), 'j1_cover'."""
    f0, b0 = shell.shells()
    parts = {"front": back_openings.apply_front_openings(front_features.apply(f0)),
             "back": back_openings.apply_back_openings(back_openings.apply_back(b0))}
    parts.update(controls.all_controls())
    parts["j1_cover"] = front_features.j1_cover()
    for n, p in parts.items():
        p.label = n
    return parts


@functools.lru_cache(maxsize=None)
def context():
    """{name: shape} of the bought / board-side bodies the enclosure fits round."""
    top, bot = params.part_envelopes()
    return {"board": params.board_solid(), "parts_top": top, "parts_bottom": bot, "module_2090": params.module_2090(),
            "battery": params.battery(), "speaker": params.speaker(), "lra": params.lra()}


# ---------------------------------------------------------------- print poses
def _on_bed(p):
    """centred on the origin in XY, lowest point on z 0."""
    bb = p.bounding_box()
    return Pos(-(bb.min.X + bb.max.X) / 2, -(bb.min.Y + bb.max.Y) / 2, -bb.min.Z) * p


def print_pose(name, part):
    """front shell + J1 cover face-down (flipped about X), back shell back-down (as built), controls per
    controls.print_pose."""
    if name in ("front", "j1_cover"):
        p = _on_bed(Rot(180, 0, 0) * part)
    elif name == "back":
        p = _on_bed(part)
    else:
        p = controls.print_pose(part)
    p.label = name
    return p


# ---------------------------------------------------------------- exports
def export_all(parts, ctx):
    os.makedirs(OUT, exist_ok=True)
    for n, p in parts.items():
        pp = print_pose(n, p)
        export_stl(pp, os.path.join(OUT, f"{n}.stl"), tolerance=MESH_TOL, angular_tolerance=MESH_ANG)
        m = Mesher()
        m.add_shape(pp, linear_deflection=MESH_TOL, angular_deflection=MESH_ANG, part_number=n)
        m.write(os.path.join(OUT, f"{n}.3mf"))
    asm = []
    for n, p in parts.items():
        p.color = _colour(n)
        asm.append(p)
    for n, c in ctx.items():
        c.label, c.color = n, _colour(n)
        asm.append(c)
    export_step(Compound(label="gb2_assembly", children=asm), os.path.join(OUT, "assembly.step"))   # named, coloured
    lens = front_screen.export_lens(os.path.join(OUT, "lens"))
    return lens


COLOURS = {"front": "ivory", "back": "lightgray", "j1_cover": "dimgray", "board": "darkgreen", "parts_top": "gray",
           "parts_bottom": "gray", "module_2090": "royalblue", "battery": "orange", "speaker": "gold", "lra": "purple",
           "DPAD": "black", "DPAD_WASHER": "white", "A": "firebrick", "B": "firebrick", "START": "slategray", "SELECT": "slategray",
           "POWER": "steelblue"}


def _colour(n):
    return Color(COLOURS.get(n, "darkslategray"))          # edge caps: darkslategray


def push_viewer(parts, ctx):
    with socket.socket() as sk:
        sk.settimeout(0.3)
        if sk.connect_ex(("127.0.0.1", VIEWER_PORT)) != 0:
            print(f"viewer not running (port {VIEWER_PORT}): open the OCP CAD Viewer in VS Code, then rerun")
            return
    from ocp_vscode import show
    objs = {**parts, **ctx}
    names = list(objs)
    show(*objs.values(), names=names, colors=[_colour(n) for n in names],
         alphas=[SHELL_ALPHA if n in ("front", "back") else 1.0 for n in names])
    print("sent to the OCP CAD Viewer")


if __name__ == "__main__":
    t0 = time.time()
    parts, ctx = build(), context()
    print(f"built {len(parts)} parts in {time.time() - t0:.1f} s")
    for n, p in parts.items():
        bb = p.bounding_box()
        print(f"  {n:9s} {len(p.solids())} solid(s), valid {p.is_valid}, {p.volume:8.1f} mm3, "
              f"{bb.size.X:6.1f} x {bb.size.Y:6.1f} x {bb.size.Z:5.1f}")
    lens = export_all(parts, ctx)
    print(f"exported out/<part>.stl + .3mf ({len(parts)} parts, print pose), out/assembly.step, "
          f"{', '.join(os.path.relpath(l, HERE) for l in lens)} in {time.time() - t0:.1f} s")
    push_viewer(parts, ctx)
