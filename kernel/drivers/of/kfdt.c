/*
 * kfdt.c — kernel-side flattened-device-tree reader (see kfdt.h).
 *
 * Structure-block walk (verified vs the devicetree spec v0.4 + the real blob):
 *   header: magic@0, totalsize@4, off_dt_struct@8, off_dt_strings@12 (BE u32s).
 *   tokens (BE u32, 4-byte aligned): BEGIN_NODE(0x1)+name(NUL,pad4),
 *   END_NODE(0x2), PROP(0x3)+{len,nameoff}(BE)+value(pad4), NOP(0x4), END(0x9).
 *   nameoff is a byte offset into the strings block. /chosen is a child of the
 *   empty-named root, i.e. depth 2 with root counted as depth 1.
 */

#include <stdint.h>
#include "kfdt.h"

#define FDT_MAGIC       0xd00dfeedu
#define FDT_BEGIN_NODE  0x1u
#define FDT_END_NODE    0x2u
#define FDT_PROP        0x3u
#define FDT_NOP         0x4u
#define FDT_END         0x9u

static uint32_t be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static int streq(const char *a, const char *b)
{
	while (*a && *a == *b) { a++; b++; }
	return *a == *b;
}
static uint32_t slen(const char *s) { uint32_t n = 0; while (s[n]) n++; return n; }

int kfdt_valid(const void *dtb)
{
	if (!dtb) return 0;
	return be32((const uint8_t *)dtb) == FDT_MAGIC;
}

uint32_t kfdt_totalsize(const void *dtb)
{
	return be32((const uint8_t *)dtb + 4);
}

/* Count path components ("/chosen" -> 1, "/soc/serial" -> 2). */
static int path_ncomp(const char *path)
{
	int n = 0;
	const char *p = path;
	while (*p) {
		while (*p == '/') p++;
		if (!*p) break;
		n++;
		while (*p && *p != '/') p++;
	}
	return n;
}

/* Return a pointer to the idx-th (0-based) path component, and its length in
 * *outlen (excluding any trailing '/'). NULL if idx is out of range. */
static const char *path_comp(const char *path, int idx, uint32_t *outlen)
{
	const char *p = path;
	int n = 0;
	while (*p) {
		while (*p == '/') p++;
		if (!*p) break;
		const char *start = p;
		while (*p && *p != '/') p++;
		if (n == idx) { *outlen = (uint32_t)(p - start); return start; }
		n++;
	}
	return 0;
}

/* Does FDT node `nodename` (may carry an @unit-address suffix) equal path
 * component [comp, comp+clen)? The unit-address is ignored. */
static int node_matches(const char *nodename, const char *comp, uint32_t clen)
{
	uint32_t i = 0;
	for (; i < clen; i++) {
		if (nodename[i] == '\0' || nodename[i] == '@' || nodename[i] != comp[i])
			return 0;
	}
	/* full component consumed; node must end here or start its @unit-address */
	return nodename[i] == '\0' || nodename[i] == '@';
}

/*
 * Walk the struct block tracking node depth (root = 1) and how many leading
 * path components have matched along the CURRENT branch (`matched`). A node at
 * depth d corresponds to path component index d-2 (root is depth 1, not a
 * component). We're inside the target node exactly when matched == ncomp and
 * depth == ncomp+1. On END_NODE we reduce `matched` so it never claims a node
 * we've left: leaving depth d caps matched at d-2. Component matching ignores
 * a node's @unit-address, so "/memory" matches "memory@40000000".
 */
const void *kfdt_getprop(const void *dtb, const char *path, const char *name,
                         uint32_t *len_out)
{
	const uint8_t *d = (const uint8_t *)dtb;
	if (!kfdt_valid(d))
		return 0;
	uint32_t off_struct  = be32(d + 8);
	uint32_t off_strings = be32(d + 12);
	const char *strings  = (const char *)(d + off_strings);
	int ncomp = path_ncomp(path);

	int depth = 0;                          /* current node depth (root = 1) */
	int matched = 0;                        /* leading components matched */

	/* The struct block always precedes the strings block; never read past it,
	 * so a truncated/malformed blob can't walk into unmapped memory. */
	uint32_t p = off_struct;
	uint32_t limit = off_strings;
	for (;;) {
		if (p + 4 > limit)
			break;                          /* ran off the struct block: give up */
		uint32_t tok = be32(d + p); p += 4;
		if (tok == FDT_END)
			break;
		if (tok == FDT_NOP)
			continue;

		if (tok == FDT_BEGIN_NODE) {
			const char *nodename = (const char *)(d + p);
			uint32_t nl = slen(nodename) + 1;
			p += (nl + 3) & ~3u;
			depth++;
			if (depth >= 2) {
				int ci = depth - 2;         /* this node's component index */
				if (matched == ci && ci < ncomp) {
					uint32_t clen;
					const char *comp = path_comp(path, ci, &clen);
					if (comp && node_matches(nodename, comp, clen))
						matched = ci + 1;
				}
			}
			continue;
		}

		if (tok == FDT_END_NODE) {
			if (matched > depth - 2)
				matched = depth - 2;        /* left this node: cap the prefix */
			if (matched < 0)
				matched = 0;
			depth--;
			continue;
		}

		if (tok == FDT_PROP) {
			if (p + 8 > limit)
				break;
			uint32_t len  = be32(d + p);
			uint32_t noff = be32(d + p + 4);
			p += 8;
			/* Reject a bogus len that would run past the struct block or wrap the
			 * pad step (len+3) — malformed blob; give up rather than over-read. */
			if (len > limit - p || noff >= be32(d + 4))
				break;
			const char *pname = strings + noff;
			const uint8_t *pval = d + p;
			p += (len + 3) & ~3u;

			if (matched == ncomp && depth == ncomp + 1 && streq(pname, name)) {
				if (len_out) *len_out = len;
				return pval;
			}
			continue;
		}

		return 0;                           /* unknown token: bail safely */
	}
	return 0;
}

