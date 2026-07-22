/*
 * ice40up5k.c — Generic iCE40UP5K FPGA device
 *
 * Loads any netlist, exposes pins, ticks gate-level simulation.
 * Knows nothing about SPI, LCD, or any protocol.
 */
#include "ice40up5k.h"
#include "netlist.h"
#include "eval.h"
#include <string.h>
#include <stdio.h>

void ice40up5k_init(struct ice40up5k *dev, const char *netlist_path)
{
    memset(dev, 0, sizeof(*dev));

    dev->fpga = netlist_load(netlist_path, NULL);
    memset(dev->fpga->nets, 0, sizeof(dev->fpga->nets));

    /* Discover pins from netlist ports */
    struct sim_state *s = dev->fpga;
    for (int i = 0; i < s->num_ports; i++) {
        struct port_info *p = &s->ports[i];
        if (p->width == 1) {
            struct ice40_pin *pin = &dev->pins[dev->num_pins++];
            strncpy(pin->name, p->name, sizeof(pin->name) - 1);
            pin->net_id = p->bits[0];
            pin->direction = p->direction;
            pin->level = 0;
        } else {
            for (int b = 0; b < p->width && dev->num_pins < ICE40_MAX_PINS; b++) {
                struct ice40_pin *pin = &dev->pins[dev->num_pins++];
                snprintf(pin->name, sizeof(pin->name), "%s[%d]", p->name, b);
                pin->net_id = p->bits[b];
                pin->direction = p->direction;
                pin->level = 0;
            }
        }
    }

    /* Apply DFF power-on values (iCE40: 0 for SB_DFF/DFFE/DFFESR, 1 for SS types).
     * Yosys encodes initial values into the DFF type choice — DFFs that need to
     * start at 1 are mapped to SB_DFFESS/SB_DFFSS. Set their Q outputs here. */
    for (int i = 0; i < s->num_dffs; i++) {
        struct cell *c = &s->cells[s->dff_list[i]];
        if (c->init_val && c->output >= 0)
            s->nets[c->output] = 1;
    }

    /* Settle combinational logic from DFF initial values (simulates GSR release). */
    eval_combinational(s);

    fprintf(stderr, "[ice40up5k] Loaded: %d cells, %d hard cells, %d nets, %d pins\n",
            s->num_cells, s->num_hard_cells, s->num_nets, dev->num_pins);
}

void ice40up5k_tick(struct ice40up5k *dev)
{
    struct sim_state *s = dev->fpga;

    /* Step 1: Clock edge — all registered elements sample current nets
     * and update their outputs simultaneously. */
    eval_clock_edge(s);

    /* Step 2: Combinational settle — propagate new DFF/BRAM/SPRAM outputs
     * through all LUTs and carry chains. After this, nets are stable
     * until the next clock edge. */
    eval_combinational(s);

    /* Check output pins for changes — two passes so all levels are
     * current before any callback fires (callbacks may read other pins). */
    for (int i = 0; i < dev->num_pins; i++) {
        struct ice40_pin *p = &dev->pins[i];
        if (p->direction != 1) continue;
        p->level = (p->net_id >= 0) ? s->nets[p->net_id] : 0;
    }
    for (int i = 0; i < dev->num_pins; i++) {
        struct ice40_pin *p = &dev->pins[i];
        if (p->direction != 1) continue;
        uint8_t cur = (p->net_id >= 0) ? s->nets[p->net_id] : 0;
        if (cur != p->prev_level) {
            gpio_set(&p->out, cur);
            p->prev_level = cur;
        }
    }
}


void ice40up5k_set_pin(struct ice40up5k *dev, int pin_idx, uint8_t level)
{
    if (pin_idx < 0 || pin_idx >= dev->num_pins) return;
    struct ice40_pin *p = &dev->pins[pin_idx];
    if (p->net_id >= 0)
        dev->fpga->nets[p->net_id] = level;
}

int ice40up5k_find_pin(struct ice40up5k *dev, const char *name)
{
    for (int i = 0; i < dev->num_pins; i++)
        if (strcmp(dev->pins[i].name, name) == 0)
            return i;
    return -1;
}

void ice40up5k_free(struct ice40up5k *dev)
{
    if (dev->fpga)
        netlist_free(dev->fpga);
    dev->fpga = NULL;
}
