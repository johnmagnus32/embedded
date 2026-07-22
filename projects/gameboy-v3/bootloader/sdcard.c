/*
 * sdcard.c — SD-card driver for the gameboy-v3 bootloader (T113-S3 SMHC0).
 *
 * Brings up the SMHC0 controller and an SDHC/SDXC card, then reads 512-byte
 * blocks by LBA using PIO (the CPU drains the controller FIFO). This mirrors
 * U-Boot's own SPL path (ACCESS_BY_AHB), which deliberately avoids the IDMAC
 * descriptor machinery — simplest and proven for this SoC, and plenty fast for
 * loading a ~7 MB kernel at 25 MHz 4-bit.
 *
 * Register offsets + bit values verified byte-identical across awboot (T113-S3),
 * U-Boot drivers/mmc/sunxi_mmc.{c,h}, and the Linux sun20i-d1 driver. Clock regs
 * (CCU 0x830 mod-clock, 0x84c gate/reset) triple-confirmed.
 */

#include <stdint.h>
#include "sdcard.h"

int printf(const char *fmt, ...);

/* ---- bases ---------------------------------------------------------------- */
#define SMHC0_BASE   0x04020000u
#define CCU_BASE     0x02001000u
#define PIO_BASE     0x02000000u

/* ---- CCU clock control ---------------------------------------------------- */
#define CCU_SMHC0_CLK   (CCU_BASE + 0x0830u)   /* SMHC0 module clock            */
#define CCU_SMHC_BGR    (CCU_BASE + 0x084Cu)   /* SMHC bus gate + reset         */
#define SMHC0_GATE      (1u << 0)
#define SMHC0_RST       (1u << 16)
#define CLK_ENABLE      (1u << 31)
#define CLK_SRC_OSC24M  (0u << 24)

/* ---- SMHC register offsets (struct sunxi_mmc) ----------------------------- */
#define R_GCTRL   0x00u
#define R_CLKCR   0x04u
#define R_TMOUT   0x08u
#define R_WIDTH   0x0Cu
#define R_BLKSZ   0x10u
#define R_BYTECNT 0x14u
#define R_CMD     0x18u
#define R_ARG     0x1Cu
#define R_RESP0   0x20u
#define R_RESP1   0x24u
#define R_RESP2   0x28u
#define R_RESP3   0x2Cu
#define R_IMASK   0x30u
#define R_RINT    0x38u
#define R_STATUS  0x3Cu
#define R_FIFO    0x200u                        /* FIFO window on sun20i         */

/* GCTRL bits */
#define GCTRL_SOFT_RESET   (1u << 0)
#define GCTRL_FIFO_RESET   (1u << 1)
#define GCTRL_DMA_RESET    (1u << 2)
#define GCTRL_HW_RESET     (GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET)
#define GCTRL_ACCESS_BY_AHB (1u << 31)

/* CLKCR bits */
#define CLKCR_CARD_CLK_ON  (1u << 16)
#define CLKCR_MASK_D0      (1u << 31)

/* CMD bits */
#define CMD_RESP_EXPIRE    (1u << 6)
#define CMD_LONG_RESP      (1u << 7)
#define CMD_CHECK_CRC      (1u << 8)
#define CMD_DATA_EXPIRE    (1u << 9)
#define CMD_WRITE          (1u << 10)
#define CMD_WAIT_PRE_OVER  (1u << 13)
#define CMD_SEND_INIT_SEQ  (1u << 15)
#define CMD_UPCLK_ONLY     (1u << 21)
#define CMD_START          (1u << 31)

/* RINT bits */
#define RINT_CMD_DONE      (1u << 2)
#define RINT_DATA_OVER     (1u << 3)
#define RINT_ERROR_MASK    0xBFC2u

/* STATUS bits */
#define STAT_FIFO_EMPTY    (1u << 2)
#define STAT_CARD_BUSY     (1u << 9)

/* PE bank / SDC0 pins: SD is on PF bank per our board. On the MangoPi/T113
 * SDC0 uses PF0-PF5 (mux function 2). PF bank index = 5. */
#define PF_BANK   (PIO_BASE + 5u * 0x30u)
#define PF_CFG0   (PF_BANK + 0x00u)      /* config for PF0-7 */
#define PF_PULL0  (PF_BANK + 0x24u)      /* pull for PF0-7   */
#define SDC0_MUX  2u                     /* PF0-5 alt-func 2 = SDC0 */

/* ---- response type flags (for our cmd helper) ----------------------------- */
#define RSP_NONE  0u
#define RSP_R1    (CMD_RESP_EXPIRE | CMD_CHECK_CRC)
#define RSP_R1b   (CMD_RESP_EXPIRE | CMD_CHECK_CRC)
#define RSP_R2    (CMD_RESP_EXPIRE | CMD_LONG_RESP | CMD_CHECK_CRC)
#define RSP_R3    (CMD_RESP_EXPIRE)                       /* OCR: no CRC */
#define RSP_R6    (CMD_RESP_EXPIRE | CMD_CHECK_CRC)
#define RSP_R7    (CMD_RESP_EXPIRE | CMD_CHECK_CRC)

