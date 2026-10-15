"""Draws the consolidated rev B architecture figures (true scale) from arch_model.py.

    ../../enclosure/.venv/bin/python arch_draw.py [1 2 3 4 5]      (no args = all)

01_three_ways.png          side sections at the screen centre (y 71): taped / floating / cut-out / recommended
02_recommended_section.png the recommended build in detail: full section, stack zoom, FPC-end zoom, grip section, key zoom
03_exploded.png            exploded side view, numbered in assembly order
04_floorplan_front.png     board front: zones, panel, lens, keep-outs, screws, buttons
05_floorplan_back.png      board back: cell, plate, bay/door, connectors, chips, posts, RF rings
"""
import sys, os, math
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Polygon as MPoly, Circle, Patch, FancyBboxPatch
from matplotlib.lines import Line2D
from shapely.geometry import Polygon, Point, box as sbox
import arch_model as M
from arch_model import CAT_COL

HERE = M.HERE
INK, INK2, BG = "#111111", "#4a4a4a", "#ffffff"
plt.rcParams.update({"font.size": 13, "text.color": INK, "axes.labelcolor": INK2, "xtick.color": INK2,
                     "ytick.color": INK2, "axes.edgecolor": "#c8c8c8", "figure.facecolor": BG,
                     "axes.facecolor": BG, "savefig.facecolor": BG, "font.family": "DejaVu Sans"})

# one palette for every figure: (face, hatch, edge)
PAL = {
    "lens":    ("#8fd3f4", None, "#3b8fb5"),
    "front":   ("#c3ccd6", None, "#7d8a97"),     # front shell (face plate, walls, lens land, key guides)
    "back":    ("#e6d3b8", None, "#a38966"),     # back shell (floor, walls, bay, posts, rib)
    "door":    ("#c79b62", None, "#8a6a3c"),     # battery door (part of the back)
    "display": ("#2b2f3a", None, "#000000"),
    "gasket":  ("#8e24aa", None, "#5c1470"),     # black foam gasket in reality - drawn purple to stand out
    "foam":    ("#e7c6ef", "xx", "#8e24aa"),     # compressible foam filling a swell space
    "tape":    ("#f06292", None, "#ad1457"),     # adhesive / tape layers
    "fpc":     ("#ff9f00", None, "#b36b00"),
    "conn":    ("#d84315", None, "#7f2208"),
    "pcb":     ("#2e7d32", None, "#174d1a"),
    "parts":   ("#5c6bc0", None, "#2c387e"),     # chips, switches, speaker, connectors other than J3
    "keys":    ("#26a69a", None, "#00695c"),     # keycaps, D-pad
    "plate":   ("#90a4ae", "////", "#455a64"),   # stainless battery plate
    "cell":    ("#fdd835", None, "#a68b00"),
    "swell":   ("#fffbe6", "...", "#d4a017"),    # FREE swell gap (air)
    "air":     ("#ffffff", None, "#bdbdbd"),
    "screw":   ("#212121", None, "#000000"),
}
LEG_ORDER = [("lens", "cover lens"), ("front", "front shell"), ("display", "display panel"),
             ("gasket", "gasket (foam ring)"), ("tape", "tape / adhesive"), ("fpc", "display flex (FPC)"),
             ("conn", "FPC connector J3"), ("pcb", "PCB"), ("parts", "parts (chips, switches, speaker)"),
             ("keys", "keycaps / D-pad"), ("plate", "battery plate (steel)"), ("cell", "battery cell"),
             ("swell", "free swell gap"), ("foam", "foam in swell space"), ("back", "back shell / posts"),
             ("door", "battery door")]


def r1(x):
    """round half up to 0.1 mm (python's .1f rounds 20.95 down to 20.9; the text says 21.0)."""
    return f"{math.floor(x * 10 + 0.5 + 1e-6) / 10:.1f}"


def legend_handles(keys=None):
    out = []
    for k, lab in LEG_ORDER:
        if keys and k not in keys:
            continue
        fc, h, ec = PAL[k]
        out.append(Patch(facecolor=fc, hatch=h, edgecolor=ec, label=lab, lw=1.0))
    return out


def rect(ax, x0, x1, z0, z1, kind, lw=0.6, alpha=1.0, ls="-", zorder=2, ec=None):
    fc, h, e = PAL[kind]
    ax.add_patch(Rectangle((min(x0, x1), min(z0, z1)), abs(x1 - x0), abs(z1 - z0), fc=fc, hatch=h,
                           ec=ec or e, lw=lw, alpha=alpha, ls=ls, zorder=zorder))


def scalebar(ax, x, y, L=10, label=None, h=None, fs=12):
    ax.plot([x, x + L], [y, y], color=INK, lw=3, solid_capstyle="butt", zorder=20)
    for xx in (x, x + L):
        ax.plot([xx, xx], [y - (h or 0.5), y + (h or 0.5)], color=INK, lw=1.5, zorder=20)
    ax.text(x + L / 2, y - (h or 0.5) * 1.6, label or f"{L} mm", ha="center", va="top", fontsize=fs, zorder=20,
            weight="bold")


def dim(ax, x, z0, z1, text, side="right", fs=15, color=INK, box=True):
    ax.annotate("", xy=(x, z1), xytext=(x, z0),
                arrowprops=dict(arrowstyle="<|-|>", lw=1.6, color=color, shrinkA=0, shrinkB=0,
                                mutation_scale=14), zorder=25)
    dx = 0.8 if side == "right" else -0.8
    ax.text(x + dx, (z0 + z1) / 2, text, ha="left" if side == "right" else "right", va="center", fontsize=fs,
            weight="bold", color=color, zorder=26,
            bbox=dict(fc="white", ec=color, lw=1.0, boxstyle="round,pad=0.25") if box else None)


def callout(ax, xy, xytext, text, fs=12, ha="left", va="center", color=INK, lw=0.9, box=True):
    ax.annotate(text, xy=xy, xytext=xytext, fontsize=fs, ha=ha, va=va, color=color, zorder=30,
                arrowprops=dict(arrowstyle="-", lw=lw, color="#555555", shrinkA=0, shrinkB=1),
                bbox=dict(fc="white", ec="#bbbbbb", lw=0.6, boxstyle="round,pad=0.2", alpha=0.95) if box else None)


# ================================================================================================ section y = 71
def section_y71(ax, o, show_proj=True):
    """True-scale x-z section through the screen centre (y 71) for option o. Returns a dict of key z levels."""
    O = M.OPTIONS[o]
    t = O["t"]
    zf = M.z_front(o)
    zb = M.z_back_bay(o)
    (xl, xr), = M.span_at(M.SIL, y=71.0)
    lens_t = O["front"][-1]["t"]
    zpl = zf - M.PLATE_T                              # plate inner face
    # panel bottom
    zp0 = t + sum(l["t"] for l in O["front"][:-3]) if o != "R" else t + 0.3 + 0.115
    if o == "F":
        zp0 = t + 2.3
    zp1 = zp0 + 4.0
    # ---- front shell: walls, plate, lens land
    for a, b in ((xl, xl + 2.0), (xr - 2.0, xr)):
        rect(ax, a, b, t, zf, "front")
        rect(ax, a, b, M.Z_BACK_GRIP, t, "back")
    dpad = (36.65, 43.25)
    hole = (dpad[0] - 1.3, dpad[1] + 1.3)
    for a, b in ((xl + 2.0, hole[0]), (hole[1], M.LENS[0]), (M.LENS[2], xr - 2.0)):
        rect(ax, a, b, zpl, zf, "front")
    land_top = zf - lens_t - 0.13
    for a, b in ((M.LENS[0], M.POCKET[0]), (M.POCKET[2], M.LENS[2])):
        land_bot = zp0 if o == "F" else land_top - 1.0
        rect(ax, a, b, land_bot, land_top, "front")
        rect(ax, a, b, land_top, land_top + 0.13, "tape", lw=0.3)
    if o == "F":   # shell frame lip under the panel edge (the panel sits on it)
        rect(ax, M.POCKET[0], M.POCKET[0] + 1.2, zp0 - 0.8, zp0, "front")
        rect(ax, M.LENS[0], M.POCKET[0], zp0 - 0.8, zp0, "front")
    rect(ax, M.LENS[0], M.LENS[2], zf - lens_t, zf, "lens")
    # ---- D-pad (down arm, leg onto SW3), SW3, its post
    rect(ax, dpad[0], dpad[1], zf, zf + 1.8, "keys")
    rect(ax, dpad[0] + 1.0, dpad[1] - 1.0, zpl, zf, "keys")
    rect(ax, hole[0] - 1.0, hole[1] + 1.0, zpl - 0.8, zpl, "keys")
    rect(ax, 38.65, 41.25, t + 1.6, zpl - 0.8, "keys")
    rect(ax, 37.41, 42.49, t, t + 1.2, "parts")          # TS-1187A body: cover (hard stop) at keystake - 0.3
    rect(ax, 38.95, 40.95, t + 1.2, t + 1.5, "parts")    # keystake top at board top + 1.5 (params.py:115, :120)
    rect(ax, 37.95, 41.95, -2.0, -0.05, "back")
    # ---- board
    bspans = M.span_at(M.board_revb(), y=71.0)
    for a, b in bspans:
        if o == "C":
            for aa, bb in ((a, min(b, 65.1)), (max(a, 104.6), b)):
                if bb > aa:
                    rect(ax, aa, bb, 0, t, "pcb")
        elif o == "F":
            pass
        else:
            rect(ax, a, b, 0, t, "pcb")
    if o == "F":                                       # no FPC opening in F
        rect(ax, bspans[0][0], bspans[-1][1], 0, t, "pcb")
    # ---- panel + its tape / gasket
    rect(ax, M.PANEL[0], M.PANEL[2], zp0, zp1, "display")
    if o in ("T", "C"):
        rect(ax, M.PANEL[0], M.PANEL[0] + 1.5, t, t + 0.115, "tape", lw=0.3)
    if o == "R":
        rect(ax, M.PANEL[0], M.PANEL[0] + 1.5, zp0 - 0.115, zp0, "tape", lw=0.3)
        rect(ax, M.PANEL[0] + 0.3, M.PANEL[0] + 2.1, zp1, zp1 + 0.3, "gasket", lw=0.3)
    if o in ("T", "F", "C"):                         # bezel adhesive / foam at the far end
        if o == "F":
            rect(ax, M.PANEL[0] + 0.3, M.PANEL[0] + 2.1, zp1, zp1 + 0.7, "gasket", lw=0.3)
    # ---- parts under the panel (F only)
    if o == "F":
        for a, b, h in ((65.5, 73.5, 0.8), (75.0, 81.0, 0.92), (82.5, 87.0, 1.25), (88.0, 102.5, 1.0)):
            rect(ax, a, b, t, t + h, "parts")
        rect(ax, 104.65, 111.65, t, t + 2.0, "conn")
    # ---- FPC: fold at the right end, flat behind the panel, then to J3
    fpc_z = zp0 - 0.2
    fold = [(M.PANEL[2] - 0.6, zp0 + 1.4), (M.PANEL[2] + 0.55, zp0 + 1.25), (M.PANEL[2] + 1.0, zp0 + 0.6),
            (M.PANEL[2] + 0.55, fpc_z), (M.PANEL[2] - 11.9, fpc_z)]
    rect(ax, M.PANEL[2] - 11.9, M.PANEL[2] - 1.5, fpc_z - 0.9, fpc_z - 0.1, "fpc", lw=0.3)   # 1.0 component area
    if o == "F":
        pts = fold + [(111.65, fpc_z), (111.0, t + 0.9)]
        jx = None
    else:
        j = M.J3_R
        slot = -0.88
        pts = fold + [(120.2, fpc_z), (119.0, fpc_z - 0.45), (118.7, t * 0.5), (118.55, -0.2), (117.8, slot + 0.12),
                      (116.8, slot), (j[2] - 2.84, slot)]
        rect(ax, j[0], j[2], -2.0, 0, "conn")
        jx = j
    xs, zs = zip(*pts)
    ax.plot(xs, zs, color=PAL["fpc"][0], lw=2.6, solid_capstyle="round", zorder=12)
    ax.plot(xs, zs, color=PAL["fpc"][2], lw=0.6, zorder=13)
    # ---- back shell: grip floors, bay walls, door, cell
    bx0, bx1 = O["bay_x"]
    for a, b in ((xl + 2.0, bx0 - 1.2), (bx1 + 1.2, xr - 2.0)):
        rect(ax, a, b, M.Z_BACK_GRIP, -2.0, "back")
    for a in (bx0 - 1.2, bx1):
        rect(ax, a, a + 1.2, zb, -2.0, "back")
    if o == "C":
        rect(ax, bx0 - 1.2, bx1 + 1.2, M.Z_BACK_GRIP, -2.0, "air", lw=0)   # (no floor over the window)
    cx0, cx1 = O["cell"]
    if o == "R":
        rect(ax, bx0, bx0 + 1.1, -1.6, -0.7, "back")                     # plate ledge
        rect(ax, M.RIB_R[0], M.RIB_R[2], zb + 1.2, -0.7, "back")          # rib
        rect(ax, M.PLATE_R[0], M.PLATE_R[2], -0.7, -0.2, "plate")
        rect(ax, cx0, cx1, -0.85, -0.7, "tape", lw=0.3)
        rect(ax, cx0, cx1, -9.35, -0.85, "cell")
        rect(ax, cx0, cx1, -10.2, -9.35, "swell")
        cz = (-9.35, -0.85)
    elif o == "C":
        cf = t + 0.115 - 0.3
        rect(ax, cx0, cx1, cf - 8.5, cf, "cell")
        rect(ax, cx0, cx1, cf - 9.35, cf - 8.5, "foam")
        cz = (cf - 8.5, cf)
    else:
        rect(ax, cx0, cx1, -8.8, -0.3, "cell")
        rect(ax, cx0, cx1, -9.65, -8.8, "foam")
        cz = (-8.8, -0.3)
    rect(ax, bx0, bx1, zb, zb + 1.2, "door")
    # ---- projected parts (dashed): speaker (y 72-99) and U12 (y 46-60) are just outside this cut
    if show_proj:
        s_top = zpl - 0.3
        ax.add_patch(Rectangle((M.SPK[0] - 13.5, s_top - 4.9), 27.0, 4.9, fc="none", ec=PAL["parts"][0], lw=1.2,
                               ls="--", zorder=3))
        ax.add_patch(Rectangle((M.U12[0], -1.7), M.U12[2] - M.U12[0], 1.7, fc="none", ec=PAL["parts"][0], lw=1.2,
                               ls="--", zorder=3))
    return dict(t=t, zf=zf, zb=zb, zp0=zp0, zp1=zp1, xl=xl, xr=xr, bay=(bx0, bx1), cell=(cx0, cx1), cz=cz,
                zpl=zpl, fpc_z=fpc_z, j3=jx, lens_t=lens_t)


