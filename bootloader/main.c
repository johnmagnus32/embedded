/*
 * main.c — gameboy-v3 custom bootloader.
 *
 * Stage 1: bring up UART0 (PE2/PE3) and print — proves the BROM->eGON->SRAM->
 *          our-code->UART foundation.
 * Stage 2: initialize the 128 MB DDR3 (sunxi_dram_init, adapted from U-Boot's
 *          proven sun20i driver) and run a small memory test to prove RAM works.
 *
 * Still to come: Stage 3 (read kernel/DTB/initramfs off the SD) and Stage 4
 * (patch the DTB and jump to Linux).
 */

#include <stdint.h>
#include "uart.h"
#include "sdcard.h"
#include "fat.h"
#include "fdt.h"
#include "spinor.h"
#include "nor_layout.h"

/* Boot source for Stage 3: read the payload from SD/FAT (BOOT_SRC_SD) or from
 * raw SPI-NOR offsets (BOOT_SRC_NOR). Selected at compile time via -DBOOT_SRC=…;
 * defaults to NOR (the remote FEL-load-loader + components-in-NOR dev loop). */
#define BOOT_SRC_SD   0
#define BOOT_SRC_NOR  1
#ifndef BOOT_SRC
#define BOOT_SRC BOOT_SRC_NOR
#endif

/* boot_kernel: ARM Linux handoff (in start.S). Sets r0=0/r1=~0/r2=dtb, MMU off,
 * and branches to the kernel entry. Never returns. */
void boot_kernel(uint32_t entry, uint32_t dtb);

/* From dram.c (U-Boot's sun20i DRAM driver): inits DDR3, returns size in bytes. */
unsigned long sunxi_dram_init(void);

int printf(const char *fmt, ...);   /* tiny printf in dram_shim.c */

#define DRAM_BASE  0x40000000u      /* CFG_SYS_SDRAM_BASE for 32-bit sunxi */

/* Where we load the boot payload in DRAM — U-Boot's own sunxi load addresses
 * for this SoC (kernel >32MiB, DTB & initramfs spread out, non-overlapping). */
#define KERNEL_ADDR   0x41000000u
#define DTB_ADDR      0x41800000u
#define INITRD_ADDR   0x41C00000u
#define KERNEL_MAX    0x00800000u   /* 8 MiB cap (zImage ~5.4M)  */
#define DTB_MAX       0x00100000u   /* 1 MiB cap (dtb ~18K)      */
#define INITRD_MAX    0x00400000u   /* 4 MiB cap (initrd ~1.2M)  */

#define KERNEL_NAME   "zImage"      /* mainline zImage OR custom zImage-headed  */
#define DTB_NAME      "sun8i-t113s-mangopi-mq-r-t113.dtb"
#define INITRD_NAME   "initramfs.cpio.gz"

/*
 * A quick, honest memory test over a sampling of the reported DRAM. We can't
 * exhaustively test 128 MB quickly with a busy-loop delay-calibrated build, so
 * we write a pseudo-random pattern to one address per 1 MiB across the whole
 * range and read it back. Catches dead rows, aliasing (wrong size), and stuck
 * data lines — the common DRAM-init failure modes.
 */
static int dram_quicktest(unsigned long size_bytes)
{
	volatile uint32_t *dram = (volatile uint32_t *)DRAM_BASE;
	unsigned long step = 0x100000u / 4u;         /* one word per 1 MiB */
	unsigned long words = size_bytes / 4u;
	uint32_t seed = 0x12345678u;
	unsigned long i;
	int errors = 0;

	/* write pass */
	for (i = 0; i < words; i += step)
		dram[i] = seed ^ (uint32_t)i;

	/* read/verify pass */
	for (i = 0; i < words; i += step) {
		uint32_t want = seed ^ (uint32_t)i;
		uint32_t got = dram[i];
		if (got != want) {
			if (errors < 4)
				printf("  MISMATCH @ +0x%x: wrote 0x%x read 0x%x\n",
				       (unsigned)(i * 4), want, got);
			errors++;
		}
	}
	return errors;
}

