/*
 * eval.c — Simulation core: cell evaluation
 */
#include "eval.h"
#include <string.h>

void eval_combinational(struct sim_state *s)
{
    for (int i = 0; i < s->num_eval; i++) {
        int idx = s->eval_order[i];
        if (idx < 0) {
            /* Hard cell read: index = -(hard_cell_index + 1) */
            if (s->skip_hard_reads) continue;
            int h = (-idx) - 1;
            struct hard_cell *hc = &s->hard_cells[h];
            if (hc->type == HARD_SPRAM) {
                /* SPRAM has registered output (updates on clock edge) */
                write_port_bits(s, &hc->ports[SPRAM_DATAOUT], 16, hc->rdata_out);
            } else if (hc->type == HARD_BRAM) {
                /* BRAM has registered output — emit the value latched on
                 * the previous clock edge (stored in rdata_out). */
                write_port_bits(s, &hc->ports[BRAM_RDATA], 16, hc->rdata_out);
            }
        } else {
            struct cell *c = &s->cells[idx];
            if (c->type == CELL_LUT4) {
                int lut_idx = (get_net(s, c->inputs[3]) << 3) |
                              (get_net(s, c->inputs[2]) << 2) |
                              (get_net(s, c->inputs[1]) << 1) |
                               get_net(s, c->inputs[0]);
                s->nets[c->output] = (c->lut_init >> lut_idx) & 1;
            } else if (c->type == CELL_CARRY) {
                uint8_t i0 = get_net(s, c->inputs[0]);
                uint8_t i1 = get_net(s, c->inputs[1]);
                uint8_t ci = get_net(s, c->inputs[2]);
                s->nets[c->output] = (i0 & i1) | ((i0 ^ i1) & ci);
            }
        }
    }
}

void eval_clock_edge(struct sim_state *s)
{
    /* All registered elements sample their inputs simultaneously (pre-edge values).
     * Then all outputs update simultaneously. */

    /* Phase 1: Sample all DFF D inputs */
    static uint8_t new_q[MAX_CELLS];
    for (int i = 0; i < s->num_dffs; i++) {
        struct cell *c = &s->cells[s->dff_list[i]];
        int enabled = (c->enable < 0 || get_net(s, c->enable));
        if (!enabled) {
            new_q[i] = s->nets[c->output];  /* hold */
        } else if (c->reset >= 0 && get_net(s, c->reset)) {
            new_q[i] = 0;
        } else if (c->set >= 0 && get_net(s, c->set)) {
            new_q[i] = 1;
        } else {
            new_q[i] = get_net(s, c->inputs[0]);
        }
    }

    /* Phase 1b: Hard cell writes sample pre-edge nets */
    eval_hard_cells_write(s);

    /* Phase 2: Update all DFF Q outputs */
    for (int i = 0; i < s->num_dffs; i++) {
        struct cell *c = &s->cells[s->dff_list[i]];
        s->nets[c->output] = new_q[i];
    }
}

void eval_hard_cells_write(struct sim_state *s)
{
    for (int i = 0; i < s->num_hard_cells; i++) {
        struct hard_cell *hc = &s->hard_cells[i];
        if (hc->type == HARD_SPRAM) {
            uint16_t addr = (uint16_t)read_port_bits(s, &hc->ports[SPRAM_ADDRESS], 14);
            uint16_t din  = (uint16_t)read_port_bits(s, &hc->ports[SPRAM_DATAIN], 16);
            uint8_t  mask = (uint8_t)read_port_bits(s, &hc->ports[SPRAM_MASKWREN], 4);
            uint8_t  wren = get_net(s, hc->ports[SPRAM_WREN]);
            uint8_t  cs   = get_net(s, hc->ports[SPRAM_CHIPSELECT]);
            uint16_t *mem = (uint16_t *)hc->state;
            if (cs && wren) {
                if (mask & 1) mem[addr] = (mem[addr] & 0xFFF0) | (din & 0x000F);
                if (mask & 2) mem[addr] = (mem[addr] & 0xFF0F) | (din & 0x00F0);
                if (mask & 4) mem[addr] = (mem[addr] & 0xF0FF) | (din & 0x0F00);
                if (mask & 8) mem[addr] = (mem[addr] & 0x0FFF) | (din & 0xF000);
            }
            /* Registered read: latch DATAOUT on clock edge */
            if (cs)
                hc->rdata_out = mem[addr];
        } else if (hc->type == HARD_BRAM) {
            uint8_t  we    = get_net(s, hc->ports[BRAM_WE]);
            uint8_t  wclke = get_net(s, hc->ports[BRAM_WCLKE]);
            uint16_t *mem  = (uint16_t *)hc->state;
            if (we && wclke) {
                uint16_t waddr = (uint16_t)read_port_bits(s, &hc->ports[BRAM_WADDR], 11);
                uint16_t wdata = (uint16_t)read_port_bits(s, &hc->ports[BRAM_WDATA], 16);
                if (waddr < 256) mem[waddr] = wdata;
            }
            /* Registered read: latch RDATA on clock edge (available next tick) */
            uint8_t re = get_net(s, hc->ports[BRAM_RE]);
            uint8_t rclke = get_net(s, hc->ports[BRAM_RCLKE]);
            if (re && rclke) {
                uint16_t raddr = (uint16_t)read_port_bits(s, &hc->ports[BRAM_RADDR], 11);
                hc->rdata_out = (raddr < 256) ? mem[raddr] : 0;
            }
        }
    }
}

void eval_hard_cells_read(struct sim_state *s)
{
    for (int i = 0; i < s->num_hard_cells; i++) {
        struct hard_cell *hc = &s->hard_cells[i];
        if (hc->type == HARD_SPRAM) {
            uint16_t addr = (uint16_t)read_port_bits(s, &hc->ports[SPRAM_ADDRESS], 14);
            uint8_t  cs   = get_net(s, hc->ports[SPRAM_CHIPSELECT]);
            uint16_t *mem = (uint16_t *)hc->state;
            if (cs)
                write_port_bits(s, &hc->ports[SPRAM_DATAOUT], 16, mem[addr]);
        } else if (hc->type == HARD_BRAM) {
            uint8_t  re    = get_net(s, hc->ports[BRAM_RE]);
            uint8_t  rclke = get_net(s, hc->ports[BRAM_RCLKE]);
            uint16_t *mem  = (uint16_t *)hc->state;
            if (re && rclke) {
                uint16_t raddr = (uint16_t)read_port_bits(s, &hc->ports[BRAM_RADDR], 11);
                uint16_t val = (raddr < 256) ? mem[raddr] : 0;
                write_port_bits(s, &hc->ports[BRAM_RDATA], 16, val);
            }
        }
    }
}