def setup_section_ax(ax, xlim=(15, 176), zlim=(-13.5, 13.0)):
    ax.set_xlim(*xlim); ax.set_ylim(*zlim); ax.set_aspect("equal")
    ax.set_yticks(range(-12, 13, 4)); ax.tick_params(labelsize=10)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)


# ================================================================================================ 01
def fig01():
    rows = ["T", "F", "C", "R"]
    notes = {
        "T": ["+ thin: grips 10.9 mm", "- the panel is glued to the board:", "  it bends with the board, and",
              "  replacing it means peeling tape", "  off the glass", "- FPC needs a hole in the board"],
        "F": ["+ chips can sit under the panel", "  (short display wiring, no hole),", "  as in the Analogue Pocket, Miyoo",
              "- 2.8 mm thicker than taped; 3.1 /", "  2.5 mm thicker than D (grip / bump)", "+ panel held by the shell"],
        "C": ["+ thinnest bump (17.1 mm)", "- cell is almost as big as the panel:", "  the hole nearly cuts the board",
              "- the swelling cell pushes on the LCD", "- pros put a rigid wall in between:", "  steel plate (RP3), AA well (GBA)"],
        "R": ["+ grips 10.6 mm (thinnest)", "+ panel held by the shell (GBA style)", "  board can't bend it, swappable",
              "+ cell has its own plate + door:", "  swells away from the screen", "- needs a hole in the board for the FPC"],
    }
    fig = plt.figure(figsize=(24, 19.5))
    gs = fig.add_gridspec(5, 2, width_ratios=[4.25, 1], height_ratios=[1, 1, 1, 1, 0.3], hspace=0.30, wspace=0.02,
                          left=0.035, right=0.99, top=0.905, bottom=0.03)
    for i, o in enumerate(rows):
        ax = fig.add_subplot(gs[i, 0])
        g = section_y71(ax, o, show_proj=False)
        setup_section_ax(ax, zlim=(-14.4, 15.5))          # room under the bay bracket label for the x axis
        tt = M.totals(o)
        rec = o == "R"
        ax.set_title(f"{'ABCD'[i]}.  {M.OPTIONS[o]['title']}   (board {g['t']} mm)", loc="left", fontsize=17,
                     weight="bold", color="#1b5e20" if rec else INK, pad=8)
        # region brackets
        ytop = 13.4
        for a, b, lab in ((g["xl"], M.POCKET[0], "LEFT GRIP"), (M.POCKET[0], M.POCKET[2], "SCREEN"),
                          (M.POCKET[2], g["xr"], "RIGHT GRIP")):
            ax.plot([a + 0.4, a + 0.4, b - 0.4, b - 0.4], [ytop - 0.6, ytop, ytop, ytop - 0.6], color=INK2, lw=1.2)
            ax.text((a + b) / 2, ytop + 0.35, lab, ha="center", va="bottom", fontsize=13, weight="bold", color=INK2)
        bx0, bx1 = g["bay"]
        yb = g["zb"] - 0.9
        ax.plot([bx0 - 1.2, bx0 - 1.2, bx1 + 1.2, bx1 + 1.2], [yb + 0.6, yb, yb, yb + 0.6], color=INK2, lw=1.2)
        ax.text((bx0 + bx1) / 2, yb - 0.35, "BATTERY BUMP (behind the screen)", ha="center", va="top", fontsize=13,
                weight="bold", color=INK2)
        # thickness dims
        dim(ax, 53.5, M.Z_BACK_GRIP, g["zf"], f"grip\n{r1(tt['grip'])} mm", side="left")
        dim(ax, 89.0 if o != "F" else 70.0, g["zb"], g["zf"], f"{r1(tt['bump'])} mm", side="right")
        # a few labels
        lab_fs = 12
        ax.text(80, g["zp0"] + 2.0, "display panel", color="white", fontsize=13, ha="center", va="center",
                weight="bold", zorder=30)
        ax.text(78, (g["cz"][0] + g["cz"][1]) / 2, "battery cell", fontsize=13, ha="center", va="center",
                weight="bold", zorder=30) if o != "F" else ax.text(97, -5.0, "battery cell", fontsize=13,
                ha="center", va="center", weight="bold", zorder=30)
        ax.text(110, g["zf"] - g["lens_t"] / 2, "cover lens", fontsize=10.5, ha="center", va="center", zorder=30)
        callout(ax, (30, g["t"] / 2), (25, -6.5), "PCB", fs=lab_fs)
        if o in ("T", "C"):
            callout(ax, (64.0, g["t"] + 0.06), (44.0, -6.5), "panel taped\nto the board", fs=lab_fs)
        if o == "F":
            callout(ax, (95, g["t"] + 0.6), (128, -5.5), "chips under the panel", fs=lab_fs)
        if o == "R":
            callout(ax, (64.4, g["zp1"] + 0.15), (41.0, -7.6), "foam gasket ring holds\nthe panel to the lens",
                    fs=lab_fs, va="center")
            # leaders kept right of the bump dimension box and below J3 (x 106.2-112, z -2..0)
            callout(ax, (98, -0.45), (128, -7.4), "steel battery plate", fs=lab_fs)
            callout(ax, (95, -9.75), (128, -10.0), "free swell gap", fs=lab_fs)
        if o == "C":
            callout(ax, (102, -3.0), (128, -5.0), "cell in a hole in the board,\n0.3 mm from the LCD", fs=lab_fs)
        if o in ("T", "F"):
            callout(ax, (95, -9.2), (128, -9.6), "foam in the swell space", fs=lab_fs)
        if g["j3"] is not None:
            callout(ax, (109, -1.4), (128, -2.2 if o != "C" else -1.0), "FPC connector J3 (back)", fs=lab_fs)
        callout(ax, (37.0, g["zf"] + 1.0), (24.2, g["t"] + 2.6), "D-pad", fs=lab_fs, va="center")
        scalebar(ax, 152, -11.6, 10)
        # notes column
        axn = fig.add_subplot(gs[i, 1]); axn.axis("off")
        axn.text(0.0, 0.93, M.OPTIONS[o]["short"].upper(), fontsize=19, weight="bold",
                 color="#1b5e20" if rec else INK, va="top", transform=axn.transAxes)
        axn.text(0.0, 0.76, f"grip {r1(tt['grip'])} mm\nscreen/battery {r1(tt['bump'])} mm", fontsize=15,
                 va="top", transform=axn.transAxes, weight="bold", color=INK2)
        for k, line in enumerate(notes[o]):
            col = "#1b5e20" if line.startswith("+") else ("#b71c1c" if line.startswith("-") else INK)
            axn.text(0.0, 0.55 - k * 0.098, line, fontsize=13.5, va="top", transform=axn.transAxes, color=col)
        if rec:
            axn.add_patch(FancyBboxPatch((-0.04, -0.12), 1.02, 1.12, transform=axn.transAxes, fc="none",
                                         ec="#2e7d32", lw=3, boxstyle="round,pad=0.01", clip_on=False))
    axl = fig.add_subplot(gs[4, :]); axl.axis("off")
    axl.legend(handles=legend_handles(), loc="center", ncol=8, fontsize=13, frameon=False, handlelength=2.2,
               handleheight=1.3, columnspacing=1.6)
    fig.suptitle("gb2 rev B: three ways to stack the screen and battery (plus the recommended one)\n"
                 "Side cuts through the screen centre (board y = 71), all at the SAME true scale; "
                 "numbers = total device thickness", fontsize=21, weight="bold", x=0.035, ha="left", y=0.985)
    fig.text(0.035, 0.005, "Sources: stack totals from arch_model.py (= me_stacks.py, REVB_DESIGN.md §3.1, mechanical "
             "journal cut-out 17.05 mm); panel MI0283QT-11 P.5; cell PKCELL LP-803860 p.5; outline board.json "
             "mech.silhouette. Axes in mm (x = board x, z = 0 at the board's back face).", fontsize=11, color=INK2)
    out = os.path.join(HERE, "01_three_ways.png")
    fig.savefig(out, dpi=100)
    plt.close(fig)
    print("wrote", out)


