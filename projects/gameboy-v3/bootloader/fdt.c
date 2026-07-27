/*
 * fdt.c — tiny flattened-device-tree helper (see fdt.h for the philosophy).
 *
 * FDT layout (all multi-byte fields BIG-endian): a header, a memory-reservation
 * block, a "structure block" of tokens, and a "strings block". We walk the
 * structure block to find /chosen and set its `bootargs` property.
 *
 * Unlike the original in-place-only version, we behave like a real (minimal)
 * libfdt setprop: we can GROW an existing bootargs value, or INSERT the property
 * if it's absent — so the shared DTB no longer needs a reserved placeholder
 * string. We do this with no allocator: the blob is loaded with slack after it
 * (DTB_MAX >> dtb size), and we memmove the tail up to make room, fixing the
 * header offsets. Requires the conventional dtc layout where the strings block
 * is LAST (struct precedes strings); we verify that before editing.
 *
 * Structure-block tokens (each a BE u32, 4-byte aligned):
 *   0x1 BEGIN_NODE  followed by NUL-terminated name, padded to 4 bytes
 *   0x2 END_NODE
 *   0x3 PROP        followed by {u32 len, u32 nameoff}, then `len` value bytes
 *                   (padded to 4), where nameoff indexes the strings block
 *   0x4 NOP
 *   0x9 END
 *
 * Header words (BE u32 each):
 *   0 magic  1 totalsize  2 off_dt_struct  3 off_dt_strings  4 off_mem_rsvmap
 *   5 version 6 last_comp_version 7 boot_cpuid_phys
 *   8 size_dt_strings  9 size_dt_struct           (both present since v17; dtc emits v17)
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

/* header word indices */
#define H_TOTALSIZE     1
#define H_OFF_STRUCT    2
#define H_OFF_STRINGS   3
#define H_SIZE_STRINGS  8
#define H_SIZE_STRUCT   9

#define ALIGN4(x)       (((x) + 3u) & ~3u)

