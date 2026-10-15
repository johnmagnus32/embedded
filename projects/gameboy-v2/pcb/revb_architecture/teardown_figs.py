"""Teardown research figures for gb2 rev B (sources: see teardown notes in the workflow result).
fig 1: thickness by device, coloured by internal architecture.
fig 2: schematic side sections of the three architectures seen in teardowns (not to scale)."""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch
import os, math
OUT = os.path.dirname(os.path.abspath(__file__))
SURF, INK, INK2, GRID = "#fcfcfb", "#0b0b0b", "#52514e", "#e4e3df"
C = {"A": "#2a78d6", "B": "#eb6834", "C": "#1baf7a", "gb2": "#8a8984"}
NAMES = {"A": "A  stacked: display in front shell, PCB behind, cell in rear-shell well",
         "B": "B  midframe: cell BESIDE the board, display on the other face",
         "C": "C  metal midframe, board ring around cell (cut-out on a plate)",
         "gb2": "gb2 (rev A built, rev B candidates)"}
# (label, thickness mm, archetype, source tag)
D = [("iPhone 15 Pro", 8.25, "B", "Apple 111829"),
     ("Playdate", 9.0, "B", "play.date specs"),
     ("Switch Lite", 14.0, "B", "Wikipedia/Nintendo"),
     ("Miyoo Mini", 18.0, "A", "handheld sheet"),
     ("DS Lite (closed)", 21.5, "A", "Wikipedia"),
     ("Analogue Pocket", 21.8, "A", "Wikipedia 0.86 in"),
     ("RG35XX (2024)", 22.0, "A", "anbernic.com"),
     ("GBA SP (closed)", 24.4, "A", "Wikipedia"),
     ("GBA AGB-001", 24.5, "A", "Wikipedia"),
     ("Retroid Pocket 3", 24.23, "C", "handheld sheet, max at grips"),
     ]
# gb2 rows from the same stack model as 01-05 (arch_model.py): taped is the t = 1.0 baseline (REVB_DESIGN.md §1);
# the earlier 11.5 / 18.8 here was the t = 1.6 fallback. Rev A: Z_FRONT 20.9 - Z_BACK -3.6 (params.py:15-16).
import arch_model as AM
r1 = lambda v: math.floor(v * 10 + 0.5 + 1e-6) / 10          # round half up (20.95 -> 21.0, as in the text)
GB2 = [("R", "gb2 rev B RECOMMENDED (grip / cell)"), ("C", "gb2 rev B cut-out (grip / cell)"),
       ("T", "gb2 rev B taped, board 1.0 (grip / cell)"), ("F", "gb2 rev B floating (grip / cell)")]
D += [(lab, r1(AM.totals(o)["bump"]), "gb2", "arch_model.py") for o, lab in GB2]
D += [("gb2 rev A", 24.5, "gb2", "params.py:15-16")]
grip = {lab: r1(AM.totals(o)["grip"]) for o, lab in GB2}

plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 9, "axes.edgecolor": INK2,
                     "axes.labelcolor": INK2, "xtick.color": INK2, "ytick.color": INK})
fig, ax = plt.subplots(figsize=(10, 6.6), facecolor=SURF)
ax.set_facecolor(SURF)
for i, (lab, t, a, src) in enumerate(D):
    y = len(D) - 1 - i
    ax.barh(y, t, height=0.62, color=C[a], edgecolor=SURF, linewidth=2)
    if lab in grip:
        ax.barh(y, grip[lab], height=0.62, color="#5c5b57", edgecolor=SURF, linewidth=2)
        ax.text(grip[lab] - 0.3, y, f"{grip[lab]:g}", va="center", ha="right", color="white", fontsize=8)
    ax.text(t + 0.3, y, f"{t:.1f} mm" if a == "gb2" else f"{t:g} mm", va="center", color=INK, fontsize=8.5)
    ax.text(31.5, y, src, va="center", color=INK2, fontsize=7.5)
ax.set_yticks(range(len(D)), [d[0] for d in D][::-1])
for tl in ax.get_yticklabels():
    if "RECOMMENDED" in tl.get_text(): tl.set_fontweight("bold")
