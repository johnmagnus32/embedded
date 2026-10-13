#!/usr/bin/env python3
"""sync_netlist.py — bring gb3.kicad_pcb in line with its schematic (generated from gb3-bom.csv + gb3-nets.csv).

The board-side partner of cad/generate_schematic.py. KiCad's own "Update PCB from Schematic" (F8) is
unsafe on this board: every footprint on it carries a library-less ID ("R0603") while the symbols say
"easyeda2kicad:R0603", so F8 would try to swap all ~190 of them. This does the netlist part directly,
from the schematic's own netlist (kicad-cli sch export netlist), so every name is the one F8 would use
(a one-sheet net is "/<sheet>/NAME", a no-connect "unconnected-(...)"). The CSVs are the cross-check:
if the netlist disagrees with gb3-bom.csv / gb3-nets.csv the schematic is stale and nothing is written.
  * a part in the schematic but not on the board is added (footprint, value and symbol link from the
    netlist) on the bottom side, on a free spot next to its ANCHOR pad, for the placement review to refine;
  * a part on the board but not in the schematic is removed (board-only footprints such as LRA1/SPK1
    excepted); the others get the schematic's value and symbol link (a changed footprint stops the run);
  * every pad gets its schematic net (missing nets are created; KiCad drops emptied ones on save);
  * every footprint's "LCSC Part" field is set from gb3-bom.csv (the library values are wrong);
  * MOVE: a part whose role changed is moved next to its new pin, only on the run that changes its nets
    (so a later run never undoes hand placement) and only if it is not in that neighbourhood already;
  * library-relative 3D model paths ("symbols/...") are made project-relative like the rest of the board.
Re-runnable: a second run changes nothing. Placement of everything else is never touched.

Run (KiCad closed):  <kicad python> sync_netlist.py [board.kicad_pcb]      (default gb3.kicad_pcb)
The CSVs and gb3.kicad_sch are read from the board's directory.
"""
import os, re, sys, csv, math, json, subprocess, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
FPLIBS = {n: os.path.join(REPO, "cad", "symbols", n + ".pretty") for n in ("easyeda2kicad", "manual")}
KICAD_CLI = os.environ.get("KICAD_CLI", "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli")

# Per-change tables (this change: step 1c, 2026-10-13; step 1b had R47-R53/C92/C93/TP4-TP12 + C13/C39,
# step 1 R43-R46/TP1-TP3 + C33/C35/C42). New parts are added in refdes order, so R55 can anchor to Q1.
# new part -> the pad it belongs beside (ref, pad)
ANCHOR = {"Q1": ("U10", "12"), "R54": ("R16", "1"), "R55": ("Q1", "1"), "R56": ("R52", "1"),
          "R57": ("R53", "1"), "C94": ("U1", "101")}
# existing part whose job changed -> its new pin
MOVE = {}
SEARCH = (1.5, 9.0)      # mm: ring radii searched for a free spot
MOVE_DONE = 10.0         # mm, beyond SEARCH: a part already this close to its new pin stays put
GAP = 0.25               # mm between bounding boxes
HOLE_GAP = 0.5           # mm from a part's box to any drilled hole


def rows(p):
    return [r for r in csv.reader(open(p, newline="")) if r and r[0] != "refdes"]


def netlist(sch):
    """the schematic's netlist: {ref: (value, footprint, symbol path)}, {(ref, pin): net name as on the board}"""
    fd, out = tempfile.mkstemp(suffix=".net"); os.close(fd)
    try:
        r = subprocess.run([KICAD_CLI, "sch", "export", "netlist", "--format", "kicadsexpr", "-o", out, sch],
                           capture_output=True, text=True)
        if r.returncode: sys.exit("netlist export failed:\n" + r.stdout + r.stderr)
        s = open(out).read()
    finally:
        os.unlink(out)
    unq = lambda t: t.replace('\\"', '"').replace("\\\\", "\\")
    stamps = re.compile(r'\(sheetpath\s+\(names "[^"]*"\)\s+\(tstamps "([^"]*)"\)\s*\)\s*\(tstamps "([^"]*)"')
    comps = {}
    for m in re.finditer(r'\(comp\s+\(ref "([^"]*)"\)\s+\(value "((?:[^"\\]|\\.)*)"\)\s+\(footprint "([^"]*)"\)', s):
        t = stamps.search(s, m.end())
        comps[m.group(1)] = (unq(m.group(2)), m.group(3), t.group(1) + t.group(2))
    heads = list(re.finditer(r'\(net\s+\(code "\d+"\)\s+\(name "((?:[^"\\]|\\.)*)"\)', s))
    nets = {}
    for i, h in enumerate(heads):
        body = s[h.end():heads[i + 1].start() if i + 1 < len(heads) else len(s)]
        for ref, pin in re.findall(r'\(node\s+\(ref "([^"]*)"\)\s+\(pin "([^"]*)"\)', body):
            nets[(ref, pin)] = unq(h.group(1))
    return comps, nets


