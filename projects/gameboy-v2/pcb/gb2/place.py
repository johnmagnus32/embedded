#!/usr/bin/env python3
"""place.py — bring the schematic's parts into gb2.kicad_pcb (what "Update PCB from Schematic" does) and do the
first placement pass.

Import: exports the netlist from gb2.kicad_sch with kicad-cli, loads each footprint from the project's
fp-lib-table libraries, and adds it with its reference, value, symbol fields, schematic link (sheet path +
symbol UUID, so a later GUI "Update PCB from Schematic" recognises every part) and pad nets. Re-runnable:
footprints already on the board keep their position and get their nets/fields refreshed; footprints no
longer in the schematic are deleted (like the GUI's "Delete footprints with no symbols").

Placement (new footprints; all of them with --reset; just the buttons with --place-buttons; just the\ndisplay sockets with --place-display):
  * Buttons go where the GBA has its controls, in draw_mech.py's widened frame (same geometry, same stretch).
    They are matched by their button NET (BTN_UP, BTN_A, ...), not refdes, so renumbering can't misplace one:
    D-pad at ±8 mm around the GBA D-pad centre (gb3's spacing), A/B at the GBA's A/B, Start/Select at the GBA's,
    L/R on the top edge in the GBA shoulder areas (actuators up, parallel to the outline), Vol+/- on the right edge (actuators out, like gb3)
    and the side power switch SW1 (net PWR_SW) at the GBA's power switch, lever out the bottom-left edge.
  * The display sockets J3/J4 go under the 2090's JP1/JP2: rotation + position solved so every socket pad lands
    on its module pin (pad 1 = module pin 1, at the +Y end — 180 degrees from gameboy v1's socket orientation).
  * USB-C (found by its VBUS/CC nets) at the top edge on the centre line, mouth up, overhanging the board edge as far
    as its front shell tabs allow; its two CC pull-downs right behind their own CC pins.
  * Everything else is packed just outside (below) the GBA outline, one shelf-packed block per subcircuit
    sheet, boxed + labelled on User.3, in power-chain / signal-flow order. --repack re-packs every part that
    isn't on the board yet (outside the outline) without touching the placed ones.

Run:  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 place.py [--reset | --place-buttons | --place-display | --place-usb | --place-fpga | --place-bt | --place-charger | --place-buckboost | --place-mcu | --place-lcdboost | --place-nor | --place-haptics | --place-batsense | --place-display-caps | --place-amp | --set-4layer | --place-fpgaldo | --place-imu | --place-expander | --place-i2c | --repack] [--tidy-labels]
"""
import os, sys, re, subprocess, math, csv

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import draw_mech as M                 # geometry + stretch + outline (also imports pcbnew)
import pcbnew

PCB = os.path.join(HERE, "gb2.kicad_pcb")
SCH = os.path.join(HERE, "gb2.kicad_sch")
NET = "/tmp/gb2-place.net"
CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
MM = pcbnew.FromMM; TO = pcbnew.ToMM
def V(x, y): return pcbnew.VECTOR2I(int(round(MM(x))), int(round(MM(y))))

PACK_GAP, GROUP_GAP, PACK_TOP = 1.5, 5.0, 20.0   # mm between parts / between sheets / below the outline bottom
GROUP_HEAD = 4.5                                 # mm from a group box's top to its first row (title + refdes labels)
PACK_W = 255.0                                   # mm wide area (from the outline's left edge) the groups are packed into
EDGE_INSET = M.SHELL_WALL + 0.8                  # mm from the GBA outline (the shell's OUTER surface) to a side switch's
                                                 # actuator tip: printed wall + ~0.8 cap flange / pre-travel gap


