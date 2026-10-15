#!/usr/bin/env python3
"""gen.py — regenerate the gb2b schematic with cad/generate_schematic.py, plus the project-local `gb2b` library.

The shared generator only loads `easyeda2kicad` + `manual` from one --symbols directory. Rev B's new parts live in
lib/gb2b.kicad_sym (nickname `gb2b`), so this wrapper loads that library too and then runs the generator unchanged
(same CLI, same VALIDATE gate). It does not modify cad/generate_schematic.py.

Run (close KiCad first), from anywhere:
    python3 projects/gameboy-v2/pcb/gb2b/gen.py            # regenerates gb2b in place, then run title.py
    python3 projects/gameboy-v2/pcb/gb2b/gen.py --project /tmp/x/gb2b   # dry run into a copy (same CSV args)
Without --bom/--nets it uses this directory's gb2b-bom.csv / gb2b-nets.csv; --project defaults to this directory.
"""
import importlib.util
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
GEN = REPO / "cad/generate_schematic.py"
LOCAL_LIB_DIR = HERE / "lib"
LOCAL_NICK = "gb2b"

spec = importlib.util.spec_from_file_location("generate_schematic", GEN)
gs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gs)

_load = gs.load_symbols


def load_symbols(symbols_dir, cli):
    """The shared libraries as usual, then lib/gb2b.kicad_sym under the `gb2b` nickname."""
    by_libid = _load(symbols_dir, cli)
    saved = gs.LIBS
    gs.LIBS = [LOCAL_NICK]
    try:
        local = _load(LOCAL_LIB_DIR, cli)
    finally:
        gs.LIBS = saved
    print(f"  + {len(local)} symbols from {LOCAL_LIB_DIR / (LOCAL_NICK + '.kicad_sym')}")
    by_libid.update(local)
    return by_libid


gs.load_symbols = load_symbols

if __name__ == "__main__":
    argv = sys.argv[1:]
    if "--bom" not in argv:
        argv += ["--bom", str(HERE / "gb2b-bom.csv")]
    if "--nets" not in argv:
        argv += ["--nets", str(HERE / "gb2b-nets.csv")]
    if "--project" not in argv:
        argv += ["--project", str(HERE)]
    sys.argv = [str(GEN)] + argv
    gs.main()
