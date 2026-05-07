/*
 * netlist.c — Parse Yosys JSON netlist and build simulation state
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netlist.h"

/* --- Minimal JSON streaming parser --- */

static char *jbuf;
static int jpos, jlen;

static void json_load(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "Cannot open %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); jlen = ftell(f); fseek(f, 0, SEEK_SET);
    jbuf = malloc(jlen + 1); fread(jbuf, 1, jlen, f); jbuf[jlen] = 0; fclose(f);
    jpos = 0;
}

static void jskip_ws(void)
{
    while (jpos < jlen && (jbuf[jpos]==' '||jbuf[jpos]=='\n'||jbuf[jpos]=='\r'||jbuf[jpos]=='\t'))
        jpos++;
}

static char jpeek(void) { jskip_ws(); return jbuf[jpos]; }
static char jnext(void) { jskip_ws(); return jbuf[jpos++]; }

static void jexpect(char c)
{
    char g = jnext();
    if (g != c) { fprintf(stderr, "JSON: expected '%c' got '%c' at pos %d\n", c, g, jpos-1); exit(1); }
}

static void jstr(char *out, int max)
{
    jexpect('"');
    int i = 0;
    while (jbuf[jpos] != '"' && i < max-1) {
        if (jbuf[jpos] == '\\') jpos++;
        out[i++] = jbuf[jpos++];
    }
    out[i] = 0; jpos++;
}

static int jint(void)
{
    jskip_ws();
    int v = 0, neg = 0;
    if (jbuf[jpos] == '-') { neg = 1; jpos++; }
    while (jbuf[jpos] >= '0' && jbuf[jpos] <= '9')
        v = v * 10 + (jbuf[jpos++] - '0');
    return neg ? -v : v;
}

static void jskip_value(void)
{
    jskip_ws();
    char c = jbuf[jpos];
    if (c == '"') {
        jpos++;
        while (jbuf[jpos] != '"') { if (jbuf[jpos] == '\\') jpos++; jpos++; }
        jpos++;
    } else if (c == '{') {
        int d = 1; jpos++;
        while (d) {
            if (jbuf[jpos] == '{') d++;
            else if (jbuf[jpos] == '}') d--;
            else if (jbuf[jpos] == '"') { jpos++; while (jbuf[jpos] != '"') { if (jbuf[jpos] == '\\') jpos++; jpos++; } }
            jpos++;
        }
    } else if (c == '[') {
        int d = 1; jpos++;
        while (d) {
            if (jbuf[jpos] == '[') d++;
            else if (jbuf[jpos] == ']') d--;
            else if (jbuf[jpos] == '"') { jpos++; while (jbuf[jpos] != '"') { if (jbuf[jpos] == '\\') jpos++; jpos++; } }
            jpos++;
        }
    } else {
        while (jbuf[jpos] != ',' && jbuf[jpos] != '}' && jbuf[jpos] != ']' && jpos < jlen)
            jpos++;
    }
}

static int parse_net_ref(void)
{
    jskip_ws();
    if (jbuf[jpos] == '"') {
        char s[8]; jstr(s, sizeof(s));
        return (s[0] == '1') ? NET_CONST_1 : NET_CONST_0;
    }
    return jint();
}

static uint16_t parse_lut_init(const char *s)
{
    uint16_t v = 0;
    int len = strlen(s);
    for (int i = 0; i < len && i < 16; i++)
        if (s[i] == '1') v |= (1 << (len - 1 - i));
    return v;
}

/* --- Module auto-detection --- */

static int is_library_module(const char *name)
{
    if (name[0] == '$') return 1;
    if (strncmp(name, "SB_", 3) == 0) return 1;
    if (strncmp(name, "ICESTORM_", 9) == 0) return 1;
    return 0;
}

/* --- Netlist parsing --- */

static void track_net(struct sim_state *s, int id)
{
    if (id >= 0 && id >= s->num_nets) s->num_nets = id + 1;
}