static inline void wr(uint32_t off, uint32_t v) { *(volatile uint32_t *)(SMHC0_BASE + off) = v; }
static inline uint32_t rd(uint32_t off) { return *(volatile uint32_t *)(SMHC0_BASE + off); }
static inline void mmio_w(uint32_t a, uint32_t v) { *(volatile uint32_t *)a = v; }
static inline uint32_t mmio_r(uint32_t a) { return *(volatile uint32_t *)a; }

/* bit helpers on SMHC regs (defined before first use) */
static inline void setbits_le32_local(uint32_t off, uint32_t set) { wr(off, rd(off) | set); }
static inline void clrbits_le32_local(uint32_t off, uint32_t clr) { wr(off, rd(off) & ~clr); }

extern void udelay(unsigned long us);  /* from dram_shim (busy loop) */

/* card state */
static uint32_t card_rca;
static int card_is_hc;      /* high-capacity (block addressing) */

/* ---- low-level: send one command, wait for completion --------------------- */
static int sd_cmd(uint32_t idx, uint32_t arg, uint32_t rsp_flags, uint32_t *resp)
{
	uint32_t cmdval = CMD_START | CMD_WAIT_PRE_OVER | idx | rsp_flags;
	uint32_t timeout;

	wr(R_RINT, 0xffffffff);            /* clear stale interrupts */
	wr(R_ARG, arg);
	wr(R_CMD, cmdval);

	/* wait for command-done or error */
	timeout = 1000000;
	while (timeout--) {
		uint32_t r = rd(R_RINT);
		if (r & RINT_ERROR_MASK)
			return -1;
		if (r & RINT_CMD_DONE)
			break;
	}
	if (timeout == 0)
		return -2;

	if (resp) {
		if (rsp_flags & CMD_LONG_RESP) {
			/* R2: 136-bit, returned across RESP0-3 (reversed) */
			resp[0] = rd(R_RESP3);
			resp[1] = rd(R_RESP2);
			resp[2] = rd(R_RESP1);
			resp[3] = rd(R_RESP0);
		} else {
			resp[0] = rd(R_RESP0);
		}
	}
	return 0;
}

/* ---- update the card clock (REQUIRED after any divider change) ------------ *
 * This special command reprograms the internal CIU clock divider WITHOUT
 * sending anything on the bus. Skipping it means a new clock never latches and
 * the next real command hangs — a classic silent-hang trap.
 */
static int sd_update_clock(void)
{
	uint32_t timeout = 100000;
	setbits_le32_local(R_CLKCR, CLKCR_MASK_D0);
	wr(R_CMD, CMD_START | CMD_UPCLK_ONLY | CMD_WAIT_PRE_OVER);
	while ((rd(R_CMD) & CMD_START) && timeout--)
		;
	wr(R_RINT, 0xffffffff);
	clrbits_le32_local(R_CLKCR, CLKCR_MASK_D0);
	return timeout ? 0 : -1;
}

/* ---- set the module (card) clock via the CCU ------------------------------ */
static void sd_set_mod_clock(uint32_t hz)
{
	/* Disable, select 24 MHz OSC, divide down, re-enable.
	 * card_clk = 24MHz / (2^N * (M+1)); for init we want ~400 kHz. */
	uint32_t n = 0, m;
	mmio_w(CCU_SMHC0_CLK, 0);                 /* disable while reconfiguring */
	if (hz <= 400000u) {
		/* 24MHz / (2^1 * 30) = 400 kHz  -> N=1, M=29 */
		n = 1; m = 29;
	} else {
		/* 24MHz / (2^0 * 1) = 24 MHz default-speed-ish; card tolerates */
		n = 0; m = 0;
	}
	mmio_w(CCU_SMHC0_CLK, CLK_ENABLE | CLK_SRC_OSC24M | (n << 8) | (m << 0));
}

