#ifndef EVAL_H
#define EVAL_H

#include "netlist.h"

/* Evaluate all combinational cells (single pass, topo-sorted order) */
void eval_combinational(struct sim_state *s);

/* Capture all DFF D inputs → Q outputs (rising clock edge) */
void eval_clock_edge(struct sim_state *s);

/* Read a net value, handling constants */
static inline uint8_t get_net(struct sim_state *s, int id)
{
    if (id == NET_CONST_0) return 0;
    if (id == NET_CONST_1) return 1;
    return s->nets[id];
}

#endif
