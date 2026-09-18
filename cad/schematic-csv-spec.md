# Schematic input format: `<board>-bom.csv` + `<board>-nets.csv`

`generate_schematic.py` builds a complete, ERC-checked KiCad schematic from two CSVs — the **source of
truth** for the design. `bom.csv` says *what parts exist and where they go*; `nets.csv` says *what connects
to what*. They are joined on **`refdes`**.

```
generate_schematic.py --bom <board>-bom.csv --nets <board>-nets.csv --project <dir>
    PLACE stage (bom)  -> every place=yes part, grouped by subcircuit, one child sheet per subcircuit
    WIRE  stage (nets) -> net labels / power symbols / PWR_FLAGs, connectivity by net name
    VALIDATE gate      -> every pin on a net + ERC clean + every rendered net lands on its pin
```

Both files are hand-edited (spreadsheet-friendly, one row per part / per pin). Keep them a superset: the
tool reads the columns it needs and ignores the rest, which serve fab/PCB and human review.

---

## `bom.csv` — one row per physical part

| column | required | used by | meaning / allowed values |
|---|---|---|---|
| `refdes` | **yes** | tool (join key, symbol reference) | Schematic reference, e.g. `C1`, `U10`. **Unique.** The stable key `nets.csv` joins on. A blank placeable row is auto-assigned the next free number for its prefix and written back. |
| `subcircuit` | for grouping | tool (sheet grouping) | Which subcircuit the part belongs to, e.g. `soc`, `core-buck`, `microsd`. One child sheet is generated per distinct value (`gb3-<subcircuit>.kicad_sch`). If blank, falls back to `section`. |
| `section` | no | tool (fallback grouping) | Coarse functional area, e.g. `1. Core — SoC…`. Only used to group when `subcircuit` is blank. |
| `lcsc` | no | tool + fab | LCSC order code. Written as the placed symbol's `LCSC` field; also used by a fab/assembly BOM. |
| `fit` | no | fab | Assembly method: `JLC` \| `Hand`. Not used by this tool. |
| `symbol` | **yes** | tool (placement) | Library id `nick:name` to place, e.g. `easyeda2kicad:T113-S3_C5197687`. If blank or `place=no`, the row is skipped. Must resolve in the symbol libraries. |
| `footprint` | no | fab / PCB | Footprint id. **Not** used by this tool — the placed footprint comes from the symbol definition. Carried for the PCB/fab step. |
| `place` | **yes** | tool | `yes` \| `no` (also `true`/`1`). Whether the part is placed on the schematic. |
| `note` | no | human | Free-form. The single human-readable column — part description, role, provenance, etc. |

Required columns (the tool errors if missing): **`refdes`, `symbol`, `place`.**

---

## `nets.csv` — one row per pin

| column | required | used by | meaning / allowed values |
|---|---|---|---|
| `refdes` | **yes** | tool (join key) | The part this pin belongs to; must match a `bom.csv` `refdes`. |
| `pin` | **yes** | tool | Pin **number** (as in the symbol), e.g. `1`, `80`. Pins are matched by number, not name. |
| `net` | **yes** | tool (connectivity) | The net this pin connects to, e.g. `+3V3`, `SD_CLK`. Special values: **blank** = intentional no-connect (a `no_connect` marker is drawn); **`TODO_*`** = not yet assigned (not rendered, and **fails validation**). |
| `kind` | for power | tool (symbol vs label) | Net classification — **the tool never guesses from the net name.** `gnd`, `pwr`, or blank/`sig` (signal). Must be **consistent across all rows of the same net** (the tool errors otherwise). |
| `note` | no | human | Free-form. The single human-readable column — pin function/name, rationale, etc. |

### What `kind` does (WIRE stage)
| `kind` | rendering |
|---|---|
| `gnd` | a `GND` power symbol at each pin, **plus one `PWR_FLAG`** on the net (marks it driven for ERC) |
| `pwr` | a rail power symbol (`power:<net>`) at each pin, **plus one `PWR_FLAG`** |
| `sig` (or blank) | a **net label** — *global* if the net spans more than one subcircuit (to cross child sheets), else a *local* label |

`PWR_FLAG`s are placed automatically for every `gnd`/`pwr` net (one per net, on the first child sheet),
so ERC's "power input not driven" is satisfied by declared data, not by name heuristics.

---

## Relationship + validation

- **Join:** every `nets.csv` `refdes` should be a placed part in `bom.csv`, and every placed part's pins
  should appear in `nets.csv`.
- The tool's mandatory **VALIDATE** gate fails the build unless:
  1. every pin has a net (no `TODO_*`; a blank net is an accepted intentional no-connect);
  2. ERC has **0 error-severity violations and 0 unconnected pins** (warnings are tolerated);
  3. every rendered net actually lands on its intended pin (exported-netlist check).

## Notes on pin electrical types (why ERC is clean)
Pin electrical types (`passive`/`input`/`output`/`bidirectional`/`power_in`…) are a property of the
**part**, so they live in the **symbol library** (`cad/symbols/…`), *not* in these CSVs — fix once, reuse on
every board. Passives (R/C/L/D/SW) are `passive`; IC signal pins should be typed by function. (`kind` in
`nets.csv` is different: it's a per-*net*, per-*design* property, so it correctly lives with the netlist.)
