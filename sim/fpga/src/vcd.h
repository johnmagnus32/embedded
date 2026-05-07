#ifndef VCD_H
#define VCD_H

#include <stdint.h>
#include "netlist.h"

struct vcd_writer;

struct vcd_writer *vcd_open(const char *path, struct port_info *ports, int num_ports);
void vcd_sample(struct vcd_writer *w, uint64_t time_ns, uint8_t *nets, uint8_t *prev, int num_nets);
void vcd_close(struct vcd_writer *w);

#endif
