/*
 * spinor.c — SPI0 + W25Q128 SPI-NOR reader for the gameboy-v3 bootloader.
 *
 * Brings up the SPI0 controller (PC2-PC5, mux func 2) on the 24 MHz oscillator
 * and reads raw byte ranges from NOR by PIO (CPU drains the RX FIFO). Read-only:
 * we only load the boot components pre-flashed via `xfel spinor write`.
 *
 * Register offsets + bit values are the standard sun6i/sun20i SPI layout,
 * verified against the T113-S3 datasheet (SPI0 @ 0x04025000: GCR 0x04, TCR 0x08,
 * FCR 0x18, FSR 0x1C, MBC 0x30, MTC 0x34, BCC 0x38, TXD 0x200, RXD 0x300) and
 * cross-checked with U-Boot drivers/spi/spi-sunxi.c + awboot. Clock/pin facts
 * match uart.c/sdcard.c: CCU @ 0x02001000, PIO @ 0x02000000, PC bank = index 2.
 */

#include <stdint.h>
#include "spinor.h"

int printf(const char *fmt, ...);

/* ---- bases --------------------------------------------------------------- */
#define SPI0_BASE   0x04025000u
#define CCU_BASE    0x02001000u
#define PIO_BASE    0x02000000u

/* ---- CCU: SPI0 clock + bus gate/reset ------------------------------------ */
#define CCU_SPI0_CLK   (CCU_BASE + 0x0940u)   /* SPI0 mod clock                */
#define CCU_SPI_BGR    (CCU_BASE + 0x096Cu)   /* SPI bus gate + reset          */
#define SPI0_GATE      (1u << 0)              /* SPI_BGR bit0: SPI0 bus gate    */
#define SPI0_RST       (1u << 16)             /* SPI_BGR bit16: SPI0 reset      */
#define SPI0_CLK_EN    (1u << 31)             /* SPI0_CLK bit31: clock enable   */
#define SPI0_CLK_SRC_HOSC (0u << 24)          /* clock source = 24 MHz HOSC     */
/* mod-clock divider: HOSC(24MHz)/(N*(M+1)); use N=0(/1), M=0 -> ~24MHz. NOR
 * reads fine well above that; keep it simple + safe for the bodge. */

/* ---- PIO: PC bank (index 2), pins PC2-PC5 -> func 2 (spi0) ---------------- */
#define PIO_BANK_SIZE  0x30u
#define PC_BANK        (PIO_BASE + 2u * PIO_BANK_SIZE)  /* 0x02000060          */
#define PC_CFG0        (PC_BANK + 0x00u)                /* config PC0-PC7      */
#define PC_MUX_SPI0    2u                               /* alt func 2 = spi0   */

/* ---- SPI register offsets ------------------------------------------------ */
#define SPI_GCR   0x04u
#define SPI_TCR   0x08u
#define SPI_FCR   0x18u
#define SPI_FSR   0x1Cu
#define SPI_CCR   0x24u
#define SPI_MBC   0x30u   /* master burst counter (total bytes)               */
#define SPI_MTC   0x34u   /* master transmit counter                          */
#define SPI_BCC   0x38u   /* burst control (single/dual, dummy)               */
#define SPI_TXD   0x200u  /* transmit data FIFO                               */
#define SPI_RXD   0x300u  /* receive data FIFO                                */

/* GCR bits */
#define GCR_EN     (1u << 0)    /* enable SPI                                  */
#define GCR_MODE   (1u << 1)    /* 1 = master mode                            */
#define GCR_TP_EN  (1u << 7)    /* transmit pause on RX-FIFO-full             */
#define GCR_SRST   (1u << 31)   /* soft reset                                 */