/* parse an unsigned integer from `s` (hex if 0x-prefixed, else decimal); stops
 * at the first non-digit. writes the value; returns the char after the number. */
static const char *parse_uint(const char *s, uint32_t *out)
{
	uint32_t v = 0;
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		s += 2;
		for (;;) {
			char c = *s;
			uint32_t d;
			if (c >= '0' && c <= '9') d = c - '0';
			else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
			else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
			else break;
			v = v * 16 + d; s++;
		}
	} else {
		while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
	}
	*out = v;
	return s;
}

int kfdt_initrd(const void *dtb, uint32_t *base_out, uint32_t *size_out)
{
	/* Mechanism (a): /chosen/linux,initrd-start + linux,initrd-end (what QEMU
	 * writes, and what a device-tree bootloader normally uses). Each is a scalar
	 * cell = a single big-endian u32 address on our 32-bit target. Preferred. */
	uint32_t sl = 0, el = 0;
	const uint8_t *s = kfdt_getprop(dtb, "/chosen", "linux,initrd-start", &sl);
	const uint8_t *e = kfdt_getprop(dtb, "/chosen", "linux,initrd-end", &el);
	if (s && e && sl >= 4 && el >= 4) {
		uint32_t start = be32(s), end = be32(e);
		if (end > start) {
			if (base_out) *base_out = start;
			if (size_out) *size_out = end - start;
			return 0;
		}
	}

	/* Mechanism (b): initrd=<hexaddr>,<decsize> in /chosen/bootargs (what our
	 * T113 bootloader writes). */
	uint32_t len = 0;
	const char *args = kfdt_getprop(dtb, "/chosen", "bootargs", &len);
	if (!args || len == 0)
		return -1;

	/* find "initrd=" as a whole token (start of string or preceded by space).
	 * Bound the scan by `len` (the property value length) so a value that isn't
	 * NUL-terminated within the blob can't over-read. Need 8 bytes for "initrd=". */
	for (uint32_t i = 0; i + 7 < len && args[i]; i++) {
		const char *s = args + i;
		if ((i == 0 || s[-1] == ' ') &&
		    s[0]=='i'&&s[1]=='n'&&s[2]=='i'&&s[3]=='t'&&s[4]=='r'&&s[5]=='d'&&s[6]=='=') {
			const char *v = s + 7;
			uint32_t base, size;
			v = parse_uint(v, &base);
			if (*v != ',') return -1;
			v++;
			parse_uint(v, &size);
			if (base_out) *base_out = base;
			if (size_out) *size_out = size;
			return 0;
		}
	}
	return -1;
}

/* Read a u32 property from the ROOT node ("/"), defaulting if absent. Used for
 * #address-cells / #size-cells, which govern how /memory's `reg` is laid out. */
static uint32_t root_u32(const void *dtb, const char *name, uint32_t dflt)
{
	uint32_t len = 0;
	const uint8_t *p = kfdt_getprop(dtb, "/", name, &len);
	return (p && len >= 4) ? be32(p) : dflt;
}

int kfdt_memory(const void *dtb, uint32_t *base_out, uint32_t *size_out)
{
	/* /memory reg = <address(#address-cells)><size(#size-cells)>. The T113 DTB
	 * uses 1/1 (8 bytes); QEMU virt uses 2/2 (16 bytes, 64-bit base+size). We're
	 * a 32-bit kernel, so take the LOW 32 bits of each field (the high cell is 0
	 * for our sub-4GB RAM). Cell counts are inherited from the parent (root). */
	uint32_t ac = root_u32(dtb, "#address-cells", 1);
	uint32_t sc = root_u32(dtb, "#size-cells", 1);
	if (ac < 1 || ac > 2 || sc < 1 || sc > 2)
		return -1;                         /* unexpected cell sizes */

	uint32_t len = 0;
	const uint8_t *reg = kfdt_getprop(dtb, "/memory", "reg", &len);
	if (!reg || len < (ac + sc) * 4)
		return -1;

	/* low word of the address field = reg[(ac-1)] ; low word of size follows. */
	uint32_t base = be32(reg + (ac - 1) * 4);
	uint32_t size = be32(reg + ac * 4 + (sc - 1) * 4);
	if (base_out) *base_out = base;
	if (size_out) *size_out = size;
	return 0;
}
