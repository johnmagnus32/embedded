"""Diagrams for the gb2 rev B mechanical architecture study (read-only on every KiCad / enclosure file).

Run:  ../../enclosure/.venv/bin/python me_draw.py      (writes the PNGs next to this file)
  me_zstacks.png   - to-scale z stacks, 3 regions x 4 options, every layer named with thickness + source tag
  me_sections.png  - to-scale x-z sections through the screen centre (y 71) for T, F and R, plus an R zoom at the FPC end
  me_plan_R.png    - plan view of the recommended architecture (front view, KiCad mm, +Y down)
  me_assembly_R.png- exploded side view of R with the assembly order
"""
import json, os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Polygon as MPoly, Circle, FancyArrowPatch
from shapely.geometry import Polygon, LineString, box as sbox, Point
from me_stacks import stack, OPTS, REGIONS, SOURCES, C, front_layers, board_t, back_bay_layers

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
BJ = json.load(open(os.path.join(ROOT, "enclosure", "build", "board.json")))
INK, INK2, SURF = "#0b0b0b", "#52514e", "#fcfcfb"
plt.rcParams.update({"font.size": 8, "text.color": INK, "axes.labelcolor": INK2, "xtick.color": INK2,
                     "ytick.color": INK2, "axes.edgecolor": "#bdbdbd", "figure.facecolor": SURF,
                     "axes.facecolor": SURF, "savefig.facecolor": SURF})


def spread(ys, gap):
    """Push label y positions apart to at least `gap`, keeping order (simple relaxation)."""
    ys = list(ys)
    for _ in range(200):
        moved = False
        for i in range(1, len(ys)):
            if ys[i] - ys[i - 1] < gap:
                d = (gap - (ys[i] - ys[i - 1])) / 2
                ys[i - 1] -= d; ys[i] += d; moved = True
        if not moved:
            break
    return ys


# ------------------------------------------------------------------------------------------------ 1. z stacks
def draw_zstacks():
    opts = ["T", "F", "R", "R2"]
    fig, axs = plt.subplots(3, 4, figsize=(22, 25), sharey=True)
    for i, reg in enumerate(REGIONS):
        for j, o in enumerate(opts):
            ax = axs[i][j]
            ls, zb, zf = stack(o, reg)
            z = zb
            mids = []
            for l in ls:
                fc = C[l["kind"]]
                ax.add_patch(Rectangle((0, z), 1.6, l["t"], fc=fc, ec="#555555", lw=0.4,
                                       hatch="////" if l["kind"] in ("gap", "swell") else None))
                mids.append((z + l["t"] / 2, l))
                z += l["t"]
            ys = spread([m for m, _ in mids], 0.62)
            for (m, l), y in zip(mids, ys):
                ax.plot([1.62, 1.95, 2.15], [m, y, y], color="#9e9e9e", lw=0.5)
                ax.text(2.2, y, f"{l['name']}  {l['t']:.3g}  [{l['src']}]", va="center", fontsize=7.6, color=INK)
            # board datum and totals
            ax.axhline(0, color="#2e7d32", lw=0.6, ls=":")
            ax.annotate("", xy=(-0.35, zf), xytext=(-0.35, zb), arrowprops=dict(arrowstyle="<->", lw=0.8, color=INK))
            ax.text(-0.45, (zb + zf) / 2, f"{zf - zb:.2f} mm", rotation=90, ha="right", va="center", fontsize=11,
                    weight="bold")
            ax.text(0.8, zf + 0.35, f"front face z {zf:+.2f}", ha="center", fontsize=7.5, color=INK2)
            ax.text(0.8, zb - 0.55, f"back face z {zb:+.2f}", ha="center", fontsize=7.5, color=INK2)
            ax.set_xlim(-1.6, 9.5); ax.set_ylim(-13, 12)
            ax.set_xticks([]); ax.grid(axis="y", color="#eeeeee", lw=0.5)
            ax.set_aspect("equal")
            if j == 0:
                ax.set_ylabel(f"{REGIONS[reg]}\n\nz (mm), 0 = board bottom", fontsize=10)
            if i == 0:
                ax.set_title(f"{o}: {OPTS[o]}\nboard t = {board_t(o)}", fontsize=11, weight="bold", loc="left")
    src = "  ".join(f"[{k}] {v}" for k, v in SOURCES.items())
    fig.text(0.01, 0.006, "Sources: " + src, fontsize=7, wrap=True, color=INK2)
    fig.suptitle("gb2 rev B: to-scale z stacks (same mm scale in every panel). Hatched = air. "
                 "R2 needs the face plate thinned to ~1.0 over the edge switches / USB-C (plate inner face only "
                 "board top + 3.0).", fontsize=12, x=0.01, ha="left", y=0.995)
    fig.tight_layout(rect=(0, 0.03, 1, 0.985))
    fig.savefig(os.path.join(HERE, "me_zstacks.png"), dpi=110)
    plt.close(fig)


