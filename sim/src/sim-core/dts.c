/*
 * dts.c — Minimal device tree source (.dts) parser
 *
 * Extracts compatible strings, reg addresses, and labels from nodes.
 * Not a full DTS parser — just enough for our board files.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dts.h"

static void skip_ws(const char **p)
{
    while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') (*p)++;
    /* Skip C-style comments */
    while (**p == '/' && *(*p+1) == '*') {
        *p += 2;
        while (**p && !(**p == '*' && *(*p+1) == '/')) (*p)++;
        if (**p) *p += 2;
        while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') (*p)++;
    }
}

static int parse_string(const char **p, char *out, int maxlen)
{
    skip_ws(p);
    if (**p != '"') return -1;
    (*p)++;
    int i = 0;
    while (**p && **p != '"' && i < maxlen - 1)
        out[i++] = *(*p)++;
    out[i] = '\0';
    if (**p == '"') (*p)++;
    return 0;
}

static uint32_t parse_angle_int(const char **p)
{
    skip_ws(p);
    if (**p != '<') return 0;
    (*p)++;
    skip_ws(p);
    uint32_t val = (uint32_t)strtoul(*p, (char**)p, 0);
    skip_ws(p);
    if (**p == '>') (*p)++;
    return val;
}

static void parse_node(const char **p, struct dts *d, int depth)
{
    skip_ws(p);

    /* Node name: "label: name@addr {" or "name {" */
    char name[64] = {0};
    char label[32] = {0};
    int ni = 0;
    while (**p && **p != '{' && **p != ';' && ni < 63) {
        name[ni++] = *(*p)++;
    }
    name[ni] = '\0';

    /* Trim trailing whitespace */
    while (ni > 0 && (name[ni-1] == ' ' || name[ni-1] == '\t')) name[--ni] = '\0';

    /* Extract label if present: "label: rest" */
    char *colon = strchr(name, ':');
    if (colon) {
        *colon = '\0';
        strncpy(label, name, 31);
        /* rest is the node name */
        const char *rest = colon + 1;
        while (*rest == ' ') rest++;
        memmove(name, rest, strlen(rest) + 1);
    }

    if (**p != '{') return;
    (*p)++;  /* skip '{' */

    /* Create a node entry */
    int node_idx = -1;
    if (d->nnodes < DTS_MAX_NODES) {
        node_idx = d->nnodes++;
        memset(&d->nodes[node_idx], 0, sizeof(struct dts_node));
        d->nodes[node_idx].dc_pin = -1;
        d->nodes[node_idx].cs_pin = -1;
        strncpy(d->nodes[node_idx].label, label, 31);
    }

    /* Parse properties and child nodes */
    while (**p) {
        skip_ws(p);
        if (**p == '}') { (*p)++; skip_ws(p); if (**p == ';') (*p)++; return; }

        /* Check if this is a child node (has '{' before ';') */
        const char *lookahead = *p;
        int is_child = 0;
        while (*lookahead && *lookahead != ';' && *lookahead != '{') lookahead++;
        if (*lookahead == '{') is_child = 1;

        if (is_child) {
            parse_node(p, d, depth + 1);
            continue;
        }

        /* Property: "name = value;" */
        char prop[64] = {0};
        int pi = 0;
        while (**p && **p != '=' && **p != ';' && pi < 63)
            prop[pi++] = *(*p)++;
        prop[pi] = '\0';
        while (pi > 0 && prop[pi-1] == ' ') prop[--pi] = '\0';

        if (**p == '=') {
            (*p)++;
            skip_ws(p);

            if (node_idx >= 0 && strcmp(prop, "compatible") == 0) {
                parse_string(p, d->nodes[node_idx].compatible, 64);
            } else if (node_idx >= 0 && strcmp(prop, "reg") == 0) {
                d->nodes[node_idx].reg = parse_angle_int(p);
                d->nodes[node_idx].has_reg = 1;
            } else if (strcmp(prop, "clock-frequency") == 0) {
                d->sysclk_hz = parse_angle_int(p);
            } else if (node_idx >= 0 && strcmp(prop, "dc-pin") == 0) {
                d->nodes[node_idx].dc_pin = (int)parse_angle_int(p);
            } else if (node_idx >= 0 && strcmp(prop, "cs-pin") == 0) {
                d->nodes[node_idx].cs_pin = (int)parse_angle_int(p);
            } else if (node_idx >= 0 && strcmp(prop, "spi-bus") == 0) {
                d->nodes[node_idx].spi_bus = parse_angle_int(p);
            } else {
                /* Skip value */
                while (**p && **p != ';') (*p)++;
            }
        }

        if (**p == ';') (*p)++;
    }
}

int dts_parse(struct dts *d, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    memset(d, 0, sizeof(*d));

    const char *p = buf;
    skip_ws(&p);

    /* Skip root "/" */
    if (*p == '/') {
        p++;
        skip_ws(&p);
        if (*p == '{') {
            p++;
            while (*p) {
                skip_ws(&p);
                if (*p == '}') break;

                /* Check for child node */
                const char *la = p;
                int is_child = 0;
                while (*la && *la != ';' && *la != '{') la++;
                if (*la == '{') is_child = 1;

                if (is_child) {
                    parse_node(&p, d, 0);
                } else {
                    /* Root property — skip */
                    while (*p && *p != ';') p++;
                    if (*p == ';') p++;
                }
            }
        }
    }

    free(buf);
    return 0;
}

const struct dts_node *dts_find(const struct dts *d, const char *compatible)
{
    for (int i = 0; i < d->nnodes; i++)
        if (strcmp(d->nodes[i].compatible, compatible) == 0)
            return &d->nodes[i];
    return NULL;
}
