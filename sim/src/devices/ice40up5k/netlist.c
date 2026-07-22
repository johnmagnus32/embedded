/*
 * netlist.c — Parse Yosys JSON netlist using cJSON
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netlist.h"

#include "cJSON.h"

static void hard_cell_init_state(struct hard_cell *hc)
{
    if (hc->type == HARD_SPRAM)
        hc->state = calloc(16384, sizeof(uint16_t));
    else if (hc->type == HARD_BRAM)
        hc->state = calloc(256, sizeof(uint16_t));
}

static void hard_cell_free_state(struct hard_cell *hc)
{
    free(hc->state);
    hc->state = NULL;
}

static void track_net(struct sim_state *s, int id)
{
    if (id >= 0 && id >= s->num_nets) s->num_nets = id + 1;
}

static int parse_net_ref(cJSON *item)
{
    if (cJSON_IsNumber(item)) return item->valueint;
    if (cJSON_IsString(item))
        return (item->valuestring[0] == '1') ? NET_CONST_1 : NET_CONST_0;
    return NET_CONST_0;
}

static int get_first_net(cJSON *conns, const char *port_name)
{
    cJSON *arr = cJSON_GetObjectItem(conns, port_name);
    if (!arr || cJSON_GetArraySize(arr) == 0) return NET_CONST_0;
    return parse_net_ref(cJSON_GetArrayItem(arr, 0));
}

static void read_port_nets(cJSON *conns, const char *port_name, int *out, int max, struct sim_state *s)
{
    cJSON *arr = cJSON_GetObjectItem(conns, port_name);
    if (!arr) return;
    int n = cJSON_GetArraySize(arr);
    if (n > max) n = max;
    for (int i = 0; i < n; i++) {
        out[i] = parse_net_ref(cJSON_GetArrayItem(arr, i));
        track_net(s, out[i]);
    }
}

static uint16_t parse_lut_init(const char *str)
{
    uint16_t v = 0;
    int len = strlen(str);
    for (int i = 0; i < len && i < 16; i++)
        if (str[i] == '1') v |= (1 << (len - 1 - i));
    return v;
}

static int is_library_module(const char *name)
{
    if (name[0] == '$') return 1;
    if (strncmp(name, "SB_", 3) == 0) return 1;
    if (strncmp(name, "ICESTORM_", 9) == 0) return 1;
    return 0;
}

static void parse_module(cJSON *mod_json, struct sim_state *s)
{
    /* Ports */
    cJSON *ports = cJSON_GetObjectItem(mod_json, "ports");
    if (ports) {
        cJSON *port_obj = ports->child;
        while (port_obj) {
            cJSON *bits = cJSON_GetObjectItem(port_obj, "bits");
            cJSON *dir = cJSON_GetObjectItem(port_obj, "direction");
            if (bits && s->num_ports < MAX_PORTS) {
                struct port_info *p = &s->ports[s->num_ports++];
                strncpy(p->name, port_obj->string, sizeof(p->name) - 1);
                p->width = cJSON_GetArraySize(bits);
                if (p->width > 64) p->width = 64;
                p->direction = (dir && strcmp(dir->valuestring, "output") == 0) ? 1 : 0;
                for (int i = 0; i < p->width; i++) {
                    p->bits[i] = parse_net_ref(cJSON_GetArrayItem(bits, i));
                    track_net(s, p->bits[i]);
                }
            }
            port_obj = port_obj->next;
        }
    }

    /* Cells */
    cJSON *cells = cJSON_GetObjectItem(mod_json, "cells");
    if (!cells) return;

    cJSON *cell_obj = cells->child;
    while (cell_obj) {
        cJSON *type_item = cJSON_GetObjectItem(cell_obj, "type");
        cJSON *conns = cJSON_GetObjectItem(cell_obj, "connections");
        cJSON *params = cJSON_GetObjectItem(cell_obj, "parameters");
        const char *type = type_item ? type_item->valuestring : "";

        if (strcmp(type, "SB_LUT4") == 0) {
            struct cell c = { .type = CELL_LUT4,
                .inputs = {NET_CONST_0, NET_CONST_0, NET_CONST_0, NET_CONST_0},
                .output = -1, .clock = -1 };
            c.inputs[0] = get_first_net(conns, "I0");
            c.inputs[1] = get_first_net(conns, "I1");
            c.inputs[2] = get_first_net(conns, "I2");
            c.inputs[3] = get_first_net(conns, "I3");
            c.output = get_first_net(conns, "O");
            if (params) {
                cJSON *lut = cJSON_GetObjectItem(params, "LUT_INIT");
                if (lut && cJSON_IsString(lut))
                    c.lut_init = parse_lut_init(lut->valuestring);
            }
            track_net(s, c.output);
            for (int i = 0; i < 4; i++) track_net(s, c.inputs[i]);
            if (s->num_cells < MAX_CELLS) s->cells[s->num_cells++] = c;

        } else if (strncmp(type, "SB_DFF", 6) == 0) {
            struct cell c = { .type = CELL_DFF,
                .inputs = {NET_CONST_0, NET_CONST_0, NET_CONST_0, NET_CONST_0},
                .output = -1, .clock = -1, .enable = -1, .reset = -1, .set = -1, .init_val = 0 };
            /* Detect power-on value from suffix: SS or NSS → init 1 */
            if (strstr(type, "SS") != NULL)
                c.init_val = 1;
            c.inputs[0] = get_first_net(conns, "D");
            c.output = get_first_net(conns, "Q");
            c.clock = get_first_net(conns, "C");
            if (cJSON_GetObjectItem(conns, "E")) c.enable = get_first_net(conns, "E");
            if (cJSON_GetObjectItem(conns, "R")) c.reset = get_first_net(conns, "R");
            if (cJSON_GetObjectItem(conns, "S")) c.set = get_first_net(conns, "S");
            track_net(s, c.output);
            track_net(s, c.inputs[0]);
            track_net(s, c.clock);
            if (c.enable >= 0) track_net(s, c.enable);
            if (c.reset >= 0) track_net(s, c.reset);
            if (c.set >= 0) track_net(s, c.set);
            if (s->num_cells < MAX_CELLS) s->cells[s->num_cells++] = c;

        } else if (strcmp(type, "SB_CARRY") == 0) {
            struct cell c = { .type = CELL_CARRY,
                .inputs = {NET_CONST_0, NET_CONST_0, NET_CONST_0, NET_CONST_0},
                .output = -1, .clock = -1 };
            c.inputs[0] = get_first_net(conns, "I0");
            c.inputs[1] = get_first_net(conns, "I1");
            c.inputs[2] = get_first_net(conns, "CI");
            c.output = get_first_net(conns, "CO");
            track_net(s, c.output);
            for (int i = 0; i < 3; i++) track_net(s, c.inputs[i]);
            if (s->num_cells < MAX_CELLS) s->cells[s->num_cells++] = c;

        } else if (strcmp(type, "SB_HFOSC") == 0) {
            s->hfosc_clk_net = get_first_net(conns, "CLKHF");
            track_net(s, s->hfosc_clk_net);

        } else if (strcmp(type, "SB_SPRAM256KA") == 0) {
            struct hard_cell hc;
            memset(&hc, 0xFF, sizeof(hc));
            hc.type = HARD_SPRAM;
            hc.state = NULL;
            read_port_nets(conns, "ADDRESS", &hc.ports[SPRAM_ADDRESS], 14, s);
            read_port_nets(conns, "DATAIN", &hc.ports[SPRAM_DATAIN], 16, s);
            read_port_nets(conns, "DATAOUT", &hc.ports[SPRAM_DATAOUT], 16, s);
            read_port_nets(conns, "MASKWREN", &hc.ports[SPRAM_MASKWREN], 4, s);
            hc.ports[SPRAM_WREN] = get_first_net(conns, "WREN");
            hc.ports[SPRAM_CHIPSELECT] = get_first_net(conns, "CHIPSELECT");
            track_net(s, hc.ports[SPRAM_WREN]);
            track_net(s, hc.ports[SPRAM_CHIPSELECT]);
            hard_cell_init_state(&hc);
            if (s->num_hard_cells < MAX_HARD_CELLS)
                s->hard_cells[s->num_hard_cells++] = hc;

        } else if (strcmp(type, "SB_RAM40_4K") == 0) {
            struct hard_cell hc;
            memset(&hc, 0xFF, sizeof(hc));
            hc.type = HARD_BRAM;
            hc.state = NULL;
            hc.read_mode = 0;
            hc.write_mode = 0;
            /* Parse READ_MODE/WRITE_MODE from parameters */
            if (params) {
                cJSON *rm = cJSON_GetObjectItem(params, "READ_MODE");
                cJSON *wm = cJSON_GetObjectItem(params, "WRITE_MODE");
                if (rm && cJSON_IsString(rm)) hc.read_mode = (uint8_t)strtol(rm->valuestring, NULL, 2);
                if (wm && cJSON_IsString(wm)) hc.write_mode = (uint8_t)strtol(wm->valuestring, NULL, 2);
            }
            /* Initialize all ports to NET_CONST_0 */
            for (int p = 0; p < HARD_MAX_PORTS; p++) hc.ports[p] = NET_CONST_0;
            read_port_nets(conns, "RADDR", &hc.ports[BRAM_RADDR], 11, s);
            read_port_nets(conns, "RDATA", &hc.ports[BRAM_RDATA], 16, s);
            read_port_nets(conns, "WADDR", &hc.ports[BRAM_WADDR], 11, s);
            read_port_nets(conns, "WDATA", &hc.ports[BRAM_WDATA], 16, s);
            read_port_nets(conns, "MASK", &hc.ports[BRAM_MASK], 16, s);
            hc.ports[BRAM_WE] = get_first_net(conns, "WE");
            hc.ports[BRAM_WCLKE] = get_first_net(conns, "WCLKE");
            hc.ports[BRAM_WCLK] = get_first_net(conns, "WCLK");
            hc.ports[BRAM_RE] = get_first_net(conns, "RE");
            hc.ports[BRAM_RCLKE] = get_first_net(conns, "RCLKE");
            hc.ports[BRAM_RCLK] = get_first_net(conns, "RCLK");
            track_net(s, hc.ports[BRAM_WE]);
            track_net(s, hc.ports[BRAM_WCLKE]);
            track_net(s, hc.ports[BRAM_WCLK]);
            track_net(s, hc.ports[BRAM_RE]);
            track_net(s, hc.ports[BRAM_RCLKE]);
            track_net(s, hc.ports[BRAM_RCLK]);
            hard_cell_init_state(&hc);
            if (s->num_hard_cells < MAX_HARD_CELLS)
                s->hard_cells[s->num_hard_cells++] = hc;
        }
        /* Unknown cell types silently skipped */

        cell_obj = cell_obj->next;
    }
}

