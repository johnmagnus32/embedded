#!/usr/bin/env python3
"""
fetch_symbols.py — Download KiCad symbols + footprints from JLCPCB/LCSC
                   and patch in human-readable descriptions.

Prerequisites:
    pip install easyeda2kicad

Usage:
    cd /Users/johmagnu/Desktop/embedded/cad
    source .venv/bin/activate
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
    ("C165948",  "USB Type-C Receptacle, Power Only, 16-pin"),
    ("C358687",  "Pin Header 1x5, 2.54mm, Through-Hole"),
    ("C49257",   "Pin Header 1x3, 2.54mm, Through-Hole"),
    ("C318884",  "Tactile Switch 6x6mm, SMD"),
    ("C2986027", "Green LED, 0603"),
    ("C2905435", "Pin Header 1x4, 2.54mm, Through-Hole"),
    ("C37208",   "Pin Header 1x6, 2.54mm, Through-Hole"),
    ("C2932700", "Pin Header 1x7, 2.54mm, Through-Hole"),
    ("C50981",   "Pin Header 1x20, 2.54mm, Through-Hole"),
]


def fetch_all():
    """Download symbols and footprints from LCSC."""
    Path(OUTPUT).mkdir(parents=True, exist_ok=True)
    print("Fetching symbols and footprints from JLCPCB/LCSC...\n")

    for i, (lcsc_id, desc) in enumerate(PARTS, 1):
        print(f"  [{i:2d}/{len(PARTS)}] {lcsc_id} — {desc}")
        result = subprocess.run(
            ["easyeda2kicad", f"--lcsc_id={lcsc_id}",
             "--symbol", "--footprint", "--overwrite", "--output", OUTPUT],
            capture_output=True, text=True
        )
        if result.returncode != 0:
            print(f"         WARN: {result.stderr.strip()}")

    print(f"\nFetched {len(PARTS)} parts.")


def patch_descriptions():
    """Patch or insert ki_description fields in the .kicad_sym file."""
    sym_path = Path(SYM_FILE)
    if not sym_path.exists():
        print(f"ERROR: {SYM_FILE} not found. Run fetch first.")
        sys.exit(1)

    content = sym_path.read_text()

    # Build map of LCSC ID → description
    lcsc_to_desc = {lcsc_id: desc for lcsc_id, desc in PARTS}

    patched = 0
    for lcsc_id, desc in PARTS:
        if lcsc_id not in content:
            continue

        # Find the LCSC property line with this ID
        lcsc_str = f'"{lcsc_id}"'
        idx = content.find(lcsc_str)
        if idx == -1:
            continue

        # Walk backwards to find the start of this symbol block
        sym_start = content.rfind('\n  (symbol "', 0, idx)
        if sym_start == -1:
            continue

        # Find the end of this symbol (next top-level symbol or end of file)
        next_sym = content.find('\n  (symbol "', idx)
        if next_sym == -1:
            next_sym = content.rfind('\n)')  # end of library
        
        block = content[sym_start:next_sym]

        # Check if ki_description already exists in this block
        ki_desc_pattern = r'(\(property\s*\n\s*"ki_description"\s*\n\s*)"[^"]*"'
        match = re.search(ki_desc_pattern, block)

        if match:
            # Update existing
            new_block = re.sub(ki_desc_pattern, f'\\1"{desc}"', block, count=1)
        else:
            # Insert after the "Value" property block
            # Find the end of the Value property (after its closing parenthesis)
            value_match = re.search(
                r'(\(property\s*\n\s*"Value"\s*\n\s*"[^"]*"\s*\n\s*\(id 1\)\s*\n\s*\(at [^)]+\)\s*\n\s*\(effects[^)]*\)[^)]*\)\s*\n\s*\))',
                block
            )
            if value_match:
                insert_pos = value_match.end()
                desc_block = (
                    f'\n    (property\n'
                    f'      "ki_description"\n'
                    f'      "{desc}"\n'
                    f'      (id 9)\n'
                    f'      (at 0 0 0)\n'
                    f'      (effects (font (size 1.27 1.27) ) hide)\n'
                    f'    )'
                )
                new_block = block[:insert_pos] + desc_block + block[insert_pos:]
            else:
                continue

        if new_block != block:
            content = content[:sym_start] + new_block + content[next_sym:]
            patched += 1

    sym_path.write_text(content)
    print(f"Patched descriptions for {patched}/{len(PARTS)} symbols.")


if __name__ == "__main__":
    fetch_all()
    print()
    patch_descriptions()
    print(f"\nDone. Library at: {SYM_FILE}")
    print("Add to KiCad: Preferences → Manage Symbol/Footprint Libraries")