static void parse_module(struct sim_state *s)
{
    jexpect('{');
    while (jpeek() != '}') {
        char sec[64]; jstr(sec, sizeof(sec)); jexpect(':');

        if (strcmp(sec, "ports") == 0) {
            jexpect('{');
            while (jpeek() != '}') {
                char pname[64]; jstr(pname, sizeof(pname)); jexpect(':');
                jexpect('{');
                int bits[32]; int nbits = 0;
                while (jpeek() != '}') {
                    char k[32]; jstr(k, sizeof(k)); jexpect(':');
                    if (strcmp(k, "bits") == 0) {
                        jexpect('[');
                        while (jpeek() != ']') { bits[nbits++] = jint(); if (jpeek() == ',') jnext(); }
                        jexpect(']');
                    } else jskip_value();
                    if (jpeek() == ',') jnext();
                }
                jexpect('}');
                struct port_info *p = &s->ports[s->num_ports++];
                strncpy(p->name, pname, sizeof(p->name));
                p->width = nbits;
                for (int i = 0; i < nbits; i++) { p->bits[i] = bits[i]; track_net(s, bits[i]); }
                if (jpeek() == ',') jnext();
            }
            jexpect('}');
        } else if (strcmp(sec, "cells") == 0) {
            jexpect('{');
            while (jpeek() != '}') {
                char cname[128]; jstr(cname, sizeof(cname)); jexpect(':');
                jexpect('{');
                struct cell c = { .inputs = {NET_CONST_0, NET_CONST_0, NET_CONST_0, NET_CONST_0},
                                  .output = -1, .clock = -1 };
                int valid = 0;

                while (jpeek() != '}') {
                    char k[32]; jstr(k, sizeof(k)); jexpect(':');
                    if (strcmp(k, "type") == 0) {
                        char t[32]; jstr(t, sizeof(t));
                        if (strcmp(t, "SB_LUT4") == 0) { c.type = CELL_LUT4; valid = 1; }
                        else if (strncmp(t, "SB_DFF", 6) == 0) { c.type = CELL_DFF; valid = 1; }
                        else if (strcmp(t, "SB_CARRY") == 0) { c.type = CELL_CARRY; valid = 1; }
                    } else if (strcmp(k, "connections") == 0) {
                        jexpect('{');
                        while (jpeek() != '}') {
                            char port[16]; jstr(port, sizeof(port)); jexpect(':');
                            jexpect('[');
                            int net = parse_net_ref();
                            while (jpeek() != ']') { jnext(); if (jpeek() != ']') parse_net_ref(); }
                            jexpect(']');
                            if (c.type == CELL_LUT4) {
                                if (strcmp(port, "I0") == 0) c.inputs[0] = net;
                                else if (strcmp(port, "I1") == 0) c.inputs[1] = net;
                                else if (strcmp(port, "I2") == 0) c.inputs[2] = net;
                                else if (strcmp(port, "I3") == 0) c.inputs[3] = net;
                                else if (strcmp(port, "O") == 0) c.output = net;
                            } else if (c.type == CELL_DFF) {
                                if (strcmp(port, "C") == 0) c.clock = net;
                                else if (strcmp(port, "D") == 0) c.inputs[0] = net;
                                else if (strcmp(port, "Q") == 0) c.output = net;
                            } else if (c.type == CELL_CARRY) {
                                if (strcmp(port, "I0") == 0) c.inputs[0] = net;
                                else if (strcmp(port, "I1") == 0) c.inputs[1] = net;
                                else if (strcmp(port, "CI") == 0) c.inputs[2] = net;
                                else if (strcmp(port, "CO") == 0) c.output = net;
                            }
                            if (jpeek() == ',') jnext();
                        }
                        jexpect('}');
                    } else if (strcmp(k, "parameters") == 0) {
                        jexpect('{');
                        while (jpeek() != '}') {
                            char pk[32]; jstr(pk, sizeof(pk)); jexpect(':');
                            if (strcmp(pk, "LUT_INIT") == 0) { char v[32]; jstr(v, sizeof(v)); c.lut_init = parse_lut_init(v); }
                            else jskip_value();
                            if (jpeek() == ',') jnext();
                        }
                        jexpect('}');
                    } else jskip_value();
                    if (jpeek() == ',') jnext();
                }
                jexpect('}');
                if (valid) {
                    track_net(s, c.output);
                    for (int i = 0; i < 4; i++) track_net(s, c.inputs[i]);
                    track_net(s, c.clock);
                    s->cells[s->num_cells++] = c;
                }
                if (jpeek() == ',') jnext();
            }
            jexpect('}');
        } else jskip_value();
        if (jpeek() == ',') jnext();
    }
    jexpect('}');
}

