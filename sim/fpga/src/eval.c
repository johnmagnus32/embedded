/*
 * eval.c — Simulation core: cell evaluation
 */
#include "eval.h"

void eval_combinational(struct sim_state *s)
{
    for (int i = 0; i < s->num_eval; i++) {
        struct cell *c = &s->cells[s->eval_order[i]];
        if (c->type == CELL_LUT4) {
            int idx = (get_net(s, c->inputs[3]) << 3) |
                      (get_net(s, c->inputs[2]) << 2) |
                      (get_net(s, c->inputs[1]) << 1) |
                       get_net(s, c->inputs[0]);
            s->nets[c->output] = (c->lut_init >> idx) & 1;
        } else if (c->type == CELL_CARRY) {
            uint8_t i0 = get_net(s, c->inputs[0]);
            uint8_t i1 = get_net(s, c->inputs[1]);
            uint8_t ci = get_net(s, c->inputs[2]);
            s->nets[c->output] = (i0 & i1) | ((i0 ^ i1) & ci);
        }
    }
}

void eval_clock_edge(struct sim_state *s)
{
    for (int i = 0; i < s->num_dffs; i++) {
        struct cell *c = &s->cells[s->dff_list[i]];
        s->nets[c->output] = get_net(s, c->inputs[0]);
    }
}
