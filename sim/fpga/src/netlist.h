#ifndef NETLIST_H
#define NETLIST_H

#include <stdint.h>

#define MAX_NETS    4096
#define MAX_CELLS   1024
#define MAX_PORTS   32

enum cell_type { CELL_LUT4, CELL_DFF, CELL_CARRY };

#define NET_CONST_0  (-1)
#define NET_CONST_1  (-2)

struct cell {
    enum cell_type type;
    uint16_t lut_init;      /* LUT4: 16-bit truth table */
    int inputs[4];          /* net IDs (NET_CONST_0/1 for constants) */
    int output;             /* net ID this cell drives */
    int clock;              /* DFF: clock net ID */
};

struct port_info {
    char name[64];
    int width;
    int bits[32];           /* net ID per bit */
};

struct sim_state {
    uint8_t nets[MAX_NETS];
    uint8_t nets_prev[MAX_NETS];
    int num_nets;

    struct cell cells[MAX_CELLS];
    int num_cells;

    int eval_order[MAX_CELLS];  /* topo-sorted combinational cells */
    int num_eval;
    int dff_list[MAX_CELLS];
    int num_dffs;

    struct port_info ports[MAX_PORTS];
    int num_ports;

    int clk_net;
};

/* Load a netlist from Yosys JSON. module_name=NULL for auto-detect. */
struct sim_state *netlist_load(const char *json_path, const char *module_name);
void netlist_free(struct sim_state *s);

#endif