# ------------------------------------------------------------------------------------------------ geometry
SIL = Polygon(BJ["mech"]["silhouette"])
PANEL = (63.25, 45.90, 132.55, 96.10)
AA = (66.45, 49.40, 124.05, 92.60)
OPEN = (62.95, 45.60, 133.85, 96.40)
FPC_OPEN = (118.25, 47.55, 133.05, 94.45)
LENS = (54.15, 43.0, 136.35, 99.0)
BAY = (62.8, 37.0, 120.7, 108.0)
CELL_T = (65.6, 40.0, 104.1, 102.0)      # studies' cell position
CELL_R = (65.0, 40.0, 103.5, 102.0)      # R: 0.6 left to make room for the plate rib
PLATE_R = (63.5, 38.2, 104.6, 103.6)
RIB_R = (103.6, 38.2, 104.6, 103.6)
J3 = {"T": (105.73, 55.7, 111.48, 86.3), "R": (106.23, 55.7, 111.98, 86.3)}
J8 = (106.0, 95.5, 114.0, 103.1)
SWD = (104.9, 90.0, 118.0, 94.8)
FPGA = (110.0, 33.2, 125.0, 47.0)
MCU = (136.0, 38.5, 158.0, 58.5)
LEAD_POCKET = (65.1, 102.5, 114.0, 107.5)
POSTS = {"SW2": (39.95, 56.09), "SW3": (39.95, 72.09), "SW4": (31.95, 64.09), "SW5": (47.95, 64.09),
         "SW8": (49.49, 83.82), "SW9": (49.44, 91.82), "A": (156.88, 61.92), "B": (143.94, 66.25)}
SHELL_SCREWS = {"S1": (57.39, 35.47), "S2": (133.19, 35.45), "S3": (29.24, 91.91), "S4": (140.5, 100.5),
                "S5": (165.5, 73.5)}
BOARD_SCREWS = {"B1": (55.0, 101.5), "B2": (126.5, 103.0)}
DOOR_SCREWS = {"D1": (63.9, 105.0), "D2": (118.9, 104.6)}
SPK = (151.45, 85.1, 27.0)


def part(ref):
    return next(p for p in BJ["parts"] if p["ref"] == ref)


def bbox(ref):
    xs = [c[0] for c in part(ref)["courtyard"]]; ys = [c[1] for c in part(ref)["courtyard"]]
    return min(xs), min(ys), max(xs), max(ys)


def board_revb():
    """Rev B outline (REVB_DESIGN.md §3.6): the silhouette inset 2.5 (draw_mech recipe), minus rev A's speaker notch
    (the part of the inset that rev A's board leaves out right of x 134), minus the FPC opening."""
    inset = SIL.buffer(-2.5, join_style=2)
    a = Polygon(BJ["board"]["outline"]).buffer(0)
    missing = inset.difference(a)
    notch = missing.intersection(sbox(136.4, 60.0, 200.0, 120.0))     # rev A's speaker notch (edge x 136.45)
    return inset.difference(notch.buffer(0.01)).difference(sbox(*FPC_OPEN))


