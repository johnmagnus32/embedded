"""Draw the EE floor plans (variants T and F), the stack-ups and the block connectivity. Data from ee_floorplan.py."""
import os, math
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle, Polygon as MPoly, FancyBboxPatch
import ee_floorplan as e

HERE = e.HERE
INK, INK2, MUTED = '#1f1f1e', '#55554f', '#9a9a92'
RT_STYLE = {'bus': ('#7a5200', '-', 2.6), 'fast': ('#174f95', '-', 2.0), 'slow': ('#55554f', (0, (4, 2)), 1.6),
            'pwr': ('#b03a10', '-', 3.0)}
RT_NAME = {'bus': 'display bus', 'fast': 'fast / clocked', 'slow': 'slow, long', 'pwr': 'power trunk'}

def outline(ax, v):
    xs, ys = zip(*e.OUT[0]); ax.fill(xs, ys, facecolor='#fbfbf8', edgecolor=INK, lw=1.4, zorder=0)
    for r in e.OUT[1:]:
        xs, ys = zip(*r); ax.fill(xs, ys, facecolor='white', edgecolor=INK, lw=0.8, zorder=6)
    if v['opening']:
        x0, y0, x1, y1 = v['opening']
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, facecolor='white', edgecolor=INK, lw=1.2, zorder=6))
        ax.text((x0 + x1) / 2, (y0 + y1) / 2, 'FPC OPENING\n(through, all layers)\nsplits the planes', ha='center', va='center',
                fontsize=6.5, color=INK, rotation=90, zorder=7)

def kept(ax, v, side):
    for ref, ps, tht, pg, h in e.kept_parts(v):
        if ps != side and not tht: continue
        x0, y0, x1, y1 = pg.bounds
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, facecolor='#d9d9d3', edgecolor='#a8a8a0', lw=0.4, zorder=2))
        if (x1 - x0) * (y1 - y0) > 14 and ref[:2] in ('SW', 'U1', 'J7', 'L2', 'U6', 'U8', 'U9', 'U5'):
            ax.text((x0 + x1) / 2, (y0 + y1) / 2, ref, fontsize=4.8, ha='center', va='center', color=INK2, zorder=3)

