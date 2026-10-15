#!/usr/bin/env python3
"""enclosure.py - the gb3 enclosure, assembled (build123d 0.13). Long form of the contract: CONTRACT.md.

MODULE CONTRACT
  Each module file front.py, back.py, controls.py (imported if present, in that order) exposes

      build(ctx) -> {"front_add": [Shape], "front_cut": [Shape],   # solids fused into / cut from the front shell
                     "back_add":  [Shape], "back_cut":  [Shape],   # ... the back shell
                     "parts":    {name: Shape},   # extra printed or bought parts at rest in the assembly frame
                     "keepouts": {name: Shape},   # volumes the OTHER modules keep their material out of
                     "notes":    [str]}           # findings, open issues, contract problems

  Missing keys default to empty; an unknown key is an error (typo guard). Every module may add to / cut from either
  shell. All modules' adds go on first, then all cuts (front = bare + every front_add - every front_cut; back the
  same), so no module's material can fill another's opening; a cut removes material from every module, so cuts must
  stay out of other modules' keepouts too (checks.py). Part names are unique across modules and may not be "front",
  "back" or a context name; keepouts are registered as "<module>:<name>" next to the core ones ("core:<name>").
  Optional module-level hooks:  print_pose(name, solid) -> solid on the bed (z 0 down), or None = bought, not printed
                                COLOURS = {part name: colour}
                                export(out_dir) -> [paths]: extra files written by export_all() (back.py: the
                                battery plate DXF); build() itself writes no files
  Module files import params / context / shell / build123d only (never enclosure: it imports them).

  ctx (a Ctx; the same object for every module):
      ctx.params         the params module (stack, flags, frame helpers, named positions, contracts)
      ctx.flags          params.FLAGS {"FRONT", "BACK", "CELL", "LCD_PLATE", "PROCESS"}
      ctx.front          bare front shell (Solid, z SPLIT..Z_FACE)       shell.shells()[0]
      ctx.back           bare back shell  (Solid, z Z_BACK_MIN..SPLIT)   shell.shells()[1]
      ctx.context        {name: (shape, colour)} context bodies          context.context()
      ctx.core_keepouts  {name: Shape} core keep-outs                    context.keepouts()
      filled by build() after the modules ran (checks.py reads them):
      ctx.modules        {module: its validated result dict}
      ctx.keepouts       {"core:<n>" / "<module>:<n>": Shape} every keep-out
      ctx.part_owner     {part name: module}  ("front" / "back" -> "shell")
      ctx.timings        {step: seconds}

  checks.py exposes run(parts, ctx) -> [(check, "PASS" | "FAIL", detail)]; `python checks.py` builds, prints them and
  exits 1 on any FAIL. parts = build()[0] = {"front", "back", + every module part}: the finished shells and parts.

RUN (from this folder; GB3_* flags as in params.py):
  .venv/bin/python enclosure.py [--no-export] [--no-render] [--check]
     -> out/assembly.step (every part + context body, named and coloured), out/<part>.stl + .3mf (printed parts in
        their print poses), out/battery_plate.dxf, build/render_*.png + build/section_*.png (headless), and the OCP
        viewer only when OCP_PORT is set and that port is listening (never the default 3939 by accident).
        A variant (any GB3_* flag off its default) writes to out/variants/<slug>/ and build/variants/<slug>/
        instead (params.VARIANT_SLUG), so the baseline's files are never overwritten.
     --check runs checks.run(fast=True) on the same build at the end (exit 1 on any FAIL); the full checks
        (with the assembly sweep and the slab scan) are `.venv/bin/python checks.py`.
"""
import functools, importlib, os, socket, sys, time
from dataclasses import dataclass, field
from types import ModuleType
from build123d import Color, Compound, Mesher, Pos, Rot, Shape, export_step, export_stl
import params as P
import context as C
import shell