def xrange_at(poly, y):
    seg = poly.intersection(LineString([(0, y), (300, y)]))
    xs = [c[0] for g in getattr(seg, "geoms", [seg]) for c in g.coords]
    return min(xs), max(xs)


def R(ax, x0, x1, z0, z1, fc, ec="#555555", lw=0.4, **kw):
    ax.add_patch(Rectangle((x0, min(z0, z1)), x1 - x0, abs(z1 - z0), fc=fc, ec=ec, lw=lw, **kw))


# ------------------------------------------------------------------------------------------------ 2. sections
def section(ax, o, zoom=False):
    """x-z section at y = 71 (screen centre) for option o, true scale."""
    t = board_t(o)
    fl, fh = front_layers(o)
    zf = t + fh
    bay = back_bay_layers(o)
    zb_bay = -sum(l["t"] for l in bay)
    zb_grip = -3.6
    xl, xr = xrange_at(SIL, 71.0)
    oR = "R" if o in ("R", "R2") else o
    lens_t = fl[-1]["t"]
    # --- front shell: plate (minus opening), lens pocket, walls
    R(ax, xl, xl + 2.0, zb_grip + 1.6, zf, C["plate"])                       # left wall (front + back shells)
    R(ax, xr - 2.0, xr, zb_grip + 1.6, zf, C["plate"])
    for a, b in ((xl, LENS[0]), (LENS[2], xr)):
        R(ax, a, b, zf - 1.6, zf, C["plate"])
    for a, b in ((LENS[0], OPEN[0]), (OPEN[2], LENS[2])):                    # land under the lens
        land0 = (zf - lens_t - 0.13 - 1.0) if oR == "R" else zf - 1.6 - 1.2
        R(ax, a, b, land0, zf - lens_t - 0.13, C["plate"])
    R(ax, LENS[0], LENS[2], zf - lens_t, zf, C["glass"] if oR == "R" else C["lens"])
    # --- board
    bx0 = xrange_at(board_revb(), 71.0)[0]
    for a, b in ((bx0, FPC_OPEN[0]), (FPC_OPEN[2], 136.45)):
        R(ax, a, b, 0, t, C["board"])
    # --- panel and its interfaces
    z = t
    for l in fl[:-1]:
        if l["kind"] == "panel":
            R(ax, PANEL[0], PANEL[2], z, z + l["t"], C["panel"])
            zp0, zp1 = z, z + l["t"]
        elif l["kind"] == "tape":
            R(ax, PANEL[0], PANEL[0] + 1.5, z, z + l["t"], C["tape"])
        elif l["kind"] == "gasket":
            R(ax, PANEL[0] + 0.3, PANEL[0] + 2.2, z, z + l["t"], C["gasket"])
        elif l["kind"] == "parts" and o == "F":
            R(ax, 72, 88, t, t + 1.6, C["parts"]); ax.text(80, t + 0.8, "parts", ha="center", va="center",
                                                              fontsize=6, color="white")
        z += l["t"]
    # --- FPC: fold at the right edge, flat behind the panel, joggle down through the opening to J3 on the back
    fpc_z = zp0 - 0.2
    pts = [(PANEL[2] - 0.5, zp0 + 1.4), (PANEL[2] + 0.6, zp0 + 1.2), (PANEL[2] + 1.0, zp0 + 0.6),
           (PANEL[2] + 0.6, fpc_z), (PANEL[2] - 11.9, fpc_z)]
    R(ax, PANEL[2] - 11.9, PANEL[2] - 1.5, fpc_z - 0.9, fpc_z - 0.1, "#ef6c00", ec="none")   # 1.0 component area
    if o == "F":
        pts += [(106.0, fpc_z), (104.0, t + 1.0)]
        R(ax, 100.0, 106.0, t, t + 2.0, "#ef6c00")
        ax.text(103, t + 2.3, "J3 on front", fontsize=6, ha="center")
    else:
        j = J3["R" if oR == "R" else "T"]
        slot = -0.88
        pts += [(120.2, fpc_z), (119.0, fpc_z - 0.5), (118.65, t * 0.5), (118.55, -0.15), (117.9, slot + 0.1),
                (117.0, slot), (j[2] - 2.84, slot)]
        R(ax, j[0], j[2], 0, -2.0, "#ef6c00")
        ax.text((j[0] + j[2]) / 2, -2.5, "J3 (back)", fontsize=6, ha="center")
    xs, zs = zip(*pts)
    ax.plot(xs, zs, color="#c79a00", lw=1.4 if zoom else 1.0, solid_capstyle="round")
    # --- back shell: grip floor, bay walls, door
    for a, b in ((xl + 2.0, BAY[0] - 1.2), (BAY[2] + 1.2, xr - 2.0)):
        R(ax, a, b, zb_grip, zb_grip + 1.6, C["floor"])
    for a in (BAY[0] - 1.2, BAY[2]):
        R(ax, a, a + 1.2, zb_bay, -2.0, C["floor"])
    R(ax, BAY[0], BAY[2], zb_bay, zb_bay + 1.2, C["door"])
    ax.text((BAY[0] + BAY[2]) / 2, zb_bay + 0.6, "battery door", ha="center", va="center", fontsize=6)
    # cell + its interfaces
    z = 0.0
    cell = CELL_R if oR == "R" else CELL_T
    for l in bay:
        z0, z1 = z, z - l["t"]
        if l["kind"] == "cell":
            R(ax, cell[0], cell[2], z0, z1, C["cell"]); ax.text((cell[0] + cell[2]) / 2, (z0 + z1) / 2,
                                                                 "cell 8.5 max", ha="center", va="center", fontsize=7)
        elif l["kind"] == "steel":
            R(ax, PLATE_R[0], PLATE_R[2], z0, z1, C["steel"])
            R(ax, RIB_R[0], RIB_R[2], z1, zb_bay + 1.2, C["floor"])                # rib carries the plate edge
            R(ax, BAY[0], PLATE_R[0] + 1.0, z1 - 0.8, z1, C["floor"])              # ledge on the bay wall
        elif l["kind"] == "swell":
            R(ax, cell[0], cell[2], z0, z1, C["swell"] if o != "R" and o != "R2" else "white",
              hatch="xxx" if o in ("T", "F") else "////")
        elif l["kind"] == "adh":
            R(ax, cell[0], cell[2], z0, z1, C["adh"], ec="none")
        z = z1
    # posts under the face switches (t < 1.6), SW3 is on this section line
    if t < 1.6:
        R(ax, 37.95, 41.95, -2.0, -0.05, C["floor"])
    sx = part("SW3")["x"]
    R(ax, sx - 2.55, sx + 2.55, t, t + 1.5, C["parts"])
    R(ax, sx - 1.2, sx + 1.2, t + 1.7, zf + 1.8, "#cfd8dc")
    ax.text(sx, zf + 2.1, "D-pad", ha="center", fontsize=6)
    # projected parts (outside the section plane)
    u = bbox("U12")
    R(ax, u[0], u[2], 0, -1.7, "none", ec="#7b1fa2", lw=0.6, ls="--")
    ax.text((u[0] + u[2]) / 2, -2.6, "U12 BT830\n(proj.)", ha="center", fontsize=5.5, color="#7b1fa2")
    sz1 = zf - 1.6 - 0.3
    R(ax, SPK[0] - SPK[2] / 2, SPK[0] + SPK[2] / 2, sz1 - 4.9, sz1, "none", ec="#1565c0", lw=0.6, ls="--")
    ax.text(SPK[0], sz1 - 2.5, "speaker (proj.)", ha="center", fontsize=5.5, color="#1565c0")
    ax.axhline(0, color="#2e7d32", lw=0.4, ls=":")
    ax.set_aspect("equal")
    ax.grid(color="#f0f0f0", lw=0.4)
    return zf, zb_bay, zb_grip