# ================================================================================================ grip section
GX = 156.88   # through the A button (SW6), the MCU on the back and the speaker


def grip_section(ax):
    """True-scale y-z section at x = 156.88 (right grip) for R. Horizontal axis = board y (top edge on the left)."""
    t, zf = 1.2, M.z_front("R")
    zpl = zf - M.PLATE_T
    (yt, yb), = M.span_at(M.SIL, x=GX)
    (b0, b1), = M.span_at(M.board_revb(), x=GX)
    # shells
    for a, b in ((yt, yt + 2.0), (yb - 2.0, yb)):
        rect(ax, a, b, t, zf, "front")
        rect(ax, a, b, M.Z_BACK_GRIP, t, "back")
    hole = (61.92 - 4.9, 61.92 + 4.9)
    dx = GX - M.SPK[0]
    gr = math.sqrt(11.0 ** 2 - dx ** 2)
    slots = [(M.SPK[1] - gr + 1.0 + k * 2.7, M.SPK[1] - gr + 2.2 + k * 2.7) for k in range(7)]
    edges = [yt + 2.0, hole[0]], [hole[1], slots[0][0]]
    rect(ax, yt + 2.0, hole[0], zpl, zf, "front")
    rect(ax, hole[1], slots[0][0], zpl, zf, "front")
    for k in range(len(slots) - 1):
        rect(ax, slots[k][1], slots[k + 1][0], zpl, zf, "front")
    rect(ax, slots[-1][1], yb - 2.0, zpl, zf, "front")
    rect(ax, yt + 2.0, yb - 2.0, M.Z_BACK_GRIP, -2.0, "back")
    # board
    rect(ax, b0, b1, 0, t, "pcb")
    # MCU (LQFP-64, 1.6 max) on the back: its block spans x 136-158, so the chip itself (12 x 12 with leads, centred
    # near x 147) sits BESIDE this x = 156.88 cut -> dashed, like the R button; the small parts of its block are on the cut
    ax.add_patch(Rectangle((43.5, -1.6), 10.0, 1.6, fc="none", ec=PAL["parts"][0], ls="--", lw=1.3, zorder=4))
    ax.add_patch(Rectangle((42.6, -1.45), 11.8, 0.2, fc="none", ec=PAL["parts"][0], ls="--", lw=0.8, zorder=4))
    for y0 in (39.2, 40.8, 55.5, 57.0):
        rect(ax, y0, y0 + 1.0, -0.8, 0, "parts", lw=0.3)
    # A switch (TS-1187A, 5.1 x 5.1: board.json SW6 courtyard y 59.38-64.47), cover at board top + 1.2, keystake to + 1.5
    rect(ax, 61.92 - 2.55, 61.92 + 2.55, t, t + 1.2, "parts")
    rect(ax, 60.92, 62.92, t + 1.2, t + 1.5, "parts")
    rect(ax, 61.92 - 4.7, 61.92 + 4.7, zpl, zf + 1.8, "keys")
    rect(ax, 61.92 - 5.9, 61.92 + 5.9, zpl - 0.8, zpl, "keys")
    rect(ax, 61.92 - 2.0, 61.92 + 2.0, t + 1.7 + 0.5, zpl - 0.8, "keys")
    rect(ax, 61.92 - 1.2, 61.92 + 1.2, t + 1.7, t + 1.7 + 0.5, "keys")
    rect(ax, 61.92 - 2.0, 61.92 + 2.0, -2.0, -0.05, "back")
    # speaker (shell-held in the board's notch), foam seal, back cradle fingers
    sh = math.sqrt(13.5 ** 2 - dx ** 2)
    s0, s1 = M.SPK[1] - sh, M.SPK[1] + sh
    rect(ax, s0, s1, zpl - 0.3 - 4.9, zpl - 0.3, "parts")
    for a, b in ((s0, s0 + 1.4), (s1 - 1.4, s1)):
        rect(ax, a, b, zpl - 0.3, zpl, "gasket", lw=0.3)
        rect(ax, a - 0.6 if a == s0 else b - 0.4, a + 0.4 if a == s0 else b + 0.6, -2.0, zpl - 0.3 - 4.9 + 1.0,
             "back")
    # R shoulder switch + cap: behind the cut (x 150.4-155.2), dashed
    ax.add_patch(Rectangle((36.11, t), 3.9, 3.35, fc="none", ec=PAL["parts"][0], ls="--", lw=1.2, zorder=4))
    ax.add_patch(Rectangle((yt - 0.6, t + 0.5), 3.6, 3.4, fc="none", ec=PAL["keys"][0], ls="--", lw=1.2, zorder=4))
    return dict(t=t, zf=zf, zpl=zpl, yt=yt, yb=yb, b0=b0, b1=b1, s0=s0, s1=s1, slots=slots)


def stack_column(ax, x0, w, layers_bottom_up, z0, label_x, gap=0.62, fs=12):
    """Draw a stack (list of (name, t, kind), bottom first) as rectangles from z0 up; labels at label_x."""
    z = z0
    mids = []
    for name, th, kind in layers_bottom_up:
        rect(ax, x0, x0 + w, z, z + th, kind, lw=0.6)
        mids.append((z + th / 2, name, th))
        z += th
    ys = [m for m, _, _ in mids]
    for _ in range(300):
        moved = False
        for i in range(1, len(ys)):
            if ys[i] - ys[i - 1] < gap:
                d = (gap - (ys[i] - ys[i - 1])) / 2
                ys[i - 1] -= d; ys[i] += d; moved = True
        if not moved:
            break
    for (m, name, th), y in zip(mids, ys):
        ax.plot([x0 + w, x0 + w + 0.5, label_x - 0.3], [m, y, y], color="#777777", lw=0.7, zorder=5)
        ax.text(label_x, y, f"{th:g}  {name}", va="center", fontsize=fs, zorder=6)
    return z