/* --- Topological sort for combinational cells --- */

static void build_eval_order(struct sim_state *s)
{
    int net_driver[MAX_NETS];
    memset(net_driver, -1, sizeof(net_driver));

    int comb_cells[MAX_CELLS];
    int num_comb = 0;

    for (int i = 0; i < s->num_cells; i++) {
        if (s->cells[i].type == CELL_DFF) {
            s->dff_list[s->num_dffs++] = i;
        } else {
            comb_cells[num_comb++] = i;
            if (s->cells[i].output >= 0)
                net_driver[s->cells[i].output] = num_comb - 1;
        }
    }

    /* Add hard cell reads as virtual combinational nodes.
     * Their RDATA output nets are "driven" by them.
     * Convention: virtual node index = num_comb + hard_cell_index */
    for (int h = 0; h < s->num_hard_cells; h++) {
        struct hard_cell *hc = &s->hard_cells[h];
        int vnode = num_comb + h;
        if (hc->type == HARD_SPRAM) {
            for (int b = 0; b < 16; b++) {
                int net = hc->ports[SPRAM_DATAOUT + b];
                if (net >= 0) net_driver[net] = vnode;
            }
        } else if (hc->type == HARD_BRAM) {
            for (int b = 0; b < 16; b++) {
                int net = hc->ports[BRAM_RDATA + b];
                if (net >= 0) net_driver[net] = vnode;
            }
        }
    }

    int total_nodes = num_comb + s->num_hard_cells;
    int in_degree[MAX_CELLS + MAX_HARD_CELLS];
    memset(in_degree, 0, sizeof(in_degree));

    /* Compute in-degree for LUT/CARRY cells */
    for (int ci = 0; ci < num_comb; ci++) {
        struct cell *c = &s->cells[comb_cells[ci]];
        for (int p = 0; p < 4; p++) {
            int net = c->inputs[p];
            if (net >= 0 && net_driver[net] >= 0 && net_driver[net] != ci)
                in_degree[ci]++;
        }
    }

    /* Compute in-degree for hard cell read nodes (depend on RADDR nets) */
    for (int h = 0; h < s->num_hard_cells; h++) {
        int vnode = num_comb + h;
        struct hard_cell *hc = &s->hard_cells[h];
        int addr_start, addr_len;
        if (hc->type == HARD_SPRAM) { addr_start = SPRAM_ADDRESS; addr_len = 14; }
        else { addr_start = BRAM_RADDR; addr_len = 11; }
        for (int b = 0; b < addr_len; b++) {
            int net = hc->ports[addr_start + b];
            if (net >= 0 && net_driver[net] >= 0 && net_driver[net] != vnode)
                in_degree[vnode]++;
        }
    }

    /* Topological sort (BFS) */
    int queue[MAX_CELLS + MAX_HARD_CELLS];
    int qhead = 0, qtail = 0;

    for (int i = 0; i < total_nodes; i++)
        if (in_degree[i] == 0)
            queue[qtail++] = i;

    while (qhead < qtail) {
        int ni = queue[qhead++];

        if (ni < num_comb) {
            /* Regular combinational cell */
            s->eval_order[s->num_eval++] = comb_cells[ni];
        } else {
            /* Hard cell read — store as negative (-(hard_cell_index + 1)) */
            s->eval_order[s->num_eval++] = -(ni - num_comb + 1);
        }

        /* Find output nets for this node */
        int out_nets[16];
        int num_outs = 0;
        if (ni < num_comb) {
            int out = s->cells[comb_cells[ni]].output;
            if (out >= 0) { out_nets[0] = out; num_outs = 1; }
        } else {
            int h = ni - num_comb;
            struct hard_cell *hc = &s->hard_cells[h];
            int data_start = (hc->type == HARD_SPRAM) ? SPRAM_DATAOUT : BRAM_RDATA;
            for (int b = 0; b < 16; b++) {
                int net = hc->ports[data_start + b];
                if (net >= 0) out_nets[num_outs++] = net;
            }
        }

        /* Decrement in-degree of dependents */
        for (int oi = 0; oi < num_outs; oi++) {
            int out_net = out_nets[oi];
            for (int j = 0; j < total_nodes; j++) {
                if (j == ni) continue;
                int depends = 0;
                if (j < num_comb) {
                    struct cell *dep = &s->cells[comb_cells[j]];
                    for (int p = 0; p < 4; p++)
                        if (dep->inputs[p] == out_net) { depends = 1; break; }
                } else {
                    int h = j - num_comb;
                    struct hard_cell *hc = &s->hard_cells[h];
                    int addr_start = (hc->type == HARD_SPRAM) ? SPRAM_ADDRESS : BRAM_RADDR;
                    int addr_len = (hc->type == HARD_SPRAM) ? 14 : 11;
                    for (int b = 0; b < addr_len; b++)
                        if (hc->ports[addr_start + b] == out_net) { depends = 1; break; }
                }
                if (depends) {
                    in_degree[j]--;
                    if (in_degree[j] == 0)
                        queue[qtail++] = j;
                }
            }
        }
    }

    /* Fallback: add any remaining nodes (cycles) */
    for (int i = 0; i < total_nodes; i++) {
        if (in_degree[i] > 0) {
            if (i < num_comb)
                s->eval_order[s->num_eval++] = comb_cells[i];
            else
                s->eval_order[s->num_eval++] = -(i - num_comb + 1);
        }
    }
}