def draw_sections():
    fig = plt.figure(figsize=(20, 15.5))
    gs = fig.add_gridspec(4, 1, height_ratios=[1, 1, 1, 1.5])
    for i, o in enumerate(["T", "F", "R"]):
        ax = fig.add_subplot(gs[i])
        zf, zb, zg = section(ax, o)
        ax.set_xlim(15, 175); ax.set_ylim(-13, 12)
        ax.set_title(f"{o}: {OPTS[o]}  |  section y 71 (screen centre), true scale  |  t {board_t(o)}  |  "
                     f"grip {zf - zg:.2f} mm, bump {zf - zb:.2f} mm", loc="left", fontsize=10, weight="bold")
        ax.set_ylabel("z (mm)")
    ax = fig.add_subplot(gs[3])
    zf, zb, zg = section(ax, "R", zoom=True)
    ax.set_xlim(98, 140); ax.set_ylim(-4.0, 8.0)
    ax.set_title("R, zoom on the FPC end (true scale, 1:1 in x and z)", loc="left", fontsize=10, weight="bold")
    notes = [
        (126.0, 7.3, "1.1 glass cover lens, bonded to the shell land with 0.13 PSA"),
        (64.5, 6.0, ""),
        (100.5, 6.25, "0.3 air gap = the black foam gasket ring on the lens mask (ring is at the panel border, x 63-65)"),
        (100.5, 3.6, "panel 4.0 (+tape 0.115, liner left on)"),
        (100.5, 1.37, "0.3 back clearance over a zero-height board zone"),
        (119.0, 2.3, "factory fold; 1.0 component area hangs in the board opening x 118.25-133.05"),
        (113.0, 0.35, "opening edge: no copper, polyimide tape"),
        (112.5, -1.25, "S-joggle in the single-layer tail (r~1), drop 2.45"),
        (100.5, -0.45, "0.5 stainless battery plate, 0.2 under the board"),
        (100.5, -3.3, "plate rib (x 103.6-104.6) splits cell / J3 compartment"),
    ]
    for x, y, s in notes:
        if s:
            ax.text(x, y, s, fontsize=7.5, color=INK, bbox=dict(fc="white", ec="none", alpha=0.75, pad=0.5))
    ax.set_ylabel("z (mm)"); ax.set_xlabel("x (KiCad mm)")
    for a in fig.axes:
        for tx in a.texts:
            tx.set_clip_on(True)
    fig.subplots_adjust(left=0.04, right=0.99, top=0.97, bottom=0.04, hspace=0.32)
    fig.savefig(os.path.join(HERE, "me_sections.png"), dpi=110)
    plt.close(fig)