HERE = os.path.dirname(os.path.abspath(__file__))
_SUB = ("variants", P.VARIANT_SLUG) if P.VARIANT_SLUG else ()
OUT = os.path.join(HERE, "out", *_SUB)            # exports: out/ (baseline) or out/variants/<slug>/
BUILD = os.path.join(HERE, "build", *_SUB)        # renders: build/ (baseline) or build/variants/<slug>/
MODULES = ("front", "back", "controls")
RESULT_KEYS = ("front_add", "front_cut", "back_add", "back_cut", "parts", "keepouts", "notes")
MESH_TOL, MESH_ANG = 0.02, 0.15          # mm / rad: STL + 3MF tessellation
SHELL_ALPHA = 0.35
SHELL_COLOURS = {"front": "#ece6cf", "back": "#b9bec4"}
MODULE_COLOURS = {"front": "#d8d2bb", "back": "#9aa3ab", "controls": "#3b3f46"}
EXPLODE = {"back": -44.0, "back-mounted": -22.0, "board": 0.0, "panel": 18.0, "front": 40.0, "controls": 56.0}
EXPLODE_VIEW = dict(az=-62, el=30, width=2000, height=1500)   # el >= ~30: render.py's depth-jump outline test
                                                             # darkens flat faces seen at lower elevations


@dataclass
class Ctx:
    params: ModuleType
    flags: dict
    front: Shape
    back: Shape
    context: dict
    core_keepouts: dict
    modules: dict = field(default_factory=dict)
    keepouts: dict = field(default_factory=dict)
    part_owner: dict = field(default_factory=dict)
    timings: dict = field(default_factory=dict)
    extra_files: list = field(default_factory=list)      # files the modules' export() hooks wrote (export_all)


def make_ctx():
    """a fresh Ctx (bare shells + context; the modules not run). A module's self-test can do
    `import enclosure; r = build(enclosure.make_ctx())`."""
    t0 = time.time()
    front, back = shell.shells()
    t1 = time.time()
    ctx = Ctx(params=P, flags=dict(P.FLAGS), front=front, back=back, context=C.context(),
              core_keepouts=C.keepouts())
    ctx.timings.update({"shells": t1 - t0, "context": time.time() - t1})
    return ctx


def _validate(mod, r):
    if not isinstance(r, dict):
        raise TypeError(f"{mod}.build() returned {type(r).__name__}, not a dict")
    bad = set(r) - set(RESULT_KEYS)
    if bad:
        raise KeyError(f"{mod}.build(): unknown key(s) {sorted(bad)}; the contract keys are {RESULT_KEYS}")
    out = {k: r.get(k) or ([] if k in ("front_add", "front_cut", "back_add", "back_cut", "notes") else {})
           for k in RESULT_KEYS}
    for k in ("front_add", "front_cut", "back_add", "back_cut"):
        out[k] = list(out[k])
        for i, s in enumerate(out[k]):
            if not isinstance(s, Shape):
                raise TypeError(f"{mod}.build()['{k}'][{i}] is {type(s).__name__}, not a build123d Shape")
    for k in ("parts", "keepouts"):
        if not isinstance(out[k], dict):
            raise TypeError(f"{mod}.build()['{k}'] must be a dict name -> Shape")
        for n, s in out[k].items():
            if not isinstance(s, Shape):
                raise TypeError(f"{mod}.build()['{k}']['{n}'] is {type(s).__name__}, not a build123d Shape")
    out["notes"] = [str(n) for n in out["notes"]]
    return out


def _single(s, label):
    """the single solid of a boolean result when there is one (else the shape as is), labelled."""
    sol = s.solids()
    if len(sol) == 1:
        s = sol[0]
    s.label = label
    return s


_MODULE_OBJS = {}                        # contract name -> the module object build() ran (for the optional hooks)


def load_modules():
    """{name: module} of the module files present (front, back, controls)."""
    out = {}
    for m in MODULES:
        if os.path.exists(os.path.join(HERE, f"{m}.py")):
            out[m] = importlib.import_module(m)
    return out


