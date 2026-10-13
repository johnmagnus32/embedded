#!/usr/bin/env python3
"""fab.py — gb2's fabrication + assembly outputs for JLCPCB, and the pre-fab footprint checklist.

Writes (into fab/out/, regenerated every run; not committed):
  gerbers/            F/In1/In2/B copper, paste, mask, silk, Edge.Cuts (Protel extensions, X2, silk minus mask)
                      + Excellon drills (PTH / NPTH separate, mm, absolute origin) + drill map PDF
  gb2-gerbers.zip     the above, zipped for upload
  gb2-bom-jlc.csv     JLC assembly BOM: Comment, Designator, Footprint, LCSC Part #  (BOM.md "Src" = JLC only)
  gb2-cpl-jlc.csv     JLC placement: Designator, Mid X, Mid Y, Layer, Rotation  (the same parts)
  gb2-hand-parts.csv  everything you fit yourself (BOM.md "Src" = Hand / DigiKey / Adafruit)
and fab/FOOTPRINT_CHECKLIST.md (committed): one row per footprint / part, side, pin-1 / polarity notes.

Run:  python3 fab/fab.py        (needs kicad-cli; the board must be saved with zones filled — pour.py does that)
"""
import os, sys, csv, re, subprocess, zipfile, collections, shutil
HERE = os.path.dirname(os.path.abspath(__file__)); BRD = os.path.join(HERE, "..")
PCB = os.path.join(BRD, "gb2.kicad_pcb"); OUT = os.path.join(HERE, "out")
CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
LAYERS = "F.Cu,In1.Cu,In2.Cu,B.Cu,F.Paste,B.Paste,F.Mask,B.Mask,F.Silkscreen,B.Silkscreen,Edge.Cuts"

def run(*a):
    r = subprocess.run([CLI, *a], capture_output=True, text=True)
    if r.returncode: sys.exit(f"kicad-cli {' '.join(a[:3])} failed:\n{r.stdout}{r.stderr}")
shutil.rmtree(OUT, ignore_errors=True); g = os.path.join(OUT, "gerbers"); os.makedirs(g)
run("pcb", "export", "gerbers", "-o", g + "/", "-l", LAYERS, "--subtract-soldermask", "--check-zones", PCB)
run("pcb", "export", "drill", "-o", g + "/", "--format", "excellon", "--excellon-units", "mm",
    "--excellon-zeros-format", "decimal", "--excellon-separate-th", "--generate-map", "--map-format", "pdf", PCB)
with zipfile.ZipFile(os.path.join(OUT, "gb2-gerbers.zip"), "w", zipfile.ZIP_DEFLATED) as z:
    for f in sorted(os.listdir(g)):
        if not f.endswith(".pdf"): z.write(os.path.join(g, f), f)
pos = os.path.join(OUT, "pos.csv")
run("pcb", "export", "pos", "-o", pos, "--side", "both", "--format", "csv", "--units", "mm", PCB)

# ---- BOM.md: part rows by index (#), with their source column
rows = {}
for l in open(os.path.join(BRD, "..", "BOM.md"), encoding="utf-8"):
    c = [x.strip() for x in l.strip().strip("|").split("|")]
    if l.startswith("| ") and c[0].isdigit() and len(c) == 9: rows[int(c[0])] = c
bom = list(csv.DictReader(open(os.path.join(BRD, "gb2-bom.csv"), encoding="utf-8")))
def idx(note):
    m = re.search(r"#(\d+)\s*$", note); return int(m.group(1)) if m else None
placed = {r["Ref"]: r for r in csv.DictReader(open(pos, encoding="utf-8"))}
jlc, hand, problems = collections.OrderedDict(), [], []
for b in bom:
    ref = b["refdes"]; i = idx(b["note"]); row = rows.get(i)
    src = row[7] if row else "?"; part = row[3] if row else b["note"]
    if ref not in placed: problems.append(f"{ref}: not on the board"); continue
    if src == "JLC":
        if not b["lcsc"].startswith("C"): problems.append(f"{ref}: JLC part with no LCSC number"); continue
        jlc.setdefault(b["lcsc"], {"comment": re.sub(r"\s*·.*", "", part), "fp": b["symbol"].split(":")[-1], "refs": []})["refs"].append(ref)
    elif src != "—":
        hand.append((ref, part, src, placed[ref]["Side"]))