def fig02():
    fig = plt.figure(figsize=(24, 30.5))
    gs = fig.add_gridspec(5, 2, width_ratios=[1.0, 1.45], height_ratios=[4.1, 7.4, 5.9, 6.9, 0.7],
                          hspace=0.17, wspace=0.10, left=0.04, right=0.985, top=0.95, bottom=0.015)
    tt = M.totals("R")
    # ---------------- (a) full section
    ax = fig.add_subplot(gs[0, :])
    g = section_y71(ax, "R")
    setup_section_ax(ax, xlim=(15, 176), zlim=(-14.5, 12.5))
    ax.set_title("A.  Across the device through the screen centre (board y = 71), true scale", loc="left",
                 fontsize=17, weight="bold")
    axA = ax
    dim(ax, 53.5, M.Z_BACK_GRIP, g["zf"], f"grip {r1(tt['grip'])} mm", side="left")
    # label LEFT of the arrow, over the cell: on the right it covered J3 and the plate/J3 leaders
    dim(ax, 89.0, g["zb"], g["zf"], f"{r1(tt['bump'])} mm at the battery", side="left")
    fs = 12.5
    ax.text(80, g["zp0"] + 2.0, "display panel 4.0", color="white", fontsize=13, ha="center", va="center",
            weight="bold", zorder=30)
    ax.text(78, -5.6, "battery cell 8.5", fontsize=13, ha="center", va="center", weight="bold", zorder=30)
    callout(ax, (100, g["zf"] - 0.5), (100, 10.6), "glass cover lens 1.1 in a pocket of the front shell",
            fs=fs, ha="center")
    callout(ax, (64.4, g["zp1"] + 0.15), (58, 10.6), "0.3 foam gasket ring", fs=fs, ha="center")
    callout(ax, (24.5, g["t"] / 2), (19.0, -5.0), "PCB 1.2", fs=fs)
    callout(ax, (40.0, -1.0), (38.0, -7.8), "post under\nthe switch", fs=fs)
    callout(ax, (40.0, g["zf"] + 1.4), (27.0, 10.6), "D-pad", fs=fs, ha="center")
    callout(ax, (36.5, -1.0), (17.5, -12.8), "BT830 (dashed: 11 mm off this\ncut, drawn for its height)", fs=11)
    callout(ax, (133.3, g["zp0"] + 0.8), (141, 10.6), "FPC folded behind the panel", fs=fs)
    # right column, top to bottom in the order of the targets; every leader passes under J3 (z -2..0)
    callout(ax, (109, -1.0), (126, -3.0), "FPC connector J3 on the back", fs=fs)
    callout(ax, (94, -0.45), (126, -5.6), "0.5 steel battery plate", fs=fs)
    callout(ax, (104.1, -6.0), (126, -8.2), "rib: cell | connector bay", fs=fs)
    callout(ax, (95, -9.8), (126, -10.8), "0.85 free swell gap, then the 1.2 door", fs=fs)
    callout(ax, (150, 2.5), (160, -5.5), "speaker (dashed: 0.6 mm\noff this cut, in the notch)", fs=11,
            ha="center")
    callout(ax, (62.2, -6.0), (52, -12.8), "bay wall", fs=fs, ha="right")
    scalebar(ax, 160, -13.2, 10)
    # ---------------- (b) the two stacks, every layer
    sub = gs[1, 0].subgridspec(1, 4, width_ratios=[0.8, 2.0, 0.8, 1.7], wspace=0.0)
    names = {"gap": "gap", "battery plate 0.5 stainless": "steel battery plate",
             "stretch-release tape": "stretch-release tape", "cell 8.5 max": "battery cell (8.5 max)",
             "FREE swell gap (no foam)": "FREE swell gap (no foam)", "door": "battery door",
             "clearance over a no-parts zone": "clearance (no parts)",
             "panel's own tape, liner left on": "panel's tape (liner on)",
             "display panel": "display panel", "black foam gasket = air gap": "foam gasket = air gap",
             "glass cover lens": "glass cover lens"}
    R = M.OPTIONS["R"]
    scr = [(names[l["name"]], l["t"], l["kind"]) for l in reversed(R["bay"])]
    scr += [("PCB", 1.2, "pcb")]
    scr += [(names[l["name"]], l["t"], l["kind"]) for l in R["front"]]
    grip = [("back floor", 1.6, "back"), ("clearance", 0.3, "air"),
            ("back parts (BT830, MCU)", 1.7, "parts"), ("PCB", 1.2, "pcb"),
            ("room for keys +\nparts (<= 3.9)", 4.215, "air"), ("face plate", 1.6, "front")]
    for col, layers, zz, head in ((0, scr, M.z_back_bay("R"), "SCREEN +\nBATTERY"),
                                  (2, grip, M.Z_BACK_GRIP, "GRIP")):
        ax = fig.add_subplot(sub[0, col])
        stack_column(ax, 0, 3.0, layers, zz, 4.4, gap=0.95, fs=12.5)
        ax.annotate("", xy=(-0.7, M.z_front("R")), xytext=(-0.7, zz),
                    arrowprops=dict(arrowstyle="<|-|>", lw=1.6, color=INK, shrinkA=0, shrinkB=0))
        tot = sum(th for _, th, _ in layers)          # exact sum of the layers drawn (18.415 / 10.615)
        ax.text(1.5, zz - 0.5, f"total\n{tot:.3f} mm", ha="center", va="top", fontsize=15, weight="bold")
        ax.text(1.5, M.z_front("R") + 0.6, head, ha="center", va="bottom", fontsize=14, weight="bold")
        ax.axhline(0, color=PAL["pcb"][0], lw=0.8, ls=":")
        ax.set_xlim(-1.5, 4.0); ax.set_ylim(-15.0, 10.5); ax.set_aspect("equal")
        ax.set_xticks([]); ax.tick_params(labelsize=10)
        for s_ in ("top", "right", "bottom"):
            ax.spines[s_].set_visible(False)
        if col == 0:
            ax.set_ylabel("z (mm), 0 = board back face", fontsize=12)
            ax.set_title("B.  Every layer, front (top) to back, in mm", loc="left", fontsize=17, weight="bold",
                         x=-0.6)
        else:
            ax.set_yticks([]); ax.spines["left"].set_visible(False)
    # ---------------- (c) FPC end zoom
    ax = fig.add_subplot(gs[1, 1])
    g = section_y71(ax, "R")
    setup_section_ax(ax, xlim=(97.5, 141.5), zlim=(-12.4, 11.5))
    ax.set_xticks(range(100, 141, 5))

    def px_per_mm(a):                 # after the equal-aspect box is applied
        a.apply_aspect()
        p = a.transData.transform([(0.0, 0.0), (10.0, 0.0)])
        return (p[1][0] - p[0][0]) / 10.0
    zoom = px_per_mm(ax) / px_per_mm(axA)
    ax.set_title(f"C.  Zoom on the display-cable end (same cut, {zoom:.1f}x larger than A)", loc="left",
                 fontsize=17, weight="bold")
    fs = 12.5
    callout(ax, (133.4, g["zp0"] + 1.0), (135.0, 10.3), "factory 180 deg fold\n(bulge <= 1.0)", fs=fs, ha="center")
    callout(ax, (126, g["fpc_z"] - 0.5), (128.5, -4.4), "FPC's 1.0 mm part sits in a\nHOLE in the board "
            "(x 118.3-133.1)", fs=fs, ha="center")
    callout(ax, (118.6, 0.0), (113.5, 4.8), "S-bend through the hole's\nedge (thin single-layer part)", fs=fs,
            ha="center")
    callout(ax, (109.0, -1.3), (108.2, -4.8), "J3: 50-pin ZIF connector,\ntop contact, on the BACK",
            fs=fs, ha="center")
    callout(ax, (104.1, -6.5), (111.0, -8.3), "rib keeps the cell out of\nthe connector bay", fs=fs)
    callout(ax, (100.5, -0.45), (99.0, 1.8), "0.5 steel plate", fs=fs)
    callout(ax, (101.0, -9.8), (99.0, -11.6), "free swell gap + door", fs=fs)
    callout(ax, (121.3, -6.0), (124.0, -8.6), "bay wall", fs=fs)
    callout(ax, (135.0, g["zf"] - 1.7), (138.2, 3.5), "lens land\n(front shell)", fs=fs, ha="center")
    callout(ax, (110.0, g["t"] + 0.2), (106.0, 9.2), "0.3 clearance: no parts here", fs=fs, ha="center")
    ax.text(103.5, g["zp0"] + 2.0, "display panel", color="white", fontsize=13, ha="center", va="center",
            weight="bold", zorder=30)
    ax.text(101.0, -5.1, "cell", fontsize=13, ha="center", va="center", weight="bold", zorder=30)
    scalebar(ax, 129, -11.4, 5, "5 mm")
    # ---------------- (d) grip section
    ax = fig.add_subplot(gs[2, :])
    G = grip_section(ax)
    ax.set_xlim(31, 104); ax.set_ylim(-6.8, 12.6); ax.set_aspect("equal")
    ax.set_yticks(range(-4, 13, 4)); ax.tick_params(labelsize=10)
    for s_ in ("top", "right"):
        ax.spines[s_].set_visible(False)
    ax.set_xlabel("board y (mm): top edge (USB-C side) on the left, bottom edge on the right", fontsize=12)
    ax.set_title(f"D.  Through the right grip (board x = {GX}): A button, MCU, speaker, true scale", loc="left",
                 fontsize=17, weight="bold")
    dim(ax, 70.8, M.Z_BACK_GRIP, G["zf"], f"{r1(tt['grip'])} mm", side="right")
    fs = 13
    callout(ax, (48.5, -0.8), (48.5, -5.6), "MCU (STM32F411, 1.6 tall) on the BACK: dashed, it sits\n"
            "just beside this cut; the small parts of its block are on it", fs=fs, ha="center")
    callout(ax, (61.9, G["zf"] + 1.2), (55.0, 11.2), "A key cap", fs=fs, ha="center")
    callout(ax, (85.0, G["zpl"] - 2.6), (84.0, 11.2), "speaker 27 x 4.9, held by the shell, sealed with 0.3 foam",
            fs=fs, ha="center")
    callout(ax, (90.6, G["zf"] - 0.3), (101.0, 9.0), "grille", fs=fs, ha="center")
    callout(ax, (37.0, 4.3), (36.0, 9.4), "R button (dashed: behind the cut)", fs=fs)
    callout(ax, (69.6, 0.6), (77.0, -4.6), "board ends at the speaker notch", fs=fs)
    callout(ax, (61.9, -1.0), (62.0, -5.6), "post", fs=fs, ha="center")
    callout(ax, (40.0, -2.8), (34.0, -5.6), "floor", fs=fs, ha="center")
    scalebar(ax, 93.0, -5.3, 5, "5 mm")
    # ---------------- (e) key zoom
    ax = fig.add_subplot(gs[3, 0])
    G = grip_section(ax)
    ax.set_xlim(52.5, 72.0); ax.set_ylim(-4.6, 10.6); ax.set_aspect("equal")
    ax.tick_params(labelsize=10)
    for s_ in ("top", "right"):
        ax.spines[s_].set_visible(False)
    ax.set_title("E.  Zoom: a face button (A), true scale", loc="left", fontsize=17, weight="bold")
    fs = 12.5
    c = 61.92
    callout(ax, (c + 3.5, G["zf"] + 1.0), (66.6, 9.8), "cap 1.8 proud", fs=fs)
    callout(ax, (54.0, G["zpl"] + 0.8), (53.0, 9.8), "face plate 1.6", fs=fs)
    callout(ax, (c - 5.5, G["zpl"] - 0.4), (52.8, 3.6), "flange keeps\nthe cap in", fs=fs)
    callout(ax, (c + 1.0, 2.8), (66.0, 3.6), "0.2 rest gap, then\n0.2 travel (stop 0.3)", fs=fs)
    callout(ax, (c + 2.2, 1.9), (66.0, 1.7), "TS-1187A switch 1.5", fs=fs)
    callout(ax, (69.0, 0.6), (67.6, -0.9), "PCB 1.2", fs=fs)
    callout(ax, (c + 1.5, -1.0), (65.3, -2.6), "post, 4 dia, 0.05 under\nthe board: takes the press",
            fs=fs)
    callout(ax, (57.0, -2.8), (53.0, -4.1), "back floor 1.6", fs=fs)
    scalebar(ax, 69.0, 7.6, 2, "2 mm", h=0.25)
    # ---------------- notes: why each number
    axn = fig.add_subplot(gs[3, 1]); axn.axis("off")
    notes = [
        ("Why each number", None),
        ("Glass lens 1.1", "8.6x stiffer than 1.5 acrylic (STRUCTURAL_REVIEW.md #5): a press only pushes it onto "
                           "the panel at about 64 N, against about 12 N for acrylic (me_numbers.py). First prints: "
                           "1.5 acrylic + 0.5 gasket (+0.6 mm)."),
        ("Gasket 0.3", "IS the lens-to-panel air gap, held to about +-0.05; the panel hangs from it "
                       "(GBA: 'adhesive gasket that secures it to the shell', iFixit 2002)."),
        ("Clearance 0.3", "absorbs the panel's +-0.20 thickness tolerance (MI0283QT-11 P.5 note 11). Worst case the "
                          "panel rests flat on bare solder mask, never on a part."),
        ("PCB 1.2", "no panel glued to it any more, so it must be stiffer than 1.0; 1.2 mm is also JLC's "
                    "thinnest 6-layer board (1.2 / 1.6 / 2.0, jlcpcb.com/impedance), so 6 layers cost no thickness."),
        ("Steel plate 0.5", "carries the cell: sags 0.08 mm under a 20 N back press with all 4 edges supported "
                            "(me_numbers.py, Roark), at most 0.14 with its bottom edge free (arch_model.py), inside "
                            "its 0.2 gap. Screwed or snapped down; Kapton on the board side; grounded at one point."),
        ("Swell gap 0.85", "10 % of 8.5 mm; pouch cells swell 5-10 % (dlcpo.com guide). Left EMPTY: squeezing even "
                           "the softest 0.85-1.0 mm PORON by 25 % pushes 148 N into the board (PORON 4701-30 data)."),
        ("Door 1.2", "the deliberately weakest wall: a swelling cell bulges the door, not the screen."),
        ("Posts", "under all 8 face switches: a 1.2 board alone bends 0.47 mm under a hard press, more "
                  "than the 0.2 +-0.1 key travel (me_numbers.py)."),
    ]
    yy = 0.98
    import textwrap
    for head, body in notes:
        if body is None:
            axn.text(0.0, yy, head, fontsize=17, weight="bold", va="top", transform=axn.transAxes)
            yy -= 0.075
            continue
        axn.text(0.0, yy, head, fontsize=13.5, weight="bold", va="top", transform=axn.transAxes, color="#1b5e20")
        wrapped = textwrap.fill(body, 90)
        axn.text(0.20, yy, wrapped, fontsize=13, va="top", transform=axn.transAxes)
        yy -= 0.034 * (wrapped.count("\n") + 1) + 0.038
    # ---------------- legend
    axl = fig.add_subplot(gs[4, :]); axl.axis("off")
    axl.legend(handles=legend_handles([k for k, _ in LEG_ORDER if k != "foam"]), loc="center", ncol=8, fontsize=13,
               frameon=False, handlelength=2.2, handleheight=1.3, columnspacing=1.6)
    fig.suptitle("gb2 rev B, RECOMMENDED build: the screen hangs in the front shell with nothing under it; the cell "
                 "sits on its own plate behind the board", fontsize=20, weight="bold", x=0.04, ha="left", y=0.99)
    fig.text(0.04, 0.973, "All views true scale; z = 0 is the board's back face. Thicknesses from arch_model.py (ME "
             "study me_stacks.py, REVB_DESIGN.md §3.1, MI0283QT-11 P.5, PKCELL p.5); key heights from "
             "enclosure/params.py:115-127 and DESIGN.md:66-68.", fontsize=13, color=INK2)
    out = os.path.join(HERE, "02_recommended_section.png")
    fig.savefig(out, dpi=100)
    plt.close(fig)
    print("wrote", out)


