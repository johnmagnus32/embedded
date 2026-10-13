#!/usr/bin/env python3
"""import_route.py — pull a FreeRouting Specctra session (.ses) back into gb3.kicad_pcb, then re-pour copper.

The loop (KiCad 10 has no autorouter and kicad-cli has no DSN export, but the Python API has both):

  1. prep_route.py        clear the pre-existing DRC errors, refill
  2. route.sh             export_dsn.py -> N parallel FreeRouting 2.1.0 jobs (via FrRun.java) -> each
                          scored through this script + fanout_planes.py + DRC -> route/best.ses
  3. import_route.py route/best.ses, then fanout_planes.py, finish_planes.py (plane antipads) and
     post_route_fixes.py (audit fixes)                                       apply the winner

FreeRouting 2.1.0 gotchas, all learned the hard way:
  * 2.2+ is built for Java 25 (class file 69); 2.1.0 is the newest that runs on Java 23.
  * It parses -mp but never wires it to the batch loop, whose stop pass is hardcoded at 999, so a run
    never ends on its own. The job timeout (FREEROUTING__ROUTER__JOB_TIMEOUT) is the only working stop.
  * The -do .ses is NOT the board it reports. The output is only re-serialised from job.board inside a
    board-updated event; after a "Restoring an earlier board" step the router works on a copy that
    reaches job.board only once the loop exits, with no event after it. Any run long enough to be
    useful restores, so a 50-minute run reporting 146 unrouted wrote a 269-unconnected board. -mt makes
    it worse. FrRun.java writes job.board itself at JVM exit; its file matches the printed stats.
  * It keeps only its 0.2 mm clearance to the outline, not the 0.5 mm edge rule; export_dsn.py adds a
    DSN-only keepout band along the edge.
  * Telemetry to api.freerouting.app is on by default; route.sh turns it off.
  * caffeinate -i or the laptop idles to sleep and the job just pauses.

ImportSpecctraSES replaces every track and via on the board with the session's, so re-running this
with a newer .ses is safe. Zones are re-poured afterwards because the new vias and traces cut them.

Via drills are re-derived from their diameters. KiCad's SES import takes a via's diameter from the
router's padstack but its drill from the net's netclass, so a HighCurrent net (via 1.0/0.5) routed
with the 0.6/0.3 padstack came in as 0.6/0.5, a 0.05 mm ring. The router cleared room for the 0.6
pad, so the drill goes back to diameter - 2 * RING and the copper footprint doesn't change.

FreeRouting leaves dead ends, found with KiCad's own DRC (track_dangling / via_dangling):
  * junk -- orphan vias and sub-STUB_MAX slivers (0.0001 mm segments are common). Deleted, repeating
    until none are left, since removing one can expose the next.
  * partial routes -- a long trace that stops short of its pad because the job timed out mid-route.
    Kept and listed: they are most of the way there and a better start for hand routing than nothing.

Run:  <kicad python> import_route.py gb3.ses [out.kicad_pcb]
      (default out is gb3.kicad_pcb in place)
"""
import pcbnew, os, sys, re, json, subprocess, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
BOARD = os.path.join(HERE, "gb3.kicad_pcb")
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
DANGLING = ("track_dangling", "via_dangling")
STUB_MAX = 0.5                                   # mm; a dangling track shorter than this is junk
RING = pcbnew.FromMM(0.15)                       # annular ring to restore; rule minimum is 0.10


def dangling(path):
    """(junk uuids, partial-route descriptions) DRC flags as dead ends. Runs beside the .kicad_pro so
    the project's rules apply."""
    rpt = os.path.join(tempfile.mkdtemp(), "drc.json")
    subprocess.run([KICAD_CLI, "pcb", "drc", "--format", "json", "-o", rpt,
                    "--severity-error", "--severity-warning", path],
                   cwd=os.path.dirname(path), capture_output=True, check=True)
    junk, partial = set(), []
    for v in json.load(open(rpt))["violations"]:
        if v["type"] not in DANGLING: continue
        it = v["items"][0]
        m = re.search(r"length ([0-9.]+) mm", it["description"])
        if v["type"] == "via_dangling" or (m and float(m.group(1)) < STUB_MAX):
            junk.add(it["uuid"])
        else:
            partial.append("%s at (%.2f, %.2f)" % (it["description"], it["pos"]["x"], it["pos"]["y"]))
    return junk, partial


