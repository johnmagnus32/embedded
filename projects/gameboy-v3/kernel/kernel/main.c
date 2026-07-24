/*
 * kmain.c — gameboy-v3 custom kernel, Stage 5.
 *
 * Proves the bare-metal foundation everything else (MMU, processes, syscalls)
 * will build on: exception vectors + GIC-400 + a periodic timer interrupt.
 *
 * Flow: UART up -> GIC up -> timer armed -> enable IRQs -> idle loop. The timer
 * fires ~10x/sec; irq_dispatch() acknowledges it via the GIC, re-arms the timer,
 * and prints a heartbeat. Seeing "tick" advance on the UART = a real hardware
 * interrupt is being taken and serviced by our code. That's the whole S5 goal.
 */

#include <stdint.h>
#include "uart.h"
#include "gic.h"
#include "timer.h"
#include "pmm.h"
#include "mmu.h"
#include "proc.h"
#include "ramfs.h"
#include "cpio.h"
#include "vfs.h"
#include "kfdt.h"
#include "inflate.h"

#define INIT_PATH "/init"      /* PID 1 the kernel spawns from the initramfs */

int printf(const char *fmt, ...);

/* from start.S */
void irq_enable(void);
uint32_t boot_dtb(void);       /* r2 the bootloader passed: DTB phys addr (or 0) */

/* Real fault handling lives in proc.c (fault_trap) — it needs the process model
 * to kill the faulting process and reschedule. See proc.c. */

#define TICK_HZ  10               /* 10 ms-ish ticks (100 would be finer) */

static volatile uint32_t g_ticks;

/* Monotonic uptime in milliseconds, from the timer tick. The time syscalls
 * (gettimeofday/clock_gettime) use this — coarse (1000/TICK_HZ ms resolution)
 * but monotonic and cheap, and it needs no 64-bit division (none is linked).
 * Wraps after ~49 days of uptime, irrelevant for a handheld. */
uint32_t ktime_uptime_ms(void) { return g_ticks * (1000u / TICK_HZ); }

/*
 * mmu_selftest — prove translation works after mmu_init().
 *
 * Take a fresh physical page, map it at an otherwise-unused virtual address
 * (0x50000000, well above DRAM), write a pattern through the *virtual* address,
 * and read it back through the page's *physical* address. If the two agree, the
 * MMU translated 0x50000000 -> our physical page exactly as the descriptor said.
 * (VA and PA differ here, so this genuinely exercises translation — unlike the
 * identity-mapped regions where VA==PA would prove nothing.)
 */
static void mmu_selftest(void)
{
	const uint32_t test_va = 0x50000000u;      /* unused 1 MB-aligned VA */
	uint32_t pa = pmm_alloc_aligned(256, 0x100000u);  /* 1 MB, 1MB-aligned */
	if (!pa) { printf("mmu selftest: no 1MB frame\n"); return; }

	mmu_map_section(test_va, pa, MEM_NORMAL);

	volatile uint32_t *via_va = (volatile uint32_t *)(uintptr_t)test_va;
	volatile uint32_t *via_pa = (volatile uint32_t *)(uintptr_t)pa;   /* identity-mapped */

	via_va[0] = 0xC0FFEE42u;                    /* write through the new VA */
	via_va[1] = 0xDEADBEEFu;

	int ok = (via_pa[0] == 0xC0FFEE42u) && (via_pa[1] == 0xDEADBEEFu);
	printf("mmu selftest: VA 0x%x -> PA 0x%x, readback %s\n",
	       test_va, pa, ok ? "MATCH (translation works)" : "MISMATCH!");
	/* (leave the test region mapped/allocated — harmless; kernel runs on) */
}

/* Preemption master switch. When 0 the timer tick is just a clock (cooperative
 * behavior, unchanged). When 1 the tick drives a USER-MODE context switch via
 * proc_preempt. Kept 0 until the switchable IRQ path is proven to round-trip
 * byte-identically; then flipped on. See irq_dispatch / proc_preempt. */
#ifndef PREEMPT_ENABLED
#define PREEMPT_ENABLED 1
#endif

