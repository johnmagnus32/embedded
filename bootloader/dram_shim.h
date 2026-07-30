/*
 * dram_shim.h — freestanding replacements for the U-Boot facilities that
 * dram.c (U-Boot's dram_sun20i_d1.c, copied verbatim) expects. This lets us
 * reuse the exact, proven DRAM-init sequence in our bare-metal bootloader
 * without pulling in U-Boot's build system.
 *
 * Everything here is a thin, well-understood shim: MMIO accessors, a busy-loop
 * delay, a tiny printf, and the board tunables. The tunables are the ONLY
 * board-specific values, and they are taken verbatim from the MangoPi MQ-R
 * (T113-S3) U-Boot defconfig we already verified — same in-package DDR3 die as
 * our board, so they port unchanged.
 */

#ifndef GV3_DRAM_SHIM_H
#define GV3_DRAM_SHIM_H

#include <stdint.h>
#include <stdbool.h>

/* ---- types the driver uses ---------------------------------------------- */
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

#define DIV_ROUND_UP(n, d)	(((n) + (d) - 1) / (d))

/* Linux-kernel-style macros the driver uses (from bitops.h / kernel.h). */
#define BIT(n)			(1u << (n))
#define max(a, b)		((a) > (b) ? (a) : (b))
#define min(a, b)		((a) < (b) ? (a) : (b))

/* ---- register bases (sun20i / T113-S3), from mainline sources ----------- */
/* dram.c has #ifndef fallbacks for these, but we define them explicitly. */
#define SUNXI_CCM_BASE		0x02001000
#define SUNXI_SID_BASE		0x03006200
#define CFG_SYS_SDRAM_BASE	0x40000000	/* DRAM base for 32-bit sunxi */

/* ---- board DRAM tunables (verbatim from mangopi_mq_r_defconfig) ---------- *
 * These feed the driver's static `dram_para`/`dram_config` initializers and
 * ns_to_t(). 128 MB DDR3 @ 792 MHz; ZQ tied to the board's 240 ohm resistor.
 */
#define CONFIG_DRAM_CLK			792
#define CONFIG_DRAM_SUNXI_DRAM_TYPE	3	/* SUNXI_DRAM_TYPE_DDR3 */
#define CONFIG_SUNXI_DRAM_TYPE		3	/* driver's static para uses this name */
#define CONFIG_DRAM_ZQ			8092667
#define CONFIG_DRAM_SUNXI_ODT_EN	0
#define CONFIG_DRAM_SUNXI_TPR0		0x004a2195
#define CONFIG_DRAM_SUNXI_TPR11		0x340000
#define CONFIG_DRAM_SUNXI_TPR12		0x46
#define CONFIG_DRAM_SUNXI_TPR13		0x34000100

/* ---- MMIO accessors (U-Boot's readl/writel + bit helpers) --------------- */
static inline void writel(u32 val, unsigned long addr)
{
	*(volatile u32 *)addr = val;
}
static inline u32 readl(unsigned long addr)
{
	return *(volatile u32 *)addr;
}
static inline void setbits_le32(unsigned long addr, u32 set)
{
	writel(readl(addr) | (set), addr);
}
static inline void clrbits_le32(unsigned long addr, u32 clr)
{
	writel(readl(addr) & ~(clr), addr);
}
static inline void clrsetbits_le32(unsigned long addr, u32 clr, u32 set)
{
	writel((readl(addr) & ~(clr)) | (set), addr);
}

/* ---- timing --------------------------------------------------------------*
 * Busy-loop microsecond delay (real functions in dram_shim.c so other units,
 * e.g. sdcard.c, can link against them). The CPU runs at the BROM's boot clock
 * during early init (PLLs not yet raised); the loop is deliberately conservative
 * — callers only need "at least N us", so overshooting is harmless. Calibrated
 * loosely for a ~several-hundred-MHz A7; tune if timing proves sensitive on
 * silicon.
 */
void udelay(unsigned long us);
void mdelay(unsigned long ms);

/* ---- tiny printf (only %d %u %x %s %c, enough for the driver's messages) - *
 * Provided by dram_shim.c; routes to our UART0 putc.
 */
int printf(const char *fmt, ...);

/* The driver uses debug() for verbose tracing — compile it out (no-op). */
#define debug(...)	do { } while (0)

#endif /* GV3_DRAM_SHIM_H */