# ---------------------------------------------------------------- netlist
def sexpr(text):
    toks = re.findall(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()"]+', text)
    stack = [[]]
    for t in toks:
        if t == "(": stack.append([])
        elif t == ")": x = stack.pop(); stack[-1].append(x)
        else: stack[-1].append(t[1:-1].replace('\\"', '"') if t.startswith('"') else t)
    return stack[0][0]

def child(node, key):
    return next((c for c in node[1:] if isinstance(c, list) and c and c[0] == key), None)

def val(node, key):
    c = child(node, key); return c[1] if c and len(c) > 1 else None

RING_LABELED = set()                  # refs whose labels a placement ring already positioned
PINFUNC = {}                           # (ref, pin) -> pin function, e.g. ('U2', '33') -> 'VCCIO_0_33'

def read_netlist():
    r = subprocess.run([CLI, "sch", "export", "netlist", "--format", "kicadsexpr", "-o", NET, SCH], capture_output=True, text=True)
    if r.returncode: sys.exit("netlist export failed:\n" + r.stderr)
    root = sexpr(open(NET, encoding="utf-8").read())
    comps = {}
    for c in child(root, "components")[1:]:
        props = {val(p, "name"): val(p, "value") for p in c[1:] if isinstance(p, list) and p[0] == "property"}
        fields = {}
        f = child(c, "fields")
        for fl in (f[1:] if f else []):
            fields[val(fl, "name")] = fl[2] if len(fl) > 2 else ""
        sp = child(c, "sheetpath")
        comps[val(c, "ref")] = dict(value=val(c, "value"), footprint=val(c, "footprint"), fields=fields,
                                    sheetname=props.get("Sheetname", ""), sheetfile=props.get("Sheetfile", ""),
                                    path=val(sp, "tstamps") + val(c, "tstamps"))
    nets = {}                          # (ref, pin) -> net name
    for n in child(root, "nets")[1:]:
        name = val(n, "name")
        for nd in n[1:]:
            if isinstance(nd, list) and nd[0] == "node":
                nets[(val(nd, "ref"), val(nd, "pin"))] = name
                PINFUNC[(val(nd, "ref"), val(nd, "pin"))] = val(nd, "pinfunction") or ""
    return comps, nets

def lib_paths():
    t = open(os.path.join(HERE, "fp-lib-table"), encoding="utf-8").read()
    return {n: os.path.normpath(u.replace("${KIPRJMOD}", HERE)) for n, u in re.findall(r'\(name "([^"]+)"\)\(type "[^"]*"\)\(uri "([^"]+)"\)', t)}


# ---------------------------------------------------------------- button targets (widened GBA frame -> board)
def button_targets(nets):
    """refdes -> (x, y, rot) board mm, for every switch whose button net (or SYS_SRC for the power switch) has a target."""
    S = M.stretch(); side = M.layout(S)[0]; poly = M.silhouette(S); gx, gy = M.GBA_ORIGIN
    B_ = lambda x, y: (gx + x, gy + y)
    def right_edge(y):                 # outline x at height y (rightmost crossing)
        xs = [x0 + (y - y0) * (x1 - x0) / (y1 - y0) for (x0, y0), (x1, y1) in zip(poly, poly[1:] + poly[:1]) if (y0 > y) != (y1 > y)]
        return max(xs)
    def bottom_edge(x):
        ys = [y0 + (x - x0) * (y1 - y0) / (x1 - x0) for (x0, y0), (x1, y1) in zip(poly, poly[1:] + poly[:1]) if (x0 > x) != (x1 > x)]
        return max(ys)
    def top_edge(x):
        ys = [y0 + (x - x0) * (y1 - y0) / (x1 - x0) for (x0, y0), (x1, y1) in zip(poly, poly[1:] + poly[:1]) if (x0 > x) != (x1 > x)]
        return min(ys)
    def edge_mount(t, edge, tip):
        """Pose a side-actuated switch on the outline, like the GBA does: body parallel to the outline there,
        actuator pointing straight out along the outward normal, its tip EDGE_INSET inside the outline.
        t = x for the top/bottom edge, y for the right edge. `tip` = the actuator tip centre in footprint-local
        mm at rot 0, where the actuator points +Y."""
        if edge == "right":
            k = (right_edge(t + 1.5) - right_edge(t - 1.5)) / 3.0        # dx/dy along the edge
            L = math.hypot(1, k); nx, ny = 1 / L, -k / L                # outward normal (right)
            ex, ey = right_edge(t), t
        else:
            f = top_edge if edge == "top" else bottom_edge
            m = (f(t + 1.5) - f(t - 1.5)) / 3.0                         # dy/dx along the edge (+Y down)
            L = math.hypot(1, m)
            nx, ny = (m / L, -1 / L) if edge == "top" else (-m / L, 1 / L)
            ex, ey = t, f(t)
        th = math.atan2(nx, ny)                                         # KiCad rot (CCW) taking local +Y onto the normal
        tx, ty = tip
        rx, ry = tx * math.cos(th) + ty * math.sin(th), -tx * math.sin(th) + ty * math.cos(th)
        px, py = ex - EDGE_INSET * nx, ey - EDGE_INSET * ny             # where the actuator tip goes
        return (*B_(px - rx, py - ry), round(math.degrees(th), 2))
    SKRT_TIP = (-0.02, 1.80)          # SKRTLBE010 actuator tip centre (footprint silk), actuator +Y at rot 0
    MSK_TIP = (0.86, 3.57)            # MSK12C02-HB lever tip centre, lever +Y at rot 0
    dp = (side(19.95), 34.09)                                          # GBA D-pad centre
    by_net = {}
    for n, (dx, dy) in (("BTN_UP", (0, -8)), ("BTN_DOWN", (0, 8)), ("BTN_LEFT", (-8, 0)), ("BTN_RIGHT", (8, 0))):
        by_net[n] = (*B_(dp[0] + dx, dp[1] + dy), 0)
    by_net["BTN_A"] = (*B_(side(130.88), 31.92), 0)                      # GBA A / B
    by_net["BTN_B"] = (*B_(side(117.94), 36.25), 0)
    by_net["BTN_START"] = (*B_(side(29.49), 53.82), 0)
    by_net["BTN_SELECT"] = (*B_(side(29.44), 61.82), 0)
    # L / R: on the top edge, centred in the GBA's shoulder-button areas, actuators up (the GBA's own shoulder
    # switches sit ~15 mm inboard under long pivoting caps; ours press straight in at the edge)
    for n, nm in (("BTN_L", "L shoulder"), ("BTN_R", "R shoulder")):
        by_net[n] = edge_mount(side(M.item("gba", nm)["x"]), "top", SKRT_TIP)
    for n, y in (("BTN_VOLUP", 24.0), ("BTN_VOLDOWN", 32.0)):           # right edge, actuators out
        by_net[n] = edge_mount(y, "right", SKRT_TIP)
    # SW1 MSK12C02-HB right-angle slide (net PWR_SW = its common) at the GBA's power switch, bottom-left edge.
    by_net["PWR_SW"] = edge_mount(side(22.8), "bottom", MSK_TIP)
    t = {}
    for (ref, pin), n in nets.items():
        n = n.split("/")[-1]             # sheet-local nets come through as /<sheet>/NAME (e.g. /charger/PWR_SW)
        if ref.startswith("SW") and n in by_net:
            if ref in t and t[ref] != by_net[n]: sys.exit(f"{ref} is on two button nets")
            t[ref] = by_net[n]
    unplaced = set(by_net) - {n.split("/")[-1] for (r, p), n in nets.items() if r in t}
    if unplaced: print("note: no switch found for", ", ".join(sorted(unplaced)))
    return t


# ---------------------------------------------------------------- display sockets (under the 2090's headers)
DISPLAY_SOCKETS = {"J3": "JP1", "J4": "JP2"}      # mainboard socket -> 2090 header it receives (BOM #16/#17)

def display_pins():
    """{header: {pin n: (x, y) board mm}} for the 2090's JP1/JP2 as drawn by draw_mech.py (same frame, same flip).
    Pin n sits at module Y = 24.13 - 2.54 (n-1) (Adafruit .brd; pin 1 = GND at the +Y end)."""
    S = M.stretch(); _, _, (mcx, mcy), sx = M.layout(S); gx, gy = M.GBA_ORIGIN
    tcx, tcy = gx + mcx, gy + mcy
    out = {}
    for hdr in ("JP1", "JP2"):
        hx = M.item("tft", hdr + " header")["x"]
        out[hdr] = {n: (tcx + sx * hx, tcy + 24.13 - 2.54 * (n - 1)) for n in range(1, 21)}
    return out

def pose_on_pins(fp, want):
    """Rotation + position putting the footprint's pads onto `want` = {pad number: (x, y)} (uses the first and
    last pad as the anchors). Returns (x, y, rot_deg) and the worst pad miss in mm."""
    fp.SetOrientationDegrees(0); fp.SetPosition(V(0, 0))
    local = {p.GetNumber(): (TO(p.GetPosition().x), TO(p.GetPosition().y)) for p in fp.Pads()}
    a, b = str(min(want)), str(max(want))
    (lax, lay), (lbx, lby) = local[a], local[b]
    (wax, way), (wbx, wby) = want[int(a)], want[int(b)]
    ang = lambda dx, dy: math.atan2(-dy, dx)                           # KiCad rotation is CCW on screen (+Y down)
    th = ang(wbx - wax, wby - way) - ang(lbx - lax, lby - lay)
    R = lambda x, y: (x * math.cos(th) + y * math.sin(th), -x * math.sin(th) + y * math.cos(th))
    rx, ry = R(lax, lay); ox, oy = wax - rx, way - ry
    miss = max(math.hypot(ox + R(*local[str(n)])[0] - x, oy + R(*local[str(n)])[1] - y) for n, (x, y) in want.items())
    return (ox, oy, round(math.degrees(th), 3)), miss


# ---------------------------------------------------------------- USB-C at the top edge
USB_TAB_EDGE_GAP = 0.3     # mm between the receptacle's front shell-tab copper and the board edge
CC_GAP = 0.8               # mm between J7's signal-pad row and the CC resistors behind it

def usb_parts(nets):
    """(receptacle ref, {cc net: resistor ref}) found by net, not refdes."""
    by_ref = {}
    for (r, p), n in nets.items(): by_ref.setdefault(r, set()).add(n.split("/")[-1])
    j = [r for r, ns in by_ref.items() if r.startswith("J") and {"VBUS", "USBC_CC1", "USBC_CC2"} <= ns]
    rr = {cc: r for cc in ("USBC_CC1", "USBC_CC2") for r, ns in by_ref.items() if r.startswith("R") and cc in ns}
    return (j[0] if j else None), rr

def place_usb(fps, nets):
    """J7 at the top edge (centre), mouth up; R15/R16 right behind their own CC pins, CC pad toward J7.
    Returns report lines."""
    jref, rr = usb_parts(nets)
    if not jref: return ["  (no USB-C receptacle found)"]
    S = M.stretch(); side = M.layout(S)[0]; poly = M.silhouette(S); gx, gy = M.GBA_ORIGIN
    x_mid = M.XC + S / 2
    ys = [y0 + (x_mid - x0) * (y1 - y0) / (x1 - x0) for (x0, y0), (x1, y1) in zip(poly, poly[1:] + poly[:1]) if (x0 > x_mid) != (x1 > x_mid)]
    edge_y = gy + min(ys) + M.BOARD_INSET                               # board edge (top) at the centre line
    j = fps[jref]; j.SetOrientationDegrees(0); j.SetPosition(V(0, 0))
    tab_front = max(TO(p.GetBoundingBox().GetBottom()) for p in j.Pads() if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH)
    body_front = 5.09                                                   # courtyard front = receptacle mouth (rot 0, +Y)
    # rot 180: local +Y -> board -Y (mouth up). Front tab copper sits USB_TAB_EDGE_GAP inside the board edge.
    ox, oy = gx + x_mid, edge_y + USB_TAB_EDGE_GAP + tab_front
    j.SetOrientationDegrees(180); j.SetPosition(V(ox, oy))
    out = [f"  {jref:5s} USB-C at the top edge, mouth up: overhangs the board edge by {body_front - tab_front - USB_TAB_EDGE_GAP:.2f} mm "
           f"(mouth {M.BOARD_INSET - (body_front - tab_front - USB_TAB_EDGE_GAP):.2f} mm inside the shell's outer surface)"]
    sig = {p.GetNumber(): p for p in j.Pads() if p.GetAttribute() == pcbnew.PAD_ATTRIB_SMD}
    row_back = max(TO(p.GetBoundingBox().GetBottom()) for p in sig.values())   # board y of the signal pads' inner edge
    for cc, rref in rr.items():
        pad = next(p for p in sig.values() if p.GetNetname().split("/")[-1] == cc)
        px = TO(pad.GetPosition().x)
        r = fps[rref]; r.SetOrientationDegrees(0); r.SetPosition(V(0, 0))
        loc = {p.GetNumber(): (TO(p.GetPosition().x), TO(p.GetPosition().y), p.GetNetname().split("/")[-1], p) for p in r.Pads()}
        (ccn,) = [n for n, v in loc.items() if v[2] == cc]; (gn,) = [n for n in loc if n != ccn]
        pitch = math.hypot(loc[gn][0] - loc[ccn][0], loc[gn][1] - loc[ccn][1])
        half = max(TO(loc[ccn][3].GetBoundingBox().GetHeight()), TO(loc[ccn][3].GetBoundingBox().GetWidth())) / 2
        y_cc = row_back + CC_GAP + half
        (x, y, rot), miss = pose_on_pins(r, {int(ccn): (px, y_cc), int(gn): (px, y_cc + pitch)})
        r.SetOrientationDegrees(rot); r.SetPosition(V(x, y))
        out.append(f"  {rref:5s} {cc} Rd behind J7 pad {pad.GetNumber()}: CC pad {CC_GAP} mm behind the pad row, rot {rot:g}")
    return out


# ---------------------------------------------------------------- FPGA next to J3, behind the screen
HUB_J3_GAP = 3.0           # mm: routing channel between U2's pads and J3's pads (the display bus fans out across it)
HUB_BODY_MARGIN = 1.0      # mm inside the 2090's PCB body outline ("behind the screen")
RING_STANDOFF = 1.0        # mm from U2's pad edge to a passive's near pad edge
RING_DEPTH = 4.5           # mm of room kept for the passives ring on the sides that don't face J3
RING_PITCH_GAP = 0.9       # mm between neighbouring passives along a side (their 0.8 mm labels need it)
DISPLAY_NETS = [f"LCD_D{i}" for i in range(8)] + ["LCD_WR", "LCD_DC", "LCD_CS", "LCD_RST"]
POWER = lambda n: n == "GND" or n.startswith("+") or n.startswith("unconnected")

def place_fpga(fps, nets, comps, notes, hub="U2", sock="J3"):
    """U2 on the front, inside the 2090's body outline next to J3, at the rotation/position that minimises the total
    pin-to-pin distance of the display bus (LCD_WR via its series R: U2's LCD_WR_FPGA pin <-> J3's LCD_WR pin).
    Its passives ring the three sides that don't face J3 (that side stays a clear routing channel), each next to the
    U2 pin it serves, near pad toward the chip. Returns report lines."""
    S = M.stretch(); _, _, (mcx, mcy), _ = M.layout(S); gx, gy = M.GBA_ORIGIN
    tcx, tcy = gx + mcx, gy + mcy
    body = (tcx - 35.56 + HUB_BODY_MARGIN, tcy - 25.4 + HUB_BODY_MARGIN, tcx + 35.56 - HUB_BODY_MARGIN, tcy + 25.4 - HUB_BODY_MARGIN)
    net_of = lambda r, p: nets.get((r, p), "").split("/")[-1]
    J = fps[sock]; jpad = {net_of(sock, p.GetNumber()): (TO(p.GetPosition().x), TO(p.GetPosition().y)) for p in J.Pads()}
    jbox = [min(TO(p.GetBoundingBox().GetLeft()) for p in J.Pads()), min(TO(p.GetBoundingBox().GetTop()) for p in J.Pads()),
            max(TO(p.GetBoundingBox().GetRight()) for p in J.Pads()), max(TO(p.GetBoundingBox().GetBottom()) for p in J.Pads())]
    U = fps[hub]; U.SetOrientationDegrees(0); U.SetPosition(V(0, 0))
    uloc = {p.GetNumber(): (TO(p.GetPosition().x), TO(p.GetPosition().y)) for p in U.Pads()}
    ubb = [(TO(b.GetLeft()), TO(b.GetTop()), TO(b.GetRight()), TO(b.GetBottom())) for b in (p.GetBoundingBox() for p in U.Pads())]
    pairs = []
    for n in DISPLAY_NETS:
        un = n + "_FPGA" if n == "LCD_WR" else n
        up = [p for p in uloc if net_of(hub, p) == un]
        if up and n in jpad: pairs.append((up[0], jpad[n]))
    rot = lambda x, y, th: (x * math.cos(th) + y * math.sin(th), -x * math.sin(th) + y * math.cos(th))
    best = None
    for deg in (0, 90, 180, 270):
        th = math.radians(deg)
        rp = {n: rot(x, y, th) for n, (x, y) in uloc.items()}
        cs = [rot(x0, y0, th) for b in ubb for x0, y0 in ((b[0], b[1]), (b[2], b[3]))]
        l, t, r, b = min(c[0] for c in cs), min(c[1] for c in cs), max(c[0] for c in cs), max(c[1] for c in cs)
        x0 = max(body[0] - l, jbox[2] + HUB_J3_GAP - l); x1 = body[2] - RING_DEPTH - r
        y0 = body[1] + RING_DEPTH - t; y1 = body[3] - RING_DEPTH - b
        k = 0
        while x0 + k * 0.25 <= x1:
            cx = x0 + k * 0.25; k += 1; j = 0
            while y0 + j * 0.25 <= y1:
                cy = y0 + j * 0.25; j += 1
                cost = sum(math.hypot(cx + rp[u][0] - jx, cy + rp[u][1] - jy) for u, (jx, jy) in pairs)
                if best is None or cost < best[0]: best = (cost, deg, cx, cy, (l, t, r, b))
    def inversions(deg_):             # data-line crossings: order along U2 vs order along J3
        th_ = math.radians(deg_); d = [(rot(*uloc[u], th_), (jx, jy)) for u, (jx, jy) in pairs[:8]]
        a_ = [q[1] for _, q in sorted(d, key=lambda t: t[0][1])]      # J3 y in U2 pin order (by U2 y)
        return sum(1 for i in range(len(a_)) for j in range(i + 1, len(a_)) if a_[i] > a_[j])
    cost, deg, cx, cy, (l, t, r, b) = best
    U.SetOrientationDegrees(deg); U.SetPosition(V(cx, cy))
    L, T, R, B_ = cx + l, cy + t, cx + r, cy + b                         # chip pad box on the board
    out = [f"  {hub:5s} rot {deg}, centre ({cx:.2f},{cy:.2f}): display bus total {cost:.1f} mm over {len(pairs)} nets "
           f"(avg {cost / len(pairs):.1f} mm); {HUB_J3_GAP} mm channel to {sock} kept clear of passives",
           f"  data-line crossings (of 28 pairs) at this rotation: {inversions(deg)}; by rotation: "
           + ", ".join(f"{d_}: {inversions(d_)}" for d_ in (0, 90, 180, 270))]
    upos = {p.GetNumber(): (TO(p.GetPosition().x), TO(p.GetPosition().y)) for p in U.Pads()}
    # ---- anchor each passive of the hub's sheet to a U2 pin (or to another passive)
    group = [r for r in comps if comps[r]["sheetname"] == comps[hub]["sheetname"] and r != hub]
    def note_pin(r):
        m = re.search(r"pin (\d+)", notes.get(r, ""))
        if m: return m.group(1)
        for (rr, pn), f in PINFUNC.items():
            if rr == hub and f and re.search(r"\b" + re.escape(f.rsplit("_", 1)[0]) + r"\b", notes.get(r, ""), re.I):
                return pn
        return None
    anchor = {}
    for r in group:
        rn = [net_of(r, p.GetNumber()) for p in fps[r].Pads()]
        sig = [n for n in rn if not POWER(n)]
        shared = [pn for pn in upos if net_of(hub, pn) in sig]
        if r.startswith("C"):
            pn = note_pin(r) or next((pn for pn in upos if net_of(hub, pn) in rn and not net_of(hub, pn) == "GND"), None)
            if pn: anchor[r] = ("pin", pn, next(n for n in rn if n != "GND"))
        elif shared:
            anchor[r] = ("pin", shared[0], net_of(hub, shared[0]))
    for r in group:                   # chains: a passive whose signal net only meets another passive (R7 -> D2)
        if r in anchor: continue
        sig = [net_of(r, p.GetNumber()) for p in fps[r].Pads() if not POWER(net_of(r, p.GetNumber()))]
        mate = next((o for o in group if o != r and o in anchor and any(net_of(o, p.GetNumber()) in sig for p in fps[o].Pads())), None)
        if mate: anchor[r] = ("chain", mate, sig[0])
    for r in group:                   # last resort: the BOM note names the pin
        if r in anchor or not note_pin(r): continue
        pn = note_pin(r); rn = [net_of(r, p.GetNumber()) for p in fps[r].Pads()]
        anchor[r] = ("pin", pn, next((n for n in rn if n == net_of(hub, pn)), rn[0]))
    # ---- placement: every part anchored to its own pin.
    #  * power-only parts (decoupling caps + the PLL ferrite) go on the BACK, right behind their pin: a cap's rail pad
    #    sits under the pin's pad (a via off the pin's tip joins them) and its GND pad points inward under U2's
    #    exposed pad, onto the EP's thermal-via field = the shortest decoupling loop; FB2 sits just outside the edge
    #    beside its pin so +1V2 -> FB2 -> C20/C19 -> VCCPLL is a short chain. The front stays free for the bus.
    #  * signal parts (pull-ups, the CDONE LED + its resistor, the WR series R) go on the FRONT, radially off their pin.
    #  * along each side, parts that want the same spot are spread with the least total displacement from their pins
    #    (isotonic fit) — nothing is pushed along a row or round a corner.
    def side_of(x, y):
        d = {"left": abs(x - L), "right": abs(x - R), "top": abs(y - T), "bottom": abs(y - B_)}
        return min(d, key=d.get)
    NORM = {"left": (-1, 0), "right": (1, 0), "top": (0, -1), "bottom": (0, 1), "below": (0, 1)}
    def along(sd, x, y): return y if sd in ("left", "right") else x
    def at(sd, t, r_):                # point at along-edge coordinate t, distance r_ from the chip centre along the normal
        nx, ny = NORM[sd]
        return (cx + nx * r_, t) if sd in ("left", "right") else (t, cy + ny * r_)
    def pads_of(r):
        fp = fps[r]; fp.SetOrientationDegrees(0); fp.SetPosition(V(0, 0))
        return {pp.GetNumber(): pp for pp in fp.Pads()}
    def pad_dims(r):                  # (pad pitch, pad length along the axis, pad width across it) at rot 0
        pads = list(pads_of(r).values()); a_, b_ = pads[0], pads[1]
        pitch = math.hypot(TO(b_.GetPosition().x - a_.GetPosition().x), TO(b_.GetPosition().y - a_.GetPosition().y))
        bb = a_.GetBoundingBox(); w_, h_ = TO(bb.GetWidth()), TO(bb.GetHeight())
        return pitch, (w_ if abs(TO(b_.GetPosition().x - a_.GetPosition().x)) > 0.1 else h_), (h_ if abs(TO(b_.GetPosition().x - a_.GetPosition().x)) > 0.1 else w_)
    def pose(r, near_net, near_xy, far_xy, back):
        fp = fps[r]
        if fp.IsFlipped() != back: fp.Flip(fp.GetPosition(), False)
        pads = pads_of(r)
        nn = next((n for n in pads if net_of(r, n) == near_net), sorted(pads)[0]); fn = next(n for n in pads if n != nn)
        key = lambda n: int(n) if n.isdigit() else n
        (x, y, rt), _ = pose_on_pins(fp, {key(nn): near_xy, key(fn): far_xy})
        fp.SetOrientationDegrees(rt); fp.SetPosition(V(x, y))
        got = {pp.GetNumber(): (TO(pp.GetPosition().x), TO(pp.GetPosition().y)) for pp in fp.Pads()}
        err = max(math.hypot(got[nn][0] - near_xy[0], got[nn][1] - near_xy[1]), math.hypot(got[fn][0] - far_xy[0], got[fn][1] - far_xy[1]))
        if err > 0.01: sys.exit(f"{r}: pose miss {err:.3f} mm (flipped={back})")
    def spread(items, gap):           # items: [(want, order, ref, width)] on one side -> {ref: t}; least-squares spacing
        items = sorted(items); blocks = []
        for want, _, ref, w in items:
            blocks.append({"refs": [ref], "w": [w], "want": [want]})
            while len(blocks) > 1:
                a_, b_ = blocks[-2], blocks[-1]
                def offs(bl):
                    o, acc = [], 0.0
                    for i, w in enumerate(bl["w"]):
                        o.append(acc); acc += (w + bl["w"][i + 1]) / 2 + gap if i + 1 < len(bl["w"]) else 0
                    return o
                sa = sum(wt - o for wt, o in zip(a_["want"], offs(a_))) / len(a_["want"])
                end_a = sa + offs(a_)[-1] + a_["w"][-1] / 2 + gap + b_["w"][0] / 2
                sb = sum(wt - o for wt, o in zip(b_["want"], offs(b_))) / len(b_["want"])
                if end_a <= sb: break
                blocks.pop(); a_["refs"] += b_["refs"]; a_["w"] += b_["w"]; a_["want"] += b_["want"]
        res = {}
        for bl in blocks:
            o = []; acc = 0.0
            for i, w in enumerate(bl["w"]):
                o.append(acc); acc += (w + bl["w"][i + 1]) / 2 + gap if i + 1 < len(bl["w"]) else 0
            st = sum(wt - oo for wt, oo in zip(bl["want"], o)) / len(bl["want"])
            for ref, oo in zip(bl["refs"], o): res[ref] = st + oo
        return res
    _dims = {r: pad_dims(r) for r in anchor}      # measured ONCE (pad_dims resets the footprint to the origin)
    pad_dims = lambda r: _dims[r]
    pin_r = {}                        # each anchor pin: its side and pad geometry (distance of pad centre / tip from centre)
    for pn, (x, y) in upos.items():
        sd = side_of(x, y); nx, ny = NORM[sd]; pad = next(pp for pp in U.Pads() if pp.GetNumber() == pn); bb = pad.GetBoundingBox()
        ext = TO(bb.GetWidth()) if sd in ("left", "right") else TO(bb.GetHeight())
        rc = (x - cx) * nx + (y - cy) * ny
        pin_r[pn] = (sd, along(sd, x, y), rc, rc + ext / 2)
    power_only = lambda r: all(POWER(net_of(r, pp.GetNumber())) for pp in fps[r].Pads())
    back_caps = [r for r, a in anchor.items() if a[0] == "pin" and power_only(r) and r.startswith("C")]
    back_ferr = [r for r, a in anchor.items() if a[0] == "pin" and power_only(r) and not r.startswith("C")]
    front = [r for r in anchor if r not in back_caps and r not in back_ferr]
    where = {}
    # back caps: rail pad under the pin pad, GND pad inward
    by_side = {}
    for r in back_caps:
        sd, t, rc, tip = pin_r[anchor[r][1]]; pitch, plen, pw = pad_dims(r)
        by_side.setdefault(sd, []).append((t, r, r, pw))
    def farads(r):
        m = re.match(r"([\d.]+)\s*([pnuµm]?)F", comps[r]["value"])
        return float(m.group(1)) * {"p": 1e-12, "n": 1e-9, "u": 1e-6, "µ": 1e-6, "m": 1e-3, "": 1}[m.group(2)] if m else 1.0
    BACK_GAP = 0.35                   # mm between back caps' pads (JLC min 0.2); their silk outlines go to B.Fab below
    for sd, items in by_side.items():
        pos = spread(items, BACK_GAP)
        # caps sharing a pin: the smallest (the HF one) gets the slot nearest the pin, the bulk one the next
        for pn in {anchor[r][1] for _, _, r, _ in items}:
            mates = [r for _, _, r, _ in items if anchor[r][1] == pn]
            if len(mates) > 1:
                t_pin = pin_r[pn][1]; slots_ = sorted((pos[r] for r in mates), key=lambda v: abs(v - t_pin))
                for r, v in zip(sorted(mates, key=farads), slots_): pos[r] = v
        for _, _, r, _ in items:
            pitch, plen, pw = pad_dims(r); rc = pin_r[anchor[r][1]][2]
            pose(r, anchor[r][2], at(sd, pos[r], rc), at(sd, pos[r], rc + pitch), True); where[r] = ("back", sd)
    # corner fix-up: caps from two sides pointing inward can meet under a corner; slide the clashing cap along its own
    # side, away from the corner, by the smallest amount that clears every other back-side pad (0.1 mm steps)
    def bboxes_B(r):
        return [(TO(b.GetLeft()) - 0.1, TO(b.GetTop()) - 0.1, TO(b.GetRight()) + 0.1, TO(b.GetBottom()) + 0.1)
                for b in (pp.GetBoundingBox() for pp in fps[r].Pads() if pp.IsOnLayer(pcbnew.B_Cu))]
    def hit(r, others):
        mine = bboxes_B(r)
        return any(x[0] < y[2] and y[0] < x[2] and x[1] < y[3] and y[1] < x[3] for o in others for x in mine for y in bboxes_B(o))
    # ferrite: on the back at the nearest free spot (clear of the caps' pads and J3's through-hole pads) outside the
    # corner of the chip nearest its pin — the PLL filter's input side, a short +1V2_PLL hop from C19/C20
    for r in back_ferr:
        sd, t, rc, tip = pin_r[anchor[r][1]]; pitch, plen, pw = pad_dims(r)
        px, py = upos[anchor[r][1]]
        kx = L if px < cx else R; ky = T if py < cy else B_                  # nearest chip corner
        obst = [o for o in where] + [sock]
        best_ = None
        for k in range(0, 60):
            rad = 2.0 + 0.25 * k
            for j in range(16):
                ang = math.atan2(ky - cy, kx - cx) + (j - 8) * math.pi / 16
                qx, qy = kx + rad * math.cos(ang), ky + rad * math.sin(ang)
                for deg_ in (0, 90):
                    ux_, uy_ = (1, 0) if deg_ == 0 else (0, 1)
                    pose(r, anchor[r][2], (qx - ux_ * pitch / 2, qy - uy_ * pitch / 2), (qx + ux_ * pitch / 2, qy + uy_ * pitch / 2), True)
                    if not hit(r, obst):
                        best_ = (math.hypot(qx - px, qy - py), qx, qy, deg_); break
                if best_: break
            if best_: break
        if best_:
            _, qx, qy, deg_ = best_; ux_, uy_ = (1, 0) if deg_ == 0 else (0, 1)
            near = min(((qx - ux_ * pitch / 2, qy - uy_ * pitch / 2), (qx + ux_ * pitch / 2, qy + uy_ * pitch / 2)), key=lambda q: math.hypot(q[0] - px, q[1] - py))
            far = (2 * qx - near[0], 2 * qy - near[1])
            pose(r, anchor[r][2], near, far, True)
        where[r] = ("back", sd)
    # front signal parts: radial off their pin, near pad 0.6 mm past the pin's tip
    by_side = {}  # (front)
    order = 0
    for r in sorted(front, key=lambda r: (anchor[r][0] == "chain", r)):
        a = anchor[r]; pn = a[1] if a[0] == "pin" else anchor[a[1]][1]
        sd, t, rc, tip = pin_r[pn]; pitch, plen, pw = pad_dims(r); order += 1
        by_side.setdefault(sd, []).append((t, (a[1] if a[0] == "chain" else r) + ("~" if a[0] == "chain" else ""), r, pw + 0.5))
    for sd, items in by_side.items():
        pos = spread([(t, o, r, w) for t, o, r, w in items], 0.5)
        for _, _, r, _ in items:
            a = anchor[r]; pn = a[1] if a[0] == "pin" else anchor[a[1]][1]
            sd_, t, rc, tip = pin_r[pn]; pitch, plen, pw = pad_dims(r)
            nnet = a[2]
            if a[0] == "chain":       # R7: its pad on the shared net goes OUTER, beside its mate's outer pad
                nnet = next((net_of(r, pp.GetNumber()) for pp in fps[r].Pads() if net_of(r, pp.GetNumber()) != a[2]), a[2])
            r0 = tip + 1.0 + plen / 2  # 1 mm past the pin tip: the part's silk outline clears U2's
            pose(r, nnet, at(sd, pos[r], r0), at(sd, pos[r], r0 + pitch), False); where[r] = ("front", sd)
    # on-board check: every pad >= EDGE_COPPER inside the board (Edge.Cuts incl. the battery/speaker cutouts); a front
    # part that isn't (R29 under the battery notch) moves to the nearest valid spot near its pin — clear of other
    # pads and out of the J3 data channel — searched outward in 0.25 mm rings, both orientations
    EDGE_COPPER = 0.5
    board = M.board_shape(M.stretch(), M.fit(M.stretch()))
    inb = lambda x, y: board.Contains(pcbnew.VECTOR2I(int(MM(x)), int(MM(y))))
    def on_board(r):
        for pp in fps[r].Pads():
            b = pp.GetBoundingBox(); l_, t_, r2, b2 = TO(b.GetLeft()) - EDGE_COPPER, TO(b.GetTop()) - EDGE_COPPER, TO(b.GetRight()) + EDGE_COPPER, TO(b.GetBottom()) + EDGE_COPPER
            if not all(inb(x, y) for x, y in ((l_, t_), (r2, t_), (l_, b2), (r2, b2))): return False
        return True
    chan = (jbox[2], T, L, B_)                                         # the J3 <-> U2 data channel
    # the U2 <-> U1 (STM32) control channel: the band in front of U2's MCU-facing pins (PPU SPI CS/SCK/MOSI,
    # CRESET_B, CDONE — all on U2's right side, in the same order as on U1's left side) up to U1's pads, kept clear
    # of parts so the five lines run straight across
    mcu_nets = {"PPU_SPI_CS", "PPU_SPI_SCK", "PPU_SPI_MOSI", "FPGA_CRESET_B", "FPGA_CDONE"}
    mchans = []
    if "U1" in fps:
        hp = [TO(pp.GetPosition().y) for pp in U.Pads() if net_of(hub, pp.GetNumber()) in mcu_nets]
        mp = [TO(pp.GetBoundingBox().GetLeft()) for pp in fps["U1"].Pads() if net_of("U1", pp.GetNumber()) in mcu_nets]
        if hp and mp:
            mchans.append((R, min(hp) - 0.45, min(mp), max(hp) + 0.45))
    def front_boxes(r):
        return [(TO(b.GetLeft()) - 0.2, TO(b.GetTop()) - 0.2, TO(b.GetRight()) + 0.2, TO(b.GetBottom()) + 0.2)
                for b in (pp.GetBoundingBox() for pp in fps[r].Pads() if pp.IsOnLayer(pcbnew.F_Cu))]
    def front_ok(r):
        mine = front_boxes(r)
        if any(x[0] < c[2] and c[0] < x[2] and x[1] < c[3] and c[1] < x[3] for c in [chan, *mchans] for x in mine): return False
        others = [o for o in [hub, sock, *where] if o != r]
        return not any(x[0] < y[2] and y[0] < x[2] and x[1] < y[3] and y[1] < x[3] for o in others for x in mine for y in front_boxes(o))
    relocated = []
    for r, (lay, sd) in list(where.items()):
        if lay != "front" or (on_board(r) and front_ok(r)): continue
        a = anchor[r]; pn = a[1] if a[0] == "pin" else anchor[a[1]][1]; px, py = upos[pn]; pitch, plen, pw = pad_dims(r)
        nnet = a[2]; found = None
        for k in range(1, 81):
            rad = 0.25 * k; n_ = max(8, int(2 * math.pi * rad / 0.25))
            for j in range(n_):
                ang = 2 * math.pi * j / n_
                qx, qy = px + rad * math.cos(ang), py + rad * math.sin(ang)
                for ux_, uy_ in ((0, 1), (1, 0), (0, -1), (-1, 0)):
                    pose(r, nnet, (qx, qy), (qx + ux_ * pitch, qy + uy_ * pitch), False)
                    if on_board(r) and front_ok(r): found = rad; break
                if found: break
            if found: break
        relocated.append(f"{r} -> {found:.2f} mm from pin {pn}" if found else f"{r}: NO valid spot found")
        where[r] = ("front", "below" if found else sd)
    if relocated: out.append("  re-seated on the board: " + "; ".join(relocated))
    # labels: front parts radially outside (0.8 mm, on F.Silkscreen); back parts' refdes on B.Fab only (assembly
    # drawing) — under-chip caps are too dense for legible silk, and it keeps the back silk clean
    for r, (lay, sd) in where.items():
        t_ = fps[r].Reference(); t_.SetTextSize(V(0.8, 0.8)); t_.SetTextThickness(MM(0.12)); t_.SetTextAngleDegrees(0)
        if lay == "back":
            t_.SetLayer(pcbnew.B_Fab)
            # dense decoupling field: the easyeda 0603 silk box (0.25 mm line ~0.4 mm outside the pads) would overlap
            # its neighbours' at this pitch, so it moves to the assembly (fab) layer, as is usual for cap fields
            for g in fps[r].GraphicalItems():
                if g.GetLayer() == pcbnew.B_SilkS: g.SetLayer(pcbnew.B_Fab)
            continue
        pads = [pp for pp in fps[r].Pads()]; nx, ny = NORM[sd]
        far = max(pads, key=lambda pp: (TO(pp.GetPosition().x) - cx) * nx + (TO(pp.GetPosition().y) - cy) * ny)
        fx, fy = TO(far.GetPosition().x), TO(far.GetPosition().y); wlab = len(r) * 0.8 * 0.8
        t_.SetPosition(V(fx + nx * (0.9 + wlab / 2), fy + ny * (0.9 + 0.4)) if sd in ("left", "right") else V(fx, fy + ny * 1.3))
    hl = fps[hub].Reference(); hl.SetTextAngleDegrees(0)            # hub label: below its free bottom-right corner
    hl.SetPosition(V(R - TO(hl.GetTextSize().x) * len(hub) * 0.4, B_ + 0.3 + TO(hl.GetTextSize().y) / 2 + 0.2))
    RING_LABELED.update(where); RING_LABELED.add(hub)
    # distance report: the part's pad on the pin's net -> the pin
    for r in sorted(anchor, key=lambda r: (r[0], int(re.sub(r'\D', '', r)))):
        a = anchor[r]; pn = a[1] if a[0] == "pin" else anchor[a[1]][1]; px, py = upos[pn]
        pnet = net_of(hub, pn)
        cand = [pp for pp in fps[r].Pads() if net_of(r, pp.GetNumber()) == pnet] or list(fps[r].Pads())
        d = min(math.hypot(TO(pp.GetPosition().x) - px, TO(pp.GetPosition().y) - py) for pp in cand)
        lay, sd = where.get(r, ("?", "?"))
        out.append(f"      {r:4s} {lay:5s} -> U2 pin {pn:>2s} ({pin_r[pn][0]:6s} {pnet:13s}) {d:4.2f} mm" + (f"  [after {a[1]}]" if a[0] == "chain" else ""))
    # clash check per copper side (SMD pads on their own side; THT pads, e.g. J3, on both)
    def boxes(r, side):
        res = []
        for pp in fps[r].Pads():
            onF, onB = pp.IsOnLayer(pcbnew.F_Cu), pp.IsOnLayer(pcbnew.B_Cu)
            if (side == "F" and onF) or (side == "B" and onB):
                b = pp.GetBoundingBox(); res.append((TO(b.GetLeft()) - 0.1, TO(b.GetTop()) - 0.1, TO(b.GetRight()) + 0.1, TO(b.GetBottom()) + 0.1))
        return res
    near_refs = [hub, sock, *where]
    clash = set()
    for side in ("F", "B"):
        bx = {r: boxes(r, side) for r in near_refs}
        for a_ in near_refs:
            for b_ in near_refs:
                if a_ < b_ and any(x[0] < y[2] and y[0] < x[2] and x[1] < y[3] and y[1] < x[3] for x in bx[a_] for y in bx[b_]):
                    if {a_, b_} != {hub} and not (a_ == hub and b_ in back_caps) and not (b_ == hub and a_ in back_caps):
                        clash.add(f"{a_}/{b_}({side})")
    out.insert(2, "  pad clashes: " + (", ".join(sorted(clash)) if clash else "none") +
               f"; back: {len(back_caps) + len(back_ferr)} (decoupling + ferrite), front: {len(front)}")
    miss = [r for r in group if r not in anchor]
    if miss: out.append(f"  UNANCHORED: {miss}")
    return {hub, *anchor}, out


# ---------------------------------------------------------------- Bluetooth module in the top-left corner (back)
# BT830 datasheet §12.1: module at the board edge, "preferably in the corner with the antenna facing the corner", no
# copper in the antenna keep-out on any layer. Its own Figure 18 (Ezurio's layout) is the model: the antenna end at
# one board edge, the keep-out's long side toward the other edge across a narrow strip that carries the decoupling
# caps under the supply pins, and the keep-out continued to both edges. Footprint frame (rot 0, front): module body
# x 0..12.85, y -8.5..0; pins 1-7 on the x=0 side, 8-14 on the y=0 side, the antenna + keep-out at the x=12.85 end
# against the y=0 side (x 7.08..12.85, y -6.3..0). On the back the only orientation putting antenna end AND keep-out
# side into the top-left corner is: +x -> up (antenna at the top), +y -> left (strip toward the left edge), so the
# UART/EN pins face down, toward the D-pad and the rest of the board.
BT = "U12"
BT_KEEPOUT = (7.08, -6.3, 12.85, 0.0)          # footprint frame
BT_EXT = 8.0                                   # mm the keep-out is continued past the module (clipped by the board edge)
# caps, footprint frame: (ref, pad on the module's supply net, its centre, GND pad centre). 0603 pads 0.8 x 0.9 at
# 1.4 mm pitch; the strip row sits BT_CAP_GAP off the module's pad tips (they end at y = +0.345 / x = -0.345) — the
# castellations are hand-soldered, so the iron needs room at every pad tip (and the solder fillet climbs out there).
#   C47 4.7u  VREG_OUT_HV(pin 10) -> straight under pin 10, its GND pad under GND pin 12 (the LDO-stability loop)
#   C49 100n  VREG_IN_HV(pin 9)   -> under pin 9, GND pad under GND pin 30; EN (pin 8) leaves between its pads
#   C50 4.7u  3.3 V reservoir     -> second row, 3V3 pad right under C49's (feed -> C50 -> C49 -> pin 9), clear of EN
#   C48 100n  VDD_PADS(pin 1)     -> beside pin 1 on the pins-1-7 side, GND pad toward GND pin 2; 2.05 out so its
#                                    pads clear the footprint's pin-1 silk dot (centre x -1.2, outer r 0.3)
BT_CAP_GAP = 0.8
BT_ROW1, BT_ROW2 = 0.345 + BT_CAP_GAP + 0.45, 0.345 + BT_CAP_GAP + 0.9 + 0.35 + 0.45
BT_CAPS = [("C47", "BT_VREG18", (2.95, BT_ROW1), (4.35, BT_ROW1)),
           ("C49", "+3V3", (1.80, BT_ROW1), (0.40, BT_ROW1)),
           ("C50", "+3V3", (1.80, BT_ROW2), (3.20, BT_ROW2)),
           ("C48", "+3V3", (-2.05, -6.93), (-2.05, -5.53))]
BT_EDGE_COPPER = 0.5                           # pad -> board edge
BT_SCREW_CLEAR = 3.5                           # module / caps courtyard -> shell-screw hole centre

def place_bt(B, fps, nets):
    net_of = lambda r, p: nets.get((r, p), "").split("/")[-1]
    U = fps[BT]
    if not U.IsFlipped(): U.Flip(U.GetPosition(), False)
    # local -> board map for the back side: try the four rotations, keep the one with +x -> up, +y -> left
    loc = {}
    for line in open(os.path.join(HERE, "../../../../cad/symbols/manual.pretty/XCVR_BT830-SA-01-T_R.kicad_mod")):
        m = re.search(r'\(pad "?(\w+)"? smd \w+ \(at ([-\d.]+) ([-\d.]+)[^)]*\) \(size ([-\d.]+) ([-\d.]+)\)', line)
        if m: loc[m.group(1)] = tuple(float(v) for v in m.groups()[1:])
    A = None
    for deg in (0, 90, 180, 270):
        U.SetOrientationDegrees(deg); U.SetPosition(V(0, 0))
        got = {p.GetNumber(): (TO(p.GetPosition().x), TO(p.GetPosition().y)) for p in U.Pads()}
        # columns of A from pads 1 -> 15 (dx 11.42, dy -1.55) and 1 -> 30 (dx 0.22, dy 6.30)
        (x1, y1), (x15, y15), (x30, y30) = got["1"], got["15"], got["30"]
        det = 11.42 * 6.30 - (-1.55) * 0.22
        ax = ((x15 - x1) * 6.30 - (x30 - x1) * (-1.55)) / det, ((y15 - y1) * 6.30 - (y30 - y1) * (-1.55)) / det
        ay = ((x30 - x1) * 11.42 - (x15 - x1) * 0.22) / det, ((y30 - y1) * 11.42 - (y15 - y1) * 0.22) / det
        if abs(ax[1] + 1) < 0.01 and abs(ay[0] + 1) < 0.01:
            A = (deg, ax, ay, (x1 - (ax[0] * 0.33 + ay[0] * -6.63), y1 - (ax[1] * 0.33 + ay[1] * -6.63))); break
    if A is None: sys.exit("BT830: no back-side rotation puts the antenna up and the keep-out side left")
    deg, ax, ay, o0 = A
    to_b = lambda t, x, y: (t[0] + o0[0] + ax[0] * x + ay[0] * y, t[1] + o0[1] + ax[1] * x + ay[1] * y)

    S = M.stretch(); board = M.board_shape(S, M.fit(S))
    def shrunk(m):
        s = pcbnew.SHAPE_POLY_SET(board); s.Inflate(MM(-m), pcbnew.CORNER_STRATEGY_ROUND_ALL_CORNERS, MM(0.005)); return s
    in05, in03 = shrunk(BT_EDGE_COPPER), shrunk(0.3)
    inside = lambda s, x, y: s.Contains(V(x, y))
    o = board.Outline(0); pts = [(TO(o.CPoint(i).x), TO(o.CPoint(i).y)) for i in range(o.PointCount())]
    segs = [(a_, b_) for a_, b_ in zip(pts, pts[1:] + pts[:1]) if min(a_[0], b_[0]) < 80 and min(a_[1], b_[1]) < 80]
    def edge_d(x, y):
        best_ = 1e9
        for (ax_, ay_), (bx_, by_) in segs:
            dx, dy = bx_ - ax_, by_ - ay_; L2 = dx * dx + dy * dy
            u = 0 if L2 == 0 else max(0, min(1, ((x - ax_) * dx + (y - ay_) * dy) / L2))
            best_ = min(best_, math.hypot(ax_ + u * dx - x, ay_ + u * dy - y))
        return best_
    rect = lambda l, t, r, b: [(l, t), (r, t), (r, b), (l, b)]
    # local geometry that must be on the board: module pads + cap pads (0.5 to the edge), the module body (0.3)
    pad_rects = [rect(x - w / 2, y - h / 2, x + w / 2, y + h / 2) for x, y, w, h in loc.values()]
    cap_rects = []
    for _, _, (nx_, ny_), (fx_, fy_) in BT_CAPS:
        horiz = abs(ny_ - fy_) < 1e-6
        for cx_, cy_ in ((nx_, ny_), (fx_, fy_)):
            cap_rects.append(rect(cx_ - (0.4 if horiz else 0.45), cy_ - (0.45 if horiz else 0.4), cx_ + (0.4 if horiz else 0.45), cy_ + (0.45 if horiz else 0.4)))
    body = rect(0, -8.5, 12.85, 0)
    k0, k1 = BT_KEEPOUT[:2], BT_KEEPOUT[2:]
    keep = [(k0[0], k0[1]), (k1[0], k0[1]), (k1[0] + BT_EXT, k0[1]), (k1[0] + BT_EXT, k1[1] + BT_EXT), (k0[0], k1[1] + BT_EXT)]
    # foreign copper: every pad of every other footprint already on the board (front SMD pads count — the keep-out
    # is all-layer); back/THT pads also must not sit under the module or the caps
    gx, gy = M.GBA_ORIGIN; sil = M.silhouette(S)
    mine = {BT, *[c[0] for c in BT_CAPS]}
    on_brd = [f for r, f in fps.items() if r not in mine and M.inside((TO(f.GetPosition().x) - gx, TO(f.GetPosition().y) - gy), sil)]
    foreign = [(TO(b.GetLeft()), TO(b.GetTop()), TO(b.GetRight()), TO(b.GetBottom()), p.IsOnLayer(pcbnew.B_Cu))
               for f in on_brd for p in f.Pads() for b in (p.GetBoundingBox(),)]
    side = M.layout(S)[0]                         # shell screws (board holes) move with their half, as drawn
    screws = [(x, y) for _, x, y in M.screws(S)]       # the shell screws the design keeps
    def poly_hits_box(poly, bx, m):    # axis-aligned board-frame bbox of poly (all our shapes are axis-aligned there)
        xs = [p[0] for p in poly]; ys = [p[1] for p in poly]
        return min(xs) - m < bx[2] and bx[0] < max(xs) + m and min(ys) - m < bx[3] and bx[1] < max(ys) + m
    def ok(t):
        for poly in pad_rects + cap_rects:
            if not all(inside(in05, *to_b(t, x, y)) for x, y in poly): return False
        if not all(inside(in03, *to_b(t, x, y)) for x, y in body): return False
        kb = [to_b(t, x, y) for x, y in keep]
        under = [[to_b(t, x, y) for x, y in pr] for pr in [rect(-0.6, -9.1, 13.1, 0.6)] + cap_rects]
        for bx in foreign:
            if poly_hits_box(kb, bx, 0.3): return False
            if bx[4] and any(poly_hits_box(u, bx, 0.2) for u in under): return False
        for sx_, sy_ in screws:
            for u in under:
                xs = [p[0] for p in u]; ys = [p[1] for p in u]
                if math.hypot(max(min(xs), min(sx_, max(xs))) - sx_, max(min(ys), min(sy_, max(ys))) - sy_) < BT_SCREW_CLEAR: return False
        return True
    # into the corner: antenna-end centre + keep-out-side centre + the keep-out's outer corner as close to the edge as
    # the constraints allow
    cost = lambda t: edge_d(*to_b(t, 12.85, -3.15)) + edge_d(*to_b(t, 9.97, 0.0)) + edge_d(*to_b(t, 12.85, 0.0))
    best = None
    for step, span, ctr in ((0.5, None, None), (0.05, 1.0, None)):
        if span is None: cand = [(x * step, y * step) for x in range(int(20 / step), int(60 / step)) for y in range(int(34 / step), int(75 / step))]
        else:
            c0 = best[1]; n = int(span / step)
            cand = [(c0[0] + i * step, c0[1] + j * step) for i in range(-n, n + 1) for j in range(-n, n + 1)]
        for t in cand:
            # t = where local (0,0) lands (board mm); to_b's o0 is folded in by subtracting it
            tt = (t[0] - o0[0], t[1] - o0[1])
            c = cost(tt)
            if (best is None or c < best[0]) and ok(tt): best = (c, t)
    if best is None: sys.exit("BT830: no valid spot in the top-left corner")
    t = (best[1][0] - o0[0], best[1][1] - o0[1])
    want = {int(n): to_b(t, loc[n][0], loc[n][1]) for n in ("1", "30")}
    (x, y, rt), miss = pose_on_pins(U, want)
    U.SetOrientationDegrees(rt); U.SetPosition(V(x, y))
    if miss > 0.01: sys.exit(f"BT830 pose miss {miss:.3f}")
    out = [f"  {BT} BT830 on the back at ({x:.2f},{y:.2f}) rot {rt:g}: antenna end {edge_d(*to_b(t, 12.85, -3.15)):.2f} mm and "
           f"keep-out side {edge_d(*to_b(t, 9.97, 0.0)):.2f} mm from the board edge"]
    for r, nnet, near, far in BT_CAPS:
        fp = fps[r]
        if not fp.IsFlipped(): fp.Flip(fp.GetPosition(), False)
        pads_ = {p.GetNumber(): p for p in fp.Pads()}
        nn = next(n for n in pads_ if net_of(r, n) == nnet); fn = next(n for n in pads_ if n != nn)
        (cx_, cy_, crt), cm = pose_on_pins(fp, {int(nn): to_b(t, *near), int(fn): to_b(t, *far)})
        fp.SetOrientationDegrees(crt); fp.SetPosition(V(cx_, cy_))
        if cm > 0.01: sys.exit(f"{r}: pose miss {cm:.3f}")
        for g in fp.GraphicalItems():               # dense strip: silk box -> B.Fab (as for the FPGA's back caps)
            if g.GetLayer() == pcbnew.B_SilkS: g.SetLayer(pcbnew.B_Fab)
        lab = fp.Reference(); lab.SetLayer(pcbnew.B_Fab); lab.SetTextSize(V(0.6, 0.6)); lab.SetTextThickness(MM(0.1))
        lab.SetTextAngleDegrees(0); lab.SetPosition(fp.GetPosition())
        fp.Value().SetVisible(False)                # its Fab value text would sprawl over the module's pads
        sp = {p.GetNumber(): p for p in fp.Pads()}
        mod_pin = next(p for p in U.Pads() if net_of(BT, p.GetNumber()) == nnet and
                       (nnet != "+3V3" or p.GetNumber() == ("1" if r == "C48" else "9")))
        d = math.hypot(TO(sp[nn].GetPosition().x - mod_pin.GetPosition().x), TO(sp[nn].GetPosition().y - mod_pin.GetPosition().y))
        out.append(f"      {r:4s} back -> U12 pin {mod_pin.GetNumber():>2s} ({nnet:9s}) {d:4.2f} mm")
    RING_LABELED.update({BT, *[c[0] for c in BT_CAPS]})
    ul = U.Reference(); ul.SetLayer(pcbnew.B_SilkS); ul.SetTextAngleDegrees(0)
    uv = U.Value(); uv.SetLayer(pcbnew.B_Fab); uv.SetTextSize(V(0.8, 0.8)); uv.SetTextThickness(MM(0.12))
    uv.SetTextAngleDegrees(90); uv.SetPosition(V(*to_b(t, 6.4, -4.25)))    # on the body, along it (assembly drawing)
    ul.SetPosition(V(*to_b(t, 6.4, -11.0)))         # beside the module's pins-15-28 side (silk under the module would be hidden)
    # the keep-out, continued to the board edge(s): a board-level rule area on every copper layer (reused if present)
    z = next((z for z in B.Zones() if z.GetIsRuleArea() and z.GetZoneName() == "BT830 antenna keep-out"), None)
    if z is None:
        z = pcbnew.ZONE(B); z.SetIsRuleArea(True); z.SetZoneName("BT830 antenna keep-out"); B.Add(z)
    z.SetDoNotAllowTracks(True); z.SetDoNotAllowVias(True); z.SetDoNotAllowPads(True); z.SetDoNotAllowZoneFills(True)
    z.SetDoNotAllowFootprints(False)
    allcu = pcbnew.LSET(); [allcu.AddLayer(l) for l in (pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.In2_Cu, pcbnew.B_Cu)]
    z.SetLayerSet(allcu)                                # explicit: AllCuMask() gets clamped to the layers that exist
    ol = z.Outline(); ol.RemoveAllContours(); ol.NewOutline()
    for x_, y_ in keep: ol.Append(V(*to_b(t, x_, y_)))
    ol.BooleanIntersection(board)
    out.append(f"  keep-out rule area: footprint keep-out continued {BT_EXT:g} mm past the module, clipped to the board")
    return {BT, *[c[0] for c in BT_CAPS]}, out


# ---------------------------------------------------------------- charger (BQ24074) below the USB-C
# TI §12.1: IN and OUT caps (and BAT's) as close as possible with short runs to IN/OUT/thermal pad; the programming
# resistors at their pins; high-current IN/OUT/BAT paths short and wide. U9 sits on the front right under J7 at rot 0,
# so its sides face: top = IN/TMR/ITERM/ISET (VBUS arrives from J7 straight above), right = CHG/OUT/OUT/ILIM (OUT ->
# Q1 -> SYS), left = TS/BAT/BAT/CE (BAT -> J8 at the cell), bottom = EN2/EN1/PGOOD/VSS. Positions in U9's frame
# (rot 0, front; pin tips at +-1.925): {pad: (x, y)}. Spacings: >= 0.5 mm from the pin tips, >= 0.24 mm pad-to-pad.
#   C36 IN cap   horizontal over pin 13: VBUS pad right above IN (the J7 trace lands on it, then drops into the pin)
#   C37 OUT cap  radial off OUT 10/11; C38 BAT cap radial off BAT 2/3 — the TI layout's arrangement
#   R18 ISET / R21 TMR above their pins, R19 ILIM off pin 12, R17 (EN2 strap) / R23 (PGOOD pull-up) under theirs
#   R22 (CHG pull-up) under the CHG run, which leaves pin 9 and drops at x 2.15 inside C37's near pad
#   Q1 (system switch) just below C37: SYS_SRC drops from C37's near pad onto S; D (SYS) points down to the loads;
#   R30 (gate pull-up) right above Q1 between G and the SYS_SRC drop
CHG_HUB = "U9"
# Floorplan: the power section lives in the 12.5 mm strip along the top edge either side of J7 (above the 2090's
# centre body), so the board's centre band stays free for the MCU. U9 goes right of J7 at rot 180 (the table below is
# in U9's own rot-0 frame and gets rotated): IN faces down/left toward J7's VBUS pins, BAT faces right toward J8
# (~11 mm), OUT faces left; SYS_SRC drops from C37 through vias and runs on the back to Q1, which sits in the gap
# under J7 on the way to the buck-boost left of J7.
CHG_AT = (107.5, 39.5)          # U9 centre: cluster 0.5 mm right of J7's shell tabs, 0.5 mm left of J8's courtyard
CHG_ROT = 180
CHG_LOCAL = {
    "C36": {1: (1.0, -2.825), 2: (2.4, -2.825)},
    "R21": {1: (-0.1, -2.83), 2: (-0.1, -4.33)},
    "R18": {1: (-1.2, -2.83), 2: (-1.2, -4.33)},
    "R19": {1: (2.83, -1.6), 2: (4.33, -1.6)},
    "C37": {1: (3.13, 0.0), 2: (5.13, 0.0)},
    "C38": {1: (-3.13, 0.0), 2: (-5.13, 0.0)},
    "R17": {1: (-0.9, 2.83), 2: (-0.9, 4.33)},
    "R23": {1: (0.6, 2.83), 2: (0.6, 4.33)},
    "R22": {1: (2.15, 2.83), 2: (2.15, 4.33)},
}
# Q1 (system switch) + R30 (its gate pull-up), board frame: in the channel under J7, between J7's CC resistors and the
# charger. S (from the charger) right, D (SYS, to the buck-boost's input caps) left; G under S, R30 beside G-S.
CHG_SWITCH = {
    "Q1":  {1: (99.7, 44.35), 2: (99.7, 42.45)},   # G, S (D lands 2.3 left of their midpoint)
    "R30": {1: (101.4, 44.35), 2: (101.4, 42.85)}, # gate pull-up G -> S
    # soft-start (#82): R31 from SW1 into the gate right under G; C51 gate -> drain (Miller) beside it. The gate node
    # (Q1.G, R31, R30, C51) is one ~2 mm cluster; R31's SW1 end continues to SW1 through the left-grip corridor.
    "R31": {2: (99.7, 45.9), 1: (99.7, 47.4)},     # Q1_GATE up, PWR_SW down
    "C51": {1: (101.4, 45.9), 2: (101.4, 47.3)},   # Q1_GATE up, SYS down
}
# J8 (battery JST-PH, mouth +Y at rot 0): under the 2090, at the battery cutout's top edge (moved 2026-10-10 from the
# top strip): it is 5.5 mm tall and the gap under the module is >= 8.35 mm (11 mm to the module PCB, minus its tallest
# back-side parts ~2.65), so it needn't avoid the screen. Mouth down, set back CHG_PLUG_BACK so the mated plug's rear
# (where the leads leave) sits at the cutout edge and the leads drop straight onto the cell; 5 mm clear of the IMU
# block (ST's connector guideline), 2.3 mm clear of J4. Cost: BAT from U9 ~11 -> ~20 mm (fine at 1 A on 1 mm).
CHG_PLUG_BACK = 3.5
CHG_J8_X = 126.2
# R20 (TS NTC) can't sit under the cell any more (the cell is in a cutout): it goes on the back, at the cutout's top
# edge, ~20 mm from U9 so the charger's heat doesn't read as a warm cell
CHG_NTC_X = 114.0

def place_charger(fps, nets):
    net_of = lambda r, p: nets.get((r, p), "").split("/")[-1]
    out = []
    def put(r, want, back=False):
        fp = fps[r]
        if fp.IsFlipped() != back: fp.Flip(fp.GetPosition(), False)
        (x, y, rt), miss = pose_on_pins(fp, want)
        fp.SetOrientationDegrees(rt); fp.SetPosition(V(x, y))
        if miss > 0.01: sys.exit(f"{r}: pose miss {miss:.3f} mm")
    ux, uy = CHG_AT
    U = fps[CHG_HUB]
    if U.IsFlipped(): U.Flip(U.GetPosition(), False)
    U.SetOrientationDegrees(CHG_ROT); U.SetPosition(V(ux, uy))
    c_, s_ = round(math.cos(math.radians(CHG_ROT))), round(math.sin(math.radians(CHG_ROT)))
    rot = lambda x, y: (c_ * x + s_ * y, -s_ * x + c_ * y)                # KiCad rotation (CCW on screen, +Y down)
    upin = {p.GetNumber(): (TO(p.GetPosition().x), TO(p.GetPosition().y)) for p in U.Pads()}
    for r, pads in CHG_LOCAL.items():
        put(r, {n: (ux + rot(x, y)[0], uy + rot(x, y)[1]) for n, (x, y) in pads.items()})
    for r, pads in CHG_SWITCH.items():
        put(r, pads)
    d = {p.GetNumber(): TO(p.GetPosition().x) for p in fps["Q1"].Pads()}
    if d["3"] > d["2"]: sys.exit("Q1: drain landed toward the charger")
    S = M.stretch(); pos = M.fit(S); gx, gy = M.GBA_ORIGIN
    bw, bh, bb = M.BATTERY; cut_top = gy + pos["battery"][1] - bh / 2 - bb
    J = fps["J8"]
    if J.IsFlipped(): J.Flip(J.GetPosition(), False)
    J.SetOrientationDegrees(0); J.SetPosition(V(0, 0))
    mouth = max(TO(b.GetBottom()) for b in (g.GetBoundingBox() for g in J.GraphicalItems() if g.GetLayer() == pcbnew.F_CrtYd))
    J.SetPosition(V(CHG_J8_X, cut_top - CHG_PLUG_BACK - mouth))
    # NTC on the back at the cutout edge (pads 0.5 + its 0.43 half-width from the edge)
    put("R20", {1: (CHG_NTC_X - 0.75, cut_top - 1.25), 2: (CHG_NTC_X + 0.75, cut_top - 1.25)}, back=True)
    for g in fps["R20"].GraphicalItems():
        if g.GetLayer() == pcbnew.B_SilkS: g.SetLayer(pcbnew.B_Fab)
    # silk: at this density the easyeda 0603/0805 outlines (0.25 mm lines ~0.4 mm outside the pads) and refdes overlap
    # each other and the pads, so the small passives' outlines + refdes go to F.Fab (assembly drawing), as is usual for
    # a tight decoupling cluster; U9, Q1 and J8 keep silk labels
    for r in [*CHG_LOCAL, "R30", "R31", "C51"]:
        fp = fps[r]
        for g in fp.GraphicalItems():
            if g.GetLayer() == pcbnew.F_SilkS: g.SetLayer(pcbnew.F_Fab)
        lab = fp.Reference(); lab.SetLayer(pcbnew.F_Fab); lab.SetTextSize(V(0.5, 0.5)); lab.SetTextThickness(MM(0.08))
        lab.SetTextAngleDegrees(0); lab.SetPosition(fp.GetPosition())
        fp.Value().SetVisible(False)
    ul = U.Reference(); ul.SetLayer(pcbnew.F_SilkS); ul.SetTextAngleDegrees(0); ul.SetPosition(V(ux + 3.0, uy + 4.0))
    ql = fps["Q1"].Reference(); ql.SetLayer(pcbnew.F_SilkS); ql.SetTextAngleDegrees(0); ql.SetPosition(V(103.6, 46.6))   # right of the soft-start pair
    for r in ("R15", "R16"):                         # J7's CC resistors: Q1 now sits right under their silk labels
        if r in fps:
            lab = fps[r].Reference(); lab.SetLayer(pcbnew.F_Fab); lab.SetTextSize(V(0.5, 0.5)); lab.SetTextThickness(MM(0.08))
            lab.SetPosition(fps[r].GetPosition())
    RING_LABELED.update([*CHG_LOCAL, "R30", "R31", "C51", "Q1", "R15", "R16"]); RING_LABELED.add(CHG_HUB)
    # report: each part's pad on its U9 net -> that U9 pin
    for r in [*CHG_LOCAL, *CHG_SWITCH]:
        best = None
        for p in fps[r].Pads():
            n = net_of(r, p.GetNumber())
            for pn, (px, py) in upin.items():
                if net_of(CHG_HUB, pn) == n and n not in ("GND", ""):
                    dd = math.hypot(TO(p.GetPosition().x) - px, TO(p.GetPosition().y) - py)
                    if best is None or dd < best[0]: best = (dd, pn, n)
        out.append(f"      {r:4s} -> U9 pin {best[1]:>2s} ({best[2]:10s}) {best[0]:4.2f} mm" if best else f"      {r:4s} (no U9 net)")
    jb = fps["J8"].GetPosition()
    out.insert(0, f"  U9 at ({ux:.2f},{uy:.2f}) rot {CHG_ROT}; J8 at ({TO(jb.x):.2f},{TO(jb.y):.2f}) mouth down at the cutout edge, {CHG_PLUG_BACK:g} mm set-back; R20 NTC on the back at the cutout edge, x {CHG_NTC_X:g}")
    return {CHG_HUB, "J8", "R20", *CHG_LOCAL, *CHG_SWITCH}, out


# ---------------------------------------------------------------- 3.3 V buck-boost (TPS63021) right of the charger
# TI §10 / Figure 31: input caps, output caps and the inductor as close as possible; wide, short power paths; the
# power ground (input + output cap returns) a single node at the IC; FB at the control side, away from the switch
# nodes. Same arrangement as TI's figure: U10 at rot 90 so its pin rows become columns — left column (top->bottom)
# L1 L1 VIN VIN EN PS PG, right column L2 L2 VOUT VOUT FB GND VINA; the inductor above the IC across both L pin
# pairs; the 2 x 10 uF input caps left of VIN and the 3 x 22 uF output caps right of VOUT, standing vertically with
# their GND pads up, so both returns meet in one tight PGND area under/beside the inductor; R24 (FB -> VOUT tie)
# off FB, sensing at the output caps; C39 (VINA bypass) off VINA; R25 (PS/SYNC pull-down) off PS. EN shares the
# VIN caps' SYS pad (EN = SYS). Coordinates: TI frame = board-aligned, U10 centre (0,0), pin tips at x +-1.8.
BB_HUB = "U10"
# Floorplan: in the top strip LEFT of J7 (see the charger note), TI's arrangement turned 180 (U10 at rot 270): VIN
# caps on the right toward Q1/J7, VOUT caps on the left toward the FPGA / Bluetooth / MCU side, the inductor below the
# IC (toward the 2090's body, away from the edge), FB/VINA/PS parts on top. BB_LOCAL is TI's frame; it's negated.
BB_AT = (82.0, 37.0)            # VIN caps 2.7 mm left of J7's shell tab (room for the SYS run up their right side)
BB_FLIP = -1
BB_LOCAL = {
    "L2":  {1: (-2.05, -4.85), 2: (2.05, -4.85)},   # body (5.4 x 5.2) 0.25 mm above the IC body: shortest switch nodes
    # caps start 0.25 mm outside the inductor body's +-2.7 sides (their GND pads reach up beside it), 0.3 mm apart
    "C40": {1: (-3.75, -0.55), 2: (-3.75, -2.55)},
    "C41": {1: (-5.4, -0.55), 2: (-5.4, -2.55)},
    "C42": {1: (3.75, -0.55), 2: (3.75, -2.55)},
    "C43": {1: (5.4, -0.55), 2: (5.4, -2.55)},
    "C44": {1: (7.05, -0.55), 2: (7.05, -2.55)},
    "R24": {1: (2.5, 0.9), 2: (4.0, 0.9)},
    "C39": {1: (2.75, 2.2), 2: (4.15, 2.2)},       # clear of U10's pin-1 silk mark
    "R25": {1: (-2.65, 1.0), 2: (-4.15, 1.0)},
}

def place_buckboost(fps, nets):
    net_of = lambda r, p: nets.get((r, p), "").split("/")[-1]
    def put(r, want):
        fp = fps[r]
        if fp.IsFlipped(): fp.Flip(fp.GetPosition(), False)
        (x, y, rt), miss = pose_on_pins(fp, want)
        fp.SetOrientationDegrees(rt); fp.SetPosition(V(x, y))
        if miss > 0.01: sys.exit(f"{r}: pose miss {miss:.3f} mm")
    bx, by = BB_AT
    f_ = BB_FLIP
    put(BB_HUB, {1: (bx + f_ * 1.45, by + f_ * 1.5), 14: (bx - f_ * 1.45, by + f_ * 1.5)})   # VINA / PG corners
    upin = {p.GetNumber(): (TO(p.GetPosition().x), TO(p.GetPosition().y)) for p in fps[BB_HUB].Pads()}
    if not ((upin["8"][0] - bx) * f_ < 0 and (upin["8"][1] - by) * f_ < 0 and (upin["7"][0] - bx) * f_ > 0): sys.exit("U10: pin map not as TI's figure")
    for r, pads in BB_LOCAL.items():
        put(r, {n: (bx + f_ * x, by + f_ * y) for n, (x, y) in pads.items()})
    for g in fps["L2"].GraphicalItems():              # its body outline would overlap U10's silk 0.25 mm below
        if g.GetLayer() == pcbnew.F_SilkS: g.SetLayer(pcbnew.F_Fab)
    for r in BB_LOCAL:                                # dense cluster: small passives' silk + refdes -> F.Fab
        if r == "L2": continue
        fp = fps[r]
        for g in fp.GraphicalItems():
            if g.GetLayer() == pcbnew.F_SilkS: g.SetLayer(pcbnew.F_Fab)
        lab = fp.Reference(); lab.SetLayer(pcbnew.F_Fab); lab.SetTextSize(V(0.5, 0.5)); lab.SetTextThickness(MM(0.08))
        lab.SetTextAngleDegrees(0); lab.SetPosition(fp.GetPosition()); fp.Value().SetVisible(False)
    ll = fps["L2"].Reference(); ll.SetLayer(pcbnew.F_SilkS); ll.SetTextAngleDegrees(0)
    ll.SetPosition(V(bx, by + f_ * -(4.85 + 2.6 + 0.8)))   # just past the inductor body, on the side away from U10
    ul = fps[BB_HUB].Reference(); ul.SetLayer(pcbnew.F_SilkS); ul.SetTextAngleDegrees(0); ul.SetPosition(V(bx - 5.0, by + 4.2))
    RING_LABELED.update(BB_LOCAL); RING_LABELED.add(BB_HUB)
    out = [f"  U10 TPS63021 at ({bx:.2f},{by:.2f}) rot {fps[BB_HUB].GetOrientationDegrees():g} (TI Fig. 31 arrangement: L2 above, VIN caps left, VOUT caps right)"]
    q1d = next(p for p in fps["Q1"].Pads() if net_of("Q1", p.GetNumber()) == "SYS") if "Q1" in fps else None
    for r in BB_LOCAL:
        best = None
        for p in fps[r].Pads():
            n = net_of(r, p.GetNumber())
            for pn, (px, py) in upin.items():
                if net_of(BB_HUB, pn) == n and n not in ("GND", ""):
                    dd = math.hypot(TO(p.GetPosition().x) - px, TO(p.GetPosition().y) - py)
                    if best is None or dd < best[0]: best = (dd, pn, n)
        out.append(f"      {r:4s} -> U10 pin {best[1]:>2s} ({best[2]:8s}) {best[0]:4.2f} mm" if best else f"      {r}")
    if q1d:
        c40 = next(p for p in fps["C40"].Pads() if net_of("C40", p.GetNumber()) == "SYS")
        out.append(f"  SYS: Q1 drain -> C40 {math.hypot(TO(q1d.GetPosition().x - c40.GetPosition().x), TO(q1d.GetPosition().y - c40.GetPosition().y)):.1f} mm")
    return {BB_HUB, *BB_LOCAL}, out


# ---------------------------------------------------------------- MCU (STM32F411, LQFP-64) in the centre band
# Floorplan: U1 between the FPGA (left) and the free right half (NOR / IMU / expander later), under the 2090 body,
# at rot 180 so the PPU-SPI / CRESET / CDONE side (footprint right) faces the FPGA; footprint bottom
# (BT UART on USART2 + BT_EN, HSE, NRST, VDDA, charger status, BAT_SENSE) faces up — BT lines at its left end, toward
# the BT830; footprint top (console USART1, PCA9555 INT, I2S2, SWDIO,
# LITE, PS/SYNC) faces down; footprint left (SPI3 + NOR_CS on PD2, SWCLK, I2C1, BOOT0) faces right, toward U4.
# Decoupling as on U2 (and on ST's own boards): 0603s on the BACK, radial, rail pad right under its VDD pin's pad (a
# via off the pin tip joins them), GND pad inward under the package onto the GND via field that also ties the VSS
# pins (front GND island under the body) — shortest loop, and the front stays free for the pin escapes. Positions
# are in the footprint's rot-0 frame: (side pin or None, t along the side, pads {n: r from centre}).
#   C1 100n VDD19 (+ C8 4.7u bulk next along the side), C7 VCAP 4.7u at pin 30 and C2 100n VDD32 (spread 1.26 apart),
#   C3 100n VDD48, C4 100n VDD64, C5 100n VBAT1 (slid 0.75 along the side: C4 and C5 meet in the corner),
#   VDDA: C9 10n at pin 13 then C10 1u, FB1 (3V3 -> 3V3A) past the end of the row; C6 NRST at pin 7, R1 BOOT0 at 60.
# Front: the HSE crystal right at OSC_IN/OSC_OUT (pins 5/6): R3 (0 R series, OSC_OUT) straight out from pin 6, Y1
# beside it, C11/C12 load caps at the crystal's own pads, GND pads of Y1 guard-grounded (routing).
MCU = "U1"
MCU_AT, MCU_ROT = (88.0, 55.5), 180   # 1.5 mm up from 57.0 (2026-10-10): the bottom edge's escape band to the
                                       # battery cutout was only 1.3 mm for ten signals
MCU_BACK = {   # ref: (pin it serves, t offset along the side from that pin, near-pad net)
    "C1": ("19", 0.0, "+3V3"), "C8": ("19", -1.26, "+3V3"),
    "C7": ("30", 0.26, "VCAP1"), "C2": ("32", 0.0, "+3V3"),
    "C3": ("48", 0.0, "+3V3"), "C4": ("64", 0.0, "+3V3"), "C5": ("1", 0.75, "+3V3"),
    "C9": ("13", 0.0, "+3V3A"), "C10": ("13", 1.25, "+3V3A"), "FB1": ("13", 2.5, "+3V3A"),
    "C6": ("7", 0.0, "NRST"), "R1": ("60", 0.0, "BOOT0"),
}
MCU_BACK_R = (5.7,)            # near (rail) pad centre, mm from the U1 centre (= the pin pad centre); inner pad = one pitch in
MCU_FRONT = {  # footprint rot-0 frame (board frame = rotated by MCU_ROT about U1's centre)
    "R3":  {1: (-1.25, 7.5), 2: (-1.25, 9.0)},              # HSE_OUT_MCU (pin 6 side) -> HSE_OUT (crystal side)
    "Y1":  {1: (-4.38, 8.4), 3: (-2.62, 10.6)},             # OSC1 (HSE_IN) by pin 5 / OSC2 (HSE_OUT) by R3
    "C11": {1: (-5.7, 8.4), 2: (-5.7, 9.24)},               # HSE_IN load, beside OSC1
    "C12": {1: (-2.62, 11.9), 2: (-3.46, 11.9)},            # HSE_OUT load, beside OSC2
}
# Debug headers + power LED (board frame): outside the 2090's outline, where they're reachable with the shell open —
# the right grip under the GBA's power-LED window, above the A/B buttons, clear of the J4 ear and the R shoulder.
MCU_ABS = {
    "D1": {1: (143.39, 44.7), 2: (144.89, 44.7)},           # power LED centred on the GBA power-LED window
    "R2": {2: (146.65, 44.7), 1: (148.15, 44.7)},           # anode-side pad next to D1's anode
    "J1": {1: (139.0, 47.0), 5: (139.0, 57.16)},            # SWD, vertical (pin 1 = 3V3 at the top)
    "J2": {1: (142.3, 47.0), 3: (142.3, 52.08)},            # UART console
}

def place_mcu(fps, nets):
    net_of = lambda r, p: nets.get((r, p), "").split("/")[-1]
    def put(r, want, back=False):
        fp = fps[r]
        if fp.IsFlipped() != back: fp.Flip(fp.GetPosition(), False)
        (x, y, rt), miss = pose_on_pins(fp, want)
        fp.SetOrientationDegrees(rt); fp.SetPosition(V(x, y))
        if miss > 0.01: sys.exit(f"{r}: pose miss {miss:.3f} mm")
    U = fps[MCU]
    if U.IsFlipped(): U.Flip(U.GetPosition(), False)
    U.SetOrientationDegrees(0); U.SetPosition(V(0, 0))
    loc = {p.GetNumber(): (TO(p.GetPosition().x), TO(p.GetPosition().y)) for p in U.Pads()}
    ux, uy = MCU_AT
    U.SetOrientationDegrees(MCU_ROT); U.SetPosition(V(ux, uy))
    c_, s_ = round(math.cos(math.radians(MCU_ROT))), round(math.sin(math.radians(MCU_ROT)))
    to_b = lambda x, y: (ux + c_ * x + s_ * y, uy - s_ * x + c_ * y)
    def side(pn):                                    # (unit normal, unit tangent, t of the pin) in the rot-0 frame
        x, y = loc[pn]
        if abs(abs(x) - 5.7) < 0.1: return ((1 if x > 0 else -1, 0), (0, 1), y)
        return ((0, 1 if y > 0 else -1), (1, 0), x)
    for r, (pn, dt, nnet) in MCU_BACK.items():
        (nx, ny), (tx, ty), t = side(pn); t += dt
        pads_ = {p.GetNumber(): p for p in fps[r].Pads()}
        nn = next(n for n in pads_ if net_of(r, n) == nnet); fn = next(n for n in pads_ if n != nn)
        rn = MCU_BACK_R[0]
        pitch = math.hypot(TO(pads_[nn].GetPosition().x - pads_[fn].GetPosition().x), TO(pads_[nn].GetPosition().y - pads_[fn].GetPosition().y))
        rf = rn - pitch                              # 0603 C: 1.4, 0603 R / ferrite: 1.5 / 1.4
        put(r, {int(nn): to_b(nx * rn + tx * t, ny * rn + ty * t), int(fn): to_b(nx * rf + tx * t, ny * rf + ty * t)}, back=True)
        for g in fps[r].GraphicalItems():
            if g.GetLayer() == pcbnew.B_SilkS: g.SetLayer(pcbnew.B_Fab)
        lab = fps[r].Reference(); lab.SetLayer(pcbnew.B_Fab); lab.SetTextSize(V(0.5, 0.5)); lab.SetTextThickness(MM(0.08))
        lab.SetTextAngleDegrees(0); lab.SetPosition(fps[r].GetPosition()); fps[r].Value().SetVisible(False)
    for r, pads in MCU_FRONT.items():
        put(r, {n: to_b(x, y) for n, (x, y) in pads.items()})
        if True:                                     # crystal block is dense too: Y1's outline would sit on C12/R3
            for g in fps[r].GraphicalItems():
                if g.GetLayer() == pcbnew.F_SilkS: g.SetLayer(pcbnew.F_Fab)
            lab = fps[r].Reference(); lab.SetLayer(pcbnew.F_Fab); lab.SetTextSize(V(0.5, 0.5)); lab.SetTextThickness(MM(0.08))
            lab.SetTextAngleDegrees(0); lab.SetPosition(fps[r].GetPosition()); fps[r].Value().SetVisible(False)
    for r, pads in MCU_ABS.items():
        put(r, pads)
    # header / LED labels beside the parts (upright), off their outlines
    for r, (dx, dy) in {"J1": (-2.6, 5.08), "J2": (2.6, 2.54), "D1": (0.0, -1.6), "R2": (0.0, -1.6)}.items():
        fp = fps[r]; a = MCU_ABS[r]; ax = sum(v[0] for v in a.values()) / 2; ay = sum(v[1] for v in a.values()) / 2
        lab = fp.Reference(); lab.SetLayer(pcbnew.F_SilkS); lab.SetTextAngleDegrees(90 if r.startswith("J") else 0)
        lab.SetPosition(V(a[min(a)][0] + dx, a[min(a)][1] + dy) if r.startswith("J") else V(ax + dx, ay + dy))
    RING_LABELED.update(MCU_ABS)
    RING_LABELED.update([*MCU_BACK, *MCU_FRONT, MCU])
    ul = U.Reference(); ul.SetLayer(pcbnew.F_SilkS); ul.SetTextAngleDegrees(0); ul.SetPosition(V(ux, uy))   # inside the pad ring
    # report: each part's pad on its U1 net -> the nearest U1 pin on that net
    out = [f"  U1 STM32F411 at ({ux:.2f},{uy:.2f}) rot {MCU_ROT}: decoupling on the back under its pins, HSE crystal on the front at pins 5/6"]
    upin = {p.GetNumber(): (TO(p.GetPosition().x), TO(p.GetPosition().y)) for p in U.Pads()}
    for r in [*MCU_BACK, *MCU_FRONT]:
        best = None
        for p in fps[r].Pads():
            n = net_of(r, p.GetNumber())
            for pn, (px, py) in upin.items():
                if net_of(MCU, pn) == n and n not in ("GND", ""):
                    dd = math.hypot(TO(p.GetPosition().x) - px, TO(p.GetPosition().y) - py)
                    if best is None or dd < best[0]: best = (dd, pn, n)
        if r == "Y1" or best is None:
            hs = [p for p in fps[r].Pads() if net_of(r, p.GetNumber()) in ("HSE_IN", "HSE_OUT")]
            out.append(f"      {r:4s} (crystal) OSC1 -> pin 5 {math.hypot(TO(hs[0].GetPosition().x) - upin['5'][0], TO(hs[0].GetPosition().y) - upin['5'][1]):.2f} mm" if hs else f"      {r}")
        else:
            out.append(f"      {r:4s} -> U1 pin {best[1]:>2s} ({best[2]:11s}) {best[0]:4.2f} mm")
    return {MCU, *MCU_BACK, *MCU_FRONT, *MCU_ABS}, out


# ---------------------------------------------------------------- U2 <-> U1 control lane: its pull-ups + CDONE LED
# The five MCU<->FPGA control lines (PPU SPI CS/SCK/MOSI, CRESET_B, CDONE) run straight across from U2's right side to
# U1's left side in the same order, so the lane between them stays clear of parts. Their passives, by hand:
#   CDONE is the bottom line of the bundle -> its pull-up R5 and the CDONE LED (D2, cathode on CDONE; R7 from +3V3 to
#   the anode) sit on the front just below the lane, tapping CDONE without crossing anything;
#   CRESET_B is the next line up, so a front pull-up would have to cross CDONE -> R4 goes on the BACK under the lane's
#   lower edge, reached through one via (a DC pull-up — the via costs nothing).
FPGA_LANE_PARTS = {   # ref: ({pad: (x, y)}, back)
    "R5": ({1: (70.0, 61.6), 2: (70.0, 63.1)}, False),      # CDONE pull-up, vertical, CDONE pad up
    "D2": ({1: (72.2, 61.6), 2: (72.2, 63.1)}, False),      # cathode (CDONE) up, anode down
    "R7": ({2: (73.9, 63.1), 1: (75.4, 63.1)}, False),      # anode pad beside D2's anode, +3V3 pad outward
    "R4": ({1: (74.0, 61.0), 2: (75.5, 61.0)}, True),       # CRESET_B pull-up on the back (via from the CRESET line)
}

def place_fpga_lane(fps):
    out = []
    for r, (pads, back) in FPGA_LANE_PARTS.items():
        fp = fps[r]
        if fp.IsFlipped() != back: fp.Flip(fp.GetPosition(), False)
        (x, y, rt), miss = pose_on_pins(fp, pads)
        fp.SetOrientationDegrees(rt); fp.SetPosition(V(x, y))
        if miss > 0.01: sys.exit(f"{r}: pose miss {miss:.3f} mm")
        for g in fp.GraphicalItems():                # tight against U2: outline + refdes on the assembly layer
            if g.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS): g.SetLayer(pcbnew.B_Fab if back else pcbnew.F_Fab)
        lab = fp.Reference(); lab.SetLayer(pcbnew.B_Fab if back else pcbnew.F_Fab); lab.SetTextSize(V(0.5, 0.5))
        lab.SetTextThickness(MM(0.08)); lab.SetTextAngleDegrees(0); lab.SetPosition(fp.GetPosition()); fp.Value().SetVisible(False)
    RING_LABELED.update(FPGA_LANE_PARTS)
    out.append("  U2<->U1 lane: R5 / D2+R7 on the front just below it (CDONE), R4 on the back (CRESET_B, one via)")
    return out


