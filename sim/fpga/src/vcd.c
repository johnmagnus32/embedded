/*
 * vcd.c — VCD (Value Change Dump) file writer for GTKWave
 */
#include <stdio.h>
#include <stdlib.h>
#include "vcd.h"

struct vcd_writer {
    FILE *fp;
    struct port_info *ports;
    int num_ports;
};

struct vcd_writer *vcd_open(const char *path, struct port_info *ports, int num_ports)
{
    struct vcd_writer *w = malloc(sizeof(*w));
    w->fp = fopen(path, "w");
    if (!w->fp) { fprintf(stderr, "Cannot open VCD file: %s\n", path); exit(1); }
    w->ports = ports;
    w->num_ports = num_ports;

    fprintf(w->fp, "$timescale 1ns $end\n");
    fprintf(w->fp, "$scope module top $end\n");
    for (int i = 0; i < num_ports; i++) {
        struct port_info *p = &ports[i];
        if (p->width == 1)
            fprintf(w->fp, "$var wire 1 p%d %s $end\n", i, p->name);
        else
            fprintf(w->fp, "$var wire %d p%d %s [%d:0] $end\n", p->width, i, p->name, p->width - 1);
    }
    fprintf(w->fp, "$upscope $end\n$enddefinitions $end\n");

    /* Initial values */
    fprintf(w->fp, "#0\n$dumpvars\n");
    for (int i = 0; i < num_ports; i++) {
        struct port_info *p = &ports[i];
        if (p->width == 1)
            fprintf(w->fp, "0p%d\n", i);
        else {
            fprintf(w->fp, "b");
            for (int b = p->width - 1; b >= 0; b--) fprintf(w->fp, "0");
            fprintf(w->fp, " p%d\n", i);
        }
    }
    fprintf(w->fp, "$end\n");
    return w;
}

static inline uint8_t read_net(uint8_t *nets, int id)
{
    if (id == NET_CONST_0) return 0;
    if (id == NET_CONST_1) return 1;
    return nets[id];
}

void vcd_sample(struct vcd_writer *w, uint64_t time_ns, uint8_t *nets, uint8_t *prev, int num_nets)
{
    (void)num_nets;
    int any_changed = 0;
    for (int i = 0; i < w->num_ports && !any_changed; i++) {
        struct port_info *p = &w->ports[i];
        for (int b = 0; b < p->width; b++)
            if (p->bits[b] >= 0 && nets[p->bits[b]] != prev[p->bits[b]]) { any_changed = 1; break; }
    }
    if (!any_changed) return;

    fprintf(w->fp, "#%lu\n", (unsigned long)time_ns);
    for (int i = 0; i < w->num_ports; i++) {
        struct port_info *p = &w->ports[i];
        int changed = 0;
        for (int b = 0; b < p->width; b++)
            if (p->bits[b] >= 0 && nets[p->bits[b]] != prev[p->bits[b]]) { changed = 1; break; }
        if (!changed) continue;

        if (p->width == 1)
            fprintf(w->fp, "%dp%d\n", read_net(nets, p->bits[0]), i);
        else {
            fprintf(w->fp, "b");
            for (int b = p->width - 1; b >= 0; b--)
                fprintf(w->fp, "%d", read_net(nets, p->bits[b]));
            fprintf(w->fp, " p%d\n", i);
        }
    }
}

void vcd_close(struct vcd_writer *w)
{
    if (w) {
        if (w->fp) fclose(w->fp);
        free(w);
    }
}
