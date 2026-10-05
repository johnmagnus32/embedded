/*
 * buildid.c — --build-id: the .note.gnu.build-id note (NT_GNU_BUILD_ID, owner "GNU"), a linker-made section placed like
 * any input (the default script and the kernel's put it with the other notes). Its descriptor is computed once the
 * whole output is written, the way GNU ld computes it (bfd's _bfd_elf_checksum_contents): a hash over the ELF header
 * with e_phoff and e_shoff zeroed, every program header, and for each section its header with sh_offset zeroed, then
 * its bytes (none for NOBITS) — the note's own descriptor still zero. Styles: sha1 (the default), md5, uuid (random),
 * 0xHEX (those bytes), none.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ld.h"

const char *build_id_style;                      /* NULL: none */

/* ---- SHA-1 (FIPS 180-4) and MD5 (RFC 1321), streaming --------------------------------------------- */
typedef struct { u32 h[5]; unsigned long long len; u8 buf[64]; int n; int md5; } Hash;
static u32 rol(u32 x, int k) { return x << k | x >> (32 - k); }
static void sha1_block(u32 *h, const u8 *p) {
	u32 w[80];
	for (int i = 0; i < 16; i++) w[i] = (u32)p[4 * i] << 24 | (u32)p[4 * i + 1] << 16 | (u32)p[4 * i + 2] << 8 | p[4 * i + 3];
	for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
	u32 a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
	for (int i = 0; i < 80; i++) {
		u32 f, k;
		if (i < 20) { f = (b & c) | (~b & d); k = 0x5a827999; }
		else if (i < 40) { f = b ^ c ^ d; k = 0x6ed9eba1; }
		else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdc; }
		else { f = b ^ c ^ d; k = 0xca62c1d6; }
		u32 t = rol(a, 5) + f + e + k + w[i]; e = d; d = c; c = rol(b, 30); b = a; a = t;
	}
	h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}
static void md5_block(u32 *h, const u8 *p) {
	static const u32 K[64] = {
		0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,
		0x6b901122,0xfd987193,0xa679438e,0x49b40821,0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
		0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,
		0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
		0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,
		0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391 };
	static const int R[64] = { 7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22, 5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
	                           4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23, 6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21 };
	u32 m[16]; for (int i = 0; i < 16; i++) m[i] = rd32(p + 4 * i);
	u32 a = h[0], b = h[1], c = h[2], d = h[3];
	for (int i = 0; i < 64; i++) {
		u32 f; int g;
		if (i < 16) { f = (b & c) | (~b & d); g = i; }
		else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
		else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) & 15; }
		else { f = c ^ (b | ~d); g = (7 * i) & 15; }
		u32 t = d; d = c; c = b; b = b + rol(a + f + K[i] + m[g], R[i]); a = t;
	}
	h[0] += a; h[1] += b; h[2] += c; h[3] += d;
}
static void h_init(Hash *x, int md5) {
	memset(x, 0, sizeof *x); x->md5 = md5;
	static const u32 s[5] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 };
	memcpy(x->h, s, sizeof s);
}
static void h_add(Hash *x, const void *v, size_t n) {
	const u8 *p = v; x->len += n;
	while (n--) { x->buf[x->n++] = *p++; if (x->n == 64) { (x->md5 ? md5_block : sha1_block)(x->h, x->buf); x->n = 0; } }
}
static void h_end(Hash *x, u8 *out) {
	unsigned long long bits = x->len * 8; u8 one = 0x80, zero = 0, L[8];
	h_add(x, &one, 1); while (x->n != 56) h_add(x, &zero, 1);
	for (int i = 0; i < 8; i++) L[i] = x->md5 ? (u8)(bits >> (8 * i)) : (u8)(bits >> (56 - 8 * i));   /* MD5 little-, SHA-1 big-endian */
	h_add(x, L, 8);
	for (int i = 0; i < (x->md5 ? 4 : 5); i++)
		for (int k = 0; k < 4; k++) out[4 * i + k] = x->md5 ? (u8)(x->h[i] >> (8 * k)) : (u8)(x->h[i] >> (24 - 8 * k));
}

/* ---- the note ------------------------------------------------------------------------------------- */
static int hexval(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
u32 build_id_size(void) {   /* the descriptor's bytes for the style (0: no note) */
	const char *s = build_id_style;
	if (!s || !strcmp(s, "none")) return 0;
	if (!strcmp(s, "sha1")) return 20;
	if (!strcmp(s, "md5") || !strcmp(s, "uuid")) return 16;
	if (!strncmp(s, "0x", 2)) {
		size_t n = strlen(s + 2); if (!n || n & 1) die("--build-id=%s: an even number of hex digits", s);
		for (size_t i = 0; i < n; i++) if (hexval(s[2 + i]) < 0 || s[2 + i] == '-') die("--build-id=%s: not hex", s);
		return (u32)(n / 2);
	}
	die("--build-id=%s: sha1, md5, uuid, 0xHEX or none", s);
}
void build_id_note(u8 *p, u32 descsz) {   /* namesz, descsz, type, "GNU\0", then the descriptor (zero until build_id_write) */
	wr32(p, 4); wr32(p + 4, descsz); wr32(p + 8, 3 /* NT_GNU_BUILD_ID */); memcpy(p + 12, "GNU", 4); memset(p + 16, 0, descsz);
}
void build_id_write(const char *path, u32 note_off) {   /* note_off: the note's file offset */
	u32 n = build_id_size(); u8 id[20];
	FILE *f = fopen(path, "r+b"); if (!f) die("%s: cannot reopen for the build-id", path);
	fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
	u8 *d = malloc(size); if (fread(d, 1, size, f) != (size_t)size) die("%s: read failed", path);
	if (!strncmp(build_id_style, "0x", 2)) for (u32 i = 0; i < n; i++) id[i] = (u8)(hexval(build_id_style[2 + 2 * i]) << 4 | hexval(build_id_style[3 + 2 * i]));
	else if (!strcmp(build_id_style, "uuid")) {
		FILE *r = fopen("/dev/urandom", "rb"); if (!r || fread(id, 1, 16, r) != 16) die("--build-id=uuid: no /dev/urandom");
		fclose(r); id[6] = (id[6] & 0x0f) | 0x40; id[8] = (id[8] & 0x3f) | 0x80;   /* RFC 4122 version 4 (libiberty's) */
	} else {
		Hash h; h_init(&h, !strcmp(build_id_style, "md5"));
		Elf32_Ehdr eh; memcpy(&eh, d, sizeof eh); eh.e_phoff = eh.e_shoff = 0; h_add(&h, &eh, sizeof eh);
		const Elf32_Ehdr *e = (const Elf32_Ehdr *)d;
		h_add(&h, d + e->e_phoff, (size_t)e->e_phnum * sizeof(Elf32_Phdr));
		for (int i = 0; i < e->e_shnum; i++) {
			Elf32_Shdr s; memcpy(&s, d + e->e_shoff + i * sizeof s, sizeof s);
			u32 off = s.sh_offset; s.sh_offset = 0; h_add(&h, &s, sizeof s);
			if (s.sh_type != SHT_NOBITS) h_add(&h, d + off, s.sh_size);
		}
		h_end(&h, id);
	}
	if (fseek(f, note_off + 16, SEEK_SET) || fwrite(id, 1, n, f) != n || fclose(f)) die("%s: writing the build-id failed", path);
	free(d);
}