# ---------------------------------------------------------------- LCD 5 V boost (MT3608) in the top strip, left
# Floorplan: with the other two converters on the top-edge power strip, in the free stretch between the 2090's JP1
# ear and the buck-boost; output (+5V_LCD, ~0.1 A) faces left toward the clear lane left of J3 that carries it down
# to J3 pins 6 and 2, input (SYS) faces right toward the buck-boost / Q1.
# Arrangement = the MT3608 datasheet's Figure 3: U5 turned so its pins form columns (left: SW, GND, FB top->bottom;
# right: NC, IN, EN); L1 straight above it from SYS (right pad) to SW (left pad, 0.7 mm above pin 1); D3's anode
# right beside L1's SW pad, cathode out left -> the SW node is just pin 1 + two adjacent pads; C28 (Cout) standing
# under D3's cathode, its GND pad level with U5's GND pin so the output loop SW -> D3 -> C28 -> GND -> pin 2 is one
# short run under nothing; C27 (Cin) right of IN with EN tied to it; the FB divider R10/R9 at the FB pin, below the
# GND run, away from the SW node (R9's 5 V end via to VOUT, as the figure's "via to VOUT").
LB_HUB = "U5"
LB_AT = (69.8, 39.3)           # x: between the JP1 ear (59.7) and the buck-boost's left caps (74.3); y: L1 0.5 mm off the edge
LB_LOCAL = {                    # board-aligned, U5 centre (0,0); U5 pin tips at x +-1.685
    "U5":  {1: (-1.15, -0.95), 6: (1.15, -0.95)},  # SW top-left, NC top-right
    "L1":  {2: (-1.6, -3.7), 1: (1.6, -3.7)},      # BST_SW left, SYS right; body 0.25 mm above U5's
    "D3":  {2: (-3.7, -3.7), 1: (-8.1, -3.7)},     # anode (SW) 0.3 mm from L1's SW pad, cathode (+5V_LCD) left
    "C28": {1: (-8.1, -1.4), 2: (-8.1, 0.6)},      # Cout: 5 V pad under the cathode (clear of D3's outline), GND pad ~level with U5's GND pin
    "C27": {1: (2.9, -0.4), 2: (2.9, 1.6)},        # Cin: SYS pad beside IN (and L1's SYS pad above it)
    "R10": {1: (-2.6, 1.6), 2: (-2.6, 3.1)},       # FB -> GND (R2), at the FB pin
    "R9":  {2: (-4.0, 1.6), 1: (-5.5, 1.6)},       # 5 V -> FB (R1)
}

