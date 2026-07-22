Replace the hand-rolled JSON streaming parser in the iCE40UP5K netlist loader with cJSON. The current parser has sync bugs when encountering multi-bit port arrays with mixed integer/string elements, causing segfaults on complex netlists like `ppu_top`. Work in `/home/johmagnu/learning/embedded/sim/src/devices/ice40up5k`.

## Problem

The existing `netlist.c` has a hand-rolled streaming JSON parser (~80 lines) that walks the Yosys JSON byte-by-byte. It breaks when:
- Port connection arrays contain quoted string constants (`"0"`, `"1"`) mixed with integers
- Multi-bit ports have more elements than expected
- The skip logic (`jskip_value`) gets out of sync inside nested structures

This causes segfaults when loading the PPU netlist (which has `SB_RAM40_4K` cells with 11-bit and 16-bit port arrays).

## Solution

Replace the hand-rolled parser with [cJSON](https://github.com/DaveGamble/cJSON) — a single-file, MIT-licensed C JSON library. Load the entire file into a DOM tree, then walk it with typed accessors. No streaming, no position tracking, no sync bugs.

## Changes

```
sim/src/devices/ice40up5k/
├── cjson.c         ← NEW: cJSON library (single file, vendored)
├── cjson.h         ← NEW: cJSON header
├── netlist.c       ← REWRITE: replace streaming parser with cJSON tree walking
├── netlist.h       ← unchanged
├── eval.c/h        ← unchanged
├── hard_cells.c/h  ← unchanged
└── ice40up5k.c/h   ← unchanged
```

## New netlist.c structure

```c
#include "cjson.h"
#include "netlist.h"
#include "hard_cells.h"

static int parse_net_ref(cJSON *item) {
    if (cJSON_IsNumber(item)) return item->valueint;
    if (cJSON_IsString(item))
        return (item->valuestring[0] == '1') ? NET_CONST_1 : NET_CONST_0;
    return NET_CONST_0;
}

static void parse_port_array(cJSON *arr, int *out, int max, struct sim_state *s) {
    int n = cJSON_GetArraySize(arr);
    for (int i = 0; i < n && i < max; i++) {
        out[i] = parse_net_ref(cJSON_GetArrayItem(arr, i));
        track_net(s, out[i]);
    }
}

static void parse_module(cJSON *mod_json, struct sim_state *s) {
    /* Ports */
    cJSON *ports = cJSON_GetObjectItem(mod_json, "ports");
    cJSON *port_obj;
    cJSON_ArrayForEach(port_obj, ports) {
        cJSON *bits = cJSON_GetObjectItem(port_obj, "bits");
        cJSON *dir = cJSON_GetObjectItem(port_obj, "direction");
        struct port_info *p = &s->ports[s->num_ports++];
        strncpy(p->name, port_obj->string, sizeof(p->name));
        p->width = cJSON_GetArraySize(bits);
        p->direction = (dir && strcmp(dir->valuestring, "output") == 0) ? 1 : 0;
        for (int i = 0; i < p->width; i++) {
            p->bits[i] = parse_net_ref(cJSON_GetArrayItem(bits, i));
            track_net(s, p->bits[i]);
        }
    }

    /* Cells */
    cJSON *cells = cJSON_GetObjectItem(mod_json, "cells");
    cJSON *cell_obj;
    cJSON_ArrayForEach(cell_obj, cells) {
        const char *type = cJSON_GetObjectItem(cell_obj, "type")->valuestring;
        cJSON *conns = cJSON_GetObjectItem(cell_obj, "connections");
        cJSON *params = cJSON_GetObjectItem(cell_obj, "parameters");

        if (strcmp(type, "SB_LUT4") == 0) {
            struct cell c = { .type = CELL_LUT4, ... };
            // read I0-I3, O from conns
            // read LUT_INIT from params
            s->cells[s->num_cells++] = c;

        } else if (strncmp(type, "SB_DFF", 6) == 0) {
            struct cell c = { .type = CELL_DFF, ... };
            // read C, D, Q from conns
            s->cells[s->num_cells++] = c;

        } else if (strcmp(type, "SB_CARRY") == 0) {
            struct cell c = { .type = CELL_CARRY, ... };
            // read I0, I1, CI, CO from conns
            s->cells[s->num_cells++] = c;

        } else if (strcmp(type, "SB_HFOSC") == 0) {
            cJSON *clkhf = cJSON_GetObjectItem(conns, "CLKHF");
            s->hfosc_clk_net = parse_net_ref(cJSON_GetArrayItem(clkhf, 0));

        } else if (strcmp(type, "SB_SPRAM256KA") == 0) {
            struct hard_cell hc = { .type = HARD_SPRAM };
            parse_port_array(cJSON_GetObjectItem(conns, "ADDRESS"), &hc.ports[SPRAM_ADDRESS], 14, s);
            parse_port_array(cJSON_GetObjectItem(conns, "DATAIN"), &hc.ports[SPRAM_DATAIN], 16, s);
            // ... etc
            hard_cell_init_state(&hc);
            s->hard_cells[s->num_hard_cells++] = hc;

        } else if (strcmp(type, "SB_RAM40_4K") == 0) {
            // TODO: implement BRAM eval
            // For now: just track nets so num_nets is correct
            cJSON *conn;
            cJSON_ArrayForEach(conn, conns) {
                cJSON *bit;
                cJSON_ArrayForEach(bit, conn) track_net(s, parse_net_ref(bit));
            }
        }
        // Unknown cell types are silently skipped — no crash
    }
}

struct sim_state *netlist_load(const char *json_path, const char *module_name) {
    // Read file into buffer
    // cJSON_Parse(buffer)
    // Find target module (auto-detect or by name)
    // parse_module(mod_json, s)
    // build_eval_order(s)
    // Clock detection (port "clk" or hfosc_clk_net)
    // cJSON_Delete(root)
    // return s
}
```

## Why this fixes the bug

- cJSON handles all JSON edge cases (escaped strings, nested arrays, mixed types)
- No manual position tracking — can't get out of sync
- Unknown cell types are simply skipped (no partial parsing that corrupts state)
- Multi-bit port arrays are read with `cJSON_GetArraySize` + `cJSON_GetArrayItem` — works for any width
- String net references (`"0"`, `"1"`) handled uniformly by `parse_net_ref`

## Getting cJSON

```bash
curl -o sim/src/devices/ice40up5k/cjson.c https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
curl -o sim/src/devices/ice40up5k/cjson.h https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
```

Single `.c` + `.h`, ~1800 lines, MIT license, no dependencies. Compiles with any C89+ compiler.

## Build changes

Add `cjson.c` to the Makefile SRCS:

```makefile
src/devices/ice40up5k/cjson.c \
```

## Testing

| Test | Expected |
|------|----------|
| Load `spi_slave` netlist (simple, no hard IP) | Loads, 23 cells, 0 hard cells |
| Load `ppu_top` netlist (BRAM + HFOSC) | Loads without crash, reports cells/nets/ports |
| Load `spi_led_top` netlist (HFOSC) | Loads, clk_net set from HFOSC |
| Run sim with PPU netlist | No segfault, firmware boots |
| Existing `make test` | 40 tests still pass |

## Verification checklist

- [ ] `netlist.c` has no hand-rolled JSON parsing code
- [ ] cJSON vendored as `cjson.c` / `cjson.h` (not a system dependency)
- [ ] PPU netlist loads without crash
- [ ] `spi_slave` netlist still loads correctly (regression)
- [ ] HFOSC clock detection works
- [ ] SPRAM ports parsed correctly (14-bit address, 16-bit data)
- [ ] SB_RAM40_4K cells don't crash (skipped gracefully for now)
- [ ] `make` builds without warnings in netlist.c
- [ ] `make test` passes (40 tests)
