/*
 * timer.c — ARMv7 generic timer via CP15. Secure physical timer (CNTP_*).
 *
 * CP15 encodings (verified vs ARMv7-A ARM generic-timer chapter):
 *   CNTFRQ    p15, 0, Rt, c14, c0, 0   (counter frequency, Hz)
 *   CNTP_TVAL p15, 0, Rt, c14, c2, 0   (down-counter; fires at 0)
 *   CNTP_CTL  p15, 0, Rt, c14, c2, 1   (bit0 ENABLE, bit1 IMASK, bit2 ISTATUS)
 *
 * Periodic tick: set TVAL = freq/hz, ENABLE=1 IMASK=0; on each IRQ reload TVAL.
 */

#include <stdint.h>
#include "timer.h"

int printf(const char *fmt, ...);

/* --- CP15 accessors --- */
static inline uint32_t rd_cntfrq(void) {
	uint32_t v; __asm__ volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(v)); return v;
}
static inline void wr_cntfrq(uint32_t v) {
	__asm__ volatile("mcr p15, 0, %0, c14, c0, 0" :: "r"(v));
}
static inline void wr_cntp_tval(uint32_t v) {
	__asm__ volatile("mcr p15, 0, %0, c14, c2, 0" :: "r"(v));
}
static inline void wr_cntp_ctl(uint32_t v) {
	__asm__ volatile("mcr p15, 0, %0, c14, c2, 1" :: "r"(v));
}

#define CNTP_CTL_ENABLE  (1u << 0)
#define CNTP_CTL_IMASK   (1u << 1)

static uint32_t g_freq;      /* counter frequency (Hz) */
static uint32_t g_reload;    /* TVAL reload for one tick */

uint32_t timer_freq(void) { return g_freq; }

void timer_init(unsigned hz)
{
	/* Read the firmware-programmed counter frequency. The T113 runs the
	 * timer at 24 MHz (DT clock-frequency = 24000000). If CNTFRQ reads 0
	 * (BROM didn't set it), fall back to 24 MHz and program it — CNTFRQ is
	 * writable in secure PL1, which is where we run. */
	uint32_t f = rd_cntfrq();
	if (f == 0) {
		f = 24000000u;
		wr_cntfrq(f);
	}
	g_freq = f;
	g_reload = f / hz;

	wr_cntp_tval(g_reload);                 /* first interval */
	wr_cntp_ctl(CNTP_CTL_ENABLE);           /* ENABLE=1, IMASK=0 */
	printf("timer: %u Hz counter, tick every %u ticks (%u Hz)\n",
	       g_freq, g_reload, hz);
}

void timer_rearm(void)
{
	/* Re-arm for the next interval. Writing TVAL restarts the down-count
	 * and clears the pending condition (ISTATUS) for this comparator. */
	wr_cntp_tval(g_reload);
}