/* append NUL-terminated src to dst[o..], return new offset */
static int append(char *dst, int o, int cap, const char *src)
{
	while (*src && o < cap - 1) dst[o++] = *src++;
	dst[o] = 0;
	return o;
}
/* append `v` in the given base (10 or 16) */
static int append_num(char *dst, int o, int cap, uint32_t v, int base)
{
	char tmp[11]; int n = 0;
	const char *digits = "0123456789abcdef";
	if (v == 0) tmp[n++] = '0';
	while (v) { tmp[n++] = digits[v % base]; v /= base; }
	while (n-- && o < cap - 1) dst[o++] = tmp[n];
	dst[o] = 0;
	return o;
}

/* Build "console=ttyS0,115200 earlycon mem=128M@0x40000000
 *        initrd=0x<addr>,<size> panic=10" into buf.
 * Bare "earlycon" (not "=on", which is a silent no-op) makes the kernel scan the
 * DTB /chosen/stdout-path and register the snps,dw-apb-uart earlycon, inheriting
 * our 115200 baud — output from the very first kernel instructions. */
static void build_cmdline(char *buf, int cap, uint32_t initrd_addr, uint32_t initrd_size)
{
	int o = 0;
	o = append(buf, o, cap, "console=ttyS0,115200 earlycon "
	                        "mem=128M@0x40000000 initrd=0x");
	o = append_num(buf, o, cap, initrd_addr, 16);
	o = append(buf, o, cap, ",");
	o = append_num(buf, o, cap, initrd_size, 10);
	o = append(buf, o, cap, " panic=10");
}

