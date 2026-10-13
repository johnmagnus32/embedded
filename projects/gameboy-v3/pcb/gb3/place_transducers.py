#!/usr/bin/env python3
"""place_transducers.py — add the speaker (SPK1) and LRA (LRA1) 3D-reference footprints at their
outline positions so they show in the 3D viewer. Board-only mech refs (no pads, not in schematic);
place.py ignores them (not in the BOM). Re-runnable: removes SPK1/LRA1 first.

Tune SPK_* / LRA_* below (rotation deg, Z mm) from the render, then re-run.
Run:  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 place_transducers.py
"""
import pcbnew, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(HERE, "gb3.kicad_pcb")
LIB = os.path.abspath(os.path.join(HERE, "../../../../cad/symbols/manual.pretty"))
MM = pcbnew.FromMM
def V(x, y): return pcbnew.VECTOR2I(int(x), int(y))

# pcbnew.FootprintLoad() is broken in KiCad 10: its GetPluginForPath() helper hands back a raw SwigPyObject
# with no FootprintLoad method, so the call dies with AttributeError. Go through the IO manager instead --
# but ONLY before LoadBoard(): loading a board leaves the PCB_IO swig binding untyped, after which even
# FindPlugin() returns a bare SwigPyObject. Same goes for fp.Models() -- it degrades to a non-subscriptable
# SwigPyObject once a board is loaded. So do ALL footprint/3D-model work here, before LoadBoard, and hand
# main() finished footprints plus the model names it wants to report.
def load_footprints(lib, parts):
    io = pcbnew.PCB_IO_MGR.FindPlugin(pcbnew.PCB_IO_MGR.KICAD_SEXP)
    out = {}
    for name, ref, xy, rot, z in parts:
        fp = io.FootprintLoad(lib, name)
        m = fp.Models()[0]
        if z: m.m_Offset = pcbnew.VECTOR3D(m.m_Offset.x, m.m_Offset.y, z)
        out[name] = (fp, m.m_Filename.split('/')[-1])
    return out

# transducer outline centres (match SPEAKER/LRA in place.py)
SPK_XY, LRA_XY = (197.3, 102.8), (133.4, 77.2)   # keep in sync with place.py SPEAKER / LRA
SPK_ROT, SPK_Z = 0, 0.0          # deg, mm  (tune from render)
LRA_ROT, LRA_Z = 0, 0.0


def main():
    PARTS = (("SPEAKER_CES2704", "SPK1", SPK_XY, SPK_ROT, SPK_Z),
             ("LRA_VG0832022D", "LRA1", LRA_XY, LRA_ROT, LRA_Z))
    lib = load_footprints(LIB, PARTS)                          # before LoadBoard -- see load_footprints()

    B = pcbnew.LoadBoard(PATH)
    for ref in ("SPK1", "LRA1"):
        fp = B.FindFootprintByReference(ref)
        if fp: B.Remove(fp)
    for name, ref, (x, y), rot, z in PARTS:
        fp, model = lib[name]
        B.Add(fp)
        fp.SetReference(ref)
        fp.SetPosition(V(MM(x), MM(y)))
        if not fp.IsFlipped(): fp.Flip(V(MM(x), MM(y)), False)     # back cavity
        fp.SetOrientationDegrees(rot)
        # No fp.GetPosition()/fp.Models() here: on a plugin-loaded footprint every swig-typed RETURN value
        # degrades to a bare SwigPyObject once a board has been loaded (setters and bool getters are fine).
        print(f"{ref}: @({x},{y}) flipped={fp.IsFlipped()} rot={rot} model={model}")
    pcbnew.SaveBoard(PATH, B)
    sys.stdout.flush(); os._exit(0)


if __name__ == "__main__":
    main()
