/* test_fpga_dff_init.c — Verify all DFF variants start at 0 */
#include <stdio.h>
#include "ice40up5k.h"
#include "netlist.h"
#include "eval.h"

int main(void) {
    struct ice40up5k dev;
    ice40up5k_init(&dev, "build/func/hw/ice40up5k/netlists/test_fpga_dff_init.json");

    int pin_q_plain = ice40up5k_find_pin(&dev, "q_plain");
    int pin_q_set = ice40up5k_find_pin(&dev, "q_with_set");
    int fail = 0;

    /* Before any clock edge, both outputs should be 0 */
    if (dev.pins[pin_q_plain].level != 0) {
        printf("FAIL: q_plain=%d at init (expected 0)\n", dev.pins[pin_q_plain].level);
        fail++;
    }
    if (dev.pins[pin_q_set].level != 0) {
        printf("FAIL: q_with_set=%d at init (expected 0)\n", dev.pins[pin_q_set].level);
        fail++;
    }

    if (fail == 0) printf("PASS: dff_init (all DFFs start at 0)\n");
    return fail;
}