def strip(path, uuid_file):
    """--strip mode: delete the listed tracks/vias, re-pour, save. Its own process each pass: after a
    Remove + Fill the KiCad 10 bindings lose their type table (Tracks() comes back as a bare
    SwigPyObject), and a second LoadBoard in one process does the same."""
    dead = set(open(uuid_file).read().split())
    B = pcbnew.LoadBoard(path)
    n = {"track": 0, "via": 0}
    for t in list(B.GetTracks()):
        if t.m_Uuid.AsString() in dead:
            n["via" if t.Type() == pcbnew.PCB_VIA_T else "track"] += 1
            B.Remove(t)
    pcbnew.ZONE_FILLER(B).Fill(B.Zones())
    pcbnew.SaveBoard(path, B)
    sys.stdout.write("%d %d\n" % (n["track"], n["via"])); sys.stdout.flush(); os._exit(0)


def main():
    if len(sys.argv) == 4 and sys.argv[1] == "--strip":
        strip(sys.argv[2], sys.argv[3])
    if len(sys.argv) < 2:
        sys.stderr.write("usage: import_route.py <session.ses> [out.kicad_pcb]\n"); os._exit(2)
    ses = os.path.abspath(sys.argv[1])
    out = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else BOARD

    B = pcbnew.LoadBoard(BOARD)
    before = (len([t for t in B.GetTracks() if t.Type() == pcbnew.PCB_TRACE_T]),
              len([t for t in B.GetTracks() if t.Type() == pcbnew.PCB_VIA_T]))
    if not pcbnew.ImportSpecctraSES(B, ses):
        sys.stderr.write("ImportSpecctraSES failed on %s\n" % ses); os._exit(1)
    tracks = [t for t in B.GetTracks() if t.Type() == pcbnew.PCB_TRACE_T]
    vias = [t for t in B.GetTracks() if t.Type() == pcbnew.PCB_VIA_T]
    redrilled = 0
    for v in vias:
        w = v.GetWidth(pcbnew.F_Cu)
        if v.GetDrillValue() > w - 2 * RING:
            v.SetDrill(w - 2 * RING); redrilled += 1

    pcbnew.ZONE_FILLER(B).Fill(B.Zones())
    pcbnew.SaveBoard(out, B)

    removed = {"track": 0, "via": 0}
    for _ in range(8):
        dead, partial = dangling(out)
        if not dead: break
        uf = os.path.join(tempfile.mkdtemp(), "dead.txt")
        open(uf, "w").write("\n".join(sorted(dead)))
        r = subprocess.run([sys.executable, os.path.abspath(__file__), "--strip", out, uf],
                           capture_output=True, text=True, check=True)
        nt, nv = (int(x) for x in r.stdout.split())
        removed["track"] += nt; removed["via"] += nv
        if nt + nv == 0: break

    length = sum(pcbnew.ToMM(t.GetLength()) for t in tracks)
    sys.stderr.write("import_route: %s -> %s\n" % (os.path.basename(ses), out))
    sys.stderr.write("  tracks %d -> %d  (%.0f mm)   vias %d -> %d   zones refilled\n"
                     % (before[0], len(tracks), length, before[1], len(vias)))
    sys.stderr.write("  vias re-drilled to their padstack: %d\n" % redrilled)
    sys.stderr.write("  dead ends removed: %d tracks, %d vias\n" % (removed["track"], removed["via"]))
    for p in dangling(out)[1]:
        sys.stderr.write("  partial route kept: %s\n" % p)
    sys.stderr.flush(); os._exit(0)


if __name__ == "__main__":
    main()
