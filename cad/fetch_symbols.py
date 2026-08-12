#!/usr/bin/env python3
"""
fetch_symbols.py — Download KiCad symbols + footprints from JLCPCB/LCSC
                   and patch in human-readable descriptions.

Prerequisites:
    pip install easyeda2kicad

Usage:
    cd /home/johmagnu/learning/embedded/cad
    source .venv/bin/activate   # (if a venv with easyeda2kicad exists)
    python3 fetch_symbols.py

Output:
    symbols/easyeda2kicad.kicad_sym       — schematic symbols
    symbols/easyeda2kicad.pretty/*.kicad_mod — PCB footprints
"""

import subprocess
import re
import sys
from pathlib import Path

OUTPUT = "./symbols"
SYM_FILE = f"{OUTPUT}/easyeda2kicad.kicad_sym"

# BOM: (LCSC_ID, description)
PARTS = [
    ("C94355",   "MCU, ARM Cortex-M4, 512KB Flash, 128KB RAM, 100MHz, LQFP-64"),
    ("C82942",   "3.3V LDO Regulator, 500mA, SOT-23-5"),
    ("C97521",   "128Mbit SPI NOR Flash, SOIC-8"),
    ("C910544",  "I2S Class-D Mono Amplifier, 3.2W, TQFN-16"),
    ("C106215",  "1µF 16V X5R Ceramic Capacitor, 0603"),
    ("C1713",    "10µF 16V X5R Ceramic Capacitor, 0805"),
    ("C14663",   "100nF 50V X7R Ceramic Capacitor, 0603"),
    ("C100042",  "10nF 50V X7R Ceramic Capacitor, 0603"),
    ("C109456",  "4.7µF 6.3V X5R Ceramic Capacitor, 0603"),
    ("C17024",   "10µF 10V X5R Ceramic Capacitor, 0805"),
    ("C99198",   "10KΩ 1% Resistor, 0603"),
    ("C105578",  "1MΩ 1% Resistor, 0603"),
    ("C14675",   "100KΩ 1% Resistor, 0603"),
    ("C23193",   "510Ω 1% Resistor, 0603"),
    ("C105580",  "5.1KΩ 1% Resistor, 0603"),
    ("C1002",    "Ferrite Bead 600Ω@100MHz, 0603"),
    ("C262640",  "18-pin 0.5mm FPC Connector, Top Contact"),
    ("C295747",  "JST PH 1.25mm 2-pin Connector, SMD"),
    ("C3029359", "MX1.25 (PicoBlade) 1.25mm 2-pin Connector, SMD Right-Angle (speaker)"),
    ("C165948",  "USB Type-C Receptacle, Power Only, 16-pin"),
    ("C358687",  "Pin Header 1x5, 2.54mm, Through-Hole"),
    ("C49257",   "Pin Header 1x3, 2.54mm, Through-Hole"),
    ("C318884",  "Tactile Switch 6x6mm, SMD"),
    ("C2986027", "Green LED, 0603"),
    ("C2905435", "Pin Header 1x4, 2.54mm, Through-Hole"),
    ("C37208",   "Pin Header 1x6, 2.54mm, Through-Hole"),
    ("C2932700", "Pin Header 1x7, 2.54mm, Through-Hole"),
    ("C50981",   "Pin Header 1x20, 2.54mm, Through-Hole"),
    ("C41417332","Female Header 1x20, 2.54mm, Square-Hole, Through-Hole (TFT mount)"),
    # --- ice40-breakout (projects/ice40-breakout) ---
    ("C2678152", "FPGA, iCE40UP5K, 5.3K LUT, QFN-48 7x7mm 0.5mm pitch"),
    ("C151376",  "1.2V LDO Regulator, 300mA, SOT-23-5"),
    ("C2932703", "Pin Header 1x13, 2.54mm, Through-Hole"),
    # --- t113-breakout (projects/t113-breakout) ---
    ("C49569761","Female Header 1x25, 2.54mm, Through-Hole (GPIO breakout, 4×)"),
    ("C5197687", "SoC, Allwinner T113-S3, dual Cortex-A7 + 128MB DDR3, eLQFP-128"),
    ("C77234",   "Synchronous Buck Regulator, FP6161 1A 0.6V-FB, SOT-23-5"),
    ("C135262",  "Power Inductor, 2.2uH 3.4A, 4x4mm SMD"),
    ("C140074",  "680KΩ 1% Resistor, 0603"),
    ("C22807",   "150KΩ 1% Resistor, 0603"),
    ("C23242",   "75KΩ 1% Resistor, 0603"),
    ("C22467599","microSD/TF Card Socket, Push-Push, SMD"),
    ("C70571",   "Crystal, 24MHz CL=18pF, SMD3225-4P"),
    ("C97607",   "Crystal, 32.768kHz CL=12.5pF, SMD2012-2P"),
    ("C1555",    "22pF 50V C0G Ceramic Capacitor, 0402"),
    ("C1549",    "18pF 50V C0G Ceramic Capacitor, 0402"),
    ("C32949",   "10pF 50V C0G Ceramic Capacitor, 0402 (buck FB feed-forward)"),
    ("C23350",   "240Ω 1% Resistor, 0603 (DDR ZQ calibration)"),
    # --- gameboy-v3 (projects/gameboy-v3/pcb/BOM.md) ---
    # NOTE: gameboy-v3 also reuses many parts from the t113-breakout section above
    # (T113 C5197687, FP6161 C77234, crystals, decoupling caps, resistors, microSD,
    # USB-C, JST-PH C295747, etc.) — only the net-new gameboy-v3 parts are listed here.
    ("C207280",    "Littelfuse SP3004-04XTG ×2 — microSD ESD array"),  # #12
    ("C9160",      "BOOMELE 0.5-40PFGPZ — Display FPC connector, 40P 0.5mm"),  # #25
    ("C2919568",   "HDGC 1.0K-1.5-6PWB — Touch FFC connector, 6P 1.0mm"),  # #26
    ("C58756",     "TI TPS61165DBVR — Backlight WLED boost driver, SOT-23-6"),  # #27
    ("C135263",    "SMNR4020-10UH — BL boost inductor, 10uH 1.6A"),  # #28
    ("C2480",      "SS14 — BL boost Schottky diode, 40V 1A, SMA"),  # #29
    ("C125847",    "YAGEO CC0805KKX7R9BB225 — BL boost output cap, 2.2uF 50V X7R 0805"),  # #30
    ("C326590",    "YAGEO CC0402KRX7R7BB224 — BL boost COMP cap, 220nF 16V X7R 0402"),  # #33
    ("C481766",    "ST LSM6DSOXTR — 6-axis IMU, LGA-14"),  # #34
    ("C527464",    "TI DRV2605LDGSR — Haptic driver, VSSOP-10"),  # #39
    ("C15849",     "1uF 16V X5R Ceramic Capacitor, 0603 (haptic REG cap)"),  # #40
    ("C54313",     "TI BQ24074RGTR — Li-ion power-path charger, VQFN-16"),  # #51
    ("C19666",     "4.7uF 16V X5R Ceramic Capacitor, 0603 (charger IN cap)"),  # #52
    ("C22975",     "2KΩ 1% Resistor, 0603 (charger ISET)"),  # #55
    ("C22765",     "1.2KΩ 1% Resistor, 0603 (charger ILIM)"),  # #56
    ("C970725",    "CH224K — USB-PD sink controller"),  # #58
    ("C21190",     "1KΩ 1% Resistor, 0603 (CH224K VDD series R)"),  # #59
    ("C202140",    "TI TPS63021DSJR — 3.3V buck-boost regulator, ~2A, QFN"),  # #63
    ("C21189",     "0Ω Resistor, 0603 (buck-boost FB tie)"),  # #64
    ("C45783",     "22uF 25V X5R Ceramic Capacitor, 0805 (buck-boost output cap)"),  # #68
    ("C408407",    "MWSA0503S-1R5MT — Buck-boost inductor, 1.5uH"),  # #69
    ("C22827",     "180KΩ 1% Resistor, 0603 (fuel-gauge divider R_top)"),  # #72
    ("C25803",     "100KΩ 1% Resistor, 0603 (fuel-gauge divider R_bot)"),  # #73
    ("C2682616",   "Maxim MAX17048G+T10 — 1-cell Li battery fuel gauge, TDFN"),  # #74
    ("C49851",     "TI INA226AIDGSR — I2C current/power monitor, VSSOP-10"),  # #78
    ("C393072",    "RLP25FEGMR010 — 10mΩ 1% 3W current-sense shunt, 2512"),  # #79
    ("C2864778",   "NXP PCA9555PWR — 16-bit I2C GPIO expander, TSSOP-24"),  # #82
    ("C49234144",  "HX-3x5-CA-1.6N — Tactile switch, right-angle (volume +/-)"),  # #88
    ("C109022",    "ST STM6601CA2BDM6F — Push-button on/off controller, TSOT-23-6"),  # #91
    ("C22843",     "1.5KΩ 1% Resistor, 0603 (I2C pull-ups)"),  # #94
    ("C7519",      "STMicro USBLC6-2SC6 — USB2 D+/D- ESD array, SOT-23-6"),  # #95
    ("C107671",    "TI PCM5102APWR — Stereo I2S DAC (headphone out), TSSOP-20"),  # #100
    ("C69901",     "TI TPA6132A2RTER — Stereo headphone amp, QFN-16"),  # #105
    ("C145813",    "PJ-327C-4A — 3.5mm TRS headphone jack, SMD"),  # #110
]