/*
 * irq_dispatch — called from irq_entry (start.S) with IRQs masked and the
 * interrupted context in `tf` (a full trapframe on the IRQ stack). Acknowledge
 * the interrupt, service it, EOI, then (if preemption is enabled and this was a
 * timer tick that hit user mode) round-robin to another process. Returns the
 * frame irq_entry should resume — the same `tf` unless we switched.
 *
 * EOI happens BEFORE the possible context switch: on the GIC, GICC_IAR raised
 * the running priority and only GICC_EOIR drops it; if we switched away without
 * EOI, no further timer IRQ would ever be delivered and preemption would fire
 * exactly once. So: ack -> service -> EOI -> maybe switch.
 */
struct trapframe *irq_dispatch(struct trapframe *tf)
{
	uint32_t iar = gic_ack();
	uint32_t intid = iar & 0x3FF;

	if (intid == GIC_SPURIOUS)
		return tf;                    /* nothing pending / spurious */

	int tick = 0;
	if (intid == TIMER_INTID) {
		timer_rearm();                /* schedule the next tick */
		g_ticks++;                    /* drives ktime_uptime_ms() */
		tick = 1;
	} else {
		printf("irq: unexpected intid %u\n", intid);
	}

	gic_eoi(iar);                     /* drop running priority BEFORE any switch */

	if (PREEMPT_ENABLED && tick)
		return proc_preempt(tf);      /* user-mode round-robin (no-op in kernel) */
	return tf;
}

/*
 * load_archive — turn a possibly-gzipped cpio at [arch, arch+sz) into ramfs
 * inodes. If it's gzip (magic 1f 8b), size a scratch buffer from the trailer
 * ISIZE, allocate it from pmm, gunzip into it, then cpio_load the result; else
 * cpio_load directly. Returns cpio_load's entry count (or negative). The scratch
 * buffer is intentionally NOT freed — decompression is a one-shot at boot and
 * the freed-page bookkeeping isn't worth it for a 1.4 MB transient. */
static int load_archive(const void *arch, uint32_t sz)
{
	const uint8_t *b = arch;
	if (!is_gzip(b, sz))
		return cpio_load(arch, sz);

	uint32_t out_sz = gzip_isize(b, sz);
	if (out_sz == 0) { printf("gunzip: zero ISIZE\n"); return -100; }
	/* page-rounded scratch from pmm (contiguous — the inflater needs one flat
	 * buffer so back-references can index earlier output). */
	uint32_t npages = (out_sz + PAGE_SIZE - 1) / PAGE_SIZE;
	uint32_t scratch = pmm_alloc_aligned(npages, PAGE_SIZE);
	if (!scratch) { printf("gunzip: no %u-page scratch buffer\n", npages); return -101; }

	long n = gunzip(b, sz, (uint8_t *)(uintptr_t)scratch, npages * PAGE_SIZE);
	if (n < 0) { printf("gunzip: inflate error %ld\n", n); return -102; }
	printf("gunzip: %u -> %u bytes\n", sz, (uint32_t)n);
	return cpio_load((const void *)(uintptr_t)scratch, (uint32_t)n);
}

