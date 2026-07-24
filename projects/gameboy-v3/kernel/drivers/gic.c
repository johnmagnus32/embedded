/*
 * gic.c — ARM GICv2 minimal driver (GIC-400 on the T113; the same GICv2 model
 * on QEMU's `virt`). Register offsets verified vs the ARM GICv2 spec (IHI 0048);
 * the distributor/CPU-interface base addresses come from board.h (they're the
 * only board-specific thing here — the programming model is identical).
 *
 * We only need: enable the distributor + this core's CPU interface, allow all
 * priorities, and enable + prioritize one PPI (the timer). On an IRQ we read
 * GICC_IAR to acknowledge (get the INTID), then write it back to GICC_EOIR.
 */

#include <stdint.h>
#include "gic.h"
#include "board.h"     /* GICD_BASE / GICC_BASE */

/* Distributor offsets */
#define GICD_CTLR         0x000
#define GICD_ISENABLER    0x100   /* +4*(intid/32); bit (intid%32)            */
#define GICD_IPRIORITYR   0x400   /* +intid; 1 byte per INTID                 */
#define GICD_ICFGR        0xC00   /* +4*(intid/16); 2 bits per INTID          */

/* CPU-interface offsets */
#define GICC_CTLR   0x000
#define GICC_PMR    0x004
#define GICC_BPR    0x008
#define GICC_IAR    0x00C
#define GICC_EOIR   0x010

static inline void d_w(uint32_t off, uint32_t v) { *(volatile uint32_t *)(GICD_BASE + off) = v; }
static inline uint32_t d_r(uint32_t off) { return *(volatile uint32_t *)(GICD_BASE + off); }
static inline void d_b(uint32_t off, uint8_t v) { *(volatile uint8_t *)(GICD_BASE + off) = v; }
static inline void c_w(uint32_t off, uint32_t v) { *(volatile uint32_t *)(GICC_BASE + off) = v; }
static inline uint32_t c_r(uint32_t off) { return *(volatile uint32_t *)(GICC_BASE + off); }

void gic_init(void)
{
	/* Enable the distributor. (Single security state for us — write 1.) */
	d_w(GICD_CTLR, 1);

	/* CPU interface: allow all priorities (PMR = 0xFF; GIC-400 reads back
	 * 0xF8), no sub-priority grouping, then enable. FIQEn stays 0 so our
	 * Group-0 interrupt is delivered as an IRQ (not FIQ) — which is what our
	 * vector table handles. */
	c_w(GICC_PMR, 0xFF);
	c_w(GICC_BPR, 0);
	c_w(GICC_CTLR, 1);
}

void gic_enable_intid(uint32_t intid, uint8_t priority)
{
	/* Priority (1 byte per INTID). Lower value = higher priority. */
	d_b(GICD_IPRIORITYR + intid, priority);

	/* PPIs (INTID 16-31) are per-CPU/banked; no ITARGETSR needed. Enable it
	 * by setting the bit in the right ISENABLER word. */
	uint32_t reg = intid / 32;
	uint32_t bit = intid % 32;
	d_w(GICD_ISENABLER + reg * 4, (1u << bit));
}

uint32_t gic_ack(void)
{
	return c_r(GICC_IAR);              /* full value; INTID = bits [9:0]      */
}

void gic_eoi(uint32_t iar)
{
	c_w(GICC_EOIR, iar);               /* write back the exact IAR value      */
}