# ------------------------------------------------------------------------------------------------ 3. plan view
def rect(ax, r, label=None, fc="none", ec=INK, lw=0.8, ls="-", alpha=1.0, fs=6.5, color=None, la="center", z=2,
         hatch=None):
    x0, y0, x1, y1 = r
    ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc=fc, ec=ec, lw=lw, ls=ls, alpha=alpha, zorder=z,
                           hatch=hatch))
    if label:
        ax.text((x0 + x1) / 2, (y0 + y1) / 2, label, ha="center", va="center", fontsize=fs, color=color or INK,
                zorder=z + 1, multialignment=la)


def draw_plan():
    fig, axs = plt.subplots(1, 2, figsize=(22, 7.6))
    for k, side in enumerate(("FRONT (viewing side)", "BACK (seen from the front, not mirrored)")):
        ax = axs[k]
        ax.add_patch(MPoly(list(SIL.exterior.coords), fc="#f5f5f5", ec=INK2, lw=1.0, zorder=0))
        bd = board_revb()
        ax.add_patch(MPoly(list(bd.exterior.coords), fc="#e8f5e9", ec="#2e7d32", lw=0.9, zorder=1))
        for h in bd.interiors:
            ax.add_patch(MPoly(list(h.coords), fc="white", ec="#2e7d32", lw=0.9, zorder=1))
        rect(ax, FPC_OPEN, "FPC\nopening", fc="white", ec="#2e7d32", fs=6)
        if k == 0:
            rect(ax, LENS, None, ec="#1565c0", lw=0.8, ls="--")
            ax.text(LENS[0] + 8.0, LENS[3] + 2.4, "1.1 glass lens 82.2 x 56 (land 2.5 outside the opening)",
                    fontsize=6.5, color="#1565c0")
            rect(ax, PANEL, None, fc="#424242", ec=INK, alpha=0.85)
            rect(ax, AA, "active area 57.6 x 43.2\ncentred on (95.25, 71.0)", fc="#607d8b", ec="none", color="white",
                 fs=7.5)
            ax.text(PANEL[0] + 0.5, PANEL[3] + 1.6, "panel 69.3 x 50.2: hung from the lens on a 0.3 gasket ring;"
                    " board under it = zero-height zone", fontsize=6.5)
            ax.add_patch(Rectangle((PANEL[0] + 0.3, PANEL[1] + 0.3), PANEL[2] - PANEL[0] - 9.0,
                                   PANEL[3] - PANEL[1] - 0.6, fc="none", ec="#ffb300", lw=2.0, zorder=3))
            ax.text(PANEL[0] + 1.2, PANEL[1] + 2.0, "gasket ring (3 sides, ~1.8 wide; 2 pads at the FPC end,"
                    " none over the COG driver)", fontsize=5.8, color="#ffb300", zorder=4)
            for n in ("SW2", "SW3", "SW4", "SW5", "SW6", "SW7", "SW8", "SW9", "SW10", "SW11", "SW12", "SW13",
                      "SW1", "J7", "L2"):
                b = bbox(n); rect(ax, b, n, fc="#bdbdbd", ec="#757575", fs=5)
            ax.add_patch(Circle(SPK[:2], SPK[2] / 2, fc="none", ec="#1565c0", lw=0.8, ls="--"))
            ax.text(SPK[0], SPK[1], "speaker", ha="center", fontsize=6, color="#1565c0")
        else:
            rect(ax, BAY, None, ec="#6d4c41", lw=1.4, ls="--", fc="#efebe9", alpha=0.6)
            ax.text(BAY[0] + 0.5, BAY[1] - 1.0, "battery bay / door 58 x 71 (back shell)", fontsize=6.5,
                    color="#6d4c41")
            rect(ax, PLATE_R, None, fc="#90a4ae", ec="#455a64", alpha=0.6)
            rect(ax, CELL_R, "cell 38.5 x 62\non stretch-release tape\nPCM end down", fc=C["cell"], ec=INK,
                 alpha=0.9, fs=7)
            rect(ax, RIB_R, None, fc="#8d6e63", ec="none")
            ax.text(PLATE_R[0] + 1.0, PLATE_R[1] + 0.4, "0.5 SS battery plate on ledges + rib (under the cell)",
                    fontsize=5.8, color="#263238", va="top", zorder=6)
            rect(ax, J3["R"], "J3\nAFC07\ntop\ncontact", fc="#ef6c00", ec=INK, fs=5.5, color="white")
            rect(ax, (J3["R"][2], 57.25, 118.25, 84.75), "FPC\ntail", fc="#ffe082", ec="none", fs=5.5)
            rect(ax, J8, "J8", fc="#ef6c00", ec=INK, fs=6, color="white")
            rect(ax, SWD, "SWD 1x5", fc="#ffcc80", ec=INK, fs=5.5)
            rect(ax, LEAD_POCKET, "lead pocket", fc="#fff3e0", ec="#6d4c41", fs=5.5, ls=":")
            rect(ax, FPGA, "FPGA block", fc="#ce93d8", ec=INK, fs=6)
            rect(ax, MCU, "MCU block (plan B)", fc="#ce93d8", ec=INK, fs=6)
            u = bbox("U12"); rect(ax, u, "U12\nBT830", fc="#b39ddb", ec=INK, fs=6)
            for r_, lab in ((20, "20 mm"), (30, "30 mm")):
                ax.add_patch(Circle(((u[0] + u[2]) / 2, (u[1] + u[3]) / 2), r_ + (u[2] - u[0]) / 2, fc="none",
                                    ec="#7b1fa2", lw=0.6, ls=":"))
                ax.text((u[0] + u[2]) / 2 + r_ + (u[2] - u[0]) / 2 - 1, (u[1] + u[3]) / 2 + 9, lab, fontsize=5.5,
                        color="#7b1fa2")
            for n, (x, y) in POSTS.items():
                ax.add_patch(Circle((x, y), 2.0, fc="#90a4ae", ec=INK, lw=0.5, zorder=4))
                ax.text(x, y - 2.8, f"post {n}", ha="center", fontsize=5, zorder=5)
            for n, (x, y) in DOOR_SCREWS.items():
                ax.add_patch(Circle((x, y), 1.2, fc="#6d4c41", ec=INK, lw=0.5, zorder=5))
                ax.text(x, y + 2.4, n, ha="center", fontsize=5.5, color="#6d4c41", zorder=5)
        for n, (x, y) in SHELL_SCREWS.items():
            ax.add_patch(Circle((x, y), 3.0, fc="none", ec="#c62828", lw=1.0, zorder=5))
            ax.add_patch(Circle((x, y), 1.0, fc="#c62828", ec="none", zorder=5))
            ax.text(x + 3.4, y, n, fontsize=6.5, color="#c62828", va="center", zorder=6)
        for n, (x, y) in BOARD_SCREWS.items():
            ax.add_patch(Circle((x, y), 3.0, fc="none", ec="#00838f", lw=1.0, zorder=5))
            ax.add_patch(Circle((x, y), 1.0, fc="#00838f", ec="none", zorder=5))
            ax.text(x + 3.4, y, n, fontsize=6.5, color="#00838f", va="center", zorder=6)
        ax.set_xlim(15, 175); ax.set_ylim(118, 24); ax.set_aspect("equal")
        ax.set_title(side, loc="left", fontsize=11, weight="bold")
        ax.grid(color="#eeeeee", lw=0.4)
    fig.text(0.01, 0.015, "Red S1-S5: back shell -> front shell screws (rev A's 4 + STRUCTURAL_REVIEW #3's 5th), clamping "
             "the board where they pass through it.  Teal B1-B2: board -> front shell only (hold the board while the FPC "
             "is connected and for bring-up before the back shell goes on).  Brown D1-D2: door screws (door hooks at the "
             "bay's top wall).  Posts: dia 4, 0.05 under the board, 8 needed at t 1.2.", fontsize=8, color=INK2)
    fig.suptitle("gb2 rev B, recommended architecture R: plan view (KiCad mm, +Y down)", fontsize=12, x=0.01,
                 ha="left")
    fig.subplots_adjust(left=0.03, right=0.99, top=0.92, bottom=0.06, wspace=0.05)
    fig.savefig(os.path.join(HERE, "me_plan_R.png"), dpi=110)
    plt.close(fig)