void kmain(void)
{
	uart0_init();
	uart0_puts("\n");
	uart0_puts("===================================================\n");
	uart0_puts("  gameboy-v3 custom kernel — Stage 10\n");
	uart0_puts("  boot handoff (DTB/initrd) + mem syscalls + tty\n");
	uart0_puts("===================================================\n");

	/* ---- Boot handoff: read the DTB the bootloader passed in r2 --------- *
	 * The bootloader/U-Boot hands us r2 = DTB physical address (ARM Linux boot
	 * protocol). If it's a valid blob, we pull the initramfs location from its
	 * /chosen/bootargs (initrd=<hexaddr>,<decsize>, as our bootloader writes)
	 * and the RAM size from /memory. Absent/invalid DTB -> fall back to the
	 * cpio embedded in the kernel image + the hardcoded 128 MB. */
	/* The DTB is normally at r2 (ARM Linux boot protocol — our T113 bootloader,
	 * and QEMU when it runs its boot stub). But QEMU `-M virt` entered from an
	 * ELF jumps straight to e_entry WITHOUT setting r2; it still places its DTB
	 * at the base of RAM (0x40000000). So: trust r2 if it's a valid FDT, else
	 * probe the RAM base. */
	uint32_t dtb = boot_dtb();
	if (!kfdt_valid((const void *)(uintptr_t)dtb) &&
	    kfdt_valid((const void *)0x40000000u))
		dtb = 0x40000000u;
	uint32_t ram_top = 0x48000000u;        /* default: 128 MB @ 0x40000000 */
	uint32_t initrd_base = 0, initrd_size = 0;
	int have_dtb = kfdt_valid((const void *)(uintptr_t)dtb);
	if (have_dtb) {
		printf("dtb: valid blob @ 0x%x (%u bytes)\n", dtb, kfdt_totalsize((const void*)(uintptr_t)dtb));
		uint32_t mbase, msize;
		if (kfdt_memory((const void *)(uintptr_t)dtb, &mbase, &msize) == 0) {
			printf("dtb: /memory = 0x%x + %u MiB\n", mbase, msize >> 20);
			/* Only trust it if it starts at our DRAM base, and clamp the top to
			 * what mmu.c actually identity-maps as Normal RAM (0x48000000) —
			 * pmm must never hand out frames the MMU maps as Device memory. */
			if (mbase == 0x40000000u) {
				uint32_t top = mbase + msize;
				ram_top = (top > 0x48000000u) ? 0x48000000u : top;
			}
		}
		if (kfdt_initrd((const void *)(uintptr_t)dtb, &initrd_base, &initrd_size) == 0)
			printf("dtb: initrd = 0x%x, %u bytes\n", initrd_base, initrd_size);
		else
			uart0_puts("dtb: no initrd= in bootargs (using embedded cpio)\n");
	} else {
		uart0_puts("dtb: none/invalid (using embedded cpio + 128 MB default)\n");
	}

	/* ---- Stage 6: physical allocator + MMU ----------------------------- */
	extern char _kernel_end;               /* from kernel.ld */
	uint32_t free_start = (uint32_t)(uintptr_t)&_kernel_end;
	pmm_init(free_start, ram_top);         /* free DRAM .. top of RAM */

	/* Protect the bootloader-placed regions BEFORE anything allocates: the DTB
	 * and the initramfs both sit inside the DRAM range pmm now manages, so
	 * reserve them or mmu_init/spawn could hand those frames out and corrupt
	 * them before we consume them. */
	if (have_dtb)
		pmm_reserve(dtb, dtb + kfdt_totalsize((const void *)(uintptr_t)dtb));
	if (initrd_base && initrd_size)
		pmm_reserve(initrd_base, initrd_base + initrd_size);

	mmu_init();                            /* identity map + enable translation */
	mmu_selftest();

	/* ---- Stage 5: interrupts (now running translated) ------------------ */
	gic_init();
	printf("gic: distributor + cpu interface enabled\n");

	gic_enable_intid(TIMER_INTID, 0xA0);   /* enable the timer PPI at the GIC */
	timer_init(TICK_HZ);                   /* arm the secure physical timer   */

	irq_enable();                          /* clear CPSR.I — interrupts live  */
	uart0_puts("IRQs enabled.\n");

	/* ---- Stage 9/10: unpack the bootloader-supplied initramfs, spawn /init - *
	 * The initramfs comes ONLY from the boot handoff (the DTB's initrd, loaded by
	 * the bootloader / QEMU -initrd) — no built-in fallback, like a stock Linux
	 * kernel with CONFIG_INITRAMFS_SOURCE="". load_archive() transparently gunzips
	 * a gzip'd image before cpio_load, so a compressed .cpio.gz works directly. */
	uart0_puts("\nStage 10: unpacking initramfs into ramfs ...\n");
	if (!initrd_base || !initrd_size) {
		uart0_puts("no initramfs from the boot handoff (need -initrd / DTB initrd) — halting\n");
		goto idle;
	}
	ramfs_init();
	int ents = load_archive((const void *)(uintptr_t)initrd_base, initrd_size);
	if (ents < 0) {
		printf("cpio: initramfs load failed (%d) — halting\n", ents);
		goto idle;
	}
	printf("cpio: unpacked %d entries from the bootloader initrd\n", ents);

	proc_init();

	if (!proc_spawn_elf(INIT_PATH, INIT_PATH)) {
		uart0_puts("failed to spawn /init\n");
		goto idle;
	}

	/* Enter the process world. proc_run_first drops to USER mode at /init's
	 * entry; from there fork/execve/wait/exit + the file syscalls drive
	 * everything. Control returns only if the last process exits. */
	proc_run_first();

idle:
	for (;;)
		__asm__ volatile("wfi");       /* sleep until the next interrupt  */
}
