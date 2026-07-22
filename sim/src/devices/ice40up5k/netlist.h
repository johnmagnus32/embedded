#ifndef NETLIST_H
#define NETLIST_H

#include <stdint.h>

#define MAX_NETS    16384
#define MAX_CELLS   16384
#define MAX_PORTS   48
#define MAX_HARD_CELLS 16

enum cell_type { CELL_LUT4, CELL_DFF, CELL_CARRY };

#define NET_CONST_0  (-1)
#define NET_CONST_1  (-2)

struct cell {
    enum cell_type type;
    uint16_t lut_init;      /* LUT4: 16-bit truth table */
    int inputs[4];          /* net IDs (NET_CONST_0/1 for constants) */
    int output;             /* net ID this cell drives */
    int clock;              /* DFF: clock net ID */
    int enable;             /* DFF: enable net ID (-1 if none) */
    int reset;              /* DFF: sync reset net ID (-1 if none) */
    int set;                /* DFF: sync set net ID (-1 if none) */
    uint8_t init_val;       /* DFF: power-on value (0 or 1) */
};

/* Hard IP cells (SPRAM, BRAM) — multi-bit ports, internal state */
enum hard_type { HARD_SPRAM, HARD_BRAM };

#define HARD_MAX_PORTS 80

struct hard_cell {
    enum hard_type type;
    int ports[HARD_MAX_PORTS];  /* net IDs, flat array */
    void *state;                /* internal memory (malloc'd) */
    uint8_t read_mode;
    uint8_t write_mode;
    uint16_t rdata_out;         /* registered read output (updates on clock edge) */
};

/* SPRAM port offsets in hard_cell.ports[] */
#define SPRAM_ADDRESS   0   /* 14 nets: ports[0..13] */
#define SPRAM_DATAIN   14   /* 16 nets: ports[14..29] */
#define SPRAM_DATAOUT  30   /* 16 nets: ports[30..45] */
#define SPRAM_MASKWREN 46   /* 4 nets: ports[46..49] */
#define SPRAM_WREN     50   /* 1 net */
#define SPRAM_CHIPSELECT 51 /* 1 net */

/* BRAM (SB_RAM40_4K) port offsets — configurable geometry */
#define BRAM_RADDR   0   /* 11 nets: ports[0..10] */
#define BRAM_RDATA  11   /* 16 nets: ports[11..26] */
#define BRAM_WADDR  27   /* 11 nets: ports[27..37] */
#define BRAM_WDATA  38   /* 16 nets: ports[38..53] */
#define BRAM_MASK   54   /* 16 nets: ports[54..69] (not used in 512x8) */
#define BRAM_WE     70   /* 1 net */
#define BRAM_WCLKE  71   /* 1 net */
#define BRAM_WCLK   72   /* 1 net */
#define BRAM_RE     73   /* 1 net */
#define BRAM_RCLKE  74   /* 1 net */
#define BRAM_RCLK   75   /* 1 net */

struct port_info {
    char name[64];
    int width;
    int bits[64];           /* net ID per bit */
    int direction;          /* 0=input, 1=output (from netlist) */
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

    struct hard_cell hard_cells[MAX_HARD_CELLS];
    int num_hard_cells;

    struct port_info ports[MAX_PORTS];
    int num_ports;

    int clk_net;
    int hfosc_clk_net;      /* -1 if no SB_HFOSC present */
    int skip_hard_reads;    /* 1 = skip BRAM/SPRAM reads in eval_combinational */
};

/* Load a netlist from Yosys JSON. module_name=NULL for auto-detect. */
struct sim_state *netlist_load(const char *json_path, const char *module_name);
void netlist_free(struct sim_state *s);

#endif