/* ---- controller + card bring-up ------------------------------------------- */
int sd_init(void)
{
	uint32_t resp[4];
	uint32_t timeout;
	int ret;

	/* 1. Pinmux PF0-PF5 -> SDC0 (func 2), enable pull-ups. */
	uint32_t cfg = 0;
	for (int p = 0; p < 6; p++)
		cfg |= (SDC0_MUX << (p * 4));
	mmio_w(PF_CFG0, cfg);
	mmio_w(PF_PULL0, 0x555);                  /* pull-up (01) on PF0-5 */

	/* 2. CCU: enable SMHC0 bus gate + de-assert reset. */
	{
		uint32_t bgr = mmio_r(CCU_SMHC_BGR);
		bgr &= ~SMHC0_RST;                /* assert reset  */
		mmio_w(CCU_SMHC_BGR, bgr);
		bgr |= SMHC0_GATE;                /* enable gate   */
		mmio_w(CCU_SMHC_BGR, bgr);
		bgr |= SMHC0_RST;                 /* de-assert     */
		mmio_w(CCU_SMHC_BGR, bgr);
	}

	/* 3. Module clock @ 400 kHz for init. */
	sd_set_mod_clock(400000u);

	/* 4. Controller hardware reset; poll self-clear. */
	wr(R_GCTRL, GCTRL_HW_RESET);
	timeout = 100000;
	while ((rd(R_GCTRL) & GCTRL_HW_RESET) && timeout--)
		;
	if (timeout == 0)
		return -1;

	wr(R_RINT, 0xffffffff);
	wr(R_TMOUT, 0xffffffff);                  /* generous timeouts */
	wr(R_WIDTH, 0);                           /* 1-bit for init */
	wr(R_CLKCR, CLKCR_CARD_CLK_ON);
	if (sd_update_clock() != 0)
		return -2;

	/* 5. Card init sequence. */
	/* CMD0: go idle (send-init 80 clocks). */
	sd_cmd(0 | CMD_SEND_INIT_SEQ, 0, RSP_NONE, 0);
	udelay(2000);

	/* CMD8: voltage check (0x1AA). Response echoes 0xAA if v2 card. */
	ret = sd_cmd(8, 0x1AA, RSP_R7, resp);
	int v2 = (ret == 0 && (resp[0] & 0xff) == 0xaa);

	/* ACMD41: negotiate OCR, request high-capacity (HCS). Poll busy bit. */
	timeout = 100000;
	do {
		sd_cmd(55, 0, RSP_R1, resp);                 /* APP_CMD */
		uint32_t arg = 0x00FF8000u | (v2 ? 0x40000000u : 0u);
		ret = sd_cmd(41, arg, RSP_R3, resp);
		if (ret != 0)
			return -3;
		udelay(1000);
	} while (!(resp[0] & 0x80000000u) && timeout--);   /* wait power-up done */
	if (timeout == 0)
		return -4;
	card_is_hc = !!(resp[0] & 0x40000000u);            /* CCS bit */

	/* CMD2: get CID (long resp). */
	if (sd_cmd(2, 0, RSP_R2, resp) != 0)
		return -5;
	/* CMD3: get relative address (RCA in top 16 bits of R6). */
	if (sd_cmd(3, 0, RSP_R6, resp) != 0)
		return -6;
	card_rca = (resp[0] >> 16) & 0xffff;
	/* CMD9: read CSD (we don't need capacity — FAT/partition tells us). */
	sd_cmd(9, card_rca << 16, RSP_R2, resp);
	/* CMD7: select the card. */
	if (sd_cmd(7, card_rca << 16, RSP_R1b, resp) != 0)
		return -7;

	/* 6. Set block length 512 (harmless on HC cards) + 4-bit bus. */
	sd_cmd(16, 512, RSP_R1, resp);
	sd_cmd(55, card_rca << 16, RSP_R1, resp);
	sd_cmd(6, 2, RSP_R1, resp);                        /* ACMD6: 4-bit */
	wr(R_WIDTH, 1);                                    /* controller 4-bit */

	/* 7. Bump to ~24 MHz for the bulk reads. */
	sd_set_mod_clock(24000000u);
	if (sd_update_clock() != 0)
		return -8;

	printf("SD: card up (rca=0x%x, %s)\n",
	       card_rca, card_is_hc ? "HC/block-addr" : "SC/byte-addr");
	return 0;
}

/* ---- read one 512-byte block via PIO -------------------------------------- */
static int sd_read_one(uint32_t lba, uint8_t *buf)
{
	uint32_t arg = card_is_hc ? lba : (lba * 512u);
	uint32_t *w = (uint32_t *)buf;
	uint32_t got = 0, timeout;

	/* wait until card not busy */
	timeout = 1000000;
	while ((rd(R_STATUS) & STAT_CARD_BUSY) && timeout--)
		;

	wr(R_RINT, 0xffffffff);
	wr(R_BLKSZ, 512);
	wr(R_BYTECNT, 512);
	setbits_le32_local(R_GCTRL, GCTRL_ACCESS_BY_AHB); /* PIO mode */

	/* CMD17 READ_SINGLE_BLOCK, with data-transfer (read). */
	wr(R_ARG, arg);
	wr(R_CMD, CMD_START | CMD_WAIT_PRE_OVER | CMD_DATA_EXPIRE |
	          RSP_R1 | 17u);

	/* drain FIFO as data arrives */
	timeout = 10000000;
	while (got < 128) {                                /* 128 words = 512 B */
		uint32_t r = rd(R_RINT);
		if (r & RINT_ERROR_MASK)
			return -1;
		if (!(rd(R_STATUS) & STAT_FIFO_EMPTY))
			w[got++] = rd(R_FIFO);
		else if (timeout-- == 0)
			return -2;
	}

	/* wait for data-over */
	timeout = 1000000;
	while (!(rd(R_RINT) & RINT_DATA_OVER) && timeout--)
		;
	return timeout ? 0 : -3;
}

int sd_read_blocks(uint32_t lba, uint32_t count, void *buf)
{
	uint8_t *p = (uint8_t *)buf;
	for (uint32_t i = 0; i < count; i++) {
		int r = sd_read_one(lba + i, p + i * 512u);
		if (r != 0) {
			printf("SD: read error %d at lba %u\n", r, lba + i);
			return r;
		}
	}
	return 0;
}