@functools.lru_cache(maxsize=None)
def build():
    """(parts, ctx): parts = {"front", "back", + every module part} at rest in the assembly frame."""
    ctx = make_ctx()
    mods = load_modules()
    _MODULE_OBJS.clear()
    _MODULE_OBJS.update(mods)
    for name, m in mods.items():
        t0 = time.time()
        ctx.modules[name] = _validate(name, m.build(ctx))
        ctx.timings[name] = time.time() - t0
    t0 = time.time()
    shells = {"front": ctx.front, "back": ctx.back}
    for sh in ("front", "back"):
        adds = [s for r in ctx.modules.values() for s in r[f"{sh}_add"]]
        cuts = [s for r in ctx.modules.values() for s in r[f"{sh}_cut"]]
        s = shells[sh]
        if adds:
            s = s.fuse(*adds).clean()
        if cuts:
            s = s.cut(*cuts).clean()
        shells[sh] = _single(s, sh)
    ctx.timings["apply"] = time.time() - t0
    parts = dict(shells)
    ctx.part_owner = {"front": "shell", "back": "shell"}
    ctx.keepouts = {f"core:{n}": s for n, s in ctx.core_keepouts.items()}
    for mod, r in ctx.modules.items():
        for n, s in r["parts"].items():
            if n in parts or n in ctx.context:
                raise KeyError(f"{mod}: part name {n!r} is already taken ({ctx.part_owner.get(n, 'context')})")
            s.label = n
            parts[n] = s
            ctx.part_owner[n] = mod
        for n, s in r["keepouts"].items():
            ctx.keepouts[f"{mod}:{n}"] = s
    return parts, ctx


# ---------------------------------------------------------------- colours, groups, poses
def part_colour(name, ctx):
    if name in SHELL_COLOURS:
        return SHELL_COLOURS[name]
    mod = ctx.part_owner.get(name)
    return getattr(_MODULE_OBJS.get(mod), "COLOURS", {}).get(name, MODULE_COLOURS.get(mod, "#777777"))


def part_group(name, ctx):
    if name in ("front", "back"):
        return name
    return {"front": "front", "back": "back", "controls": "controls"}.get(ctx.part_owner.get(name), "board")


def _on_bed(p):
    bb = p.bounding_box()
    return Pos(-(bb.min.X + bb.max.X) / 2, -(bb.min.Y + bb.max.Y) / 2, -bb.min.Z) * p


def print_pose(name, part, ctx):
    """the part as printed (on the bed at z 0, centred), or None for a bought part."""
    if name == "front":
        return _on_bed(Rot(180, 0, 0) * part)                       # face-down
    if name == "back":
        return _on_bed(part)                                        # back-down, as built
    m = _MODULE_OBJS.get(ctx.part_owner.get(name, ""))
    if m is not None and hasattr(m, "print_pose"):
        p = m.print_pose(name, part)
        return None if p is None else _on_bed(p)
    return _on_bed(part)


# ---------------------------------------------------------------- exports
def export_all(parts, ctx):
    os.makedirs(OUT, exist_ok=True)
    t0 = time.time()
    printed = []
    for n, p in parts.items():
        pp = print_pose(n, p, ctx)
        if pp is None:
            continue
        fn = n.replace(" ", "_").replace("/", "_")
        export_stl(pp, os.path.join(OUT, f"{fn}.stl"), tolerance=MESH_TOL, angular_tolerance=MESH_ANG)
        m = Mesher()
        m.add_shape(pp, linear_deflection=MESH_TOL, angular_deflection=MESH_ANG, part_number=n)
        m.write(os.path.join(OUT, f"{fn}.3mf"))
        printed.append(n)
    kids = []
    for n, p in parts.items():
        q = p.moved(Pos(0, 0, 0))
        q.label, q.color = n, Color(part_colour(n, ctx))
        kids.append(q)
    sides = {"F": [], "B": []}
    for n, (s, col) in ctx.context.items():
        q = s.moved(Pos(0, 0, 0))
        q.label, q.color = n, Color(col)
        (sides[n[0]] if n[:2] in ("F:", "B:") else kids).append(q)
    for sd, lst in sides.items():
        g = Compound(label=f"{sd} parts", children=lst)
        g.color = Color(C.COLOURS[sd])
        kids.append(g)
    export_step(Compound(label="gb3_assembly", children=kids), os.path.join(OUT, "assembly.step"))
    ctx.extra_files = []
    for mod in ctx.modules:                                          # optional module hook: export(out_dir)
        hook = getattr(_MODULE_OBJS.get(mod), "export", None)
        if callable(hook):
            ctx.extra_files += list(hook(OUT) or [])
    ctx.timings["export"] = time.time() - t0
    return printed


