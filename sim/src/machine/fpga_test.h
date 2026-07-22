#ifndef FPGA_TEST_H
#define FPGA_TEST_H

#include "stm32f411.h"
#include "trace_dev.h"
#include "chardev.h"
#include "ice40up5k.h"

#define FPGA_TEST_SYSCLK_HZ  16000000
#define FPGA_TEST_FPGA_HZ    48000000

struct fpga_test {
    struct stm32f411  soc;
    struct trace_dev  trace;
    struct ice40up5k  fpga;
    uint64_t fpga_accum;
};

void fpga_test_init(struct fpga_test *b, struct chardev_table *chardevs);
int  fpga_test_tick(struct fpga_test *b);

extern const struct machine_desc fpga_test_machine;

#endif