def place_lcdboost(fps, nets):
    net_of = lambda r, p: nets.get((r, p), "").split("/")[-1]
    bx, by = LB_AT
    for r, pads in LB_LOCAL.items():
        fp = fps[r]
        if fp.IsFlipped(): fp.Flip(fp.GetPosition(), False)
        (x, y, rt), miss = pose_on_pins(fp, {n: (bx + px, by + py) for n, (px, py) in pads.items()})
        fp.SetOrientationDegrees(rt); fp.SetPosition(V(x, y))
        if miss > 0.01: sys.exit(f"{r}: pose miss {miss:.3f} mm")
        if r in ("C27", "C28", "R9", "R10"):          # dense: small passives' outline + refdes -> F.Fab
            for g in fp.GraphicalItems():
                if g.GetLayer() == pcbnew.F_SilkS: g.SetLayer(pcbnew.F_Fab)
            lab = fp.Reference(); lab.SetLayer(pcbnew.F_Fab); lab.SetTextSize(V(0.5, 0.5)); lab.SetTextThickness(MM(0.08))
            lab.SetTextAngleDegrees(0); lab.SetPosition(fp.GetPosition()); fp.Value().SetVisible(False)
    up = {p.GetNumber(): p for p in fps[LB_HUB].Pads()}
    chk = {n: (TO(up[n].GetPosition().x) - bx, TO(up[n].GetPosition().y) - by) for n in ("1", "2", "3", "5")}
    if not (chk["1"][0] < 0 and chk["1"][1] < chk["2"][1] < chk["3"][1] and chk["5"][0] > 0): sys.exit("U5: pin map not as the MT3608 figure")
    for g in fps["L1"].GraphicalItems():              # its outline would sit on U5's, 0.25 mm below
        if g.GetLayer() == pcbnew.F_SilkS: g.SetLayer(pcbnew.F_Fab)
    for r, (dx, dy) in {"U5": (0.0, 2.6), "L1": (0.0, -3.7), "D3": (-5.6, -1.5)}.items():   # L1: on its body between the pads; D3: just below its body
        lab = fps[r].Reference(); lab.SetLayer(pcbnew.F_SilkS); lab.SetTextAngleDegrees(0); lab.SetPosition(V(bx + dx, by + dy))
    RING_LABELED.update(LB_LOCAL)
    out = [f"  U5 MT3608 at ({bx:.2f},{by:.2f}) (datasheet Fig. 3 arrangement: L1 above, D3 + Cout left, Cin right, FB divider below-left)"]
    for r in LB_LOCAL:
        if r == LB_HUB: continue
        best = None
        for p in fps[r].Pads():
            n = net_of(r, p.GetNumber())
            for pn, pp in up.items():
                if net_of(LB_HUB, pn) == n and n not in ("GND", ""):
                    dd = math.hypot(TO(p.GetPosition().x - pp.GetPosition().x), TO(p.GetPosition().y - pp.GetPosition().y))
                    if best is None or dd < best[0]: best = (dd, pn, n)
        out.append(f"      {r:4s} -> U5 pin {best[1]} ({best[2]:8s}) {best[0]:4.2f} mm" if best else f"      {r:4s} (output side)")
    return set(LB_LOCAL), out