/* --- Public API --- */

struct sim_state *netlist_load(const char *json_path, const char *module_name)
{
    /* Read file */
    FILE *f = fopen(json_path, "r");
    if (!f) { fprintf(stderr, "[netlist] Cannot open %s\n", json_path); exit(1); }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = 0;
    fclose(f);

    /* Parse JSON */
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) { fprintf(stderr, "[netlist] JSON parse error\n"); exit(1); }

    struct sim_state *s = calloc(1, sizeof(*s));
    s->hfosc_clk_net = -1;

    /* Find target module */
    cJSON *modules = cJSON_GetObjectItem(root, "modules");
    if (!modules) { fprintf(stderr, "[netlist] No 'modules' key\n"); exit(1); }

    cJSON *target_mod = NULL;
    cJSON *mod = modules->child;
    while (mod) {
        if (module_name) {
            if (strcmp(mod->string, module_name) == 0) { target_mod = mod; break; }
        } else {
            if (!is_library_module(mod->string)) { target_mod = mod; break; }
        }
        mod = mod->next;
    }

    if (!target_mod) {
        fprintf(stderr, "[netlist] Module '%s' not found\n", module_name ? module_name : "(auto)");
        exit(1);
    }

    parse_module(target_mod, s);
    cJSON_Delete(root);

    /* Find clock net */
    s->clk_net = -1;
    for (int i = 0; i < s->num_ports; i++)
        if (strcmp(s->ports[i].name, "clk") == 0) { s->clk_net = s->ports[i].bits[0]; break; }

    /* Fallback: use HFOSC output as clock */
    if (s->clk_net < 0 && s->hfosc_clk_net >= 0)
        s->clk_net = s->hfosc_clk_net;

    /* Build evaluation order */
    build_eval_order(s);

    return s;
}

void netlist_free(struct sim_state *s)
{
    for (int i = 0; i < s->num_hard_cells; i++)
        hard_cell_free_state(&s->hard_cells[i]);
    free(s);
}