def check(comps, snets, bom, nets):
    """the netlist must be what generate_schematic makes of the CSVs as they are now"""
    bad = [] if set(bom) == set(comps) else \
        ["parts only in the BOM %s, only in the schematic %s" % (sorted(set(bom) - set(comps)), sorted(set(comps) - set(bom)))]
    for ref, pin, net in ((r[0], r[1], r[2]) for r in nets):
        s = snets.get((ref, pin))
        ok = (s is None or s.startswith("unconnected-")) if not net else (s == net or (s or "").endswith("/" + net))
        if not ok: bad.append("%s.%s csv=%r schematic=%r" % (ref, pin, net, s))
    if bad:
        sys.exit("schematic is stale vs the CSVs - re-run cad/generate_schematic.py first:\n  " + "\n  ".join(bad[:12]))


def main():
    path = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, "gb3.kicad_pcb")
    d = os.path.dirname(path)
    present = json.loads(os.environ["SYNC_PRESENT"])   # refs on the board (pass 1 output); None in pass 1
    comps, snets, lcsc = {}, {}, {}
    if present is not None:
        comps, snets = netlist(os.path.splitext(path)[0] + ".kicad_sch")
        lcsc.update({r[0]: r[2].strip() for r in rows(os.path.join(d, "gb3-bom.csv"))})
        check(comps, snets, set(lcsc), rows(os.path.join(d, "gb3-nets.csv")))

    import pcbnew                       # footprints must be loaded BEFORE LoadBoard (KiCad 10 binding bug,
    MM, TO = pcbnew.FromMM, pcbnew.ToMM  # see place_transducers.py)
    io = pcbnew.PCB_IO_MGR.FindPlugin(pcbnew.PCB_IO_MGR.KICAD_SEXP)
    loaded = {}
    for ref in sorted(set(comps) - set(present or [])):
        lib, name = comps[ref][1].split(":", 1)
        fp = io.FootprintLoad(FPLIBS[lib], name)
        if fp is None: sys.exit("footprint %s not found" % comps[ref][1])
        fp.SetFPID(pcbnew.LIB_ID(lib, name))
        loaded[ref] = fp

    B = pcbnew.LoadBoard(path)
    if present is None:                 # pass 1: just report what is on the board
        print(json.dumps(sorted(fp.GetReference() for fp in B.GetFootprints())))
        sys.stdout.flush(); os._exit(0)

    log = []
    fps = {fp.GetReference(): fp for fp in B.GetFootprints()}
    pads = {(fp.GetReference(), p.GetNumber()): p for fp in B.GetFootprints() for p in fp.Pads()}

    holes = [(TO(p.GetPosition().x), TO(p.GetPosition().y), TO(max(p.GetDrillSize().x, p.GetDrillSize().y)) / 2)
             for o in B.GetFootprints() for p in o.Pads() if p.GetDrillSize().x > 0]

    def free_spot(fp, ax, ay):
        """first spot on rings around (ax, ay) where fp's bbox clears every same-side footprint (parts this
        run deletes don't count) and every drilled hole on either side."""
        side = fp.GetLayer()
        others = [(o.GetBoundingBox(False, False), o) for o in B.GetFootprints()
                  if o is not fp and o.GetLayer() == side
                  and (o.GetReference() in comps or o.GetAttributes() & pcbnew.FP_BOARD_ONLY)]
        ps = pcbnew.SHAPE_POLY_SET(); B.GetBoardPolygonOutlines(ps, True)
        for r in [SEARCH[0] + 0.25 * k for k in range(int((SEARCH[1] - SEARCH[0]) / 0.25) + 1)]:
            for k in range(24):
                a = 2 * math.pi * k / 24
                x, y = ax + r * math.cos(a), ay + r * math.sin(a)
                fp.SetPosition(pcbnew.VECTOR2I(int(MM(x)), int(MM(y))))
                bb = fp.GetBoundingBox(False, False); bb.Inflate(int(MM(GAP)))
                if not all(ps.Contains(pcbnew.VECTOR2I(c[0], c[1])) for c in
                           ((bb.GetLeft(), bb.GetTop()), (bb.GetRight(), bb.GetTop()), (bb.GetLeft(), bb.GetBottom()), (bb.GetRight(), bb.GetBottom()))):
                    continue
                if any(bb.Intersects(ob) for ob, _ in others): continue
                l, t, r_, b = TO(bb.GetLeft()), TO(bb.GetTop()), TO(bb.GetRight()), TO(bb.GetBottom())
                if any(l - hr - HOLE_GAP <= hx <= r_ + hr + HOLE_GAP and t - hr - HOLE_GAP <= hy <= b + hr + HOLE_GAP
                       for hx, hy, hr in holes): continue
                return x, y
        return None

    # add
    for ref, fp in sorted(loaded.items()):
        value, fpid, kpath = comps[ref]
        fp.SetReference(ref); fp.SetValue(value); fp.SetPath(pcbnew.KIID_PATH(kpath))
        B.Add(fp)
        if fp.GetLayer() != pcbnew.B_Cu: fp.SetLayerAndFlip(pcbnew.B_Cu)
        aref, apad = ANCHOR.get(ref, (None, None))
        a = pads.get((aref, apad))
        if a is None: sys.exit("no ANCHOR for new part %s" % ref)
        spot = free_spot(fp, TO(a.GetPosition().x), TO(a.GetPosition().y))
        if spot is None: sys.exit("no free spot for %s near %s.%s" % (ref, aref, apad))
        fps[ref] = fp
        for p in fp.Pads(): pads[(ref, p.GetNumber())] = p
        log.append("  added   %-5s %-28s at (%.2f, %.2f) beside %s.%s" % (ref, fpid, spot[0], spot[1], aref, apad))

    # value + symbol link of the parts already on the board (F8 re-links by reference); a part whose
    # footprint changed is not swapped here
    relinked = 0
    for ref, fp in sorted(fps.items()):
        if ref not in comps or ref in loaded: continue
        value, fpid, kpath = comps[ref]
        if fpid.split(":", 1)[1] != str(fp.GetFPID().GetLibItemName()):
            sys.exit("%s: the schematic wants footprint %s, the board has %s - swap it by hand first"
                     % (ref, fpid, fp.GetFPID().GetLibItemName()))
        if fp.GetValue() != value:
            log.append("  value   %-5s %s -> %s" % (ref, fp.GetValue(), value)); fp.SetValue(value); relinked += 1
        if fp.GetPath().AsString() != kpath:
            log.append("  link    %-5s -> %s (its symbol moved sheet)" % (ref, kpath)); fp.SetPath(pcbnew.KIID_PATH(kpath)); relinked += 1

    # LCSC numbers: the easyeda2kicad footprints carry the number of whichever part was imported with them
    # (every R0603 said C23219, a 620k) and fab tools read this field, so it is stamped from gb3-bom.csv;
    # a part with no LCSC number (hand-fitted / bare pad) gets it blanked
    stamped = 0
    for ref, fp in sorted(fps.items()):
        if ref not in lcsc: continue
        have = fp.GetFieldText("LCSC Part") if fp.HasField("LCSC Part") else None
        if have == lcsc[ref] or (have is None and not lcsc[ref]): continue
        fp.SetField("LCSC Part", lcsc[ref]); stamped += 1
        if have is None:                               # a new field is visible on silk by default
            f = fp.GetField("LCSC Part"); f.SetVisible(False)
            f.SetLayer(pcbnew.B_Fab if fp.GetLayer() == pcbnew.B_Cu else pcbnew.F_Fab)
        log.append("  lcsc    %-5s %s -> %s" % (ref, have if have is not None else "-", lcsc[ref] or "(blank)"))

    # 3D models: easyeda2kicad writes them relative to cad/, the board uses project-relative paths
    models = 0
    for ref, fp in sorted(fps.items()):
        ms = fp.Models()
        for i in range(len(ms)):                       # by index: iterating the vector yields copies
            m = ms[i]
            if m.m_Filename.startswith("symbols/"):
                m.m_Filename = "${KIPRJMOD}/../../../../cad/" + m.m_Filename; models += 1

    # move parts whose role changed - only on the run that changes their nets
    moved = 0
    for ref, (aref, apad) in MOVE.items():
        fp, a = fps.get(ref), pads.get((aref, apad))
        if fp is None or a is None:
            log.append("  WARNING MOVE %s -> %s.%s: no such part or pad" % (ref, aref, apad)); continue
        if all(snets.get((ref, p.GetNumber())) in (None, p.GetNetname()) for p in fp.Pads()): continue
        ax, ay = TO(a.GetPosition().x), TO(a.GetPosition().y)
        d = math.hypot(TO(fp.GetPosition().x) - ax, TO(fp.GetPosition().y) - ay)
        if d <= MOVE_DONE: continue
        old = (TO(fp.GetPosition().x), TO(fp.GetPosition().y))
        spot = free_spot(fp, ax, ay)
        if spot is None:
            fp.SetPosition(pcbnew.VECTOR2I(int(MM(old[0])), int(MM(old[1]))))
            log.append("  MOVE    %-5s no free spot near %s.%s - left at (%.2f, %.2f)" % (ref, aref, apad, old[0], old[1])); continue
        moved += 1
        log.append("  moved   %-5s (%.2f, %.2f) -> (%.2f, %.2f) beside %s.%s (was %.1f mm away)" % (ref, old[0], old[1], spot[0], spot[1], aref, apad, d))

    # remove (collected now, done last: B.Remove leaves the bindings untyped)
    # board-only footprints (LRA1 / SPK1 mechanical outlines) are deliberately absent from the schematic
    gone = [fp for fp in B.GetFootprints() if fp.GetReference() not in comps
            and not fp.GetAttributes() & pcbnew.FP_BOARD_ONLY]
    gone_refs = {fp.GetReference() for fp in gone}

    # nets
    changed = 0
    for ref, num, p in sorted(((fp.GetReference(), p.GetNumber(), p) for fp in B.GetFootprints() for p in fp.Pads()),
                              key=lambda t: t[:2]):      # every pad: several can share a number (EP, shield)
        if ref not in comps or ref in gone_refs: continue
        net = snets.get((ref, num))
        if net is None:
            if num: log.append("  WARNING %s.%s is not in the schematic - net left as %r" % (ref, num, p.GetNetname()))
            continue                                   # (unnumbered mechanical pads have no net)
        cur = p.GetNetname()
        if cur == net: continue
        ni = B.FindNet(net)
        if ni is None:
            ni = pcbnew.NETINFO_ITEM(B, net); B.Add(ni)
        p.SetNet(ni); changed += 1
        log.append("  net     %s.%-5s %-28s -> %s" % (ref, num, cur or "-", net))

    for fp in gone:
        log.append("  removed %-5s" % fp.GetReference()); B.Remove(fp)

    pcbnew.ZONE_FILLER(B).Fill(B.Zones())
    pro = os.path.splitext(path)[0] + ".kicad_pro"
    keep = open(pro, "rb").read() if os.path.exists(pro) else None
    pcbnew.SaveBoard(path, B)
    if keep is not None and open(pro, "rb").read() != keep:
        open(pro, "wb").write(keep)                    # SaveBoard rewrites the project file; it isn't ours to change
    print("sync_netlist: %d added, %d removed, %d moved, %d values/links updated, %d LCSC fields stamped, "
          "%d 3D-model paths fixed, %d pad nets changed" % (len(loaded), len(gone), moved, relinked, stamped, models, changed))
    print("\n".join(log)); sys.stdout.flush(); os._exit(0)


if __name__ == "__main__":
    if "SYNC_PRESENT" not in os.environ:
        # two passes in two processes: footprints have to be loaded before LoadBoard, but which ones to
        # load depends on what the board already has
        args = sys.argv[1:]
        out = subprocess.run([sys.executable, os.path.abspath(__file__)] + args,
                             capture_output=True, text=True, env=dict(os.environ, SYNC_PRESENT="null"))
        listed = [l for l in out.stdout.splitlines() if l.startswith("[")]
        if out.returncode or not listed: sys.exit("pass 1 failed:\n" + out.stdout + out.stderr)
        r = subprocess.run([sys.executable, os.path.abspath(__file__)] + args,
                           env=dict(os.environ, SYNC_PRESENT=listed[-1]))
        sys.exit(r.returncode)
    main()