# ---------------------------------------------------------------- renders
def _items(parts, ctx, explode=False, shells_alpha=SHELL_ALPHA, skip=()):
    groups = C.groups()
    out = []
    for n, p in parts.items():
        if n in skip:
            continue
        off = (0.0, 0.0, EXPLODE[part_group(n, ctx)]) if explode else None
        out.append((n, p, part_colour(n, ctx), shells_alpha if n in ("front", "back") else 1.0, off))
    for n, (s, col) in ctx.context.items():
        if n in skip:
            continue
        out.append((n, s, col, 1.0, (0.0, 0.0, EXPLODE[groups[n]]) if explode else None))
    return out


def _legend(ctx):
    return [("front shell", SHELL_COLOURS["front"]), ("back shell", SHELL_COLOURS["back"]), ("board", C.COLOURS["board"]),
            ("F parts", C.COLOURS["F"]), ("B parts", C.COLOURS["B"]), ("LCM", C.COLOURS["panel LCM"]),
            ("cover glass / active area", C.COLOURS["panel active area"]), ("flexes", C.COLOURS["display flex"]),
            ("cell", C.COLOURS["cell"]), ("LRA", C.COLOURS["lra"]), ("speaker", C.COLOURS["speaker"]),
            ("FPC1 as placed", C.COLOURS["FPC1 as placed"]), ("FPC1 corrected", C.COLOURS["FPC1 corrected"])]


def _zlines():
    z = [(P.Z_FACE, "face"), (P.Z_GLASS_MAX, "glass max"), (P.Z_LCM_BACK, "LCM back"), (P.LEDGE_Z0, "ledge"),
         (P.SPLIT, "split = board top"), (P.Z_BOARD_BOT, "board bottom"), (P.CELL_Z[0], "cell"),
         (P.Z_FLOOR_IN, "floor inner"), (P.Z_BACK, "back")]
    if P.HUMP:
        z.append((P.Z_BACK_HUMP, "hump"))
    return z


def render(parts, ctx):
    """headless PNGs into build/: exploded iso, top, bottom, sections at KiCad y 75 and x 160."""
    import render as R
    os.makedirs(BUILD, exist_ok=True)
    t0 = time.time()
    tag = P.VARIANT
    lg = _legend(ctx)
    done = []
    done.append(R.view(_items(parts, ctx, explode=True), os.path.join(BUILD, "render_iso_exploded.png"),
                       title=f"gb3 enclosure, exploded ({tag}); shells at alpha {SHELL_ALPHA}", legend=lg,
                       **EXPLODE_VIEW))
    done.append(R.view(_items(parts, ctx), os.path.join(BUILD, "render_top.png"), az=-90, el=90, width=2200,
                       height=1080, title=f"gb3 enclosure from the front ({tag}); front shell at alpha {SHELL_ALPHA}",
                       legend=lg))
    done.append(R.view(_items(parts, ctx), os.path.join(BUILD, "render_bottom.png"), az=90, el=-90, width=2200,
                       height=1080, title=f"from the back ({tag}); back shell at alpha {SHELL_ALPHA} (mirrored: "
                       "KiCad +x is to the left)", legend=lg))
    sec = _items(parts, ctx)
    done.append(R.section(sec, os.path.join(BUILD, "section_y75.png"), "y", -75.0, zlines=_zlines(),
                          zooms=[(19.0, 62.0), (120.0, 184.0), (178.0, 216.0)],
                          title=f"section at KiCad y 75 (x across, +z up toward the screen) - {tag}; total "
                                f"{P.TOTAL:.2f} mm"))
    done.append(R.section(sec, os.path.join(BUILD, "section_x160.png"), "x", 160.0, zlines=_zlines(),
                          zooms=[(29.0, 62.0), (62.0, 96.0), (96.0, 121.0)],
                          title=f"section at KiCad x 160 (KiCad y across: top edge left, bottom edge right) - {tag}"))
    ctx.timings["render"] = time.time() - t0
    return done


