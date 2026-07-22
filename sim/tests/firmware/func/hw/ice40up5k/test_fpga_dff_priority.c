/* test_fpga_dff_priority.c — Verify enable gates reset in SB_DFFESR */
#include <stdio.h>
#include "ice40up5k.h"
#include "netlist.h"
#include "eval.h"

int main(void) {
    struct ice40up5k dev;
    ice40up5k_init(&dev, "build/func/hw/ice40up5k/netlists/test_fpga_dff_priority.json");

    int pin_d = ice40up5k_find_pin(&dev, "d");
    int pin_en = ice40up5k_find_pin(&dev, "enable");
    int pin_r = ice40up5k_find_pin(&dev, "reset");
    int pin_q = ice40up5k_find_pin(&dev, "q");
    int fail = 0;

    /* Test 1: E=1, R=0, D=1 → Q=1 */
    ice40up5k_set_pin(&dev, pin_d, 1);
    ice40up5k_set_pin(&dev, pin_en, 1);
    ice40up5k_set_pin(&dev, pin_r, 0);
    ice40up5k_tick(&dev);
    if (dev.pins[pin_q].level != 1) { printf("FAIL test1: Q=%d expected 1\n", dev.pins[pin_q].level); fail++; }

    /* Test 2: E=0, R=1 → Q holds at 1 (enable gates reset) */
    ice40up5k_set_pin(&dev, pin_en, 0);
    ice40up5k_set_pin(&dev, pin_r, 1);
    ice40up5k_tick(&dev);
    if (dev.pins[pin_q].level != 1) { printf("FAIL test2: Q=%d expected 1 (hold)\n", dev.pins[pin_q].level); fail++; }

    /* Test 3: E=1, R=1 → Q=0 (reset fires when enabled) */
    ice40up5k_set_pin(&dev, pin_en, 1);
    ice40up5k_set_pin(&dev, pin_r, 1);
    ice40up5k_tick(&dev);
    if (dev.pins[pin_q].level != 0) { printf("FAIL test3: Q=%d expected 0\n", dev.pins[pin_q].level); fail++; }

    /* Test 4: E=1, R=0, D=1 → Q=1 again */
    ice40up5k_set_pin(&dev, pin_r, 0);
    ice40up5k_set_pin(&dev, pin_d, 1);
    ice40up5k_tick(&dev);
    if (dev.pins[pin_q].level != 1) { printf("FAIL test4: Q=%d expected 1\n", dev.pins[pin_q].level); fail++; }

    /* Test 5: E=0, R=1, D=0 → Q holds at 1 */
    ice40up5k_set_pin(&dev, pin_en, 0);
    ice40up5k_set_pin(&dev, pin_r, 1);
    ice40up5k_set_pin(&dev, pin_d, 0);
    ice40up5k_tick(&dev);
    if (dev.pins[pin_q].level != 1) { printf("FAIL test5: Q=%d expected 1 (hold)\n", dev.pins[pin_q].level); fail++; }

    if (fail == 0) printf("PASS: dff_priority (5 tests)\n");
    return fail;
}