def already_fetched(lcsc_id):
    """True if this LCSC id's symbol is already in the .kicad_sym library."""
    sym_path = Path(SYM_FILE)
    if not sym_path.exists():
        return False
    return lcsc_id in sym_path.read_text()


def fetch_all():
    """Download symbols and footprints from LCSC for any parts not already present."""
    Path(OUTPUT).mkdir(parents=True, exist_ok=True)
    print("Fetching symbols and footprints from JLCPCB/LCSC...\n")

    fetched = skipped = failed = 0
    for i, (lcsc_id, desc) in enumerate(PARTS, 1):
        if already_fetched(lcsc_id):
            print(f"  [{i:2d}/{len(PARTS)}] {lcsc_id} — already present, skipping")
            skipped += 1
            continue

        print(f"  [{i:2d}/{len(PARTS)}] {lcsc_id} — {desc}")
        # Invoke via the current interpreter so it works whether or not the
        # venv is activated (easyeda2kicad need not be on PATH).
        result = subprocess.run(
            [sys.executable, "-m", "easyeda2kicad", f"--lcsc_id={lcsc_id}",
             "--symbol", "--footprint", "--overwrite", "--output", OUTPUT],
            capture_output=True, text=True
        )
        if result.returncode != 0:
            print(f"         WARN: {result.stderr.strip()}")
            failed += 1
        else:
            fetched += 1

    print(f"\nFetched {fetched} new, skipped {skipped} existing, {failed} failed "
          f"(of {len(PARTS)} total).")