ax.set_xlim(0, 31); ax.set_xlabel("overall thickness, mm")
ax.xaxis.grid(True, color=GRID, linewidth=0.8); ax.set_axisbelow(True)
for s in ("top", "right"): ax.spines[s].set_visible(False)
h = [Rectangle((0, 0), 1, 1, color=C[k]) for k in C]
ax.legend(h, [NAMES[k] for k in C], loc="upper left", frameon=False, fontsize=8, bbox_to_anchor=(-0.32, -0.1), ncol=2)
ax.set_title("Handheld thickness by internal architecture (teardown survey)", loc="left", color=INK, fontsize=11)
ax.text(31.5, len(D) - 0.2, "source", color=INK2, fontsize=7.5, weight="bold")
fig.text(0.01, 0.01, "gb2 rev B bars: dark segment = grip thickness, full bar = at the cell (same stack model as figures 01-05). RP2+ omitted: 17-30 mm wedge, no teardown found.",
         color=INK2, fontsize=7.5)
fig.tight_layout(rect=(0, 0.1, 0.9, 1))
fig.savefig(os.path.join(OUT, "fig1_thickness_by_architecture.png"), dpi=160, facecolor=SURF)

# ---------------------------------------------------------------- fig 2 sections
COL = {"shell": "#d9d7d0", "lens": "#bfe3f5", "lcd": "#3a3a3a", "pcb": "#2f7d32", "cell": "#eda100",
       "foam": "#b9a7d9", "metal": "#9aa3ad", "part": "#1f1f1f", "dome": "#e87ba4", "flex": "#e0802a"}
def R(ax, x, z, w, h, k, lab=None, tc="black", fs=7, **kw):
    ax.add_patch(Rectangle((x, z), w, h, facecolor=COL[k], edgecolor="#555", linewidth=0.6, **kw))
    if lab: ax.text(x + w / 2, z + h / 2, lab, ha="center", va="center", fontsize=fs, color=tc)
def note(ax, x, z, s, tx, tz):
    ax.annotate(s, (x, z), (tx, tz), fontsize=7, color=INK, arrowprops=dict(arrowstyle="-", color=INK2, lw=0.6),
                va="center")
fig, axs = plt.subplots(3, 1, figsize=(11, 8.6), facecolor=SURF)
for a in axs: a.set_xlim(-2, 150); a.set_ylim(-4, 27); a.axis("off"); a.set_aspect("equal")

# A: GBA / DS Lite / Miyoo Mini / Pocket / SP
a = axs[0]; a.set_title(NAMES["A"] + "\nGBA, GBA SP, DS Lite, Miyoo Mini, Analogue Pocket, RG35XX"
                        "\nVariants the sketch hides: the GBA board is U-shaped and its AA well comes up through the notch, right behind the LCD's foam pad (2002 s12);"
                        "\nthe Analogue Pocket and Miyoo Mini carry their SoC and other chips on the board face under the screen (163397 s17, 149516 s14).",
                        loc="left", fontsize=9, color=INK)
R(a, 0, 20, 140, 2, "shell"); R(a, 45, 20.4, 50, 1.6, "lens", "lens in shell window", fs=6)
R(a, 45, 15.6, 50, 4.0, "lcd", "LCD in shell recess", "white")
R(a, 43, 19.6, 2, 0.4, "foam"); R(a, 95, 19.6, 2, 0.4, "foam")
R(a, 45, 14.6, 50, 1.0, "foam", "foam pad", fs=6)
R(a, 2, 13.0, 136, 1.6, "pcb")
for x in (10, 24, 116, 128): R(a, x, 14.6, 6, 1.0, "dome"); R(a, x + 1, 15.6, 4, 4.4, "shell")
for x in (6, 30, 100, 112, 132): R(a, x, 11.8, 3, 1.2, "part")
a.plot([95, 99, 99, 96], [17, 17, 12.2, 12.2], color=COL["flex"], lw=2)
R(a, 2, 2, 136, 2, "shell"); R(a, 35, 4, 2, 7.5, "shell"); R(a, 105, 4, 2, 7.5, "shell")
R(a, 37, 4.6, 68, 6, "cell", "cell / AA in molded well")
R(a, 35, 1.0, 72, 1.0, "shell", "battery door", fs=6)
for x in (38, 102): R(a, x - 1.2, 14.6, 2.4, 5.4, "shell"); R(a, x - 1.5, 12.4, 3, 0.6, "metal")
for x in (3, 136): R(a, x - 1, 4, 2, 16, "shell")
note(a, 44, 19.8, "adhesive gasket on bezel (GBA 2002 s15-16)", 30, 25.2)
note(a, 88, 15.1, "foam between LCD back and board (GBA photo, 2002 s12)", 72, 23.2)   # leader clear of the LCD label
note(a, 13, 15, "silicone dome -> carbon/gold pads on PCB", 0, 23.0)
note(a, 98, 14, "flex wraps board edge to connector on back (GBA 2002 s9)", 96, 25.2)
note(a, 36, 8, "shell wall = tray; nothing on the cell but plastic", -1, -2.5)
note(a, 102, 12.6, "board screwed to front-shell bosses (3 on GBA, 2002 s11)", 88, -2.5)