# ---------------------------------------------------------------- NOR flash (W25Q128, SOIC-8) at U1's SPI3 side
# Right next to U1's SPI3 pins (51-53) and NOR /CS (PD2, pin 54 — moved off PB0 on the far side for this), on the
# front, pins 1-4 (CS, DO, WP, GND) as the column facing U1 so CS and DO land straight across; DI/CLK sit on the far
# column (the SOIC pinout splits CS/DO from DI/CLK, so with U1's fixed MOSI-MISO-SCK order one of them takes a via
# or the under-body route — unavoidable). C24 right at VCC (pin 8, next to HOLD which ties to it); R8 (/CS pull-up)
# right on the CS run, just above pin 1. The shared SPI3 run to the FPGA's config pins leaves the same U1 side.
NOR = "U4"
NOR_AT = (102.6, 59.5)         # left pad column 3.5 mm right of U1's pin tips: CS/DO stay direct (~5.4 mm) while the
                               # escape lane in front of U1's 16-pad side widens to ~5.4 mm for the config bus + I2C1
NOR_LOCAL = {                   # board-aligned, U4 centre (0, 0)
    "U4":  {1: (-3.53, -1.9), 8: (3.53, -1.9)},    # pin 1 top-left, pin 8 (VCC) top-right
    "C24": {1: (5.35, -1.9), 2: (6.75, -1.9)},     # VCC decoupling, 0.3 mm beyond pin 8's pad, GND pad outward
    "R8":  {1: (-2.6, -3.5), 2: (-1.1, -3.5)},     # /CS pull-up just above pin 1's corner, clear of U4's pin-1 mark
}

def place_nor(fps, nets):
    nx, ny = NOR_AT
    for r, pads in NOR_LOCAL.items():
        fp = fps[r]
        if fp.IsFlipped(): fp.Flip(fp.GetPosition(), False)
        (x, y, rt), miss = pose_on_pins(fp, {n: (nx + px, ny + py) for n, (px, py) in pads.items()})
        fp.SetOrientationDegrees(rt); fp.SetPosition(V(x, y))
        if miss > 0.01: sys.exit(f"{r}: pose miss {miss:.3f} mm")
        if r != NOR:
            for g in fp.GraphicalItems():
                if g.GetLayer() == pcbnew.F_SilkS: g.SetLayer(pcbnew.F_Fab)
            lab = fp.Reference(); lab.SetLayer(pcbnew.F_Fab); lab.SetTextSize(V(0.5, 0.5)); lab.SetTextThickness(MM(0.08))
            lab.SetTextAngleDegrees(0); lab.SetPosition(fp.GetPosition()); fp.Value().SetVisible(False)
    ul = fps[NOR].Reference(); ul.SetLayer(pcbnew.F_SilkS); ul.SetTextAngleDegrees(0); ul.SetPosition(V(nx, ny))   # between the pad columns
    RING_LABELED.update(NOR_LOCAL)
    net_of = lambda r, p: nets.get((r, p), "").split("/")[-1]
    up = {net_of("U1", p.GetNumber()): p for p in fps["U1"].Pads()}
    out = [f"  U4 W25Q128 at ({nx:.2f},{ny:.2f}), pins 1-4 facing U1's SPI3 side"]
    for p in fps[NOR].Pads():
        n = net_of(NOR, p.GetNumber())
        if n in ("NOR_CS", "SPI3_MISO", "SPI3_MOSI", "SPI3_SCK"):
            q = up[n]; out.append(f"      {n:10s} U4 pin {p.GetNumber()} <- U1 pin {q.GetNumber():>2s}: {math.hypot(TO(p.GetPosition().x - q.GetPosition().x), TO(p.GetPosition().y - q.GetPosition().y)):5.2f} mm")
    return set(NOR_LOCAL), out