# ================================================================================================ 03 exploded
def _off(ax, dz, items):
    for x0, x1, z0, z1, kind in items:
        rect(ax, x0, x1, z0 + dz, z1 + dz, kind)


def screw(ax, x, ztip, up=True, L=6.0):
    """M2 screw drawn pointing up (head at the bottom) or down."""
    if up:
        rect(ax, x - 1.0, x + 1.0, ztip - L, ztip, "screw", lw=0.3)
        rect(ax, x - 1.9, x + 1.9, ztip - L - 2.0, ztip - L, "screw", lw=0.3)
    else:
        rect(ax, x - 1.0, x + 1.0, ztip, ztip + L, "screw", lw=0.3)
        rect(ax, x - 1.9, x + 1.9, ztip + L, ztip + L + 2.0, "screw", lw=0.3)


def arrow(ax, x, z0, z1, color="#9e9e9e"):
    ax.annotate("", xy=(x, z1), xytext=(x, z0),
                arrowprops=dict(arrowstyle="-|>", lw=3.0, color=color, mutation_scale=22), zorder=1)


def fig03():
    t, zf = 1.2, M.z_front("R")
    zpl = zf - M.PLATE_T
    (xl, xr), = M.span_at(M.SIL, y=71.0)
    fig = plt.figure(figsize=(24, 15.5))
    gs = fig.add_gridspec(2, 2, width_ratios=[3.25, 1], height_ratios=[1, 0.09], wspace=0.03, hspace=0.02,
                          left=0.01, right=0.99, top=0.91, bottom=0.01)
    ax = fig.add_subplot(gs[0, 0])
    rows = {}
    # 1 lens: it sits in a pocket on the OUTSIDE of the face plate, so it goes in from the front, before the shell is
    # turned face-down (GBA reshell 191804 s20 lens, then s21-22 gasket and screen from inside; DESIGN.md §2 step 1)
    dz = 62.0
    _off(ax, dz, [(M.LENS[0], M.LENS[2], zf - 1.1, zf, "lens"), (M.LENS[0], M.POCKET[0], zf - 1.23, zf - 1.1, "tape"),
                  (M.POCKET[2], M.LENS[2], zf - 1.23, zf - 1.1, "tape")])
    rows[1] = (dz + zf - 0.5, "glass cover lens 1.1 + 0.13 PSA,\ninto its pocket from the FRONT")
    arrow(ax, 95, 67.5, 56.0)
    # 2 front shell, then turned face-down on the padded jig: everything below goes in from its inside
    dz = 48.05
    items = [(xl, xl + 2, t, zf, "front"), (xr - 2, xr, t, zf, "front")]
    holes = [(35.35, 44.55), (47.2, 51.8), (138.8, 149.1), (151.8, 162.0)]
    segs = [(xl + 2, M.LENS[0])] + [(M.LENS[2], xr - 2)]
    cuts = []
    for a, b in segs:
        cur = a
        for h0, h1 in holes:
            if a < h0 < b:
                cuts.append((cur, h0)); cur = h1
        cuts.append((cur, b))
    items += [(a, b, zpl, zf, "front") for a, b in cuts]
    items += [(M.LENS[0], M.POCKET[0], zpl - 0.63, zpl + 0.37, "front"),
              (M.POCKET[2], M.LENS[2], zpl - 0.63, zpl + 0.37, "front")]
    for x in (29.24, 57.39, 55.0, 126.5, 133.19, 140.5, 165.5):
        items.append((x - 3.0, x + 3.0, t, zpl, "front"))
    _off(ax, dz, items)
    ax.text(72.0, dz + 10.0, "front shell (side view;\nscrew bosses are behind)", ha="center",
            va="center", fontsize=12, color=PAL["front"][2], style="italic")
    rows[2] = (dz + zf - 0.8, "front shell (plate, walls, bosses),\nthen face-down on a padded jig")
    # 3 gasket
    dz = 41.23
    _off(ax, dz, [(63.55, 65.35, 5.615, 5.915, "gasket"), (124.5, 126.3, 5.615, 5.915, "gasket"),
                  (130.4, 132.2, 5.615, 5.915, "gasket")])
    rows[3] = (dz + 5.76, "0.3 black foam gasket ring,\nonto the lens mask")
    arrow(ax, 95, 47.6, 54.6)
    # 4 panel (tail folded, held up)
    dz = 30.225
    _off(ax, dz, [(M.PANEL[0], M.PANEL[2], 1.615, 5.615, "display"),
                  (M.PANEL[0], M.PANEL[0] + 1.5, 1.5, 1.615, "tape")])
    fold = [(M.PANEL[2] - 0.6, 3.0), (M.PANEL[2] + 0.55, 2.85), (M.PANEL[2] + 1.0, 2.2), (M.PANEL[2] + 0.55, 1.415),
            (120.2, 1.415), (118.6, 0.6), (116.5, -1.0), (110.0, -1.6)]
    xs, zs = zip(*fold)
    ax.plot(xs, [z + dz for z in zs], color=PAL["fpc"][0], lw=3.0, zorder=12, solid_capstyle="round")
    rect(ax, M.PANEL[2] - 11.9, M.PANEL[2] - 1.5, dz + 0.515, dz + 1.315, "fpc", lw=0.3)
    ax.text(80, dz + 3.6, "display panel", color="white", ha="center", va="center", fontsize=13, weight="bold")
    callout(ax, (113.0, dz - 1.4), (100.0, dz - 3.2), "tail (plugs into J3 at step 8)", fs=12, ha="right")
    rows[4] = (dz + 3.6, "display panel, into the pocket\n(tail folded, kept free)")
    arrow(ax, 82, 36.4, 46.4)
    # 5 caps + speaker (side view, projected)
    dz = 16.275
    caps = [(36.65, 43.25, 39.95, "D-pad"), (47.29, 51.69, 49.47, "St/Sel"), (139.24, 148.64, 143.94, "B"),
            (152.18, 161.58, 156.88, "A")]
    for a, b, c, _ in caps:
        _off(ax, dz, [(a, b, zf, zf + 1.8), ][:0])
        rect(ax, a, b, dz + zf - 1.6, dz + zf + 1.8, "keys")
        rect(ax, a - 1.2, b + 1.2, dz + zpl - 0.8, dz + zpl, "keys")
        rect(ax, c - 1.3, c + 1.3, dz + t + 1.7, dz + zpl - 0.8, "keys")
    rect(ax, M.SPK[0] - 13.5, M.SPK[0] + 13.5, dz - 9.0 + 0.215, dz - 9.0 + 5.115, "parts")
    ax.text(M.SPK[0], dz - 9.0 + 2.67, "speaker", color="white", ha="center", va="center", fontsize=12,
            weight="bold")
    rows[5] = (dz + 3.5, "keycaps, D-pad, edge caps, slider,\nspeaker, light pipe into the shell")
    for x in (39.95, 49.47):
        arrow(ax, x, 25.6, 48.6)
    # 6 PCB with parts (projected)
    dz = -4.65
    (bx0, bx1) = M.board_revb().bounds[0], M.board_revb().bounds[2]
    rect(ax, bx0, bx1, dz, dz + t, "pcb")
    ax.add_patch(Rectangle((M.FPC_OPEN[0], dz), M.FPC_OPEN[2] - M.FPC_OPEN[0], t, fc="white", ec=PAL["pcb"][2],
                           lw=1.0, ls="--", zorder=3))
    for p in M.BJ["parts"]:
        if p["side"] != "top" or p["height"] < 0.9 or p["ref"] in M.MOVED_FRONT:
            continue
        c = p["courtyard"]; xs_ = [q[0] for q in c]
        rect(ax, min(xs_), max(xs_), dz + t, dz + t + min(p["height"], 3.4), "parts", lw=0.3, alpha=0.85)
    hb = {"MCU\nSTM32F411": 1.6, "FPGA\niCE40": 1.0, "LDO": 1.25, "J8": 5.5, "BL": 1.0, "J3": 2.0, "C": 1.0,
          "SWD": 0.1, "TEST PADS (door open)": 0.1, "RF": 1.7}
    for lab, b, cat, _ in M.BACK_BLOCKS:
        kind = "conn" if lab == "J3" else "parts"
        rect(ax, b[0], b[2], dz - hb[lab], dz, kind, lw=0.3, alpha=0.9)
    ax.text(70, dz + 0.6, "PCB 1.2", color="white", ha="center", va="center", fontsize=12, weight="bold", zorder=30)
    rows[6] = (dz + 0.6, "PCB onto the bosses; thread the\npanel tail through the board's hole")
    arrow(ax, 104, -1.0, 15.5)
    # 7 board screws B1/B2, 8 J3 latched
    for x in (55.0, 126.5):
        screw(ax, x, dz - 1.0 - 0.0, up=True, L=4.0)
    rows[7] = (dz - 5.6, "screws B1 + B2 (board to the\nfront shell)")
    callout(ax, (109.0, dz - 1.0), (60.0, dz - 7.6), "8: tail into J3, latch, test from USB-C",
            fs=12.5, ha="left")                    # clear of screw B1 (x 55) and B2 (x 126.5)
    # 10 plate, 9 back shell
    dz = -17.98
    rect(ax, M.PLATE_R[0], M.PLATE_R[2], dz - 0.7, dz - 0.2, "plate")
    rows[10] = (dz - 0.45, "0.5 steel plate (Kapton side up)\nonto the bay ledges")
    arrow(ax, 84, -19.0, -23.8)
    dz = -25.42
    bx0_, bx1_ = M.BAY[0], M.BAY[2]
    items = [(xl, xl + 2, -3.6, t, "back"), (xr - 2, xr, -3.6, t, "back"),
             (xl + 2, bx0_ - 1.2, -3.6, -2.0, "back"), (bx1_ + 1.2, xr - 2, -3.6, -2.0, "back"),
             (bx0_ - 1.2, bx0_, -11.4, -2.0, "back"), (bx1_, bx1_ + 1.2, -11.4, -2.0, "back"),
             (bx0_, bx0_ + 1.1, -1.6, -0.7, "back"), (M.RIB_R[0], M.RIB_R[2], -10.2, -0.7, "back")]
    for n, (x, y) in M.KEY_POSTS.items():
        items.append((x - 2.0, x + 2.0, -2.0, -0.05, "back"))
    for x in (29.24, 57.39, 133.19, 140.5, 165.5):
        items.append((x - 3.0, x + 3.0, -2.0, 0.0, "back"))
    _off(ax, dz, items)
    ax.text(83.0, dz - 6.0, "battery bay: open at the back,\nclosed by the door (14)", ha="center", va="center",
            fontsize=12, color=PAL["back"][2], style="italic")
    callout(ax, (104.1, dz - 6.0), (110.0, dz - 9.5), "rib", fs=12)
    rows[9] = (dz - 2.2, "back shell (bay, rib, posts under\nevery face switch)")
    arrow(ax, 26.0, -23.6, -12.5)
    arrow(ax, 163.0, -23.6, -12.5)
    # 11 shell screws
    for x in (29.24, 57.39, 133.19, 140.5, 165.5):
        screw(ax, x, dz - 3.6 - 1.0, up=True, L=5.0)
    rows[11] = (dz - 8.0, "close: back shell onto the front\nmodule, screws S1-S5")
    # 12 cell, 13 J8, 14 door
    dz = -47.275
    rect(ax, M.CELL_R[0], M.CELL_R[2], dz - 9.35, dz - 0.85, "cell")
    rect(ax, M.CELL_R[0], M.CELL_R[2], dz - 0.85, dz - 0.7, "tape", lw=0.3)
    ax.text(84, dz - 5.1, "battery cell", ha="center", va="center", fontsize=13,
            weight="bold")
    ax.plot([103.5, 106.5, 108.5], [dz - 2.0, dz - 2.0, dz + 3.0], color="#c62828", lw=2.0)
    rect(ax, 106.2, 111.6, dz + 3.0, dz + 5.0, "parts", lw=0.4)
    rows[12] = (dz - 5.1, "cell (leads at its top end) in through\nthe door opening, taped to the plate")
    callout(ax, (110.0, dz + 4.2), (122.0, dz + 6.5), "13: plug into J8", fs=12.5)
    arrow(ax, 70.0, -47.6, -38.0)
    dz = -51.615
    rect(ax, M.BAY[0], M.BAY[2], dz - 11.4, dz - 10.2, "door")
    for x in (M.DOOR_SCREWS["D1"][0], M.DOOR_SCREWS["D2"][0]):      # true x, as in 05
        screw(ax, x, dz - 11.4 - 0.6, up=True, L=2.5)
    callout(ax, (M.DOOR_SCREWS["D2"][0] + 1.0, dz - 13.5), (128.0, dz - 13.5), "D1, D2", fs=12.5)
    rows[14] = (dz - 10.8, "battery door: hooks at the top +\nscrews D1, D2")
    arrow(ax, 92.0, -61.5, -57.2)
    # numbered tags
    for n, (z, txt) in rows.items():
        ax.add_patch(Circle((14.0, z), 2.3, fc="#1b5e20" if n <= 8 else ("#6d4c41" if n <= 10 else "#37474f"),
                            ec="none", zorder=40))
        ax.text(14.0, z, str(n), color="white", ha="center", va="center", fontsize=14, weight="bold", zorder=41)
        ax.text(10.6, z, txt, ha="right", va="center", fontsize=12.5, zorder=41)
    # module brackets
    for z0, z1, lab, col in ((rows[6][0] - 9.0, rows[1][0] + 2.5, "FRONT MODULE\n(steps 1-8, built\nface-down)", "#1b5e20"),
                             (rows[9][0] - 3.5, rows[10][0] + 2.0, "BACK MODULE\n(9-10)", "#6d4c41"),
                             (rows[14][0] - 2.5, rows[11][0] + 3.0, "CLOSE-UP\n(11-14)", "#37474f")):
        ax.plot([173.5, 175, 175, 173.5], [z0, z0, z1, z1], color=col, lw=2.5, clip_on=False)
        ax.text(176.5, (z0 + z1) / 2, lab, color=col, fontsize=13.5, weight="bold", va="center", ha="left")
    ax.set_xlim(-26, 196); ax.set_ylim(-70, 74); ax.set_aspect("equal")      # -70: the door screw heads end at -68.1
    ax.axis("off")
    scalebar(ax, 150, -63.0, 10)
    ax.text(-25, 72.5, "Front of the device at the top. Side view through the screen centre (parts beside it drawn "
            "too); true scale in x and in every part's thickness, gaps opened up.", fontsize=12.5, color=INK2)
    # right column: steps + service
    axn = fig.add_subplot(gs[0, 1]); axn.axis("off")
    txt = [
        ("Assembly order", "h"),
        ("FRONT MODULE (built face-down on a padded jig)", "g"),
        ("1  lens into its pocket, from the front", ""), ("   (0.13 PSA)", ""),
        ("2  shell face-down on the jig", ""),
        ("3  gasket ring onto the lens mask", ""), ("4  panel into the pocket, tail free", ""),
        ("5  caps, D-pad, edge caps, slider,", ""), ("   speaker, light pipe", ""),
        ("6  board on, tail through the hole", ""), ("7  screws B1, B2", ""),
        ("8  tail into J3, latch.  POWER TEST", ""), ("   from USB-C: screen + keys work", ""),
        ("BACK MODULE", "b"), ("9  back shell", ""), ("10 steel plate onto the ledges", ""),
        ("CLOSE-UP", "c"), ("11 shells together, S1-S5", ""), ("12 cell onto the plate (from the back)", ""),
        ("13 plug the cell into J8", ""), ("14 door on (hooks + D1, D2)", ""),
        ("", ""),
        ("Service later", "h"),
        ("Battery swap: open the door, unplug,", ""), ("   pull the stretch-release tab.", ""),
        ("Reflash: USB-C (DFU); no opening.", ""),
        ("Reseat the screen cable: door only.", ""),
        ("Board out: door, unplug J8, S1-S5,", ""), ("   open J3, B1-B2.  Cell stays put.", ""),
        ("Screen swap: board out, warm and", ""), ("   peel the panel off its gasket;", ""),
        ("   the lens stays in the shell.", ""),
    ]
    yy = 0.985
    colmap = {"g": "#1b5e20", "b": "#6d4c41", "c": "#37474f"}
    for line, k in txt:
        if k == "h":
            axn.text(0.0, yy, line, fontsize=18, weight="bold", va="top", transform=axn.transAxes); yy -= 0.042
        elif k in colmap:
            axn.text(0.0, yy, line, fontsize=13, weight="bold", va="top", transform=axn.transAxes,
                     color=colmap[k]); yy -= 0.032
        else:
            axn.text(0.0, yy, line, fontsize=13, va="top", transform=axn.transAxes); yy -= 0.029
    axl = fig.add_subplot(gs[1, :]); axl.axis("off")
    keys = ["lens", "front", "display", "gasket", "tape", "fpc", "conn", "pcb", "parts", "keys", "plate", "cell",
            "back", "door"]
    hs = legend_handles(keys) + [Patch(facecolor=PAL["screw"][0], label="screws")]
    axl.legend(handles=hs, loc="center", ncol=8, fontsize=12.5, frameon=False, handlelength=2.0,
               handleheight=1.2, columnspacing=1.2)
    fig.suptitle("gb2 rev B, recommended build: exploded side view, numbered in assembly order", fontsize=21,
                 weight="bold", x=0.01, ha="left", y=0.985)
    fig.text(0.01, 0.945, "Order and service paths from the ME study (me_assembly_R.png) and the GBA reshell order "
             "(iFixit guide 191804, steps 20-35: lens, screen into the front shell, caps and pads, board, flex, back "
             "shell, battery).", fontsize=12.5, color=INK2)
    out = os.path.join(HERE, "03_exploded.png")
    fig.savefig(out, dpi=100)
    plt.close(fig)
    print("wrote", out)