# B: Switch Lite / phone / Playdate
a = axs[1]; a.set_title(NAMES["B"] + "\nSwitch Lite, iPhone, Playdate", loc="left", fontsize=9, color=INK)
R(a, 0, 20, 140, 1.5, "lens", "cover glass / digitizer (front frame)", fs=6)
R(a, 20, 16.5, 100, 3.3, "lcd", "LCD glued to frame / glass", "white")
R(a, 0, 15.0, 140, 1.2, "metal", "midframe (metal plate or chassis)", fs=6)
R(a, 8, 13.4, 60, 1.6, "pcb", "main board", "white", fs=6)
for x in (12, 30, 50): R(a, x, 11.6, 8, 1.8, "metal")
R(a, 74, 8.5, 58, 6.5, "cell", "cell in its own well, BESIDE the board")
R(a, 74, 8.0, 58, 0.5, "foam")
R(a, 6, 10.8, 64, 0.6, "metal", "shield plate", fs=6)
R(a, 0, 5.5, 140, 1.6, "shell", "back cover", fs=6)
note(a, 100, 8.3, "foam / stretch-release tabs; swell room to back cover", 92, 2.5)
note(a, 8, 15.6, "buttons on FPC/domes backed by midframe (Playdate s12)", 0, 24.5)
note(a, 119, 18, "screws go into the midframe, never through the panel", 96, 24.5)

# C: Retroid Pocket 3
a = axs[2]; a.set_title(NAMES["C"] + "\nRetroid Pocket 3 (ten-screw metal midframe, board window, metal cover plate)", loc="left", fontsize=9, color=INK)
R(a, 0, 20, 140, 2, "shell"); R(a, 30, 20.4, 80, 1.4, "lens", "glass", fs=6)
R(a, 30, 16.4, 80, 3.6, "lcd", "LCD", "white")
R(a, 25, 15.2, 90, 1.2, "metal", "metal midframe", fs=6)
R(a, 2, 13.6, 40, 1.6, "pcb"); R(a, 98, 13.6, 40, 1.6, "pcb")
R(a, 42, 13.6, 56, 1.6, "pcb", "board window (cut-out)", "white", fs=6, alpha=0.25)
R(a, 44, 7.6, 52, 7.6, "cell", "cell on midframe, inside window")
R(a, 2, 6.8, 136, 0.8, "metal", "metal cover plate (4 screws)", fs=6)
R(a, 0, 3.5, 140, 1.6, "shell", "back shell", fs=6)
for x in (6, 20, 104, 120): R(a, x, 15.2, 6, 1.0, "dome")
note(a, 70, 15.8, "plate between cell and LCD takes the swell (RP3 172020 s20)", 82, 24.8)
note(a, 8, 15.8, "separate L/R button boards + silicone pads", 0, 24.8)
fig.text(0.01, 0.005, "Schematic side sections, not to scale. Sources: iFixit guides 1904/2002/191804 (GBA), 12567/12788/103533 (SP), 4784/4785 (DS Lite), "
         "126223/137376/137383 (Switch Lite), 163329/163397 (Pocket), 143811 (Playdate), 172020/171361 (RP3), 179168/179131 (RG35XX), 149516 (Miyoo), 166394 (iPhone 15 Pro).",
         fontsize=6.5, color=INK2, wrap=True)
fig.tight_layout(rect=(0, 0.02, 1, 1))
fig.savefig(os.path.join(OUT, "fig2_teardown_architectures.png"), dpi=150, facecolor=SURF)
print("ok")