def _skip_string(text, i):
    """text[i] is an opening quote; return the index just past the closing quote."""
    i += 1
    while i < len(text):
        if text[i] == '\\':
            i += 2
            continue
        if text[i] == '"':
            return i + 1
        i += 1
    return i


def _match_paren(text, open_idx):
    """text[open_idx] is '('; return the index just past the matching ')'.

    Counts nesting depth and skips over quoted strings so parens inside string
    literals don't throw off the balance. Returns -1 if unbalanced.
    """
    depth = 0
    i = open_idx
    while i < len(text):
        c = text[i]
        if c == '"':
            i = _skip_string(text, i)
            continue
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return -1


def _find_property(block, name):
    """Return (start, end) of the `(property "name" ...)` s-expr in block, or None.

    Uses balanced-paren matching, so it is robust to however many nested parens
    the effects/font sub-expressions contain.
    """
    search = 0
    while True:
        p = block.find('(property', search)
        if p == -1:
            return None
        end = _match_paren(block, p)
        if end == -1:
            return None
        m = re.match(r'\(property\s+"([^"]*)"', block[p:end])
        if m and m.group(1) == name:
            return (p, end)
        search = end


def _esc(s):
    """Escape a string for embedding in a KiCad s-expr string literal."""
    return s.replace('\\', '\\\\').replace('"', '\\"')


def _make_desc_prop(desc, idnum):
    """Build a ki_description property block matching the file's 4/6-space style."""
    id_line = f'      (id {idnum})\n' if idnum is not None else ''
    return (
        f'    (property\n'
        f'      "ki_description"\n'
        f'      "{_esc(desc)}"\n'
        f'{id_line}'
        f'      (at 0 0 0)\n'
        f'      (effects (font (size 1.27 1.27) ) hide)\n'
        f'    )'
    )


def patch_descriptions():
    """Patch or insert ki_description fields in the .kicad_sym file."""
    sym_path = Path(SYM_FILE)
    if not sym_path.exists():
        print(f"ERROR: {SYM_FILE} not found. Run fetch first.")
        sys.exit(1)

    content = sym_path.read_text(encoding="utf-8")

    patched = 0
    for lcsc_id, desc in PARTS:
        lcsc_str = f'"{lcsc_id}"'
        idx = content.find(lcsc_str)
        if idx == -1:
            continue

        # Bound this symbol's block: from its `(symbol "` header to the next one.
        sym_start = content.rfind('\n  (symbol "', 0, idx)
        if sym_start == -1:
            continue
        next_sym = content.find('\n  (symbol "', idx)
        if next_sym == -1:
            next_sym = content.rfind('\n)')  # end of library
        block = content[sym_start:next_sym]

        existing = _find_property(block, "ki_description")
        if existing:
            # Replace in place, preserving the existing id if present.
            start, end = existing
            id_match = re.search(r'\(id (\d+)\)', block[start:end])
            idnum = int(id_match.group(1)) if id_match else None
            new_block = block[:start] + _make_desc_prop(desc, idnum).lstrip() + block[end:]
        else:
            value = _find_property(block, "Value")
            if not value:
                continue
            # Give the new property an id one past the highest already used.
            ids = [int(n) for n in re.findall(r'\(id (\d+)\)', block)]
            idnum = (max(ids) + 1) if ids else None
            insert_pos = value[1]
            new_block = block[:insert_pos] + '\n' + _make_desc_prop(desc, idnum) + block[insert_pos:]

        if new_block != block:
            content = content[:sym_start] + new_block + content[next_sym:]
            patched += 1

    sym_path.write_text(content, encoding="utf-8")
    print(f"Patched descriptions for {patched}/{len(PARTS)} symbols.")


if __name__ == "__main__":
    fetch_all()
    print()
    patch_descriptions()
    print(f"\nDone. Library at: {SYM_FILE}")
    print("Add to KiCad: Preferences → Manage Symbol/Footprint Libraries")
