/*
 * mk-egon.c — finalize a raw bootloader binary into a BROM-bootable eGON image.
 *
 * Our start.S already lays out the 96-byte eGON.BT0 header with the branch,
 * magic, and the check_sum field pre-seeded to the BROM stamp 0x5F0A6C39. This
 * host tool does the two things that can only be done after linking:
 *   1. pad the image up to a 512-byte multiple and write the total into
 *      the header 'length' field (offset 0x10),
 *   2. compute the BROM checksum and store it at offset 0x0C.
 *
 * Checksum algorithm (verified against U-Boot tools/sunxi_egon.c):
 *   - the check_sum field currently holds the stamp 0x5F0A6C39 (from start.S),
 *   - sum ALL length/4 little-endian 32-bit words of the (padded) image with
 *     plain 32-bit wraparound addition,
 *   - store that sum back into check_sum.
 * The BROM re-seeds the field with the stamp, re-sums, and compares.
 *
 * This is a HOST program (runs on x86_64), so it uses host byte order helpers;
 * the T113 is little-endian and so is the build host, so words map directly.
 *
 *   usage: mk-egon <in.bin> <out.bin>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define EGON_ALIGN        512u
#define STAMP_VALUE       0x5f0a6c39u
#define OFF_MAGIC         0x04
#define OFF_CHECKSUM      0x0c
#define OFF_LENGTH        0x10
#define HEADER_MIN        0x60      /* 96-byte header must be present */

static uint32_t rd32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void wr32(uint8_t *p, uint32_t v) {
	p[0] = v & 0xff; p[1] = (v >> 8) & 0xff;
	p[2] = (v >> 16) & 0xff; p[3] = (v >> 24) & 0xff;
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s <in.bin> <out.bin>\n", argv[0]);
		return 2;
	}

	FILE *f = fopen(argv[1], "rb");
	if (!f) { perror("open input"); return 1; }
	fseek(f, 0, SEEK_END);
	long raw = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (raw < HEADER_MIN) {
		fprintf(stderr, "input too small (%ld bytes) — no eGON header?\n", raw);
		fclose(f); return 1;
	}

	/* Pad up to a 512-byte multiple (BROM requires length 512-aligned). */
	size_t len = ((size_t)raw + EGON_ALIGN - 1) & ~(size_t)(EGON_ALIGN - 1);
	uint8_t *buf = calloc(1, len);
	if (!buf) { fprintf(stderr, "oom\n"); fclose(f); return 1; }
	if (fread(buf, 1, raw, f) != (size_t)raw) { perror("read"); fclose(f); free(buf); return 1; }
	fclose(f);

	/* Sanity: the magic must already be in place from start.S. */
	if (memcmp(buf + OFF_MAGIC, "eGON.BT0", 8) != 0) {
		fprintf(stderr, "missing 'eGON.BT0' magic at 0x04 — wrong input?\n");
		free(buf); return 1;
	}

	/* 1. length field. */
	wr32(buf + OFF_LENGTH, (uint32_t)len);

	/* 2. checksum: seed the slot with the stamp, then sum all LE words. */
	wr32(buf + OFF_CHECKSUM, STAMP_VALUE);
	uint32_t sum = 0;
	for (size_t i = 0; i < len; i += 4)
		sum += rd32(buf + i);          /* 32-bit wraparound */
	wr32(buf + OFF_CHECKSUM, sum);

	FILE *o = fopen(argv[2], "wb");
	if (!o) { perror("open output"); free(buf); return 1; }
	if (fwrite(buf, 1, len, o) != len) { perror("write"); fclose(o); free(buf); return 1; }
	fclose(o);
	free(buf);

	printf("eGON image: %zu bytes (padded from %ld), checksum 0x%08x\n",
	       len, raw, sum);
	return 0;
}
