/*
 * fdt.c — tiny flattened-device-tree helper (see fdt.h for the philosophy).
 *
 * FDT layout (all multi-byte fields BIG-endian): a header, a memory-reservation
 * block, a "structure block" of tokens, and a "strings block". We walk the
 * structure block to find /chosen's `bootargs` property and overwrite its value
 * in place. Because the value fits in the compile-time placeholder, no offsets,
 * sizes, or the strings block change — the blob stays structurally identical.
 *
 * Structure-block tokens (each a BE u32, 4-byte aligned):
 *   0x1 BEGIN_NODE  followed by NUL-terminated name, padded to 4 bytes
 *   0x2 END_NODE
 *   0x3 PROP        followed by {u32 len, u32 nameoff}, then `len` value bytes
 *                   (padded to 4), where nameoff indexes the strings block
 *   0x4 NOP
 *   0x9 END
 */

#include <stdint.h>
#include "fdt.h"

int printf(const char *fmt, ...);

#define FDT_MAGIC       0xd00dfeed
#define FDT_BEGIN_NODE  0x1
#define FDT_END_NODE    0x2
#define FDT_PROP        0x3
#define FDT_NOP         0x4
#define FDT_END         0x9

/* big-endian 32-bit load/store (the FDT is BE regardless of CPU endianness) */
static uint32_t be32(const uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint32_t hdr(const uint8_t *d, int word) { return be32(d + word * 4); }

static int str_eq(const char *a, const char *b) {
	while (*a && *b) { if (*a != *b) return 0; a++; b++; }
	return *a == *b;
}
static uint32_t str_len(const char *s) { uint32_t n = 0; while (s[n]) n++; return n; }

int fdt_check(const void *dtb)
{
	const uint8_t *d = dtb;
	if (be32(d) != FDT_MAGIC) {
		printf("FDT: bad magic 0x%x\n", be32(d));
		return -1;
	}
	return 0;
}

int fdt_set_bootargs(void *dtb, const char *cmdline)
{
	uint8_t *d = dtb;
	if (fdt_check(d) != 0) return -1;

	uint32_t off_struct  = hdr(d, 2);   /* off_dt_struct  */
	uint32_t off_strings = hdr(d, 3);   /* off_dt_strings */
	const char *strings  = (const char *)(d + off_strings);

	/* Walk the structure block, tracking whether we're inside /chosen. */
	uint32_t p = off_struct;
	int depth = 0;
	int in_chosen = 0;

	for (;;) {
		uint32_t tok = be32(d + p); p += 4;
		if (tok == FDT_END) break;
		if (tok == FDT_NOP) continue;

		if (tok == FDT_BEGIN_NODE) {
			const char *name = (const char *)(d + p);
			depth++;
			/* /chosen is a direct child of root (depth becomes 2). */
			in_chosen = (depth == 2 && str_eq(name, "chosen"));
			uint32_t nl = str_len(name) + 1;
			p += (nl + 3) & ~3u;
			continue;
		}
		if (tok == FDT_END_NODE) {
			if (depth == 2) in_chosen = 0;
			depth--;
			continue;
		}
		if (tok == FDT_PROP) {
			uint32_t len   = be32(d + p);
			uint32_t noff  = be32(d + p + 4);
			p += 8;
			const char *pname = strings + noff;
			uint8_t *pval = d + p;            /* property value bytes */

			if (in_chosen && str_eq(pname, "bootargs")) {
				uint32_t need = str_len(cmdline) + 1;
				if (need > len) {
					printf("FDT: bootargs placeholder too small (%u < %u)\n",
					       len, need);
					return -2;
				}
				/* overwrite value in place; pad rest with NULs */
				for (uint32_t i = 0; i < len; i++)
					pval[i] = (i < need) ? (uint8_t)cmdline[i] : 0;
				printf("FDT: bootargs set (%u/%u bytes used)\n", need, len);
				return 0;
			}
			p += (len + 3) & ~3u;
			continue;
		}
		/* unknown token — bail rather than run off the rails */
		printf("FDT: unexpected token 0x%x\n", tok);
		return -3;
	}
	printf("FDT: /chosen/bootargs not found\n");
	return -4;
}