/* TCR bits (sun6i/sun20i) */
#define TCR_CPHA      (1u << 0)   /* clock phase                              */
#define TCR_CPOL      (1u << 1)   /* clock polarity                           */
#define TCR_SPOL      (1u << 2)   /* CS active-low (1 = active low)           */
#define TCR_CS_SEL_SH 4           /* [5:4] chip-select index                  */
#define TCR_SS_OWNER  (1u << 6)   /* 1 = software (manual) CS control          */
#define TCR_CS_LEVEL  (1u << 7)   /* manual CS output level: 1=high(idle),
                                   *   0=low(asserted). U-Boot clears to select. */
#define TCR_XCH       (1u << 31)  /* start burst; auto-clears                 */

/* FCR bits */
#define FCR_RF_RST    (1u << 15)  /* RX FIFO reset                            */
#define FCR_TF_RST    (1u << 31)  /* TX FIFO reset                            */

/* FSR: RX FIFO count in bits [7:0] */
#define FSR_RF_CNT_MASK 0xffu

static inline void wr(uint32_t a, uint32_t v){ *(volatile uint32_t*)a = v; }
static inline uint32_t rd(uint32_t a){ return *(volatile uint32_t*)a; }
static inline void wb(uint32_t a, uint8_t v){ *(volatile uint8_t*)a = v; }
static inline uint8_t rb(uint32_t a){ return *(volatile uint8_t*)a; }

/* W25Q128 commands */
#define CMD_READ_ID   0x9Fu
#define CMD_READ      0x03u   /* normal read (up to ~50 MHz; fine at 24 MHz)   */

#define SPI_FIFO_DEPTH  64u   /* sun6i/sun20i RX+TX FIFO depth                 */

/*
 * One full-duplex burst, matching U-Boot's spi-sunxi model exactly:
 *   - clock `n` bytes total (MBC=MTC=BCC=n; TP disabled so we transmit all n)
 *   - write all `n` TX bytes into TXD (real bytes for the cmd/addr phase, 0x00
 *     padding for the read phase — the NOR ignores MOSI while it drives MISO)
 *   - XCH, wait for it to auto-clear
 *   - drain exactly `n` bytes from RXD (RX[i] is the byte clocked in during the
 *     i-th clocked byte; read data sits at the padding positions)
 * `n` must be <= FIFO depth. tx[] length is `n` (tx bytes then don't-care).
 */
static void spi_burst(const uint8_t *tx, uint8_t *rx, uint32_t n)
{
	/* reset FIFOs */
	wr(SPI0_BASE + SPI_FCR, FCR_RF_RST | FCR_TF_RST);
	while (rd(SPI0_BASE + SPI_FCR) & (FCR_RF_RST | FCR_TF_RST))
		;
	/* assert CS: manual CS, select CS0, drive CS LOW (clear CS_LEVEL).
	 * (U-Boot: clear TCR_CS_LEVEL to select the chip.) */
	uint32_t tcr = rd(SPI0_BASE + SPI_TCR);
	tcr &= ~(0x3u << TCR_CS_SEL_SH);     /* CS0                                */
	tcr &= ~TCR_CS_LEVEL;                /* CS low = asserted                  */
	wr(SPI0_BASE + SPI_TCR, tcr);

	wr(SPI0_BASE + SPI_MBC, n);          /* total burst bytes                 */
	wr(SPI0_BASE + SPI_MTC, n);          /* transmit all n (full-duplex)       */
	wr(SPI0_BASE + SPI_BCC, n);          /* single-wire, n tx bytes, 0 dummy   */
	for (uint32_t i = 0; i < n; i++)
		wb(SPI0_BASE + SPI_TXD, tx[i]);
	wr(SPI0_BASE + SPI_TCR, rd(SPI0_BASE + SPI_TCR) | TCR_XCH);
	while (rd(SPI0_BASE + SPI_TCR) & TCR_XCH)
		;
	for (uint32_t i = 0; i < n; i++) {
		while ((rd(SPI0_BASE + SPI_FSR) & FSR_RF_CNT_MASK) == 0)
			;
		uint8_t b = rb(SPI0_BASE + SPI_RXD);
		if (rx)
			rx[i] = b;
	}
	/* deassert CS: drive CS HIGH (set CS_LEVEL). */
	wr(SPI0_BASE + SPI_TCR, rd(SPI0_BASE + SPI_TCR) | TCR_CS_LEVEL);
}