def draw_side(ax, v, side):
    outline(ax, v)
    # panel / active area (front) - reference outline on both sides
    x0, y0, x1, y1 = e.PANEL
    ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fill=False, edgecolor=INK, lw=1.0, ls=(0, (6, 3)), zorder=8))
    if side == 'F':
        ax.text(x0 + 0.8, y1 - 0.8, 'panel 69.3 x 50.2 (front)', fontsize=6, color=INK, va='bottom', zorder=9)
    # zones
    for s, cat, name, (zx0, zy0, zx1, zy1) in v['zones']:
        if s != side: continue
        ax.add_patch(Rectangle((zx0, zy0), zx1 - zx0, zy1 - zy0, facecolor=e.CAT[cat], alpha=0.10, edgecolor=e.CAT[cat], lw=1.0, zorder=1))
        if not name: continue
        ax.text(zx0 + 0.6, zy0 + 0.6, name, fontsize=6.2, color=INK, va='top', ha='left', zorder=9, fontweight='bold',
                bbox=dict(boxstyle='square,pad=0.15', fc='white', ec='none', alpha=0.75))
    kept(ax, v, side)
    # keep-outs
    for s, kind, lab, g in v['keepouts'] + e.COMMON_KO:
        if s not in (side, 'A'): continue
        if kind == 'hole': continue
        if g.geom_type == 'Polygon':
            xs, ys = g.exterior.xy
            if kind == 'cell':
                p = MPoly(list(zip(xs, ys)), closed=True, facecolor=e.CAT['BATTERY'], alpha=0.12, edgecolor=e.CAT['BATTERY'], lw=1.2, hatch='//', zorder=4)
            elif kind == 'rf':
                p = MPoly(list(zip(xs, ys)), closed=True, facecolor=e.CAT['RF'], alpha=0.25, edgecolor=e.CAT['RF'], lw=1.2, hatch='xx', zorder=4)
            elif kind == 'low':
                p = MPoly(list(zip(xs, ys)), closed=True, fill=False, edgecolor=INK2, lw=0.8, ls=':', zorder=4)
            elif kind == 'post':
                p = MPoly(list(zip(xs, ys)), closed=True, facecolor='none', edgecolor=INK2, lw=0.8, zorder=4)
            else:
                p = MPoly(list(zip(xs, ys)), closed=True, facecolor='none', edgecolor=MUTED, lw=0.8, hatch='\\\\\\', zorder=3)
            ax.add_patch(p)
    if side == 'B':
        cx0, cy0, cx1, cy1 = v['cell']
        ax.text((cx0 + cx1) / 2, (cy0 + cy1) / 2 + 6, 'CELL\nLP-803860\n38.5 x 62 x 8.5\nPCM end + leads UP\n(no parts under it;\nmasked traces OK)',
                ha='center', va='center', fontsize=7, color=INK, zorder=9, bbox=dict(boxstyle='round,pad=0.3', fc='white', ec=e.CAT['BATTERY'], alpha=0.9))
        bx0, by0, bx1, by1 = v['bay']
        ax.add_patch(Rectangle((bx0, by0), bx1 - bx0, by1 - by0, fill=False, edgecolor=e.CAT['BATTERY'], lw=1.0, ls=(0, (2, 2)), zorder=4))
        ax.text(bx0 + 0.5, by1 + 0.4, 'battery bay / door', fontsize=5.5, color=INK2, va='top', zorder=9)
    else:
        ax.text(29.5, 45.5, 'antenna\nkeep-out', fontsize=5.5, ha='center', va='center', color=INK, zorder=9)
    # blocks
    for s, cat, name, members, (bx0, by0, bx1, by1) in v['blocks']:
        if s != side: continue
        ax.add_patch(Rectangle((bx0, by0), bx1 - bx0, by1 - by0, facecolor=e.CAT[cat], alpha=0.55, edgecolor=e.CAT[cat], lw=1.4, zorder=5))
        rot = 90 if (by1 - by0) > 2.2 * (bx1 - bx0) else 0
        ax.text((bx0 + bx1) / 2, (by0 + by1) / 2, name, fontsize=7 if name in ('MCU', 'FPGA', 'J3') else 5.6, rotation=rot,
                ha='center', va='center', color='white' if cat in ('CORE', 'HMI', 'BATTERY', 'DEBUG') else INK, fontweight='bold', zorder=9)
    # RF clearance arcs (front and back: metal anywhere matters)
    ax.add_patch(Circle(((e.ANT[0] + e.ANT[2]) / 2, (e.ANT[1] + e.ANT[3]) / 2), 20 + 4.85, fill=False, ec=e.CAT['RF'], lw=0.8, ls='--', zorder=4))
    # speaker
    ax.add_patch(Circle(e.SPEAKER[:2], e.SPEAKER[2], fill=False, ec=INK2, lw=0.9, zorder=4))
    ax.text(e.SPEAKER[0], e.SPEAKER[1], 'speaker\n(magnet)', fontsize=5.5, ha='center', va='center', color=INK2, zorder=9)
    ax.add_patch(Circle(e.LRA[:2], e.LRA[2], fill=False, ec=INK2, lw=0.8, zorder=4))
    ax.set_xlim(20, 170); ax.set_ylim(112, 29); ax.set_aspect('equal'); ax.set_xticks(range(20, 171, 10)); ax.set_yticks(range(30, 112, 10))
    ax.tick_params(labelsize=6, colors=MUTED); [sp.set_color(MUTED) for sp in ax.spines.values()]
    ax.grid(True, color='#ecece6', lw=0.4, zorder=-1)