# ---------------------------------------------------------------- viewer
def push(parts, ctx):
    """show everything in the OCP CAD Viewer - only when OCP_PORT is set and listening."""
    port = os.environ.get("OCP_PORT")
    if not port:
        print("viewer: OCP_PORT not set, not pushed (3939 belongs to another window; set OCP_PORT to this one's)")
        return False
    with socket.socket() as sk:
        sk.settimeout(0.3)
        if sk.connect_ex(("127.0.0.1", int(port))) != 0:
            print(f"viewer: nothing listening on {port}, not pushed")
            return False
    from ocp_vscode import set_port, show
    set_port(int(port))
    objs, names, cols, alphas = [], [], [], []
    for n, p in parts.items():
        objs.append(p), names.append(n), cols.append(part_colour(n, ctx))
        alphas.append(SHELL_ALPHA if n in ("front", "back") else 1.0)
    sides = {"F": [], "B": []}
    for n, (s, col) in ctx.context.items():
        if n[:2] in ("F:", "B:"):
            q = s.moved(Pos(0, 0, 0))
            q.label = n
            sides[n[0]].append(q)
            continue
        objs.append(s), names.append(n), cols.append(col), alphas.append(1.0)
    for sd, lst in sides.items():
        objs.append(Compound(label=f"{sd} parts", children=lst)), names.append(f"{sd} parts")
        cols.append(C.COLOURS[sd]), alphas.append(1.0)
    show(*objs, names=names, colors=cols, alphas=alphas, port=int(port))
    print(f"viewer: pushed {len(objs)} objects to port {port}")
    return True


# ---------------------------------------------------------------- main
def summary(parts, ctx):
    print(f"\n{len(parts)} part(s), {len(ctx.context)} context bodies, {len(ctx.keepouts)} keepouts; modules: "
          + (", ".join(ctx.modules) or "none"))
    for n, p in parts.items():
        bb = p.bounding_box()
        print(f"  {n:16s} [{ctx.part_owner.get(n)}] {len(p.solids())} solid(s), valid {p.is_valid}, "
              f"{p.volume:9.1f} mm3, {bb.size.X:6.1f} x {bb.size.Y:5.1f} x {bb.size.Z:5.2f} (z {bb.min.Z:+.2f}.."
              f"{bb.max.Z:+.2f})")
    for mod, r in ctx.modules.items():
        n = {k: len(r[k]) for k in RESULT_KEYS if k != "notes"}
        print(f"  {mod}: " + ", ".join(f"{k} {v}" for k, v in n.items()))
        for note in r["notes"]:
            print(f"    note: {note}")


if __name__ == "__main__":
    t0 = time.time()
    parts, ctx = build()
    summary(parts, ctx)
    if "--no-export" not in sys.argv:
        printed = export_all(parts, ctx)
        od = os.path.relpath(OUT, HERE)
        print(f"exported {od}/assembly.step + {od}/<part>.stl/.3mf for {', '.join(printed)}"
              + "".join(f"; {os.path.relpath(f, HERE)}" for f in getattr(ctx, "extra_files", [])))
    if "--no-render" not in sys.argv:
        for pth in render(parts, ctx):
            print(f"rendered {os.path.relpath(pth, HERE)}")
    push(parts, ctx)
    fails = 0
    if "--check" in sys.argv:                                        # the fast checks on this same build
        import checks
        t1 = time.time()
        fails = checks.report(checks.run(parts, ctx, fast=True, verbose=False), ctx, t1)
        ctx.timings["checks --fast"] = time.time() - t1
    print(f"total thickness {P.TOTAL:.2f} mm [{P.VARIANT}]")
    print("timings: " + ", ".join(f"{k} {v:.1f}s" for k, v in ctx.timings.items() if not k.startswith("check "))
          + f"; total {time.time() - t0:.1f}s")
    sys.exit(1 if fails else 0)