# ---------------------------------------------------------------- haptics (DRV2605L + LRA pads) beside the LRA
# Driver at the actuator, not at the MCU: OUT+/OUT- carry the drive pulses AND the back-EMF the DRV2605L senses for
# closed-loop auto-resonance, so they stay ~3 mm; the I2C pair (slow, open-drain, multi-drop) takes the long run to
# U1. Pocket: between the LRA outline (Ø8 + 1 buffer at draw_mech's LRA_AT, pushed out toward the grip edge) and
# Start/Select, with ~2.5 mm to every button pad (the pocket under the LRA holds the GBA's bottom-left shell screw). U8 at rot 0: OUT+ (7) / GND (8) / OUT- (9)
# face up and fan out left to J6 (pads stacked toward the motor: red + on top, blue - below, no crossing),
# REG/SCL/SDA/IN-TRIG/EN face down; SCL/SDA leave downward toward
# the gap between Start and Select. C33 (REG, 1 uF mandatory) at pin 1, nudged left so SCL drops past it; C34 (100 n)
# at VDD pin 10 on the motor side; C35 (10 uF bulk — sources each motor pulse locally) level with VDD/NC pin 6,
# running out right above SW8 (Start)'s top pads.
HAP_HUB = "U8"
HAP_AT = (38.5, 84.5)           # centred in the gap between the LRA buffer (x 33.5) and Start's pads (x 46.0); J6 3.7 mm
                                # below the D-pad's down switch, C33 4.5 mm above the power switch
HAP_LOCAL = {                   # board-aligned, U8 centre (0, 0); U8 pad rows at y +-2.35 (tips +-3.0)
    "U8":  {1: (-1.0, 2.35), 10: (-1.0, -2.35)},
    "J6":  {1: (-3.5, -6.54), 2: (-3.5, -4.0)},    # LRA pads stacked on the motor side, facing it: each lead comes straight
                                                   # in from the LRA, neither crosses the other's pad; LRA_P (red) on top —
                                                   # the outer of U8's top-row fan-out (OUT+ pin 7), LRA_N (blue) below it
    "C33": {1: (-1.5, 3.75), 2: (-1.5, 5.15)},     # HAP_REG under pin 1 (offset 0.5 left: SCL's escape)
    "C34": {1: (-2.4, -1.6), 2: (-2.4, -0.2)},     # VDD HF, beside pin 10, motor side (clear of U8's silk)
    "C35": {1: (2.2, -2.35), 2: (4.2, -2.35)},     # VDD bulk, level with pin 6, above Start's pads (they reach x 45.99)
}

def place_haptics(fps, nets):
    net_of = lambda r, p: nets.get((r, p), "").split("/")[-1]
    hx, hy = HAP_AT
    for r, pads in HAP_LOCAL.items():
        fp = fps[r]
        if fp.IsFlipped(): fp.Flip(fp.GetPosition(), False)
        (x, y, rt), miss = pose_on_pins(fp, {n: (hx + px, hy + py) for n, (px, py) in pads.items()})
        fp.SetOrientationDegrees(rt); fp.SetPosition(V(x, y))
        if miss > 0.01: sys.exit(f"{r}: pose miss {miss:.3f} mm")
        if r.startswith("C"):
            for g in fp.GraphicalItems():
                if g.GetLayer() == pcbnew.F_SilkS: g.SetLayer(pcbnew.F_Fab)
            lab = fp.Reference(); lab.SetLayer(pcbnew.F_Fab); lab.SetTextSize(V(0.5, 0.5)); lab.SetTextThickness(MM(0.08))
            lab.SetTextAngleDegrees(0); lab.SetPosition(fp.GetPosition()); fp.Value().SetVisible(False)
    up = {p.GetNumber(): p for p in fps[HAP_HUB].Pads()}
    if net_of(HAP_HUB, "7") != "LRA_P" or TO(up["7"].GetPosition().y) > hy: sys.exit("U8: OUT+ not on the top row")
    for r, (dx, dy, ang) in {"U8": (0.0, 0.0, 90), "J6": (-1.0, -6.54, 0)}.items():   # J6's label right of its + pad
        lab = fps[r].Reference(); lab.SetLayer(pcbnew.F_SilkS); lab.SetTextAngleDegrees(ang); lab.SetPosition(V(hx + dx, hy + dy))
    if "SW8" in fps:                                  # Start's label sat where C35's GND pad now is
        sl = fps["SW8"].Reference(); p8 = [p for p in fps["SW8"].Pads()]
        place_sw8_label(fps)
    RING_LABELED.update(HAP_LOCAL)
    out = [f"  U8 DRV2605L at ({hx:.2f},{hy:.2f}) beside the LRA outline; J6 (LRA leads) above OUT+/OUT-"]
    for r in HAP_LOCAL:
        if r == HAP_HUB: continue
        best = None
        for p in fps[r].Pads():
            n = net_of(r, p.GetNumber())
            for pn, pp in up.items():
                if net_of(HAP_HUB, pn) == n and n not in ("GND", ""):
                    dd = math.hypot(TO(p.GetPosition().x - pp.GetPosition().x), TO(p.GetPosition().y - pp.GetPosition().y))
                    if best is None or dd < best[0]: best = (dd, pn, n)
        out.append(f"      {r:4s} -> U8 pin {best[1]:>2s} ({best[2]:8s}) {best[0]:4.2f} mm")
    sx, sy = M.GBA_ORIGIN; side = M.layout(M.stretch())[0]
    lx, ly = sx + side(M.LRA_AT[0]), sy + M.LRA_AT[1]; d, bf = M.LRA
    near = min(math.hypot(TO(p.GetPosition().x) - lx, TO(p.GetPosition().y) - ly) - TO(max(p.GetSize().x, p.GetSize().y)) / 2
               for r in HAP_LOCAL for p in fps[r].Pads())
    out.append(f"  nearest copper to the LRA centre: {near:.2f} mm (outline + buffer radius {d / 2 + bf:g})")
    return set(HAP_LOCAL), out


# ---------------------------------------------------------------- battery sense (BAT/2 divider -> PA0) behind U1
# The divider tap is the high-impedance node (~50 kOhm source) -> it sits at the ADC pin and the long run goes on the
# stiff BAT end (a dedicated ~21 uA sense line from J8 pin 1, so the U9 -> J8 current doesn't add trace drop). On the
# BACK: the front just above BAT_SENSE's pin (PC0 = U1 pin 8 since the 2026-10-10 UART swap; it was PA0, pin 14) is
# the fan-out of U1's top-side signals (BT UART, BT_EN, charger status, NRST), the back there is empty. PC0 drops
# through one via just off its tip (staggered above the NRST via of pin 7 next door); C45 (100 nF, the ADC S/H
# reservoir) right at that via, R27 (bottom, to GND) beside it, R26 (top) above R27 with its BAT pad outermost — away
# from the sensitive node, toward the BAT sense line. In1 (GND) shields it from the HSE crystal on the front.
BS_LOCAL = {                    # board frame, all on the back
    "C45": ({1: (88.25, 46.85), 2: (88.25, 45.45)}, True),   # BAT_SENSE pad by the PC0 via (~88.25, 47.9), GND pad up
    "R27": ({1: (86.85, 46.85), 2: (85.35, 46.85)}, True),   # BAT_SENSE beside C45, GND end outward
    "R26": ({2: (86.85, 45.45), 1: (86.85, 43.95)}, True),   # BAT_SENSE end on the node, BAT end outermost
}

def place_batsense(fps, nets):
    for r, (pads, back) in BS_LOCAL.items():
        fp = fps[r]
        if fp.IsFlipped() != back: fp.Flip(fp.GetPosition(), False)
        (x, y, rt), miss = pose_on_pins(fp, pads)
        fp.SetOrientationDegrees(rt); fp.SetPosition(V(x, y))
        if miss > 0.01: sys.exit(f"{r}: pose miss {miss:.3f} mm")
        for g in fp.GraphicalItems():
            if g.GetLayer() == pcbnew.B_SilkS: g.SetLayer(pcbnew.B_Fab)
        lab = fp.Reference(); lab.SetLayer(pcbnew.B_Fab); lab.SetTextSize(V(0.5, 0.5)); lab.SetTextThickness(MM(0.08))
        lab.SetTextAngleDegrees(0); lab.SetPosition(fp.GetPosition()); fp.Value().SetVisible(False)
    RING_LABELED.update(BS_LOCAL)
    net_of = lambda r, p: nets.get((r, p), "").split("/")[-1]
    pa0 = next(p for p in fps["U1"].Pads() if net_of("U1", p.GetNumber()) == "BAT_SENSE")
    c45 = next(p for p in fps["C45"].Pads() if net_of("C45", p.GetNumber()) == "BAT_SENSE")
    d = math.hypot(TO(pa0.GetPosition().x - c45.GetPosition().x), TO(pa0.GetPosition().y - c45.GetPosition().y))
    return set(BS_LOCAL), [f"  battery sense on the back behind U1 pin {pa0.GetNumber()} (BAT_SENSE): C45's tap pad {d:.2f} mm from the pin centre (one via)"]


# ---------------------------------------------------------------- LCD VIN decoupling at J3's +5V_LCD pins
# J3 pins 6 and 2 are the 2090's VIN (+5V_LCD), pin 1 its GND. The caps go on J3's LEFT, in the lane that already
# carries +5V_LCD down from the boost (the strip right of J3 is the FPGA -> J3 LCD control lane): C26 (10 uF bulk)
# at pin 6, the first VIN pin the rail reaches; C25 (100 nF HF) at pin 2, its GND pad toward pin 1 (GND) next door —
# the tightest HF loop available. Clear of Start/Select's pads (x <= 52.99).
DISP_CAPS = {   # board frame, front
    "C26": {1: (54.6, 82.43), 2: (54.6, 84.43)},   # +5V_LCD level with pin 6, GND below (via)
    "C25": {1: (54.9, 92.59), 2: (54.9, 93.99)},   # +5V_LCD level with pin 2, GND pad toward pin 1 (GND, y 95.13)
}

def place_sw8_label(fps):
    """Start (SW8)'s refdes, vertical, on its LEFT (toward the haptics block, which ends ~5 mm away): above it is C35,
    right of it the LCD VIN caps, and Select's label already sits in the gap below."""
    p8 = list(fps["SW8"].Pads()); sl = fps["SW8"].Reference(); sl.SetTextAngleDegrees(90)
    cy = sum(TO(p.GetPosition().y) for p in p8) / len(p8); left = min(TO(p.GetBoundingBox().GetLeft()) for p in p8)
    sl.SetPosition(V(left - 1.0, cy)); RING_LABELED.add("SW8")

def place_display_caps(fps, nets):
    for r, pads in DISP_CAPS.items():
        fp = fps[r]
        if fp.IsFlipped(): fp.Flip(fp.GetPosition(), False)
        (x, y, rt), miss = pose_on_pins(fp, pads)
        fp.SetOrientationDegrees(rt); fp.SetPosition(V(x, y))
        if miss > 0.01: sys.exit(f"{r}: pose miss {miss:.3f} mm")
        for g in fp.GraphicalItems():                # tight between J3 and Start/Select: outline + refdes to F.Fab
            if g.GetLayer() == pcbnew.F_SilkS: g.SetLayer(pcbnew.F_Fab)
        lab = fp.Reference(); lab.SetLayer(pcbnew.F_Fab); lab.SetTextSize(V(0.5, 0.5)); lab.SetTextThickness(MM(0.08))
        lab.SetTextAngleDegrees(0); lab.SetPosition(fp.GetPosition()); fp.Value().SetVisible(False)
    if "SW8" in fps: place_sw8_label(fps)
    RING_LABELED.update(DISP_CAPS)
    net_of = lambda r, p: nets.get((r, p), "").split("/")[-1]
    jp = {p.GetNumber(): p for p in fps["J3"].Pads()}
    out = ["  LCD VIN decoupling on J3's left (the +5V_LCD lane):"]
    for r, pin in (("C26", "6"), ("C25", "2")):
        q = next(p for p in fps[r].Pads() if net_of(r, p.GetNumber()) == "+5V_LCD")
        out.append(f"      {r} -> J3 pin {pin}: {math.hypot(TO(q.GetPosition().x - jp[pin].GetPosition().x), TO(q.GetPosition().y - jp[pin].GetPosition().y)):.2f} mm")
    return set(DISP_CAPS), out


# ---------------------------------------------------------------- Class-D amp (MAX98357A) at the speaker
# Filterless Class-D: OUTP/OUTN are full-swing ~300 kHz square waves (the speaker coil is the filter), so the amp
# sits at the speaker and those two runs stay short (~16 mm, a pair over the solid In1 GND, ending at J5 on the
# cutout's top edge so the speaker's own flying leads are as short as possible); the slow I2S lines take the long run. Right
# grip, above the speaker cutout, in the free pocket between the debug headers (x <= 143.4) and the volume buttons
# (x >= 161). U6 at rot -90: DIN (1) on the left and BCLK (16) / LRCLK (14) on the top-left — the I2S lines arrive
# from the left without crossing (LRCLK outermost, then BCLK, then DIN); OUTP (9) / OUTN (10) on the right, J5's
# pads stacked beside them in the same order (SPK_N upper, SPK_P lower: no crossing); VDD (7/8) on the bottom with
# C29 (100 nF) right under them and C30 (10 uF bulk, sources the output pulses) beside it; R11 (620 k SD_MODE strap
# to +3V3) off SD_MODE (4). GAIN_SLOT (2) ties to SYS (6 dB) via a short hop to the caps' SYS pads (routing).
AMP_HUB = "U6"
AMP_AT = (150.0, 52.0)
AMP_LOCAL = {                   # board-aligned, U6 centre (0, 0); U6 pad tips at +-1.8
    "U6":  {1: (-1.5, -0.75), 16: (-0.75, -1.5)},  # DIN left-top, BCLK top-left (rot -90)
    "J5":  {1: (0.3, 16.3), 2: (2.84, 16.3)},      # horizontal at the speaker cutout's top edge, right above the speaker:
                                                   # leads come straight up; SPK_P left / SPK_N right = the order OUTP/OUTN
                                                   # reach it running down past C30 (OUTP is the inner of the pair)
    "C29": {1: (0.5, 2.6), 2: (0.5, 4.0)},         # VDD HF under VDD 7/8
    "C30": {1: (2.1, 2.9), 2: (2.1, 4.9)},         # VDD bulk beside it, below the output runs
    "R11": {1: (-2.6, 1.2), 2: (-4.1, 1.2)},       # SD_MODE strap, below DIN's arrival line
}

def place_amp(fps, nets):
    net_of = lambda r, p: nets.get((r, p), "").split("/")[-1]
    ax, ay = AMP_AT
    for r, pads in AMP_LOCAL.items():
        fp = fps[r]
        if fp.IsFlipped(): fp.Flip(fp.GetPosition(), False)
        (x, y, rt), miss = pose_on_pins(fp, {n: (ax + px, ay + py) for n, (px, py) in pads.items()})
        fp.SetOrientationDegrees(rt); fp.SetPosition(V(x, y))
        if miss > 0.01: sys.exit(f"{r}: pose miss {miss:.3f} mm")
        if r in ("C29", "C30", "R11"):
            for g in fp.GraphicalItems():
                if g.GetLayer() == pcbnew.F_SilkS: g.SetLayer(pcbnew.F_Fab)
            lab = fp.Reference(); lab.SetLayer(pcbnew.F_Fab); lab.SetTextSize(V(0.5, 0.5)); lab.SetTextThickness(MM(0.08))
            lab.SetTextAngleDegrees(0); lab.SetPosition(fp.GetPosition()); fp.Value().SetVisible(False)
    up = {p.GetNumber(): p for p in fps[AMP_HUB].Pads()}
    loc = {n: (TO(up[n].GetPosition().x) - ax, TO(up[n].GetPosition().y) - ay) for n in ("1", "9", "10", "7", "14")}
    if not (loc["9"][0] > 1 and loc["10"][0] > 1 and loc["1"][0] < -1 and loc["7"][1] > 1 and loc["14"][1] < -1 and loc["9"][1] > loc["10"][1]):
        sys.exit(f"U6: pin map not as planned {loc}")
    for g in fps["J5"].GraphicalItems():                # J5's "+" (SPK_P) mark: above pad 1, off the cutout edge
        if g.GetClass() in ("PCB_TEXT", "FP_TEXT") and g.GetText().strip() == "+":
            p1 = next(p for p in fps["J5"].Pads() if p.GetNumber() == "1")
            g.SetTextAngleDegrees(0); g.SetPosition(V(TO(p1.GetPosition().x) - 1.6, TO(p1.GetPosition().y) - 1.4))
    for r, (dx, dy, ang) in {"U6": (0.0, -2.9, 0), "J5": (1.57, 14.6, 0)}.items():   # J5's label just above its pads
        lab = fps[r].Reference(); lab.SetLayer(pcbnew.F_SilkS); lab.SetTextAngleDegrees(ang); lab.SetPosition(V(ax + dx, ay + dy))
    RING_LABELED.update(AMP_LOCAL)
    out = [f"  U6 MAX98357A at ({ax:.2f},{ay:.2f}) above the speaker; J5 (speaker leads) beside OUTP/OUTN"]
    for r in AMP_LOCAL:
        if r == AMP_HUB: continue
        best = None
        for p in fps[r].Pads():
            n = net_of(r, p.GetNumber())
            for pn, pp in up.items():
                if net_of(AMP_HUB, pn) == n and n not in ("GND", ""):
                    dd = math.hypot(TO(p.GetPosition().x - pp.GetPosition().x), TO(p.GetPosition().y - pp.GetPosition().y))
                    if best is None or dd < best[0]: best = (dd, pn, n)
        out.append(f"      {r:4s} -> U6 pin {best[1]:>2s} ({best[2]:11s}) {best[0]:4.2f} mm")
    return set(AMP_LOCAL), out


# ---------------------------------------------------------------- 4-layer stackup + planes
# F.Cu signal / In1.Cu solid GND / In2.Cu +3V3 power / B.Cu signal (1.6 mm, JLC's standard 4-layer build). The GND
# plane under everything is what lets the FPGA, Bluetooth, three switchers and the Class-D amp share one board, and
# every crossing becomes a via instead of a cut in the ground; +3V3 (the most widely spread rail) goes on a plane so
# it needs no trunk routing — SYS / +5V_LCD / BAT stay wide traces on the outer layers. Both planes are full-board
# zones (re-shaped from the board outline each run); the BT830 antenna rule area already covers every copper layer.
PLANES = (("In1.Cu", "GND", "GND plane (In1)"), ("In2.Cu", "+3V3", "+3V3 plane (In2)"))