# ================================================================================================ 04 / 05 floor plans
def _poly(ax, geom, **kw):
    gs_ = getattr(geom, "geoms", [geom])
    for g in gs_:
        ax.add_patch(MPoly(list(g.exterior.coords), closed=True, **kw))
        for hole in g.interiors:
            ax.add_patch(MPoly(list(hole.coords), closed=True, fc="white", ec=kw.get("ec", "k"),
                               lw=kw.get("lw", 1), zorder=kw.get("zorder", 1) + 0.1))


def _box(ax, b, **kw):
    ax.add_patch(Rectangle((b[0], b[1]), b[2] - b[0], b[3] - b[1], **kw))


def plan_base(ax, side):
    _poly(ax, M.SIL, fc="#f4f4f2", ec="#555555", lw=1.6, zorder=0)
    board = M.board_revb()
    _poly(ax, board, fc="#e8f2e8", ec=PAL["pcb"][0], lw=2.0, zorder=1)
    _box(ax, M.FPC_OPEN, fc="white", ec=PAL["pcb"][0], lw=2.0, ls="-", zorder=2)
    ax.text((M.FPC_OPEN[0] + M.FPC_OPEN[2]) / 2, 71, "HOLE through the board\nfor the display cable",
            rotation=90, ha="center", va="center", fontsize=12, weight="bold", color=PAL["pcb"][2], zorder=30)
    # screws
    for n, (x, y) in M.SHELL_SCREWS.items():
        ax.add_patch(Circle((x, y), 3.0, fc="#ffebee", ec="#c62828", lw=1.4, zorder=20))
        ax.add_patch(Circle((x, y), 1.1, fc="#c62828", ec="none", zorder=21))
        dx, dy, ha = {"S1": (-3.4, -2.2, "right"), "S5": (-3.4, -3.2, "right")}.get(n, (3.4, -2.2, "left"))
        ax.text(x + dx, y + dy, n, fontsize=12, weight="bold", color="#c62828", zorder=22, ha=ha)
    for n, (x, y) in M.BOARD_SCREWS.items():
        ax.add_patch(Circle((x, y), 3.0, fc="#e0f2f1", ec="#00796b", lw=1.4, zorder=20))
        ax.add_patch(Circle((x, y), 1.1, fc="#00796b", ec="none", zorder=21))
        ax.text(x + 3.4, y - 2.2, n, fontsize=12, weight="bold", color="#00796b", zorder=22)
    # antenna keep-out (all layers) + 20 / 30 mm lines
    ko = sbox(*M.ANT_KO)
    _box(ax, M.ANT_KO, fc="#b2dfdb", ec="#00897b", lw=1.5, hatch="xx", zorder=15, alpha=0.9)
    for d, ls, lab in ((20, "--", "20 mm: metal closer than this\n'significantly degrades' the antenna"),
                       (30, ":", "30 mm: datasheet 'safe' distance")):
        ring = ko.buffer(d).exterior
        xs, ys = ring.xy
        ax.plot(xs, ys, color="#00897b", lw=1.6, ls=ls, zorder=16)
    ax.text(M.ANT_KO[0] + 0.3, M.ANT_KO[1] - 0.6, "antenna: no copper", fontsize=11, color="#00695c",
            weight="bold", zorder=30, va="bottom")
    ax.text(M.ANT_KO[2] + 20.6, 52.0, "20 mm", fontsize=12, color="#00695c", weight="bold", zorder=30,
            rotation=90, va="center")
    # left of the dotted line (x ~63.9 at y 60): to its right the label sat on the gasket ring (04) / the plate (05)
    ax.text(M.ANT_KO[2] + 30.0 - 1.4, 60.0, "30 mm", fontsize=12, color="#00695c", weight="bold", zorder=30,
            rotation=90, va="center", ha="right")
    # section lines (fig 02)
    ax.plot([16, 174], [71, 71], color="#d32f2f", lw=1.3, ls=(0, (8, 3, 2, 3)), zorder=40)
    ax.text(16.5, 70.3, "A", fontsize=14, weight="bold", color="#d32f2f", zorder=41)
    ax.text(172.0, 70.3, "A", fontsize=14, weight="bold", color="#d32f2f", zorder=41)
    ax.plot([M.GX if hasattr(M, "GX") else GX] * 2, [29, 113], color="#d32f2f", lw=1.3, ls=(0, (8, 3, 2, 3)),
            zorder=40)
    ax.text(GX + 0.6, 30.5, "D", fontsize=14, weight="bold", color="#d32f2f", zorder=41)
    ax.text(GX + 0.6, 112.5, "D", fontsize=14, weight="bold", color="#d32f2f", zorder=41)
    ax.set_xlim(15, 175); ax.set_ylim(123, 23); ax.set_aspect("equal")
    ax.tick_params(labelsize=10)
    ax.set_xticks(range(20, 175, 10)); ax.set_yticks(range(30, 115, 10))
    ax.grid(color="#eeeeee", lw=0.6, zorder=-1)
    scalebar(ax, 18, 119.5, 10, h=0.8)


