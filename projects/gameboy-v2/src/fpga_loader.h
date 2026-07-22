#ifndef FPGA_LOADER_H
#define FPGA_LOADER_H

/* Load bitstream into iCE40UP5K via SPI slave configuration.
 * Must be called before ppu_init(). Returns 0 on success, -1 on failure. */
int fpga_load_bitstream(void);

#endif