def set_4layer(B):
    ds = B.GetDesignSettings()
    if B.GetCopperLayerCount() != 4:
        B.SetCopperLayerCount(4)
        en = B.GetEnabledLayers(); en.AddLayer(pcbnew.In1_Cu); en.AddLayer(pcbnew.In2_Cu); B.SetEnabledLayers(en)
    try:
        ds.GetStackupDescriptor().BuildDefaultStackupList(ds, 4)
    except Exception:
        pass
    # KiCad saves a layer set as the layers that exist: on the 2-layer board the BT830 antenna rule area and the
    # module footprint's own "*.Cu" keep-out were stored as F.Cu + B.Cu, so the new inner planes would pour right under
    # the antenna. Re-assert every copper layer on both (the footprint's front-only / back-only keep-outs stay as they are).
    allcu = pcbnew.LSET(); [allcu.AddLayer(l) for l in (pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.In2_Cu, pcbnew.B_Cu)]
    for z in B.Zones():
        if z.GetIsRuleArea() and z.GetZoneName() == "BT830 antenna keep-out": z.SetLayerSet(allcu)
    fpz = {f.GetReference(): f for f in B.GetFootprints()}
    if BT in fpz:
        for z in fpz[BT].Zones():
            ls = z.GetLayerSet()
            if ls.Contains(pcbnew.F_Cu) and ls.Contains(pcbnew.B_Cu): z.SetLayerSet(allcu)
    # shell-screw keep-outs: a copper-free disc (screw head / boss) around each hole draw_mech punches in the board
    Sx = M.stretch(); bsh = M.board_shape(Sx, M.fit(Sx))
    for name, hx, hy in M.screws(Sx):
        kname = "screw keep-out: " + name.split(",")[-1].strip()
        kz = next((z for z in B.Zones() if z.GetIsRuleArea() and z.GetZoneName() == kname), None)
        on_board = any(bsh.Contains(V(hx + M.SCREW_HOLE_D / 2 * math.cos(a_), hy + M.SCREW_HOLE_D / 2 * math.sin(a_))) for a_ in (0, 1.57, 3.14, 4.71))
        if not on_board: continue
        if kz is None:
            kz = pcbnew.ZONE(B); kz.SetIsRuleArea(True); kz.SetZoneName(kname); B.Add(kz)
        kz.SetDoNotAllowTracks(True); kz.SetDoNotAllowVias(True); kz.SetDoNotAllowPads(True); kz.SetDoNotAllowZoneFills(True)
        kz.SetDoNotAllowFootprints(False); kz.SetLayerSet(allcu)
        ko = kz.Outline(); ko.RemoveAllContours(); ko.NewOutline()
        for i in range(32):
            a_ = 2 * math.pi * i / 32; ko.Append(V(hx + M.SCREW_KEEPOUT_R * math.cos(a_), hy + M.SCREW_KEEPOUT_R * math.sin(a_)))
    rings = M.board_polys(M.stretch(), M.fit(M.stretch()))   # the cleaned outline (same as Edge.Cuts)
    out = []
    for lname, net, zname in PLANES:
        lid = B.GetLayerID(lname)
        ni = B.FindNet(net)
        if ni is None: sys.exit(f"net {net} not on the board")
        z = next((z for z in B.Zones() if not z.GetIsRuleArea() and z.GetZoneName() == zname), None)
        if z is None:
            z = pcbnew.ZONE(B); z.SetZoneName(zname); B.Add(z)
        z.SetLayer(lid); z.SetNet(ni); z.SetLocalClearance(MM(0.2)); z.SetMinThickness(MM(0.2))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL); z.SetThermalReliefGap(MM(0.3)); z.SetThermalReliefSpokeWidth(MM(0.4))
        ol = z.Outline(); ol.RemoveAllContours()
        for pts, holes in rings:
            ol.NewOutline()
            for x, y in pts: ol.Append(V(x, y))
        out.append(f"  {lname}: {net} plane ({zname}), full board outline")
    pcbnew.ZONE_FILLER(B).Fill(B.Zones())
    out.insert(0, f"  stackup: {B.GetCopperLayerCount()} copper layers, board {TO(ds.GetBoardThickness()):.2f} mm")
    return out


# ---------------------------------------------------------------- iCE40 1.2 V core LDO (AP2127K) + In2 island
# The front around U2 is spoken for (J3 display channel left, the U1 control lane right, the SPI3 config arrival
# above, ~1.5 mm to the cutout below), so the LDO goes on the BACK just right of U2, behind the control lane, where
# the back is empty (the depth budget has ~2 mm of slack; the BT830 back there is taller). VOUT faces U2, VIN/EN face
# away: VIN + EN (tied — EN has a 3 MOhm internal pull-down, floating = dead FPGA) drop from the In2 +3V3 plane; C21 at
# VIN, C22 (stability) right at VOUT, C23 (bulk) beside it. The two core pins sit on opposite sides of U2 (30 left,
# 5 right), so instead of a trace wrapping the chip, a +1V2 island is carved out of In2 under U2: both core pins and
# VOUT drop into it by vias. It is a tight rectangle around them only — U2's 3.3 V pins (VCCIO 1/22/33, VPP 24) keep
# their vias on the surrounding +3V3 plane.
LDO_PADS = {    # board frame, all on the back
    "U3":  {1: (75.3, 57.45), 5: (72.7, 57.45)},   # VIN right, VOUT left (toward U2)
    "C21": {1: (77.1, 57.45), 2: (77.1, 56.05)},   # +3V3 beside VIN
    "C22": {1: (71.0, 57.45), 2: (71.0, 58.85)},   # +1V2 beside VOUT
    "C23": {1: (72.0, 56.0), 2: (73.4, 56.0)},     # +1V2 bulk, just above VOUT (U3's NC/EN row is below it on the back)
}
ISLAND = (60.6, 57.0, 74.0, 61.0)  # +1V2 island on In2 (x0, y0, x1, y1): U2 core pins 30 (61.4, 59.8) + 5 (68.2, 60.8), VOUT,
                                   # C22 — bottom edge stops short of VCCIO_0 (pin 33, y 61.3) so it stays on the +3V3 plane

def place_fpgaldo(B, fps, nets):
    net_of = lambda r, p: nets.get((r, p), "").split("/")[-1]
    for r, pads in LDO_PADS.items():
        fp = fps[r]
        if not fp.IsFlipped(): fp.Flip(fp.GetPosition(), False)
        (x, y, rt), miss = pose_on_pins(fp, pads)
        fp.SetOrientationDegrees(rt); fp.SetPosition(V(x, y))
        if miss > 0.01: sys.exit(f"{r}: pose miss {miss:.3f} mm")
        for g in fp.GraphicalItems():
            if g.GetLayer() == pcbnew.B_SilkS: g.SetLayer(pcbnew.B_Fab)
        lab = fp.Reference(); lab.SetLayer(pcbnew.B_Fab); lab.SetTextSize(V(0.5, 0.5)); lab.SetTextThickness(MM(0.08))
        lab.SetTextAngleDegrees(0); lab.SetPosition(fp.GetPosition()); fp.Value().SetVisible(False)
    RING_LABELED.update(LDO_PADS)
    up = {p.GetNumber(): p for p in fps["U3"].Pads()}
    if net_of("U3", "5") != "+1V2" or net_of("U3", "3") != "+3V3": sys.exit("U3: unexpected pin nets")
    # the island (In2, above the +3V3 plane in priority)
    zname = "+1V2 island (In2)"
    z = next((z for z in B.Zones() if not z.GetIsRuleArea() and z.GetZoneName() == zname), None)
    if z is None:
        z = pcbnew.ZONE(B); z.SetZoneName(zname); B.Add(z)
    z.SetLayer(B.GetLayerID("In2.Cu")); z.SetNet(B.FindNet("+1V2")); z.SetAssignedPriority(1)
    z.SetLocalClearance(MM(0.2)); z.SetMinThickness(MM(0.2)); z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    z.SetThermalReliefGap(MM(0.3)); z.SetThermalReliefSpokeWidth(MM(0.4))
    x0, y0, x1, y1 = ISLAND; ol = z.Outline(); ol.RemoveAllContours(); ol.NewOutline()
    for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)): ol.Append(V(x, y))
    for zz in B.Zones():
        if not zz.GetIsRuleArea() and zz.GetZoneName() == "+3V3 plane (In2)": zz.SetAssignedPriority(0)
    pcbnew.ZONE_FILLER(B).Fill(B.Zones())
    core = [p for p in fps["U2"].Pads() if net_of("U2", p.GetNumber()) == "+1V2"]
    inside = [p.GetNumber() for p in core if x0 < TO(p.GetPosition().x) < x1 and y0 < TO(p.GetPosition().y) < y1]
    v3 = [p.GetNumber() for p in fps["U2"].Pads() if net_of("U2", p.GetNumber()) == "+3V3" and x0 < TO(p.GetPosition().x) < x1 and y0 < TO(p.GetPosition().y) < y1]
    vout = up["5"]; d = min(math.hypot(TO(vout.GetPosition().x - p.GetPosition().x), TO(vout.GetPosition().y - p.GetPosition().y)) for p in core)
    return set(LDO_PADS), [f"  U3 AP2127K-1.2 on the back right of U2; VOUT {d:.1f} mm from the nearest core pin",
                           f"  +1V2 island on In2 {x1 - x0:.1f} x {y1 - y0:.1f} mm: core pins inside {sorted(inside)}; U2 3.3 V pins inside it: {v3 or 'none'}"]


# ---------------------------------------------------------------- IMU (LSM6DSOX) in the upper band, right of the NOR
# MEMS rules first: no screws / standoffs / connectors within ~5 mm (board strain shifts the readings), away from heat
# (the charger, ~0.9 W charging, is the only real source here: ~12.5 mm), away from the LRA and speaker (both at the
# far ends), on rigid board (the band between the top edge and the battery cutout, not a grip). Centring doesn't
# matter for a gyro (same rate everywhere on a rigid body). U7 at rot 90: SCL/SDA (13/14) + CS (12) on the side facing
# U1's I2C1 pins (61/62); INT1 (4) on the bottom side, the side INT1 arrives from (U1 PB5 = pin 57, below the I2C
# pair). R12 (10 k INT1 pull-down — mandatory: INT1 high at POR latches the part I3C-only = dead) right under INT1;
# C31 at VDD (8), C32 at VDDIO (5), both with their GND pads toward GND 6/7. Keep the LGA's footprint free of top-layer
# traces/vias (ST soldering guidance); the strap pins (SA0/SDx/SCx -> GND, CS -> 3V3) tie locally. Firmware note: the
# chip is rotated 90 deg CCW from its datasheet orientation on the board — remap X/Y in the driver.
IMU_HUB = "U7"
IMU_AT = (113.0, 51.0)
IMU_LOCAL = {                   # board-aligned, U7 centre (0, 0); rot 90 -> pins: left 12/13/14, bottom 1-4, right 7/6/5, top 11-8
    "U7":  {1: (-0.75, 1.17), 14: (-0.93, 0.5)},
    "C31": {1: (2.0, -1.3), 2: (2.0, 0.1)},        # VDD (pin 8 top-right) / GND toward pins 6-7
    "C32": {1: (2.1, 2.35), 2: (3.5, 2.35)},       # VDDIO (pin 5 right-bottom), below-right, clear of INT1's drop
    "R12": {1: (0.75, 2.9), 2: (0.75, 4.4)},       # INT1 pull-down straight below INT1 (pin 4)
}

def place_imu(fps, nets):
    net_of = lambda r, p: nets.get((r, p), "").split("/")[-1]
    ix, iy = IMU_AT
    for r, pads in IMU_LOCAL.items():
        fp = fps[r]
        if fp.IsFlipped(): fp.Flip(fp.GetPosition(), False)
        (x, y, rt), miss = pose_on_pins(fp, {n: (ix + px, iy + py) for n, (px, py) in pads.items()})
        fp.SetOrientationDegrees(rt); fp.SetPosition(V(x, y))
        if miss > 0.01: sys.exit(f"{r}: pose miss {miss:.3f} mm")
        if r != IMU_HUB:
            for g in fp.GraphicalItems():
                if g.GetLayer() == pcbnew.F_SilkS: g.SetLayer(pcbnew.F_Fab)
            lab = fp.Reference(); lab.SetLayer(pcbnew.F_Fab); lab.SetTextSize(V(0.5, 0.5)); lab.SetTextThickness(MM(0.08))
            lab.SetTextAngleDegrees(0); lab.SetPosition(fp.GetPosition()); fp.Value().SetVisible(False)
    up = {p.GetNumber(): (TO(p.GetPosition().x) - ix, TO(p.GetPosition().y) - iy) for p in fps[IMU_HUB].Pads()}
    if not (up["13"][0] < -0.5 and up["4"][1] > 0.5 and up["8"][1] < -0.5): sys.exit(f"U7: pin map not as planned {up}")
    lab = fps[IMU_HUB].Reference(); lab.SetLayer(pcbnew.F_SilkS); lab.SetTextAngleDegrees(0); lab.SetPosition(V(ix - 0.5, iy - 2.4))
    RING_LABELED.update(IMU_LOCAL)
    # report: neighbours that matter for a MEMS part
    sx, sy = M.GBA_ORIGIN; side = M.layout(M.stretch())[0]
    screws = [(x, y) for _, x, y in M.screws(M.stretch())]
    near = lambda r: min(math.hypot(TO(p.GetPosition().x) - ix, TO(p.GetPosition().y) - iy) for p in fps[r].Pads()) if r in fps else float("nan")
    out = [f"  U7 LSM6DSOX at ({ix:.1f},{iy:.1f}) rot 90: nearest screw {min(math.hypot(ix - a, iy - b) for a, b in screws):.1f} mm, "
           f"J8 {near('J8'):.1f} mm, charger U9 {near('U9'):.1f} mm, U1 I2C pins {min(near('U1'), 99):.1f} mm"]
    for r in IMU_LOCAL:
        if r == IMU_HUB: continue
        best = None
        for p in fps[r].Pads():
            n = net_of(r, p.GetNumber())
            for pp in fps[IMU_HUB].Pads():
                if net_of(IMU_HUB, pp.GetNumber()) == n and n not in ("GND", ""):
                    dd = math.hypot(TO(p.GetPosition().x - pp.GetPosition().x), TO(p.GetPosition().y - pp.GetPosition().y))
                    if best is None or dd < best[0]: best = (dd, pp.GetNumber(), n)
        out.append(f"      {r:4s} -> U7 pin {best[1]:>2s} ({best[2]:8s}) {best[0]:4.2f} mm")
    return set(IMU_LOCAL), out


# ---------------------------------------------------------------- button expander (PCA9555) in the left grip
# The only path from the left grip to the rest of the board is a handful of tracks between J3's upper pins (J3's
# right side is the display bus end to end, and the top-left shell screw sits right above J3's top mounting hole);
# Bluetooth, the haptics I2C, PWR_SW and +5V_LCD already fill it. So the expander serves only the 7 left-grip buttons
# (all on port 0) and the 5 right-grip buttons go straight to STM32 GPIOs and never cross. Spot: up-right of the
# D-pad, in the pocket the GBA's mid-left shell screw used to occupy (dropped: draw_mech.SCREWS_DROPPED) — 10 mm from
# where I2C + INT come in past J3's upper pins, clear of the D-pad cap (0.3 mm), the Up/Right switches and the
# L-shoulder area (2.2 mm from the cap, 1.7 mm from the Up switch's pads, 4 mm below the L-shoulder area); empty board
# on the back. U11 at rot -90: port 0 (pins 4-11) is the left column, facing the D-pad,
# its pins assigned in the counter-clockwise order the buttons lie around the chip (L, UP, LEFT, DOWN, START, SELECT,
# RIGHT, spare) so the fan-out doesn't cross; SCL/SDA/VCC are at the top of the right column, facing the J3 corridor.
# C46 right above VCC (pin 24). The expander is POLLED over I2C (its INT is not connected): one less track through the
# full corridor and no long EXP_INT run around U1.
EXP_HUB = "U11"
EXP_AT = (49.0, 52.0)
EXP_LOCAL = {                   # board frame
    "U11": ({1: (49.0 - 3.02, 52.0 - 3.57), 24: (49.0 + 3.02, 52.0 - 3.57)}, False),  # INT top-left, VCC top-right
    "C46": ({1: (52.0, 47.1), 2: (52.0, 45.7)}, False),                              # VCC decoupling right above pin 24
}

def place_expander(fps, nets):
    net_of = lambda r, p: nets.get((r, p), "").split("/")[-1]
    for r, (pads, back) in EXP_LOCAL.items():
        fp = fps[r]
        if fp.IsFlipped() != back: fp.Flip(fp.GetPosition(), False)
        (x, y, rt), miss = pose_on_pins(fp, pads)
        fp.SetOrientationDegrees(rt); fp.SetPosition(V(x, y))
        if miss > 0.01: sys.exit(f"{r}: pose miss {miss:.3f} mm")
    for r in ("C46",):
        fp = fps[r]
        for g in fp.GraphicalItems():
            if g.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS): g.SetLayer(pcbnew.B_Fab if fp.IsFlipped() else pcbnew.F_Fab)
        lab = fp.Reference(); lab.SetLayer(pcbnew.B_Fab if fp.IsFlipped() else pcbnew.F_Fab); lab.SetTextSize(V(0.5, 0.5))
        lab.SetTextThickness(MM(0.08)); lab.SetTextAngleDegrees(0); lab.SetPosition(fp.GetPosition()); fp.Value().SetVisible(False)
    lab = fps[EXP_HUB].Reference(); lab.SetLayer(pcbnew.F_SilkS); lab.SetTextAngleDegrees(90); lab.SetPosition(V(EXP_AT[0], EXP_AT[1]))   # between the pad columns
    fps[EXP_HUB].Value().SetVisible(False)
    for sw in ("SW4", "SW5"):                         # D-pad left/right labels: below their switches (SW5's sat under U11)
        if sw not in fps: continue
        p4 = list(fps[sw].Pads()); sl = fps[sw].Reference(); sl.SetTextAngleDegrees(0)
        sl.SetPosition(V(sum(TO(p.GetPosition().x) for p in p4) / len(p4), max(TO(p.GetBoundingBox().GetBottom()) for p in p4) + 1.6)); RING_LABELED.add(sw)
    RING_LABELED.update({EXP_HUB, "C46"})
    up = {p.GetNumber(): (TO(p.GetPosition().x), TO(p.GetPosition().y)) for p in fps[EXP_HUB].Pads()}
    col = sorted((up[str(n)][1], net_of(EXP_HUB, str(n)) or "spare") for n in range(4, 12))
    btn = {}
    for r, f2 in fps.items():
        for p in f2.Pads():
            n = p.GetNetname().split("/")[-1]
            if n.startswith("BTN_") and r.startswith("SW"): btn.setdefault(n, (TO(p.GetPosition().x), TO(p.GetPosition().y)))
    out = [f"  U11 PCA9555 at {EXP_AT} rot {fps[EXP_HUB].GetOrientationDegrees():g}: port 0 column top->bottom = {[n for _, n in col]}"]
    out.append("      button run lengths: " + ", ".join(f"{n[4:]} {math.hypot(btn[n][0] - up[str(k)][0], btn[n][1] - up[str(k)][1]):.0f}"
                                                      for k in range(4, 12) for n in [net_of(EXP_HUB, str(k))] if n in btn) + " mm")
    return {EXP_HUB, "C46"}, out


# ---------------------------------------------------------------- I2C1 pull-ups (one pair for the whole bus)
# SCL/SDA are open-drain: every device only pulls low, so the bus needs exactly one pull-up pair to bring the lines
# back high. It sits at the master end, just right of U1's I2C pins (61 SCL / 59 SDA), above the NOR, where the two
# lines head for the IMU: each resistor taps its line (bus pad toward U1), its +3V3 end drops through a via into the
# In2 plane. (Not on the back behind the pins, U1's usual spot: BOOT0's R1 sits between those two pins.)
I2C_PULLUPS = {   # board frame, front; SCL above SDA, as they leave U1 and arrive at the IMU
    "R13": {1: (98.8, 52.6), 2: (100.3, 52.6)},
    "R14": {1: (98.8, 53.8), 2: (100.3, 53.8)},
}