def keys_front(ax, alpha=1.0, outline=False):
    kw = dict(fc=PAL["keys"][0] if not outline else "none", ec=PAL["keys"][2], lw=1.4, alpha=alpha, zorder=18)
    cx, cy = 39.95, 64.09
    a, w = 9.4, 3.3
    cross = [(cx - w, cy - a), (cx + w, cy - a), (cx + w, cy - w), (cx + a, cy - w), (cx + a, cy + w), (cx + w, cy + w),
             (cx + w, cy + a), (cx - w, cy + a), (cx - w, cy + w), (cx - a, cy + w), (cx - a, cy - w), (cx - w, cy - w)]
    ax.add_patch(MPoly(cross, closed=True, **kw))
    for ref, d in (("SW6", 9.4), ("SW7", 9.4), ("SW8", 4.0), ("SW9", 4.0)):
        p = M.part(ref)
        ax.add_patch(Circle((p["x"], p["y"]), d / 2, **kw))


def fig04():
    fig = plt.figure(figsize=(24, 15.8))
    ax = fig.add_axes([0.03, 0.13, 0.94, 0.78])
    plan_base(ax, "front")
    # functional zones
    for lab, b, cat in M.FRONT_ZONES:
        _box(ax, b, fc=CAT_COL[cat], ec=CAT_COL[cat], alpha=0.16, lw=1.5, zorder=3)
    # rev A front parts that stay
    for p in M.BJ["parts"]:
        if p["side"] != "top" or p["ref"] in M.MOVED_FRONT:
            continue
        c = p["courtyard"]
        ax.add_patch(MPoly(c, closed=True, fc="#cfd3dc", ec="#7a8190", lw=0.6, zorder=6))
    for lab, b, cat in M.FRONT_BLOCKS:
        _box(ax, b, fc=CAT_COL[cat], ec="#333333", alpha=0.9, lw=1.2, zorder=7)
        ax.text((b[0] + b[2]) / 2, (b[1] + b[3]) / 2, lab, ha="center", va="center", fontsize=11, weight="bold",
                zorder=30)
    # lens, panel pocket keep-out, panel, gasket, active area, fold
    _box(ax, M.LENS, fc="none", ec=PAL["lens"][2], lw=2.0, ls="--", zorder=12)
    ax.text(M.LENS[0] + 11.5, M.LENS[3] + 0.4, "glass lens 82.2 x 56 (its land ring: parts <= 3.2 below it)",
            fontsize=12, color=PAL["lens"][2], va="top", zorder=30, weight="bold")
    _box(ax, M.POCKET, fc="#2b2f3a", ec="#000000", alpha=0.10, lw=0, zorder=8)
    _box(ax, M.POCKET, fc="none", ec="#000000", lw=0, hatch="///", alpha=0.35, zorder=8)
    _box(ax, M.PANEL, fc="none", ec=PAL["display"][0], lw=2.4, zorder=13)
    g = 1.8
    ring = sbox(M.PANEL[0] + 0.3, M.PANEL[1] + 0.3, M.PANEL[2] - 8.8, M.PANEL[3] - 0.3).difference(
        sbox(M.PANEL[0] + 0.3 + g, M.PANEL[1] + 0.3 + g, M.PANEL[2], M.PANEL[3] - 0.3 - g))
    _poly(ax, ring, fc=PAL["gasket"][0], ec="none", alpha=0.85, zorder=14)
    for y0 in (M.PANEL[1] + 0.3, M.PANEL[3] - 0.3 - g):
        _box(ax, (M.PANEL[2] - 7.5, y0, M.PANEL[2] - 1.0, y0 + g), fc=PAL["gasket"][0], ec="none", alpha=0.85,
             zorder=14)
    _box(ax, M.AA, fc="#dfe9f5", ec="#5b7ba6", lw=1.2, alpha=0.9, zorder=11)
    _box(ax, M.FOLD, fc=PAL["fpc"][0], ec=PAL["fpc"][2], lw=0.8, zorder=14)
    ax.text(95.25, 63.0, "SCREEN AREA: NO PARTS on this side\n(only masked copper: the display wiring + ground)",
            ha="center", va="center", fontsize=15, weight="bold", zorder=30, color="#1a237e")
    ax.text(95.25, 75.5, "panel 69.3 x 50.2 hangs in the front shell, 0.3 mm above the board\n"
            "active area 57.6 x 43.2, centred on (95.25, 71)", ha="center", va="center", fontsize=12.5, zorder=30)
    callout(ax, (64.4, 85.0), (72.0, 117.5), "foam gasket ring (3 sides + 2 pads; none over the driver chip)",
            fs=12)
    callout(ax, (133.0, 52.0), (139.0, 46.5), "FPC fold", fs=12)
    # zone labels
    zl = {"power": (61.2, 33.3), "hmi": (31.0, 121.8), "audio": (172.5, 121.8), "core": (113.6, 33.6)}
    for lab, b, cat in M.FRONT_ZONES:
        x, y = zl[cat]
        ax.text(x, y, lab, fontsize=11.5, weight="bold", color="#222222", zorder=31,
                va="bottom" if cat in ("hmi", "audio") else "top", ha="right" if cat == "audio" else "left",
                bbox=dict(fc="white", ec=CAT_COL[cat], lw=1.2, alpha=0.92, boxstyle="round,pad=0.25"))
    # keys + other face items
    keys_front(ax)
    for ref, lab, dx, dy in (("SW6", "A", 0, 0), ("SW7", "B", 0, 0), ("SW8", "St", 0, 0), ("SW9", "Se", 0, 0)):
        p = M.part(ref)
        ax.text(p["x"] + dx, p["y"] + dy, lab, ha="center", va="center", fontsize=12 if len(lab) == 1 else 9,
                weight="bold", color="white", zorder=45)          # above the D-D section line through A
    ax.text(39.95, 64.09, "D-pad", ha="center", va="center", fontsize=11, weight="bold", color="white", zorder=31)
    for ref, lab in (("SW12", "L"), ("SW13", "R"), ("SW10", "Vol+"), ("SW11", "Vol-"), ("SW1", "power slide")):
        b = M.bbox(ref)
        ax.text(b[2] + 0.6 if ref in ("SW10", "SW11") else (b[0] + b[2]) / 2,
                (b[1] + b[3]) / 2 if ref in ("SW10", "SW11") else b[3] + 1.2, lab, fontsize=11.5,
                weight="bold", color=PAL["keys"][2], zorder=31,
                ha="left" if ref in ("SW10", "SW11") else "center", va="center" if ref in ("SW10", "SW11") else "top")
    ax.add_patch(Circle(M.SPK[:2], M.SPK[2] / 2, fc="#e8eaf6", ec=PAL["parts"][0], lw=1.6, ls="--", zorder=9))
    ax.text(M.SPK[0] - 2.4, M.SPK[1] + 6.0, "speaker 27 mm\n(held by the shell,\nin the board's notch)",
            ha="center", va="center", fontsize=11.5, zorder=30)      # shifted left, off the D-D line at x 156.88
    ax.add_patch(Circle(M.LRA[:2], M.LRA[2] / 2, fc="#e8eaf6", ec=PAL["parts"][0], lw=1.4, zorder=9))
    ax.text(M.LRA[0], M.LRA[1], "LRA", ha="center", va="center", fontsize=10, weight="bold", zorder=30)
    b = M.bbox("J7")
    callout(ax, ((b[0] + b[2]) / 2, b[1] + 1.0), (100.5, 26.0), "USB-C", fs=12)
    ax.add_patch(Circle((39.95, 64.09), 5.3, fc="none", ec="#6d4c41", lw=1.0, ls=":", zorder=19))
    ax.set_title("FRONT of the board (viewing side), true scale, KiCad mm (+y down)", loc="left", fontsize=16,
                 weight="bold")
    # legend
    hs = [Patch(fc=CAT_COL[c], alpha=0.35, ec=CAT_COL[c], label=n) for c, n in
          (("power", "power zone"), ("hmi", "left controls zone"), ("audio", "audio + right keys"),
           ("core", "flash + motion sensor"))]
    hs += [Patch(fc="#cfd3dc", ec="#7a8190", label="rev A parts that stay"),
           Patch(fc="#2b2f3a", alpha=0.2, hatch="///", label="no parts (under the panel)"),
           Patch(fc=PAL["gasket"][0], label="gasket ring"), Patch(fc=PAL["fpc"][0], label="FPC fold"),
           Patch(fc=PAL["keys"][0], ec=PAL["keys"][2], label="keys / D-pad"),
           Line2D([], [], color=PAL["lens"][2], ls="--", lw=2, label="cover lens"),
           Patch(fc="#b2dfdb", hatch="xx", ec="#00897b", label="antenna keep-out"),
           Line2D([], [], color="#00897b", ls="--", lw=1.6, label="20 / 30 mm from the keep-out"),
           Line2D([], [], marker="o", color="#c62828", ls="none", ms=11, mfc="#ffebee", label="shell screw S1-S5"),
           Line2D([], [], marker="o", color="#00796b", ls="none", ms=11, mfc="#e0f2f1",
                  label="board screw B1, B2"),
           Line2D([], [], color="#d32f2f", ls=(0, (8, 3, 2, 3)), lw=1.3, label="section lines (fig 02)")]
    fig.legend(handles=hs, loc="lower center", ncol=8, fontsize=12.5, frameon=False, bbox_to_anchor=(0.5, 0.015),
               handlelength=2.2, columnspacing=1.4)
    fig.suptitle("gb2 rev B, recommended layout: FRONT of the board. Everything moves out from under the screen.",
                 fontsize=20, weight="bold", x=0.03, ha="left", y=0.985)
    fig.text(0.03, 0.945, "Zones and blocks: EE floor plan variant T (ee_plan.json), adapted to the recommended build; "
             "panel, lens, opening: REVB_DESIGN.md §3.2/§3.6, ME study; antenna rule: EZ-DS-BT830 v3.0 §12.1.1 p.27; "
             "parts: board.json. Collision check: arch_model.py (0 conflicts).", fontsize=12.5, color=INK2)
    out = os.path.join(HERE, "04_floorplan_front.png")
    fig.savefig(out, dpi=100)
    plt.close(fig)
    print("wrote", out)