/* big-endian 32-bit load/store (the FDT is BE regardless of CPU endianness) */
static uint32_t be32(const uint8_t *p) {
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static void wr32(uint8_t *p, uint32_t v) {
	p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}
static uint32_t hget(const uint8_t *d, int word) { return be32(d + word * 4); }
static void     hset(uint8_t *d, int word, uint32_t v) { wr32(d + word * 4, v); }

static int str_eq(const char *a, const char *b) {
	while (*a && *b) { if (*a != *b) return 0; a++; b++; }
	return *a == *b;
}
static uint32_t str_len(const char *s) { uint32_t n = 0; while (s[n]) n++; return n; }

/* Shift bytes [from, old_total) up by `delta` (copy backwards for overlap). The
 * caller must have already ensured `old_total + delta` bytes fit in the buffer. */
static void shift_up(uint8_t *d, uint32_t from, uint32_t old_total, uint32_t delta) {
	uint32_t i = old_total;
	while (i > from) { i--; d[i + delta] = d[i]; }
}

int fdt_check(const void *dtb)
{
	const uint8_t *d = dtb;
	if (be32(d) != FDT_MAGIC) {
		printf("FDT: bad magic 0x%x\n", be32(d));
		return -1;
	}
	return 0;
}

/* Find "name\0" at an entry boundary in the strings block; return its offset
 * within the strings block, or -1 if absent. */
static long strings_find(const uint8_t *d, const char *name)
{
	uint32_t off = hget(d, H_OFF_STRINGS);
	uint32_t sz  = hget(d, H_SIZE_STRINGS);
	uint32_t i = 0;
	while (i < sz) {
		if (str_eq((const char *)(d + off + i), name)) return (long)i;
		while (i < sz && d[off + i] != 0) i++;   /* skip to NUL */
		i++;                                     /* skip the NUL  */
	}
	return -1;
}

/* Append "name\0" to the end of the strings block (which must be the last block)
 * and return its offset within the strings block. Updates size_dt_strings and
 * totalsize. */
static uint32_t strings_append(uint8_t *d, const char *name)
{
	uint32_t off   = hget(d, H_OFF_STRINGS);
	uint32_t sz    = hget(d, H_SIZE_STRINGS);
	uint32_t total = hget(d, H_TOTALSIZE);
	uint32_t n     = str_len(name) + 1;
	uint32_t nameoff = sz;
	for (uint32_t i = 0; i < n; i++) d[off + sz + i] = (uint8_t)name[i];
	hset(d, H_SIZE_STRINGS, sz + n);
	hset(d, H_TOTALSIZE,    total + n);
	return nameoff;
}

/* Replace/grow an existing /chosen/bootargs value.
 *   tokpos = offset of the FDT_PROP token; valpos = offset of its value bytes;
 *   len    = current stored value length. */
static int fdt_replace_bootargs(uint8_t *d, uint32_t tokpos, uint32_t len,
                                uint32_t valpos, const char *cmdline, uint32_t need)
{
	uint32_t old_space = ALIGN4(len);
	uint32_t new_space = ALIGN4(need);

	if (new_space > old_space) {
		/* GROW: make room after the value area, then extend the property. */
		uint32_t delta = new_space - old_space;
		uint32_t total = hget(d, H_TOTALSIZE);
		shift_up(d, valpos + old_space, total, delta);
		hset(d, H_TOTALSIZE,   total + delta);
		hset(d, H_OFF_STRINGS, hget(d, H_OFF_STRINGS) + delta);
		hset(d, H_SIZE_STRUCT, hget(d, H_SIZE_STRUCT) + delta);
		wr32(d + tokpos + 4, need);                 /* new value length */
		for (uint32_t i = 0; i < new_space; i++)
			d[valpos + i] = (i < need) ? (uint8_t)cmdline[i] : 0;
		printf("FDT: bootargs grown (%u bytes)\n", need);
	} else {
		/* FITS: overwrite in place, keep the existing length, pad with NUL. The
		 * kernel reads it as a C string, so trailing NULs are ignored. Keeping
		 * `len` unchanged means no following token needs to move. */
		for (uint32_t i = 0; i < len; i++)
			d[valpos + i] = (i < need) ? (uint8_t)cmdline[i] : 0;
		printf("FDT: bootargs set in place (%u/%u bytes)\n", need, len);
	}
	return 0;
}

/* Insert a new bootargs property into /chosen at `ip` (the first-property slot,
 * i.e. right after the node name). */
static int fdt_insert_bootargs(uint8_t *d, uint32_t ip,
                               const char *cmdline, uint32_t need)
{
	/* Ensure "bootargs" exists in the strings block (append if not). Do this
	 * first: strings is the last block, so appending only extends the tail and
	 * does not disturb `ip` or any struct offset. */
	long noff = strings_find(d, "bootargs");
	if (noff < 0) noff = (long)strings_append(d, "bootargs");

	uint32_t val_space = ALIGN4(need);
	uint32_t ins       = 12u + val_space;   /* PROP + len + nameoff + padded value */
	uint32_t total     = hget(d, H_TOTALSIZE);

	/* Open a gap of `ins` bytes at ip (shifts struct tail + strings block up). */
	shift_up(d, ip, total, ins);
	hset(d, H_TOTALSIZE,   total + ins);
	hset(d, H_OFF_STRINGS, hget(d, H_OFF_STRINGS) + ins);
	hset(d, H_SIZE_STRUCT, hget(d, H_SIZE_STRUCT) + ins);

	/* Write the new property token into the gap. nameoff is relative to the
	 * strings block, so it stays valid after the block was shifted. */
	wr32(d + ip + 0, FDT_PROP);
	wr32(d + ip + 4, need);
	wr32(d + ip + 8, (uint32_t)noff);
	for (uint32_t i = 0; i < val_space; i++)
		d[ip + 12 + i] = (i < need) ? (uint8_t)cmdline[i] : 0;

	printf("FDT: bootargs inserted (%u bytes)\n", need);
	return 0;
}

int fdt_set_bootargs(void *dtb, const char *cmdline)
{
	uint8_t *d = dtb;
	if (fdt_check(d) != 0) return -1;

	uint32_t off_struct   = hget(d, H_OFF_STRUCT);
	uint32_t off_strings  = hget(d, H_OFF_STRINGS);
	uint32_t size_strings = hget(d, H_SIZE_STRINGS);
	uint32_t total        = hget(d, H_TOTALSIZE);

	/* We grow the blob by moving the tail; that only works if the strings block
	 * is the last thing in it (the conventional dtc layout). Verify, else bail
	 * rather than corrupt. */
	if (!(off_struct < off_strings && off_strings + size_strings <= total)) {
		printf("FDT: unexpected block layout; refusing to edit\n");
		return -5;
	}

	uint32_t need = str_len(cmdline) + 1;   /* value bytes incl. NUL */

	/* Walk the structure block. Track whether the CURRENTLY-OPEN node is /chosen
	 * (a direct child of root), remember its first-property slot for a possible
	 * insert, and handle an existing bootargs property in place / by growing.
	 * NOTE: assumes /chosen has no subnodes (true for our board DTBs). */
	uint32_t p = off_struct;
	int depth = 0;
	int in_chosen = 0;
	int have_chosen = 0;
	uint32_t chosen_body = 0;

	for (;;) {
		uint32_t tokpos = p;
		uint32_t tok = be32(d + p); p += 4;

		if (tok == FDT_END) break;
		if (tok == FDT_NOP) continue;

		if (tok == FDT_BEGIN_NODE) {
			const char *name = (const char *)(d + p);
			depth++;
			p += ALIGN4(str_len(name) + 1);
			in_chosen = (depth == 2 && str_eq(name, "chosen"));
			if (in_chosen) { have_chosen = 1; chosen_body = p; }
			continue;
		}
		if (tok == FDT_END_NODE) {
			in_chosen = 0;
			depth--;
			continue;
		}
		if (tok == FDT_PROP) {
			uint32_t len    = be32(d + p);
			uint32_t noff   = be32(d + p + 4);
			uint32_t valpos = p + 8;
			const char *pname = (const char *)(d + off_strings + noff);
			if (in_chosen && str_eq(pname, "bootargs"))
				return fdt_replace_bootargs(d, tokpos, len, valpos, cmdline, need);
			p = valpos + ALIGN4(len);
			continue;
		}

		printf("FDT: unexpected token 0x%x\n", tok);
		return -3;
	}

	/* No bootargs found. If /chosen exists, insert the property there. */
	if (have_chosen)
		return fdt_insert_bootargs(d, chosen_body, cmdline, need);

	printf("FDT: /chosen not found\n");
	return -4;
}