# ------------------------------------------------------------------------------------------------ 4. assembly
def draw_assembly():
    fig, ax = plt.subplots(figsize=(18, 6.6))
    t = 1.2
    zf = 7.015
    # exploded offsets (mm, along z) - each part drawn at its true thickness and x extent
    xl, xr = 21.0, 169.5
    parts = [
        # (label, x0, x1, z0, thickness, fc, step)
        ("front shell (chassis): face plate, walls, bosses with M2 inserts, land, panel pocket", xl, xr, 34, 1.6,
         C["plate"], "A0"),
        ("1.1 glass lens (black mask printed on its back), 0.13 PSA", LENS[0], LENS[2], 40, 1.1, C["glass"], "A1"),
        ("0.3 black foam gasket ring (die-cut)", PANEL[0], PANEL[2] - 8.7, 29.5, 0.3, C["gasket"], "A2"),
        ("panel (factory 180 deg fold, FPC tail lifted vertical)", PANEL[0], PANEL[2], 24, 4.0, C["panel"], "A3"),
        ("keycaps, D-pad, edge caps, slider, speaker, light pipe -> into the front shell", 30, 60, 19, 1.5,
         "#cfd8dc", "A4"),
        ("board t 1.2 (front parts down); thread the FPC tail through the opening", 22.5, 167.3, 12.5, t,
         C["board"], "A5"),
        ("B1, B2 board screws; then J3 latch closed on the back  =>  FRONT MODULE (test it powered now)", 55, 130,
         9.0, 0.6, "#00838f", "A6"),
        ("back shell: grip floor, bay walls, posts, ledges, plate rib", xl, xr, 0, 1.6, C["floor"], "B1"),
        ("0.5 stainless battery plate onto its ledges (2 M1.6 screws or snaps)", PLATE_R[0], PLATE_R[2], -5, 0.5,
         C["steel"], "B2"),
        ("cell onto the plate with stretch-release tape, leads into the pocket", CELL_R[0], CELL_R[2], -16, 8.5,
         C["cell"], "B3"),
        ("battery door (hooks at the top wall + D1, D2); no foam on the cell", BAY[0], BAY[2], -22, 1.2,
         C["door"], "C3"),
    ]
    for lab, x0, x1, z0, th, fc, st in parts:
        ax.add_patch(Rectangle((x0, z0), x1 - x0, th, fc=fc, ec="#555555", lw=0.5))
        ax.text(x1 + 1.5, z0 + th / 2, f"{st}  {lab}", va="center", fontsize=8.5)
    # FPC tail standing up from the panel
    ax.plot([PANEL[2] - 14, PANEL[2] - 14], [24, 13.5], color="#c79a00", lw=1.5)
    ax.text(PANEL[2] - 13.5, 18, "FPC tail", fontsize=7, color="#8d6e00")
    # sub-assembly boxes
    ax.add_patch(Rectangle((15, 8.3), 230, 35.6, fc="none", ec="#00838f", lw=1.0, ls="--"))
    ax.text(16, 44.6, "A: front module, built face-down on a spacer frame (DESIGN.md §2 step 1)", color="#00838f",
            fontsize=9, weight="bold")
    ax.add_patch(Rectangle((15, -17), 230, 19.2, fc="none", ec="#6d4c41", lw=1.0, ls="--"))
    ax.text(16, 3.0, "B: back module", color="#6d4c41", fontsize=9, weight="bold")
    steps = ("C1  back module onto front module; S1-S5 screws (0.05-0.08 N m, STRUCTURAL_REVIEW #11)\n"
             "C2  through the door opening: plug the cell into J8\n"
             "C3  door on (hooks + D1, D2)\n\n"
             "SERVICE:  battery = door only (C3 -> C2 -> pull tabs).   FPC reseat / SWD flash = door only (J3, J1 sit in "
             "the bay beside the plate rib).\n"
             "board out = door, unplug J8, S1-S5 (cell stays in the back module), open J3, B1-B2.   "
             "panel swap = board out, warm and peel the panel off the gasket; the lens stays.")
    ax.text(16, -29, steps, fontsize=9, va="top", family="monospace")
    ax.set_xlim(14, 300); ax.set_ylim(-44, 48); ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title("gb2 rev B, architecture R: exploded side view (x to scale, parts at true thickness, z exploded) "
                 "and assembly order", loc="left", fontsize=12, weight="bold")
    fig.subplots_adjust(left=0.01, right=0.99, top=0.95, bottom=0.01)
    fig.savefig(os.path.join(HERE, "me_assembly_R.png"), dpi=110)
    plt.close(fig)


if __name__ == "__main__":
    draw_zstacks(); print("me_zstacks.png")
    draw_sections(); print("me_sections.png")
    draw_plan(); print("me_plan_R.png")
    draw_assembly(); print("me_assembly_R.png")