def fig05():
    fig = plt.figure(figsize=(24, 15.8))
    ax = fig.add_axes([0.03, 0.13, 0.94, 0.78])
    plan_base(ax, "back")
    # bay / door, plate, rib, cell, leads
    # the 1.2 mm bay walls as a band, the door opening (= BAY, 57.9 x 71) as the dashed line inside it
    x0, y0, x1, y1 = M.BAY
    for wb in ((x0 - 1.2, y0 - 1.2, x1 + 1.2, y0), (x0 - 1.2, y1, x1 + 1.2, y1 + 1.2),      # four strips: a polygon
               (x0 - 1.2, y0, x0, y1), (x1, y0, x1 + 1.2, y1)):                         # hole would paint white
        _box(ax, wb, fc=PAL["back"][0], ec="none", alpha=0.55, zorder=1.5)     # under the FPC hole (zorder 2)
    _box(ax, M.BAY, fc="none", ec=PAL["door"][2], lw=2.4, ls="--", zorder=17)
    ax.text(M.BAY[0] - 0.5, M.BAY[3] + 2.2, "battery bay = door opening 58 x 71, inside 1.2 mm walls (back shell)",
            fontsize=12.5, color=PAL["door"][2], weight="bold", va="top", zorder=30)
    _box(ax, M.PLATE_R, fc=PAL["plate"][0], ec=PAL["plate"][2], hatch="////", alpha=0.35, lw=1.4, zorder=9)
    # rib, with a notch at its top end where the cell leads pass to J8 (arch_model LEAD_PATH, y 37.4-39.9)
    _box(ax, (M.RIB_R[0], M.LEAD_PATH[3] + 0.1, M.RIB_R[2], M.RIB_R[3]), fc=PAL["back"][0], ec=PAL["back"][2],
         lw=1.2, zorder=11)
    _box(ax, M.CELL_R, fc=PAL["cell"][0], ec=PAL["cell"][2], alpha=0.75, lw=1.6, zorder=10)
    ax.text(84.25, 66.0, "BATTERY CELL\n38.5 x 62 x 8.5\non a 0.5 steel plate\n(no parts under it)",
            ha="center", va="center", fontsize=14, weight="bold", zorder=30)
    ax.text(84.25, 43.0, "PCM end + leads", ha="center", va="center", fontsize=11.5, zorder=30)
    # leads: up out of the PCM end, along the 3 mm pocket, through the rib notch, into J8 (x 104.9-112.9)
    ax.plot([84.25, 84.25, 105.8], [41.0, 38.6, 38.6], color="#c62828", lw=2.4, zorder=12)
    ax.text(70.0, 98.0, "plate on ledges in the bay walls,\n0.2 under the board", fontsize=11, zorder=30)
    callout(ax, (104.1, 75.0), (90.0, 107.7), "rib: cell | connector bay", fs=11.5) if False else None
    # blocks
    for lab, b, cat, note in M.BACK_BLOCKS:
        if cat == "rf":
            ax.add_patch(Rectangle((b[0], b[1]), b[2] - b[0], b[3] - b[1], fc=CAT_COL["rf"], ec="#00695c",
                                   alpha=0.55, lw=1.4, zorder=14))
            # outline again above the keep-out hatch: the module's antenna end lies under the keep-out
            ax.add_patch(Rectangle((b[0], b[1]), b[2] - b[0], b[3] - b[1], fc="none", ec="#00695c", lw=1.4,
                                   ls="--", zorder=16))
            ax.text((b[0] + b[2]) / 2, b[3] - 3.0, "BT830\nU12", ha="center", va="center", fontsize=12,
                    weight="bold", zorder=30)
            continue
        kind_ec = "#7f2208" if lab == "J3" else "#333333"
        fc = PAL["conn"][0] if lab == "J3" else CAT_COL[cat]
        _box(ax, b, fc=fc, ec=kind_ec, alpha=0.9, lw=1.3, zorder=13)
        if lab in ("C",):
            continue
        ax.text((b[0] + b[2]) / 2, (b[1] + b[3]) / 2, {"TEST PADS (door open)": "test pads", "BL": "BL"}.get(lab, lab),
                ha="center", va="center", fontsize=12 if len(lab) < 12 else 11, weight="bold", zorder=30,
                rotation=90 if lab == "J3" else 0, color="white" if lab == "J3" else INK)
    _box(ax, M.FPC_SWEEP, fc="#fff3e0", ec=PAL["fpc"][2], hatch="..", lw=1.0, zorder=12)
    ax.text((M.FPC_SWEEP[0] + M.FPC_SWEEP[2]) / 2, 71.0, "FPC tail lies here", rotation=90, ha="center",
            va="center", fontsize=11, zorder=30)
    # posts
    for n, (x, y) in M.KEY_POSTS.items():
        ax.add_patch(Circle((x, y), 2.0, fc=PAL["back"][0], ec=PAL["back"][2], lw=1.6, zorder=19))
        ax.add_patch(Circle((x, y), 2.5, fc="none", ec=PAL["back"][2], lw=0.8, ls=":", zorder=19))
    for n, (x, y) in M.DOOR_SCREWS.items():
        ax.add_patch(Circle((x, y), 1.6, fc=PAL["door"][0], ec=PAL["door"][2], lw=1.4, zorder=20))
        ax.text(x + 2.0, y + 0.3, n, fontsize=11.5, weight="bold", color=PAL["door"][2], zorder=30, va="top")
    ax.add_patch(Circle(M.SPK[:2], M.SPK[2] / 2, fc="#e8eaf6", ec=PAL["parts"][0], lw=1.6, ls="--", zorder=9))
    ax.text(M.SPK[0], M.SPK[1], "speaker\n(in the notch)", ha="center", va="center", fontsize=11.5, zorder=30)
    # callouts
    fs = 12
    callout(ax, (120.0, 40.0), (113.0, 29.0), "FPGA + LDO", fs=fs) if False else None
    notes = [((150.0, 45.0), (141.0, 25.5), "MCU hub, next to the right-grip keys"),
             ((119.0, 34.0), (104.0, 25.5), "FPGA beside J3 (13 display wires)"),
             ((108.9, 41.1), (60.0, 25.5), "J8 cell plug behind the charger: 2 mm battery trace"),
             ((139.7, 63.0), (120.0, 117.5), "SWD pads: bring-up only\n(updates go over USB-C)"),
             ((81.0, 106.4), (52.0, 117.5), "test pads: reach them\nwith the door open"),
             ((143.94, 68.25), (142.0, 117.5), "posts (8): one under\nevery face switch"),   # no crossing leaders
             ((104.1, 100.0), (86.0, 117.5), "rib between the cell\nand the connector bay"),
             ]
    for xy, xt, txt in notes:
        if txt:
            callout(ax, xy, xt, txt, fs=fs)
    ax.text(140.0, 70.0, "", fontsize=1)
    ax.set_title("BACK of the board, seen THROUGH the board from the front (x not mirrored), true scale, KiCad mm",
                 loc="left", fontsize=16, weight="bold")
    hs = [Patch(fc=CAT_COL["core"], label="MCU / FPGA / LDO"), Patch(fc=PAL["conn"][0], label="J3 FPC connector"),
          Patch(fc=CAT_COL["display"], label="BL backlight driver, caps"), Patch(fc=CAT_COL["battery"], label="J8 cell plug"),
          Patch(fc=CAT_COL["debug"], label="debug / test"), Patch(fc=CAT_COL["rf"], alpha=0.55, label="Bluetooth BT830"),
          Patch(fc=PAL["cell"][0], alpha=0.75, label="battery cell"),
          Patch(fc=PAL["plate"][0], hatch="////", alpha=0.35, ec=PAL["plate"][2], label="steel battery plate"),
          Patch(fc=PAL["back"][0], ec=PAL["back"][2], label="rib / posts / bay walls (back shell)"),
          Line2D([], [], color=PAL["door"][2], ls="--", lw=2.4, label="door opening"),
          Patch(fc="#fff3e0", hatch="..", ec=PAL["fpc"][2], label="FPC tail (no parts)"),
          Patch(fc="#b2dfdb", hatch="xx", ec="#00897b", label="antenna keep-out"),
          Line2D([], [], color="#00897b", ls="--", lw=1.6, label="20 / 30 mm from the keep-out"),
          Line2D([], [], marker="o", color="#c62828", ls="none", ms=11, mfc="#ffebee", label="shell screw"),
          Line2D([], [], marker="o", color="#00796b", ls="none", ms=11, mfc="#e0f2f1", label="board screw"),
          Line2D([], [], marker="o", color=PAL["door"][2], ls="none", ms=9, mfc=PAL["door"][0], label="door screw")]
    fig.legend(handles=hs, loc="lower center", ncol=8, fontsize=12.5, frameon=False, bbox_to_anchor=(0.5, 0.015),
               handlelength=2.2, columnspacing=1.4)
    fig.suptitle("gb2 rev B, recommended layout: BACK of the board. The chips go around the battery bay.",
                 fontsize=20, weight="bold", x=0.03, ha="left", y=0.985)
    fig.text(0.03, 0.945, "Blocks: EE floor plan variant T (ee_plan.json); cell 0.6 left + plate + rib: ME study; "
             "posts/keep-outs: REVB_DESIGN.md §3.7-3.8; antenna: EZ-DS-BT830 v3.0 p.27. Heights: outside the bay "
             "parts <= 1.7, inside <= 9.0. Collision check: arch_model.py (0 conflicts).", fontsize=12.5, color=INK2)
    out = os.path.join(HERE, "05_floorplan_back.png")
    fig.savefig(out, dpi=100)
    plt.close(fig)
    print("wrote", out)


if __name__ == "__main__":
    which = sys.argv[1:] or ["1", "2", "3", "4", "5"]
    for w in which:
        globals()["fig0" + w]()
