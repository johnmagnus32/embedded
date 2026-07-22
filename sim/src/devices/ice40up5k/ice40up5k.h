#ifndef ICE40UP5K_H
#define ICE40UP5K_H

#include <stdint.h>
#include "gpio_line.h"

struct sim_state;

#define ICE40_MAX_PINS 256

struct ice40_pin {
    char name[32];
    int net_id;
    int direction;      /* 0=input, 1=output */
    uint8_t level;
    uint8_t prev_level; /* for change detection (two-pass update) */
    struct gpio_line out;
};

struct ice40up5k {
    struct sim_state *fpga;
    struct ice40_pin pins[ICE40_MAX_PINS];
    int num_pins;
};

void ice40up5k_init(struct ice40up5k *dev, const char *netlist_path);
void ice40up5k_free(struct ice40up5k *dev);
void ice40up5k_tick(struct ice40up5k *dev);
void ice40up5k_set_pin(struct ice40up5k *dev, int pin_idx, uint8_t level);
int  ice40up5k_find_pin(struct ice40up5k *dev, const char *name);

#endif