def place_i2c(fps, nets):
    out = []
    for r, pads in I2C_PULLUPS.items():
        fp = fps[r]
        if fp.IsFlipped(): fp.Flip(fp.GetPosition(), False)
        (x, y, rt), miss = pose_on_pins(fp, pads)
        fp.SetOrientationDegrees(rt); fp.SetPosition(V(x, y))
        if miss > 0.01: sys.exit(f"{r}: pose miss {miss:.3f} mm")
        for g in fp.GraphicalItems():
            if g.GetLayer() == pcbnew.F_SilkS: g.SetLayer(pcbnew.F_Fab)
        lab = fp.Reference(); lab.SetLayer(pcbnew.F_Fab); lab.SetTextSize(V(0.5, 0.5)); lab.SetTextThickness(MM(0.08))
        lab.SetTextAngleDegrees(0); lab.SetPosition(fp.GetPosition()); fp.Value().SetVisible(False)
    RING_LABELED.update(I2C_PULLUPS)
    net_of = lambda r, p: nets.get((r, p), "").split("/")[-1]
    u1 = {net_of("U1", p.GetNumber()): p for p in fps["U1"].Pads()}
    for r in I2C_PULLUPS:
        bp = next(p for p in fps[r].Pads() if net_of(r, p.GetNumber()).startswith("I2C1"))
        q = u1[net_of(r, bp.GetNumber())]
        out.append(f"  {r} ({net_of(r, bp.GetNumber())} pull-up) {math.hypot(TO(bp.GetPosition().x - q.GetPosition().x), TO(bp.GetPosition().y - q.GetPosition().y)):.1f} mm from U1 pin {q.GetNumber()}")
    return set(I2C_PULLUPS), out


# ---------------------------------------------------------------- main
# KiCad 10 SWIG quirks: pcbnew.FootprintLoad is broken, so library footprints come through the PCB_IO plugin,
# which must run BEFORE LoadBoard; and once PCB_IO has been used in a process, footprints read from the board
# file come back as untyped SwigPyObjects (no .Pads(), no .SetPosition()). So adding the missing footprints
# happens in a child process (--add-missing), and this process — which never touches PCB_IO — does the rest.
def board_fpids():
    """ref -> "lib:footprint" for every footprint in the board file (parsed as text: no pcbnew needed)."""
    out = {}
    for chunk in open(PCB, encoding="utf-8").read().split('\n\t(footprint "')[1:]:
        m = re.search(r'\(property "Reference" "([^"]+)"', chunk)
        if m: out[m.group(1)] = chunk[:chunk.index('"')]
    return out

def board_refs():
    return set(board_fpids())

def remove_refs(refs):                 # child process: no PCB_IO, and B.Remove is the last thing it does
    B = pcbnew.LoadBoard(PCB)
    doomed = [fp for fp in B.GetFootprints() if fp.GetReference() in refs]
    for fp in doomed: B.Remove(fp)
    pcbnew.SaveBoard(PCB, B)
    sys.stdout.flush(); os._exit(0)

def run_child(*args):
    r = subprocess.run([sys.executable, os.path.abspath(__file__), *args], capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"{args[0]} failed:\n" + "\n".join(l for l in r.stderr.splitlines() if "swig/python" not in l))

def add_missing(comps, refs):
    libs = lib_paths()
    io = pcbnew.PCB_IO_MGR.FindPlugin(pcbnew.PCB_IO_MGR.KICAD_SEXP)
    fresh = {}
    for ref in refs:
        nick, name = comps[ref]["footprint"].split(":", 1)
        fp = io.FootprintLoad(libs[nick], name)
        if fp is None: sys.exit(f"{ref}: footprint {comps[ref]['footprint']} not found in {libs[nick]}")
        fp.SetFPID(pcbnew.LIB_ID(nick, name)); fp.SetReference(ref)
        fresh[ref] = fp
    B = pcbnew.LoadBoard(PCB)
    for fp in fresh.values():
        B.Add(fp)
    pcbnew.SaveBoard(PCB, B)
    sys.stdout.flush(); os._exit(0)


def main():
    reset = "--reset" in sys.argv
    if "--remove" in sys.argv:
        return remove_refs(set(sys.argv[sys.argv.index("--remove") + 1:]))
    comps, nets = read_netlist()
    if "--add-missing" in sys.argv:
        return add_missing(comps, sorted(set(comps) - board_refs()))
    # stale = no longer in the schematic; swapped = its symbol now points at a different footprint (a renumber
    # can do that: SW10 went from a 4-pin tactile to a 5-pin side switch). Both are removed; swapped ones are
    # then re-added with the right footprint and placed like new parts.
    bf = board_fpids()
    stale = sorted(set(bf) - set(comps))
    swapped = sorted(r for r in comps if r in bf and bf[r] != comps[r]["footprint"])
    if stale or swapped:
        run_child("--remove", *stale, *swapped)
    missing = sorted(set(comps) - board_refs())
    if missing:
        run_child("--add-missing")
        if set(comps) - board_refs(): sys.exit("adding footprints failed")
    added = set(missing)

    B = pcbnew.LoadBoard(PCB)
    netinfo = {}
    def net(name):
        if name not in netinfo:
            ni = B.FindNet(name)
            if ni is None:
                ni = pcbnew.NETINFO_ITEM(B, name); B.Add(ni)
            netinfo[name] = ni
        return netinfo[name]

    missing_pads = []
    fps = {fp.GetReference(): fp for fp in B.GetFootprints()}
    for ref in sorted(comps, key=lambda r: (re.sub(r"\d+", "", r), int(re.sub(r"\D", "", r) or 0))):
        c = comps[ref]
        fp = fps[ref]
        fp.SetReference(ref); fp.SetValue(c["value"])
        fp.SetPath(pcbnew.KIID_PATH(c["path"])); fp.SetSheetname(c["sheetname"]); fp.SetSheetfile(c["sheetfile"])
        for k, v in c["fields"].items():
            if k not in ("Reference", "Value", "Footprint"):
                fp.SetField(k, v)
        # The easyeda2kicad footprints carry their own "LCSC Part" property from whichever part was last imported
        # into that (shared) footprint — e.g. every R0603 said C23219. Fab tools read LCSC numbers from either
        # name, so keep "LCSC Part" identical to the schematic's LCSC field.
        if c["fields"].get("LCSC"):
            fp.SetField("LCSC Part", c["fields"]["LCSC"])
        for fl in fp.GetFields():                                       # only the refdes goes on silk
            if fl.GetName() not in ("Reference", "Value"):
                fl.SetVisible(False); fl.SetLayer(pcbnew.B_Fab if fp.IsFlipped() else pcbnew.F_Fab)
        pads = {}
        for p in fp.Pads():
            pads.setdefault(p.GetNumber(), []).append(p)
        for (r, pin), n in nets.items():
            if r != ref: continue
            if pin not in pads: missing_pads.append(f"{ref}.{pin}"); continue
            for p in pads[pin]: p.SetNet(net(n))

    # ---- placement
    targets = button_targets(nets)
    to_place = set(comps) if reset else set(added)
    for ref, (x, y, rot) in targets.items():
        if ref in to_place or "--place-buttons" in sys.argv:
            fp = fps[ref]
            fp.SetOrientationDegrees(rot); fp.SetPosition(V(x, y))
    pins = display_pins(); socket_report = []
    for ref, hdr in DISPLAY_SOCKETS.items():
        if ref not in fps: continue
        if ref in to_place or "--place-display" in sys.argv:
            (x, y, rot), miss = pose_on_pins(fps[ref], pins[hdr])
            if miss > 0.001: sys.exit(f"{ref}: pads miss the {hdr} pins by {miss:.3f} mm")
            fps[ref].SetOrientationDegrees(rot); fps[ref].SetPosition(V(x, y))
            socket_report.append(f"  {ref:5s} under {hdr}: pad 1 at ({pins[hdr][1][0]:.2f},{pins[hdr][1][1]:.2f}), rot {rot:g}, all 20 pads on the pins")
    jref, rr = usb_parts(nets); usb = {jref, *rr.values()} - {None}
    usb_report = []
    if usb and (usb & to_place or "--place-usb" in sys.argv):
        usb_report = place_usb(fps, nets)
    stk_report = set_4layer(B) if "--set-4layer" in sys.argv else []
    i2c_set, i2c_report = set(), []
    if "--place-i2c" in sys.argv:
        i2c_set, i2c_report = place_i2c(fps, nets)
    exp_set, exp_report = set(), []
    if "--place-expander" in sys.argv:
        exp_set, exp_report = place_expander(fps, nets)
    imu_set, imu_report = set(), []
    if "--place-imu" in sys.argv:
        imu_set, imu_report = place_imu(fps, nets)
    ldo_set, ldo_report = set(), []
    if "--place-fpgaldo" in sys.argv:
        ldo_set, ldo_report = place_fpgaldo(B, fps, nets)
    amp_set, amp_report = set(), []
    if "--place-amp" in sys.argv:
        amp_set, amp_report = place_amp(fps, nets)
    dc_set, dc_report = set(), []
    if "--place-display-caps" in sys.argv:
        dc_set, dc_report = place_display_caps(fps, nets)
    bs_set, bs_report = set(), []
    if "--place-batsense" in sys.argv:
        bs_set, bs_report = place_batsense(fps, nets)
    hap_set, hap_report = set(), []
    if "--place-haptics" in sys.argv:
        hap_set, hap_report = place_haptics(fps, nets)
    nor_set, nor_report = set(), []
    if "--place-nor" in sys.argv:
        nor_set, nor_report = place_nor(fps, nets)
    lb_set, lb_report = set(), []
    if "--place-lcdboost" in sys.argv:
        lb_set, lb_report = place_lcdboost(fps, nets)
    mcu_set, mcu_report = set(), []
    if "--place-mcu" in sys.argv:
        mcu_set, mcu_report = place_mcu(fps, nets)
    chg_set, chg_report = set(), []
    if "--place-charger" in sys.argv:
        chg_set, chg_report = place_charger(fps, nets)
    bb_set, bb_report = set(), []
    if "--place-buckboost" in sys.argv:
        bb_set, bb_report = place_buckboost(fps, nets)
    bt_set, bt_report = set(), []
    if "--place-bt" in sys.argv:
        bt_set, bt_report = place_bt(B, fps, nets)
    fpga_set, fpga_report = set(), []
    if "--place-fpga" in sys.argv:
        notes = {r["refdes"]: r["note"] for r in csv.DictReader(open(os.path.join(HERE, "gb2-bom.csv"), encoding="utf-8"))}
        fpga_set, fpga_report = place_fpga(fps, nets, comps, notes)
        fpga_report += place_fpga_lane(fps)
    fixed = set(targets) | set(DISPLAY_SOCKETS) | usb | fpga_set | bt_set | chg_set | bb_set | mcu_set | lb_set | nor_set | hap_set | bs_set | dc_set | amp_set | ldo_set | imu_set | exp_set | i2c_set   # never packed
    if "--repack" in sys.argv:          # re-pack every part not yet on the board (outside the GBA outline)
        S_ = M.stretch(); poly = M.silhouette(S_); gx_, gy_ = M.GBA_ORIGIN
        off = {r for r in comps if r not in fixed and not M.inside((TO(fps[r].GetPosition().x) - gx_, TO(fps[r].GetPosition().y) - gy_), poly)}
        to_place = to_place | off

    def pad_box(fp):                   # pad extents relative to the footprint origin (avoid FOOTPRINT.GetBoundingBox)
        o = fp.GetPosition(); xs, ys = [], []
        for p in fp.Pads():
            bb = p.GetBoundingBox(); xs += [TO(bb.GetLeft() - o.x), TO(bb.GetRight() - o.x)]; ys += [TO(bb.GetTop() - o.y), TO(bb.GetBottom() - o.y)]
        return (min(xs), min(ys), max(xs), max(ys)) if xs else (-1, -1, 1, 1)

    gx, gy = M.GBA_ORIGIN; W = PACK_W
    L3 = B.GetLayerID("User.3")
    # B.Remove() corrupts the SWIG typing of later calls (fp.Pads() -> untyped), so the old group labels are
    # snapshotted now and removed only after all footprint work is done.
    old_labels = [d for d in B.GetDrawings() if d.GetLayer() == L3] if (reset or added or "--repack" in sys.argv) else []
    groups = {}
    for ref in comps:
        if ref in to_place and ref not in fixed:
            groups.setdefault(comps[ref]["sheetname"], []).append(ref)
    y0 = gy + M.GBA_H + PACK_TOP
    if not reset and "--repack" not in sys.argv:   # new parts only: start a fresh row below whatever is packed
        below = [TO(fp.GetPosition().y) for r, fp in fps.items() if r in comps and r not in to_place and TO(fp.GetPosition().y) > y0]
        if below: y0 = max(below) + 10.0
    cx, cy, row_h = gx, y0, 0.0
    # groups in power-chain / signal-flow order (anything unlisted goes last, alphabetically)
    GROUP_ORDER = ["usb-c", "charger", "battery-sense", "buck-boost", "fpga-ldo", "lcd-boost",
                   "mcu", "clock", "nor", "fpga", "display", "amp", "haptics", "imu", "i2c", "expander", "bluetooth"]
    order = sorted(groups, key=lambda g: (GROUP_ORDER.index(g) if g in GROUP_ORDER else len(GROUP_ORDER), g))
    for g in order:
        refs = sorted(groups[g], key=lambda r: (-{"U": 3, "J": 2, "SW": 2}.get(re.sub(r"\d+", "", r), 0), re.sub(r"\d+", "", r), int(re.sub(r"\D", "", r) or 0)))
        for fp in (fps[r] for r in refs):
            fp.SetOrientationDegrees(0); fp.SetPosition(V(0, 0))
        boxes = [(r, pad_box(fps[r])) for r in refs]
        gw = sum(b[2] - b[0] + PACK_GAP for _, b in boxes)
        gw = min(gw, 60.0)                                     # wrap long groups into a ~60 mm wide block
        # the block: shelf-pack the group's parts inside a gw-wide column
        bx, by_, bh, bwmax, placed = 0.0, 0.0, 0.0, 0.0, []
        for r, (l, t, rr, b) in boxes:
            w, h = rr - l, b - t
            if bx > 0 and bx + w > gw: bx, by_, bh = 0.0, by_ + bh + PACK_GAP, 0.0
            placed.append((r, bx - l, by_ - t)); bx += w + PACK_GAP; bh = max(bh, h); bwmax = max(bwmax, bx)
        block_w, block_h = max(bwmax, len(f"{g} ({len(refs)})") * 1.1 + 2.0), by_ + bh   # at least as wide as its title
        if cx > gx and cx + block_w > gx + W: cx, cy, row_h = gx, cy + row_h + GROUP_GAP, 0.0
        for r, ox, oy in placed:
            fps[r].SetPosition(V(cx + ox, cy + GROUP_HEAD + oy))
        t = pcbnew.PCB_TEXT(B); t.SetText(f"{g} ({len(refs)})"); t.SetLayer(L3); t.SetPosition(V(cx, cy + 1.0))
        t.SetTextSize(V(1.3, 1.3)); t.SetTextThickness(MM(0.16)); t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT); B.Add(t)
        box = pcbnew.PCB_SHAPE(B); box.SetShape(pcbnew.SHAPE_T_RECT); box.SetLayer(L3); box.SetWidth(MM(0.1))
        box.SetStart(V(cx - 1.0, cy - 0.5)); box.SetEnd(V(cx + block_w - PACK_GAP + 1.0, cy + GROUP_HEAD + block_h + 1.0)); B.Add(box)
        cx += block_w + GROUP_GAP; row_h = max(row_h, block_h + GROUP_HEAD)

    # Reference labels just above the part's pads (upright, board frame), value just below on Fab. The
    # easyeda2kicad footprints put the refdes 4-10 mm away. Done for parts placed this run, or all with --tidy-labels.
    LABEL_GAP = 0.3
    pad_boxes = {r: [(TO(b.GetLeft()), TO(b.GetTop()), TO(b.GetRight()), TO(b.GetBottom())) for b in (p.GetBoundingBox() for p in f.Pads())]
                 for r, f in fps.items()}
    for ref in comps:
        if ref in RING_LABELED: continue
        if not (ref in to_place or "--tidy-labels" in sys.argv or (ref in targets and "--place-buttons" in sys.argv)
                or (ref in DISPLAY_SOCKETS and "--place-display" in sys.argv) or (ref in usb and "--place-usb" in sys.argv)
                or ref in fpga_set or ref in bt_set or ref in chg_set or ref in bb_set or ref in mcu_set or ref in lb_set or ref in nor_set or ref in hap_set or ref in bs_set or ref in dc_set or ref in amp_set or ref in ldo_set or ref in imu_set or ref in exp_set or ref in i2c_set):
            continue
        fp = fps[ref]; xs, ys = [], []
        for p in fp.Pads():
            bb = p.GetBoundingBox(); xs += [TO(bb.GetLeft()), TO(bb.GetRight())]; ys += [TO(bb.GetTop()), TO(bb.GetBottom())]
        if not xs: continue
        cx_ = (min(xs) + max(xs)) / 2
        r_ = fp.Reference(); h = TO(r_.GetTextSize().y); w = len(ref) * 0.8 * TO(r_.GetTextSize().x)
        def hits(ly):                   # would a label centred at (cx_, ly) sit on another part's pads?
            return any(ob[0] < cx_ + w / 2 and ob[2] > cx_ - w / 2 and ob[1] < ly + h / 2 and ob[3] > ly - h / 2
                       for other, boxes in pad_boxes.items() if other != ref for ob in boxes)
        above, below = min(ys) - LABEL_GAP - h / 2, max(ys) + LABEL_GAP + h / 2
        r_.SetTextAngleDegrees(0); r_.SetPosition(V(cx_, below if hits(above) and not hits(below) else above))
        v_ = fp.Value(); hv = TO(v_.GetTextSize().y)
        v_.SetTextAngleDegrees(0); v_.SetPosition(V(cx_, max(ys) + LABEL_GAP + hv / 2))
    # refill every copper zone LAST: placement steps above can move through-hole pads (or anything else) after a
    # step that filled the planes, which would leave the fill stale (pads shorted into / unspaced from the planes)
    if any(z for z in B.Zones() if not z.GetIsRuleArea()):
        pcbnew.ZONE_FILLER(B).Fill(B.Zones())
    for d in old_labels: B.Remove(d)
    pcbnew.SaveBoard(PCB, B)
    print(f"parts: {len(comps)} in schematic, {len(added)} added, {len(comps) - len(added)} already on the board"
          f"{' (re-placed: --reset)' if reset else ''}; nets: {len(set(nets.values()))}")
    if stale: print("deleted (no longer in the schematic):", ", ".join(stale))
    if swapped: print("footprint swapped (symbol now uses a different footprint):", ", ".join(f"{r} -> {comps[r]['footprint']}" for r in swapped))
    if missing_pads: print("WARNING netlist pins with no matching pad:", ", ".join(missing_pads))
    for line in socket_report + usb_report + fpga_report + bt_report + chg_report + bb_report + mcu_report + lb_report + nor_report + hap_report + bs_report + dc_report + amp_report + stk_report + ldo_report + imu_report + exp_report + i2c_report: print(line)
    btn = {r: n.split("/")[-1] for (r, p), n in nets.items() if r in targets and n.split("/")[-1].startswith(("BTN_", "PWR_SW"))}
    for r in sorted(targets, key=lambda r: int(r[2:])):
        x, y, rot = targets[r]; print(f"  {r:5s} {btn[r]:12s} {comps[r]['value'][:16]:16s} ({x:7.2f},{y:7.2f}) rot {rot}")
    sys.stdout.flush(); os._exit(0)


if __name__ == "__main__":
    main()