with open(os.path.join(OUT, "gb2-bom-jlc.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f); w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
    for lc, e in jlc.items(): w.writerow([e["comment"], ",".join(sorted(e["refs"], key=lambda r: (re.sub(r"\d", "", r), int(re.sub(r"\D", "", r) or 0)))), placed[e["refs"][0]]["Package"], lc])
with open(os.path.join(OUT, "gb2-cpl-jlc.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f); w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
    for e in jlc.values():
        for r in e["refs"]:
            p = placed[r]
            w.writerow([r, p["PosX"] + "mm", p["PosY"] + "mm", "Top" if p["Side"] == "top" else "Bottom", p["Rot"]])
with open(os.path.join(OUT, "gb2-hand-parts.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f); w.writerow(["Designator", "Part", "Source", "Side"]); w.writerows(hand)
os.remove(pos)
side = collections.Counter(placed[r]["Side"] for e in jlc.values() for r in e["refs"])
print(f"gerbers: {len(os.listdir(g))} files -> out/gb2-gerbers.zip")
print(f"JLC assembly: {sum(len(e['refs']) for e in jlc.values())} parts, {len(jlc)} LCSC lines (top {side['top']}, bottom {side['bottom']})")
print(f"hand-fit: {len(hand)} parts ({', '.join(r for r, *_ in hand)})")
for p in problems: print("  PROBLEM:", p)

# ======================================================================== footprint checklist
SPECIAL = {
    "J3": "**Orientation against the real 2090 (BOM.md #16):** pad 1 must land on the module's JP1 pin 1. Hold the module over a 1:1 print, screen up, and check pin 1 sits at the +Y end (place.py: 180 deg from v1's socket). Also check the socket's tails fit the 1.0 mm drill (changed from 1.2 for routing room; KiCad's PinSocket uses 1.0 for 0.64 mm square tails).",
    "J4": "Same check against the module's JP2 (pins 1 + 11 = GND).",
    "J7": "USB-C: front shell tabs reach the edge on purpose (0.3 mm, custom DRC rule); the two locating pegs are NPTH; check the mouth faces out of the top edge.",
    "J8": "**Battery polarity (BOM.md #54):** the '+' pad must meet the Adafruit 2011's red lead (Adafruit numbers these pads reversed). Check against the cell's JST plug before it's assembled.",
    "J1": "SWD header: pin 1 (square pad) = 3V3 at the west end; silk labels per pin.",
    "J5": "Speaker pads: SPK_P on the square pad.", "J6": "LRA pads: LRA_P on the square pad (polarity doesn't matter for an LRA).",
    "SW1": "Side slide switch: lever must point out through the bottom-left edge; check against the shell slot.",
    "SW10": "Side tactile (Vol+): actuator faces out of the right edge.", "SW11": "Side tactile (Vol-): actuator faces out of the right edge.",
    "SW12": "Shoulder tactile (L): actuator faces out of the top edge.", "SW13": "Shoulder tactile (R): actuator faces out of the top edge.",
    "U12": "BT830 (back): antenna end inside the board-edge keep-out; pin 1 dot; castellated pads hand-soldered.",
    "Y1": "3225 crystal: pins 1/3 are the crystal, 2/4 GND - check the footprint's pin numbering against the datasheet (a 90 deg error shorts the oscillator to GND).",
    "Q1": "SOT-23 P-FET: G / S / D pin order against the AO3401A datasheet.",
    "U10": "TPS63021 VSON-14: easyeda2kicad import - check the land pattern + exposed pad against TI's recommended pattern.",
    "U9": "BQ24074 VQFN-16: easyeda2kicad import - check the land pattern + exposed pad against TI's recommended pattern.",
    "U2": "iCE40UP5K QFN-48 (SG48): EP = GND; pin 1 corner.",
    "D1": "LED: cathode (bar on silk) = pad 1 (power LED D1 -> GND; CDONE LED D2 likewise).",
}
def kind(ref):
    p = re.sub(r"\d", "", ref)
    return {"U": 0, "Y": 1, "Q": 1, "D": 1, "J": 2, "SW": 3, "L": 4, "FB": 5, "R": 6, "C": 6, "TP": 7}.get(p, 4)
GENERIC = {0: "IC: pin-1 corner + land pattern (pad size / pitch / EP) vs the datasheet's recommended footprint",
           1: "polarized / pinout-sensitive: pin numbering vs the datasheet", 2: "connector: pinout, orientation, mating side",
           3: "switch: footprint vs datasheet, actuator direction", 4: "size vs datasheet", 5: "0603 - standard", 6: "0603 - standard",
           7: "bare pad"}
lines = collections.OrderedDict()
for b in bom:
    ref = b["refdes"]; i = idx(b["note"]); row = rows.get(i)
    if ref not in placed: continue
    key = (b["symbol"], b["lcsc"], i)
    e = lines.setdefault(key, {"refs": [], "part": row[3] if row else "", "src": row[7] if row else "?", "side": set(), "k": 9})
    e["refs"].append(ref); e["side"].add(placed[ref]["Side"]); e["k"] = min(e["k"], kind(ref))
docs = os.listdir(os.path.join(BRD, "..", "..", "..", "..", "cad", "docs"))
with open(os.path.join(HERE, "FOOTPRINT_CHECKLIST.md"), "w", encoding="utf-8") as f:
    f.write("# gb2 pre-fab footprint checklist\n\n"
            "Generated by `fab/fab.py` from gb2-bom.csv, BOM.md and the board. Wrong footprints, not routing, are what\n"
            "usually kills a first spin, so check each line against the part's datasheet before ordering. The fastest way:\n"
            "print the F.Fab / B.Fab layers 1:1 (KiCad: File > Print, 1:1 scale, check a 10 mm dimension with a ruler) and lay\n"
            "the real connectors, switches, the 2090 module and the battery on the paper.\n\n"
            "Highest risk first. LCSC links go to the part page (datasheet PDF linked there); local copies in `cad/docs/`.\n\n"
            "| ✓ | Refs | Part | Footprint | Side | Fit | What to check |\n|---|---|---|---|---|---|---|\n")
    for (sym, lc, i), e in sorted(lines.items(), key=lambda kv: (kv[1]["k"], kv[1]["refs"][0])):
        refs = ", ".join(sorted(e["refs"], key=lambda r: (re.sub(r"\d", "", r), int(re.sub(r"\D", "", r) or 0))))
        notes = [SPECIAL[r] for r in e["refs"] if r in SPECIAL] or [GENERIC[e["k"]]]
        part = e["part"].replace("|", "/")
        link = f"[{lc}](https://www.lcsc.com/product-detail/{lc}.html)" if lc.startswith("C") else "—"
        f.write(f"| ☐ | {refs} | {part} ({link}) | `{placed[e['refs'][0]]['Package']}` | {'/'.join(sorted(e['side']))} | {e['src']} | {' '.join(notes)} |\n")
    f.write("\n## Assembly files (fab/out/, check in JLC's online preview)\n\n"
            "- **Rotations:** easyeda2kicad footprints come from the same EasyEDA library JLC's placement uses, so most need no\n"
            "  offset - but check every polarized part (ICs, diodes, the LED, Q1, the crystal) in JLC's 3D/2D preview,\n"
            "  and especially the 35 **bottom-side** parts (rotation conventions differ most there).\n"
            "- **Board:** 4 layers, 1.6 mm, stackup **JLC04161H-7628** (the I2C / impedance estimates assume it); 1 oz outer;\n"
            "  min track/space 0.15/0.15 mm; vias 0.45/0.25 mm tented; ENIG recommended (0.5 mm-pitch QFN/LQFP/TSSOP flatness).\n"
            "- **Double-sided assembly** (back: MCU / FPGA decap rings, charger parts). Hand-fit after: J1, J3, J4 (sockets),\n"
            "  U12 (BT830, back), J5 speaker, J6 LRA wires.\n")
print("checklist: fab/FOOTPRINT_CHECKLIST.md,", len(lines), "lines")