void main(void)
{
	uart0_init();
	uart0_puts("\n");
	uart0_puts("===================================================\n");
	uart0_puts("  gameboy-v3 custom bootloader\n");
	uart0_puts("  Stage 1: UART0 up (hello from SRAM)\n");
	uart0_puts("  Stage 2: initializing DDR3 ...\n");
	uart0_puts("===================================================\n");

	unsigned long dram_bytes = sunxi_dram_init();
	if (dram_bytes == 0) {
		uart0_puts("!! DRAM init FAILED (see messages above)\n");
		return;                     /* start.S spins */
	}
	printf("DRAM OK: %u MiB @ 0x%x\n",
	       (unsigned)(dram_bytes >> 20), DRAM_BASE);

	uart0_puts("Running quick memory test ...\n");
	int errs = dram_quicktest(dram_bytes);
	if (errs == 0)
		uart0_puts("Memory test PASSED.\n");
	else
		printf("Memory test FAILED: %d mismatches\n", errs);

	/* ---- Stage 3: read the boot payload (SD/FAT or SPI-NOR) ------------- */
	long ksz, dsz, isz;
#if BOOT_SRC == BOOT_SRC_SD
	uart0_puts("\nStage 3: reading boot files from SD ...\n");
	if (sd_init() != 0) {
		uart0_puts("!! SD init FAILED\n");
		return;
	}
	if (fat_mount() != 0) {
		uart0_puts("!! FAT mount FAILED\n");
		return;
	}
	/* Both kernels are named "zImage" (mainline zImage or our custom
	 * zImage-headed image) — one name, one interface. */
	ksz = fat_load(KERNEL_NAME, (void *)KERNEL_ADDR, KERNEL_MAX);
	dsz = fat_load(DTB_NAME,    (void *)DTB_ADDR,    DTB_MAX);
	isz = fat_load(INITRD_NAME, (void *)INITRD_ADDR, INITRD_MAX);
	if (ksz < 0 || dsz < 0 || isz < 0) {
		uart0_puts("!! failed to load one or more boot files\n");
		return;
	}
#else /* BOOT_SRC_NOR */
	uart0_puts("\nStage 3: reading boot components from SPI-NOR ...\n");
	if (spinor_init() != 0) {
		uart0_puts("!! SPI-NOR init FAILED\n");
		return;
	}
	/* Read the component table at NOR offset 0 (GV3NOR1 magic; see nor_layout.h). */
	struct nor_table tbl;
	if (spinor_read(NOR_TABLE_OFF, &tbl, sizeof(tbl)) != 0) {
		uart0_puts("!! NOR table read FAILED\n");
		return;
	}
	if (tbl.magic[0] != 'G' || tbl.magic[1] != 'V' || tbl.magic[2] != '3' ||
	    tbl.magic[3] != 'N' || tbl.magic[4] != 'O' || tbl.magic[5] != 'R') {
		printf("!! bad NOR table magic (%c%c%c%c%c%c)\n",
		       tbl.magic[0], tbl.magic[1], tbl.magic[2],
		       tbl.magic[3], tbl.magic[4], tbl.magic[5]);
		return;
	}
	printf("NOR table: kernel@0x%x/%u  dtb@0x%x/%u  initrd@0x%x/%u\n",
	       tbl.kernel_off, tbl.kernel_size, tbl.dtb_off, tbl.dtb_size,
	       tbl.initrd_off, tbl.initrd_size);
	if (spinor_read(tbl.kernel_off, (void *)KERNEL_ADDR, tbl.kernel_size) != 0 ||
	    spinor_read(tbl.dtb_off,    (void *)DTB_ADDR,    tbl.dtb_size)    != 0 ||
	    spinor_read(tbl.initrd_off, (void *)INITRD_ADDR, tbl.initrd_size) != 0) {
		uart0_puts("!! NOR component read FAILED\n");
		return;
	}
	ksz = tbl.kernel_size;
	dsz = tbl.dtb_size;
	isz = tbl.initrd_size;
#endif

	printf("\nAll loaded into DRAM:\n");
	printf("  kernel   @ 0x%x  (%u bytes)\n", KERNEL_ADDR, (unsigned)ksz);
	printf("  dtb      @ 0x%x  (%u bytes)\n", DTB_ADDR,    (unsigned)dsz);
	printf("  initramfs@ 0x%x  (%u bytes)\n", INITRD_ADDR, (unsigned)isz);

	/* ---- Stage 4: patch the DTB and jump to Linux ---------------------- */
	uart0_puts("\nStage 4: preparing to boot Linux ...\n");
	if (fdt_check((void *)DTB_ADDR) != 0) {
		uart0_puts("!! DTB failed magic check\n");
		return;
	}

	/*
	 * Build the kernel command line:
	 *   console=ttyS0,115200   -> our UART0 console (matches the DTB alias)
	 *   earlycon               -> see the very earliest kernel output (bare form;
	 *                             "=on" is a no-op — see build_cmdline)
	 *   mem=128M@0x40000000    -> RAM size (also in the DTB /memory node)
	 *   initrd=<addr>,<size>   -> where we loaded the initramfs + its size
	 *                             (early_initrd parses exactly "addr,size")
	 *   panic=10               -> reboot 10s after a panic
	 * The initrd size is the one runtime-variable value, so we format it in.
	 * fdt_set_bootargs() now grows/inserts as needed, so the DTB no longer
	 * carries a placeholder bootargs — cmdline can be any length up to `cap`.
	 */
	char cmdline[224];
	build_cmdline(cmdline, sizeof(cmdline), INITRD_ADDR, (uint32_t)isz);
	printf("cmdline: %s\n", cmdline);

	if (fdt_set_bootargs((void *)DTB_ADDR, cmdline) != 0) {
		uart0_puts("!! failed to set bootargs in DTB\n");
		return;
	}

	/*
	 * Both kernels are zImages: the mainline self-decompressing zImage and our
	 * custom kernel (which carries a zImage-compatible header — `b _start` at
	 * offset 0, magic at 0x24). A zImage is entered at its load address (offset
	 * 0), MMU/caches off, r0=0/r1=~0/r2=DTB — exactly what boot_kernel sets up.
	 * So we jump to KERNEL_ADDR for either kernel; no format handling needed
	 * (same interface U-Boot's `bootz` uses).
	 */
	(void)ksz;
	uart0_puts("Jumping to kernel. Bye from the bootloader!\n\n");
	boot_kernel(KERNEL_ADDR, DTB_ADDR);   /* never returns */

	uart0_puts("!! boot_kernel returned — should never happen\n");
}
