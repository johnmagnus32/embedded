#ifndef EVAL_H
#define EVAL_H

#include "netlist.h"

/* Evaluate all combinational cells (single pass, topo-sorted order) */
void eval_combinational(struct sim_state *s);

/* Capture all DFF D inputs → Q outputs (rising clock edge) */
void eval_clock_edge(struct sim_state *s);

/* Evaluate hard IP cells — split into write (clocked) and read (combinational) */
void eval_hard_cells_write(struct sim_state *s);
void eval_hard_cells_read(struct sim_state *s);

/* Read a net value, handling constants */
static inline uint8_t get_net(struct sim_state *s, int id)
{
    if (id == NET_CONST_0) return 0;
    if (id == NET_CONST_1) return 1;
    return s->nets[id];
}

/* Read N bits from consecutive net IDs into an integer */
static inline uint32_t read_port_bits(struct sim_state *s, int *port_nets, int width)
{
    uint32_t val = 0;
    for (int i = 0; i < width; i++) {
        int net = port_nets[i];
        if (net == NET_CONST_1) val |= (1u << i);
        else if (net >= 0) val |= ((uint32_t)s->nets[net] << i);
    }
    return val;
}

/* Write N bits to consecutive net IDs */
static inline void write_port_bits(struct sim_state *s, int *port_nets, int width, uint32_t val)
{
    for (int i = 0; i < width; i++) {
        if (port_nets[i] >= 0)
            s->nets[port_nets[i]] = (val >> i) & 1;
    }
}

#endif