/* --- Topological sort for combinational cells --- */

static void build_eval_order(struct sim_state *s)
{
    /* Map: net_id → index of the combinational cell that drives it (-1 if none) */
    int net_driver[MAX_NETS];
    memset(net_driver, -1, sizeof(net_driver));

    /* Separate DFFs from combinational, record drivers */
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

    /* Compute in-degree: how many combinational dependencies each cell has */
    int in_degree[MAX_CELLS];
    memset(in_degree, 0, sizeof(in_degree));

    for (int ci = 0; ci < num_comb; ci++) {
        struct cell *c = &s->cells[comb_cells[ci]];
        for (int p = 0; p < 4; p++) {
            int net = c->inputs[p];
            if (net >= 0 && net_driver[net] >= 0 && net_driver[net] != ci)
                in_degree[ci]++;
        }
    }

    /* Kahn's algorithm: repeatedly emit cells with in_degree == 0 */
    int queue[MAX_CELLS];
    int qhead = 0, qtail = 0;

    for (int ci = 0; ci < num_comb; ci++)
        if (in_degree[ci] == 0)
            queue[qtail++] = ci;

    while (qhead < qtail) {
        int ci = queue[qhead++];
        s->eval_order[s->num_eval++] = comb_cells[ci];

        /* Reduce in-degree of cells that depend on this cell's output */
        int out_net = s->cells[comb_cells[ci]].output;
        if (out_net < 0) continue;
        for (int j = 0; j < num_comb; j++) {
            if (j == ci) continue;
            struct cell *dep = &s->cells[comb_cells[j]];
            for (int p = 0; p < 4; p++) {
                if (dep->inputs[p] == out_net) {
                    in_degree[j]--;
                    if (in_degree[j] == 0)
                        queue[qtail++] = j;
                    break;
                }
            }
        }
    }

    /* Any remaining cells (cycles — shouldn't happen in valid netlists) */
    for (int ci = 0; ci < num_comb; ci++) {
        if (in_degree[ci] > 0) {
            s->eval_order[s->num_eval++] = comb_cells[ci];
        }
    }
}

/* --- Public API --- */

struct sim_state *netlist_load(const char *json_path, const char *module_name)
{
    struct sim_state *s = calloc(1, sizeof(*s));
    json_load(json_path);

    jexpect('{');
    while (jpeek() != '}') {
        char key[64]; jstr(key, sizeof(key)); jexpect(':');
        if (strcmp(key, "modules") != 0) { jskip_value(); if (jpeek() == ',') jnext(); continue; }

        jexpect('{');
        int found = 0;
        while (jpeek() != '}') {
            char mname[128]; jstr(mname, sizeof(mname)); jexpect(':');

            /* Auto-detect: pick first non-library module with content */
            int match = 0;
            if (module_name) {
                match = (strcmp(mname, module_name) == 0);
            } else {
                match = !is_library_module(mname);
            }

            if (!match) { jskip_value(); if (jpeek() == ',') jnext(); continue; }
            found = 1;
            parse_module(s);
            if (jpeek() == ',') jnext();

            /* If auto-detecting and we found cells, stop */
            if (!module_name && s->num_cells > 0) break;
        }
        if (!found) { fprintf(stderr, "[netlist] Module '%s' not found\n", module_name ? module_name : "(auto)"); exit(1); }

        /* Skip remaining modules */
        while (jpeek() != '}') {
            char dummy[128]; jstr(dummy, sizeof(dummy)); jexpect(':'); jskip_value();
            if (jpeek() == ',') jnext();
        }
        jexpect('}');
        if (jpeek() == ',') jnext();
    }
    jexpect('}');
    free(jbuf); jbuf = NULL;

    /* Find clock net */
    s->clk_net = -1;
    for (int i = 0; i < s->num_ports; i++)
        if (strcmp(s->ports[i].name, "clk") == 0) { s->clk_net = s->ports[i].bits[0]; break; }

    /* Build evaluation order */
    build_eval_order(s);

    return s;
}

void netlist_free(struct sim_state *s)
{
    free(s);
}
