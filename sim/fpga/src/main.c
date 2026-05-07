/*
 * main.c — fpga-sim entry point: arg parsing, simulation loop, assertions
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netlist.h"
#include "eval.h"
#include "vcd.h"

#define MAX_ASSERTS 16

struct assertion {
    char port[32];
    uint32_t value;
};

int main(int argc, char **argv)
{
    const char *netlist_path = NULL;
    const char *vcd_path = "sim_out.vcd";
    const char *module = NULL;  /* NULL = auto-detect */
    uint64_t max_cycles = 100;
    int period_ns = 83;
    struct assertion asserts[MAX_ASSERTS];
    int num_asserts = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--cycles") == 0) max_cycles = strtoull(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--vcd") == 0) vcd_path = argv[++i];
        else if (strcmp(argv[i], "--module") == 0) module = argv[++i];
        else if (strcmp(argv[i], "--period") == 0) period_ns = atoi(argv[++i]);
        else if (strcmp(argv[i], "--assert") == 0) {
            char *spec = argv[++i];
            char *eq = strchr(spec, '=');
            if (eq) {
                *eq = 0;
                strncpy(asserts[num_asserts].port, spec, 31);
                asserts[num_asserts].value = strtoul(eq + 1, NULL, 0);
                num_asserts++;
            }
        }
        else if (!netlist_path) netlist_path = argv[i];
    }

    if (!netlist_path) {
        fprintf(stderr, "Usage: fpga-sim <netlist.json> [--module name] [--cycles N] [--vcd file.vcd] [--assert port=val]\n");
        return 1;
    }

    /* Load and prepare netlist */
    fprintf(stderr, "[fpga-sim] Loading %s\n", netlist_path);
    struct sim_state *s = netlist_load(netlist_path, module);
    fprintf(stderr, "[fpga-sim] %d cells, %d nets, %d combinational, %d DFF\n",
            s->num_cells, s->num_nets, s->num_eval, s->num_dffs);

    if (s->clk_net < 0) {
        fprintf(stderr, "[fpga-sim] No 'clk' port found\n");
        netlist_free(s);
        return 1;
    }

    /* Initialize nets to 0 */
    memset(s->nets, 0, sizeof(s->nets));
    memset(s->nets_prev, 0, sizeof(s->nets_prev));

    /* Open VCD */
    struct vcd_writer *vcd = vcd_open(vcd_path, s->ports, s->num_ports);

    /* Simulation loop */
    fprintf(stderr, "[fpga-sim] Simulating %lu cycles...\n", (unsigned long)max_cycles);

    for (uint64_t cyc = 0; cyc < max_cycles; cyc++) {
        /* Rising edge */
        s->nets[s->clk_net] = 1;
        eval_clock_edge(s);
        eval_combinational(s);
        vcd_sample(vcd, cyc * period_ns, s->nets, s->nets_prev, s->num_nets);
        memcpy(s->nets_prev, s->nets, s->num_nets);

        /* Falling edge */
        s->nets[s->clk_net] = 0;
        eval_combinational(s);
        vcd_sample(vcd, cyc * period_ns + period_ns / 2, s->nets, s->nets_prev, s->num_nets);
        memcpy(s->nets_prev, s->nets, s->num_nets);
    }

    vcd_close(vcd);

    /* Print final state */
    fprintf(stderr, "[fpga-sim] Final port values:\n");
    for (int i = 0; i < s->num_ports; i++) {
        struct port_info *p = &s->ports[i];
        if (p->width == 1) {
            fprintf(stderr, "  %s = %d\n", p->name, get_net(s, p->bits[0]));
        } else {
            uint32_t v = 0;
            for (int b = 0; b < p->width; b++) v |= (get_net(s, p->bits[b]) << b);
            fprintf(stderr, "  %s = %u (0x%x)\n", p->name, v, v);
        }
    }
    fprintf(stderr, "[fpga-sim] VCD: %s\n", vcd_path);

    /* Check assertions */
    int fail = 0;
    for (int a = 0; a < num_asserts; a++) {
        int found = 0;
        for (int i = 0; i < s->num_ports; i++) {
            if (strcmp(s->ports[i].name, asserts[a].port) == 0) {
                found = 1;
                uint32_t actual = 0;
                for (int b = 0; b < s->ports[i].width; b++)
                    actual |= (get_net(s, s->ports[i].bits[b]) << b);
                if (actual != asserts[a].value) {
                    fprintf(stderr, "ASSERT FAIL: %s = %u, expected %u\n",
                            asserts[a].port, actual, asserts[a].value);
                    fail = 1;
                } else {
                    fprintf(stderr, "ASSERT PASS: %s = %u\n", asserts[a].port, actual);
                }
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "ASSERT FAIL: port '%s' not found\n", asserts[a].port);
            fail = 1;
        }
    }

    netlist_free(s);
    return fail;
}