int spinor_init(void)
{
	/* 1. mux PC2-PC5 -> spi0 (func 2). PC2=[11:8] PC3=[15:12] PC4=[19:16]
	 *    PC5=[23:20] in PC_CFG0. */
	uint32_t cfg = rd(PC_CFG0);
	for (int pin = 2; pin <= 5; pin++) {
		cfg &= ~(0xfu << (pin * 4));
		cfg |=  (PC_MUX_SPI0 << (pin * 4));
	}
	wr(PC_CFG0, cfg);

	/* 2. SPI0 mod clock: source HOSC, enable, divider /1. */
	wr(CCU_SPI0_CLK, SPI0_CLK_EN | SPI0_CLK_SRC_HOSC);

	/* 3. ungate + de-reset SPI0 on the bus. */
	uint32_t bgr = rd(CCU_SPI_BGR);
	bgr |= SPI0_GATE;   wr(CCU_SPI_BGR, bgr);
	bgr |= SPI0_RST;    wr(CCU_SPI_BGR, bgr);

	/* 4. soft-reset the controller, then enable master mode + SW-controlled CS.
	 * Leave TP_EN off: we transmit the full burst length (full-duplex), so we
	 * never rely on the RX-full transmit-pause. */
	wr(SPI0_BASE + SPI_GCR, GCR_SRST);
	while (rd(SPI0_BASE + SPI_GCR) & GCR_SRST)
		;
	wr(SPI0_BASE + SPI_GCR, GCR_EN | GCR_MODE);

	/* CS: software owner, active-low, idle high. CCR left at reset (works at
	 * HOSC for a simple 0x03 read). */
	wr(SPI0_BASE + SPI_TCR, TCR_SS_OWNER | TCR_SPOL | TCR_CS_LEVEL);

	/* 5. probe: read JEDEC ID (0x9F + 3 read bytes). W25Q128 = EF 40 18. */
	uint8_t tx[4] = { CMD_READ_ID, 0, 0, 0 };
	uint8_t rx[4] = { 0 };
	spi_burst(tx, rx, 4);
	/* rx[0] = byte clocked in during the cmd (garbage); rx[1..3] = the ID.
	 * (printf's %x prints a full uint; mask so the log reads cleanly.) */
	printf("SPI-NOR: JEDEC id %x %x %x\n",
	       rx[1] & 0xff, rx[2] & 0xff, rx[3] & 0xff);
	if (rx[1] == 0x00 || rx[1] == 0xff) {
		printf("SPI-NOR: no flash detected\n");
		return -1;
	}
	return 0;
}

int spinor_read(uint32_t addr, void *buf, uint32_t len)
{
	uint8_t *p = buf;
	/* Each burst: 4 header bytes (0x03 + 24-bit addr) + up to (DEPTH-4) data.
	 * Keep the whole burst within the FIFO depth so a single fill/drain works. */
	const uint32_t MAX_DATA = SPI_FIFO_DEPTH - 4u;   /* 60 data bytes/burst    */
	while (len > 0) {
		uint32_t n = (len < MAX_DATA) ? len : MAX_DATA;
		uint8_t tx[SPI_FIFO_DEPTH];
		uint8_t rx[SPI_FIFO_DEPTH];
		tx[0] = CMD_READ;
		tx[1] = (uint8_t)(addr >> 16);
		tx[2] = (uint8_t)(addr >> 8);
		tx[3] = (uint8_t)addr;
		for (uint32_t i = 0; i < n; i++)
			tx[4 + i] = 0x00;            /* padding clocks out the read data */
		spi_burst(tx, rx, 4 + n);
		for (uint32_t i = 0; i < n; i++)
			p[i] = rx[4 + i];            /* data sits after the 4 header bytes */
		addr += n; p += n; len -= n;
	}
	return 0;
}