def draw_routes(ax, v):
    for i, (kind, lab, pts, tgt) in enumerate(e.ROUTES[v['key']], 1):
        c, ls, lw = RT_STYLE[kind]
        xs, ys = zip(*pts); ax.plot(xs, ys, color=c, ls=ls, lw=lw, zorder=10, solid_capstyle='round')
        mid = pts[len(pts) // 2] if len(pts) > 2 else ((pts[0][0] + pts[1][0]) / 2, (pts[0][1] + pts[1][1]) / 2)
        ax.text(mid[0], mid[1], str(i), fontsize=6, color='white', ha='center', va='center', zorder=11, fontweight='bold',
                bbox=dict(boxstyle='circle,pad=0.15', fc=c, ec='white', lw=0.8))

def legend_box(fig, v, y):
    lines = []
    for i, (kind, lab, pts, tgt) in enumerate(e.ROUTES[v['key']], 1):
        lines.append(f'{i:>2}  {lab}:  ~{e.length(pts):.0f} mm  (target <= {tgt})')
    fig.text(0.015, y, 'Critical routes (estimated pin-to-pin; all drawn on the front view whatever their layer)\n' + '\n'.join(lines), fontsize=6.6, va='top', family='monospace', color=INK)

def floorplan(v, fname, notes):
    fig, axs = plt.subplots(2, 1, figsize=(11, 12.4), gridspec_kw=dict(hspace=0.16, top=0.95, bottom=0.17, left=0.05, right=0.99))
    draw_side(axs[0], v, 'F'); draw_side(axs[1], v, 'B')
    draw_routes(axs[0], v)
    axs[0].set_title('FRONT (L1 parts) - seen from the front', fontsize=9, loc='left', color=INK)
    axs[1].set_title('BACK (L4 parts) - still seen from the front, x NOT mirrored', fontsize=9, loc='left', color=INK)
    fig.suptitle(v['title'], fontsize=10.5, x=0.02, ha='left', color=INK, fontweight='bold')
    # category legend
    hs = [Rectangle((0, 0), 1, 1, fc=c, alpha=0.55, ec=c) for c in e.CAT.values()]
    hs += [plt.Line2D([], [], color=RT_STYLE[k][0], ls=RT_STYLE[k][1], lw=RT_STYLE[k][2]) for k in RT_STYLE]
    hs += [Rectangle((0, 0), 1, 1, fc='#d9d9d3', ec='#a8a8a0'), plt.Line2D([], [], color=e.CAT['RF'], ls='--')]
    labs = [k.lower() for k in e.CAT] + [RT_NAME[k] for k in RT_STYLE] + ['rev A part kept in place', 'antenna 20 mm radius (BT830 p.27)']
    axs[0].legend(hs, labs, fontsize=6, ncol=8, loc='upper center', bbox_to_anchor=(0.5, -0.02), frameon=False)
    legend_box(fig, v, 0.135)
    fig.text(0.53, 0.135, notes, fontsize=6.6, va='top', color=INK)
    fig.savefig(os.path.join(HERE, fname), dpi=170); plt.close(fig)

NOTES_T = ("Why it looks like this (taped):\n"
 "- Front centre is dead (panel bonded on conductive tape) and back centre is the cell,\n"
 "  so all logic goes to the grips and to the bay ring. The FPC opening cuts every layer:\n"
 "  the right grip connects only through the top neck (16 front tracks; FPGA blocks the back)\n"
 "  and the 3.4 mm strip (10 + 10). ~30 of 36 slots are needed -> L3 must route there, or 6L.\n"
 "- MCU sits with its biggest local fan-out (right-grip keys, I2S, amp) and 19 mm from the FPGA.\n"
 "- J8 moves from the bay bottom (60-65 mm BAT trunk) to directly behind U9 (2 mm + vias):\n"
 "  flip the cell so the PCM end and leads face up.\n"
 "- IMU moved off the screw boss (plan B had it ~5 mm away) and away from the LRA and speaker.\n"
 "- SWD next to the MCU (shell off); field updates over USB DFU instead (new D+/D- pair).\n"
 "- Under the panel: masked L1 copper = bus corridor + bonded-GND strips under the CN4190 tape.")
NOTES_F = ("Why it looks like this (floating):\n"
 "- Panel 2.3 mm above the board: parts <= 2.0 mm fit under it, so the digital core sits in the\n"
 "  middle where every route is short, over an unbroken L2 GND, and the board needs no opening.\n"
 "- FPGA directly beside J3: 13-net bus ~5-12 mm on L1, no vias. MCU between the FPGA, the\n"
 "  power band (USB, CHG, PSYNC) and the left grip (BT, I2C); right-grip nets ~60-70 mm.\n"
 "- J3 must be bottom-contact (contacts face the board after the maker's fold) and <= 2.0 tall.\n"
 "- Cell centred: 41 mm from the antenna (meets the 30 mm left/right figure). J8 beside the\n"
 "  cell top-right, 12 mm from U9. SWD + pads in the bay's left column behind the MCU.\n"
 "- NOR becomes WSON-8 (0.80 max) to fit under the panel; the grip backs keep only U12.\n"
 "- Ground the panel's metal frame to board GND (conductive foam at 2-4 pads).")

def stackup():
    fig, axs = plt.subplots(1, 3, figsize=(13, 5.6), gridspec_kw=dict(wspace=0.08, left=0.01, right=0.99, top=0.86, bottom=0.04))
    def stack(ax, title, layers, foot):
        ax.set_xlim(0, 10); ax.set_ylim(0, 10); ax.axis('off'); ax.set_title(title, fontsize=9, color=INK, loc='left', fontweight='bold')
        y = 9.0
        for name, kind, role in layers:
            h = 0.55 if kind in ('cu', 'plane', 'gnd') else (0.5 if kind == 'pp' else 0.95)
            col = {'cu': '#c98500', 'gnd': '#2a78d6', 'plane': '#eb6834', 'pp': '#e6e2cf', 'core': '#d6d1b8'}[kind]
            ax.add_patch(Rectangle((0.2, y - h), 3.2, h, fc=col, ec='white', lw=1.5))
            ax.text(0.35, y - h / 2, name, fontsize=7.5, va='center', color='white' if kind in ('gnd', 'plane', 'cu') else INK, fontweight='bold')
            ax.text(3.6, y - h / 2, role, fontsize=6.6, va='center', color=INK)
            y -= h
        ax.text(0.2, y - 0.25, foot, fontsize=6.4, va='top', color=INK2)
    stack(axs[0], 'Variant F (floating) - 4 layers, 1.0 or 1.6 mm', [
        ('L1  F.Cu', 'cu', 'all ICs + display bus + PPU/SPI3/USB; GND pour'),
        ('prepreg', 'pp', 'thin (0.1-0.2): L1 couples to L2'),
        ('L2  GND', 'gnd', 'SOLID GND, never cut. Reference for every fast net'),
        ('core', 'core', ''),
        ('L3  PWR', 'plane', '+3V3 plane; +1V2 island under U2; SYS/BAT pours\nin the top band; GND fill elsewhere'),
        ('prepreg', 'pp', ''),
        ('L4  B.Cu', 'cu', 'slow nets only (keys, UART, I2C), BT830, J8, SWD\npads; GND pour; nothing fast crosses an L3 edge')],
        'Rev A roles kept (route/PLAN.md:8-17): the fast parts are\nall on L1 again, so SIG/GND/PWR/SIG is the right choice.\nL4 signals reference L3 (+3V3): slow nets only (TI SPRAAR7 §2.4).')
    stack(axs[1], 'Variant T (taped) - 4 layers, recommended roles', [
        ('L1  F.Cu', 'cu', 'power band, HMI, NOR/IMU; under the panel: masked bus\ncorridor + cross-board lanes + +3V3 pour; GND strips\nexposed under the CN4190 tape to bond the panel frame'),
        ('prepreg', 'pp', ''),
        ('L2  GND', 'gnd', 'SOLID GND (switcher loops on L1 return here)'),
        ('core', 'core', ''),
        ('L3  GND*', 'gnd', 'GND fill under U1/U2/J3 and every L4 fast net;\n*slow-net lanes only in the two necks; +1V2 as an L4 pour'),
        ('prepreg', 'pp', ''),
        ('L4  B.Cu', 'cu', 'MCU, FPGA, J3, backlight, J8, BT830; bus fan-out')],
        'Why not rev A roles: U1, U2 and J3 move to the back, so their\nnets would reference the +3V3 plane, which TI advises against\n(SPRAAR7 §2.4). With GND on both inner layers a signal via changes\nreference GND->GND: put a GND-GND stitch via within 1.2 mm.')
    stack(axs[2], 'Variant T, no-compromise option - 6 layers, 1.2 mm', [
        ('L1  F.Cu', 'cu', 'front parts + masked under-panel lanes'),
        ('pp', 'pp', ''),
        ('L2  GND', 'gnd', 'solid'),
        ('core', 'core', ''),
        ('L3  SIG', 'cu', 'stripline: long cross-board slow nets, neck crossings'),
        ('pp', 'pp', ''),
        ('L4  PWR', 'plane', '+3V3 / +1V2 / SYS planes'),
        ('core', 'core', ''),
        ('L5  GND', 'gnd', 'solid'),
        ('pp', 'pp', ''),
        ('L6  B.Cu', 'cu', 'back parts (MCU, FPGA, J3, BT830)')],
        'JLC builds 6 layers at 1.2 mm minimum (jlcpcb.com/impedance),\nso this costs +0.2 mm over the 1.0 mm baseline. It removes the\nneck capacity limit and gives both part sides a solid GND.')
    fig.suptitle('Stack-ups: which plane sits next to which signal layer', fontsize=10.5, x=0.01, ha='left', color=INK, fontweight='bold')
    fig.savefig(os.path.join(HERE, 'ee_stackup.png'), dpi=170); plt.close(fig)

def connectivity():
    """block diagram: net counts between functions (from gb2-nets.csv, rails excluded)"""
    fig, ax = plt.subplots(figsize=(11, 5.4)); ax.set_xlim(0, 100); ax.set_ylim(0, 50); ax.axis('off')
    N = {'U12 BT830': (8, 34, 'RF'), 'U11 expander\n+ D-pad, L, Sel/Start': (8, 16, 'HMI'), 'U8 haptics': (22, 6, 'HMI'),
         'U1 MCU (hub)': (42, 25, 'CORE'), 'U2 FPGA': (64, 25, 'CORE'), 'J3 panel FPC': (86, 25, 'DISPLAY'),
         'U4 NOR': (42, 6, 'CORE'), 'U7 IMU': (40, 44, 'CORE'), 'power: U9, U10,\nU5, J7, J8': (58, 44, 'POWER'),
         'right grip: A, B, R,\nVol, U6 amp': (78, 6, 'AUDIO'), 'SWD / USB DFU': (20, 44, 'DEBUG')}
    E = [('U1 MCU (hub)', 'U2 FPGA', 5, 'PPU SPI + config'), ('U2 FPGA', 'J3 panel FPC', 13, 'D0-7, WR, DC, CS, RST, TE'),
         ('U1 MCU (hub)', 'U12 BT830', 5, 'UART + EN'), ('U1 MCU (hub)', 'U4 NOR', 4, 'SPI3'),
         ('U1 MCU (hub)', 'right grip: A, B, R,\nVol, U6 amp', 8, '5 keys + I2S x3'), ('U1 MCU (hub)', 'U7 IMU', 3, 'I2C + INT'),
         ('U1 MCU (hub)', 'U11 expander\n+ D-pad, L, Sel/Start', 2, 'I2C'), ('U11 expander\n+ D-pad, L, Sel/Start', 'U8 haptics', 2, 'I2C'),
         ('U1 MCU (hub)', 'power: U9, U10,\nU5, J7, J8', 4, 'CHG x2, PSYNC, BAT sense'), ('U1 MCU (hub)', 'SWD / USB DFU', 5, 'SWD x3 + D+/D-'),
         ('U1 MCU (hub)', 'J3 panel FPC', 1, 'LCD_LITE (PWM)')]
    for a, b, n, lab in E:
        (x0, y0, _), (x1, y1, _) = N[a], N[b]
        if 'LITE' in lab:   # arc over the FPGA so it doesn't hide behind the bus
            import numpy as np
            t = np.linspace(0, 1, 40); xs = x0 + (x1 - x0) * t; ys = y0 + 9 * np.sin(np.pi * t)
            ax.plot(xs, ys, color=INK2, lw=1.2, alpha=0.75, zorder=1, ls=(0, (3, 2)))
            ax.text((x0 + x1) / 2, y0 + 10.2, f'{n}: {lab}', fontsize=6.4, ha='center', color=INK, zorder=3,
                    bbox=dict(boxstyle='round,pad=0.15', fc='white', ec='none', alpha=0.85))
            continue
        ax.plot([x0, x1], [y0, y1], color=INK2, lw=0.8 + 0.45 * n, alpha=0.75, zorder=1, solid_capstyle='round')
        # review fix: these four labels sat on the FPGA / J3 boxes or ran into each other; put them in clear space
        lx, ly = {('U1 MCU (hub)', 'U2 FPGA'): (53, 29.4), ('U2 FPGA', 'J3 panel FPC'): (75, 29.4),
                  ('U1 MCU (hub)', 'U7 IMU'): (36.0, 35.0)}.get((a, b), (x0 + 0.62 * (x1 - x0), y0 + 0.62 * (y1 - y0) + 1.2))
        ax.text(lx, ly, f'{n}: {lab}', fontsize=6.4, ha='center', color=INK, zorder=3,
                bbox=dict(boxstyle='round,pad=0.15', fc='white', ec='none', alpha=0.85))
    for name, (x, y, cat) in N.items():
        ax.add_patch(FancyBboxPatch((x - 7.5, y - 3), 15, 6, boxstyle='round,pad=0.4', fc=e.CAT[cat], ec='white', alpha=1.0, zorder=2))
        ax.text(x, y, name, fontsize=7, ha='center', va='center', color='white' if cat not in ('DISPLAY', 'AUDIO', 'POWER') else INK, fontweight='bold', zorder=4)
    ax.set_title('Who talks to whom (signal nets per link, gb2-nets.csv; GND/+3V3/SYS excluded). Line weight = net count.\n'
                 'The FPGA belongs beside J3, the MCU beside the FPGA; everything else fans out from the MCU.', fontsize=9, loc='left', color=INK)
    fig.savefig(os.path.join(HERE, 'ee_connectivity.png'), dpi=170, bbox_inches='tight'); plt.close(fig)

if __name__ == '__main__':
    floorplan(e.T, 'ee_floorplan_T_taped.png', NOTES_T)
    floorplan(e.F, 'ee_floorplan_F_floating.png', NOTES_F)
    stackup(); connectivity()
    print('ok')
