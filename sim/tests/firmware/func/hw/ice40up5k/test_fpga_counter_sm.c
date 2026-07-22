/* test_fpga_counter_sm.c — Counter in state machine (reproduces enable/reset priority bug) */
#include <stdio.h>
#include "ice40up5k.h"
#include "netlist.h"
#include "eval.h"

int main(void) {
    struct ice40up5k dev;
    ice40up5k_init(&dev, "build/func/hw/ice40up5k/netlists/test_fpga_counter_sm.json");

    int pin_start = ice40up5k_find_pin(&dev, "start");
    int pin_count[9];
    for (int i = 0; i < 9; i++) {
        char n[16]; snprintf(n, 16, "count[%d]", i);
        pin_count[i] = ice40up5k_find_pin(&dev, n);
    }

    ice40up5k_tick_n(&dev, 3);
    ice40up5k_set_pin(&dev, pin_start, 1);
    ice40up5k_tick(&dev);
    ice40up5k_set_pin(&dev, pin_start, 0);
    ice40up5k_tick_n(&dev, 50);

    int val = 0;
    for (int i = 0; i < 9; i++)
        if (pin_count[i] >= 0) val |= (dev.pins[pin_count[i]].level << i);

    if (val >= 8) {
        printf("PASS: counter_sm (count=%d after 50 ticks)\n", val);
        return 0;
    } else {
        printf("FAIL: counter_sm (count=%d, expected >= 8)\n", val);
        return 1;
    }
}
