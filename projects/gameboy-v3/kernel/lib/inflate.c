/*
 * inflate.c — DEFLATE (RFC 1951) inflater + gzip (RFC 1952) wrapper.
 *
 * Puff-style: decode straight into the output buffer, so LZ77 back-references
 * index earlier output (no separate window). Whole-buffer in / whole-buffer out.
 *
 * Bit conventions (the #1 source of bugs, verified vs RFC 1951 §3.1.1):
 *   - data elements (LEN, extra bits, the HLIT/HDIST/HCLEN counts) are packed
 *     LSB-first within each byte;
 *   - Huffman codes are packed MSB-first (read one bit at a time, appending
 *     each new bit as the LOW bit of the accumulated code).
 * All gzip container integers are little-endian.
 */

#include <stdint.h>
#include "inflate.h"

/* ---- bit reader --------------------------------------------------------- */
struct bitr {
	const uint8_t *src;
	uint32_t len;       /* total bytes */
	uint32_t pos;       /* next byte index */
	uint32_t bitbuf;    /* bits held (LSB = next to consume) */
	uint32_t bitcnt;    /* how many valid bits in bitbuf */
	int err;
};

/* return `n` bits (n<=24), LSB-first (RFC data-element order). */
static uint32_t bits(struct bitr *b, int n)
{
	while (b->bitcnt < (uint32_t)n) {
		if (b->pos >= b->len) { b->err = 1; return 0; }
		b->bitbuf |= (uint32_t)b->src[b->pos++] << b->bitcnt;
		b->bitcnt += 8;
	}
	uint32_t v = b->bitbuf & ((1u << n) - 1);
	b->bitbuf >>= n;
	b->bitcnt -= n;
	return v;
}

/* ---- canonical Huffman decode table ------------------------------------- *
 * Represent a code set by count[] (number of codes of each length) and a
 * symbol[] array sorted by (length, symbol) — the puff.c representation. */
#define MAXBITS 15
struct huff { int16_t count[MAXBITS + 1]; int16_t symbol[288]; };

/* build a huff table from an array of code lengths (length[0..n-1]). */
static int huff_build(struct huff *h, const uint8_t *length, int n)
{
	for (int l = 0; l <= MAXBITS; l++) h->count[l] = 0;
	for (int s = 0; s < n; s++) h->count[length[s]]++;
	if (h->count[0] == n) return 0;        /* no codes at all (all zero) — ok */

	/* check for an over-subscribed or incomplete set */
	int left = 1;
	for (int l = 1; l <= MAXBITS; l++) {
		left <<= 1;
		left -= h->count[l];
		if (left < 0) return -1;           /* over-subscribed */
	}
	/* left > 0 => incomplete; allowed only for the single-code edge case, the
	 * caller (fixed/dynamic) tolerates it as puff does. */

	int16_t offs[MAXBITS + 1];
	offs[1] = 0;
	for (int l = 1; l < MAXBITS; l++)
		offs[l + 1] = offs[l] + h->count[l];
	for (int s = 0; s < n; s++)
		if (length[s] != 0)
			h->symbol[offs[length[s]]++] = (int16_t)s;
	return left;                           /* 0 = complete, >0 = incomplete */
}

/* decode one symbol using the MSB-first Huffman convention. */
static int huff_decode(struct bitr *b, const struct huff *h)
{
	int code = 0, first = 0, index = 0;
	for (int len = 1; len <= MAXBITS; len++) {
		code |= (int)bits(b, 1);           /* append next bit as the low bit */
		int cnt = h->count[len];
		if (code - first < cnt)            /* within this length's range? */
			return h->symbol[index + (code - first)];
		index += cnt;
		first += cnt;
		first <<= 1;
		code <<= 1;
	}
	return -1;                             /* ran out of bits / bad code */
}

/* ---- length/distance base + extra tables (RFC 1951 §3.2.5, verified) ---- */
static const uint16_t len_base[29] = {
	3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 };
static const uint8_t len_extra[29] = {
	0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
static const uint16_t dist_base[30] = {
	1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,
	1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
static const uint8_t dist_extra[30] = {
	0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };

/* ---- decode one block's compressed data given lit/len + dist tables ----- */
static long inflate_block(struct bitr *b, const struct huff *lh, const struct huff *dh,
                          uint8_t *dst, uint32_t dstcap, uint32_t out)
{
	for (;;) {
		int sym = huff_decode(b, lh);
		if (b->err || sym < 0) return -1;
		if (sym == 256)                    /* end of block */
			return (long)out;
		if (sym < 256) {                   /* literal byte */
			if (out >= dstcap) return -2;
			dst[out++] = (uint8_t)sym;
			continue;
		}
		/* length code 257..285 */
		sym -= 257;
		if (sym >= 29) return -3;          /* 286/287 invalid */
		uint32_t length = len_base[sym] + bits(b, len_extra[sym]);

		int dsym = huff_decode(b, dh);
		if (b->err || dsym < 0 || dsym >= 30) return -4;
		uint32_t dist = dist_base[dsym] + bits(b, dist_extra[dsym]);
		if (dist > out) return -5;         /* refers before start of output */
		if (out + length > dstcap) return -6;

		/* copy `length` bytes from `dist` behind the current output position;
		 * overlapping copies are intentional (RLE-style), so copy byte-by-byte. */
		uint32_t from = out - dist;
		for (uint32_t i = 0; i < length; i++)
			dst[out + i] = dst[from + i];
		out += length;
	}
}

/* ---- fixed + dynamic table construction --------------------------------- */
static void build_fixed(struct huff *lh, struct huff *dh)
{
	uint8_t ll[288], dl[30];
	int i = 0;
	for (; i < 144; i++) ll[i] = 8;
	for (; i < 256; i++) ll[i] = 9;
	for (; i < 280; i++) ll[i] = 7;
	for (; i < 288; i++) ll[i] = 8;
	for (i = 0; i < 30; i++) dl[i] = 5;
	huff_build(lh, ll, 288);
	huff_build(dh, dl, 30);
}

/* HCLEN code-length order (RFC 1951 §3.2.7, verified permutation). */
static const uint8_t clc_order[19] = {
	16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };

static int build_dynamic(struct bitr *b, struct huff *lh, struct huff *dh)
{
	int hlit  = (int)bits(b, 5) + 257;
	int hdist = (int)bits(b, 5) + 1;
	int hclen = (int)bits(b, 4) + 4;
	if (hlit > 286 || hdist > 30) return -1;

	uint8_t clc_len[19] = {0};
	for (int i = 0; i < hclen; i++)
		clc_len[clc_order[i]] = (uint8_t)bits(b, 3);
	struct huff clc;
	if (huff_build(&clc, clc_len, 19) < 0) return -2;

	/* decode (hlit + hdist) code lengths, handling repeat codes 16/17/18. */
	uint8_t lengths[288 + 30] = {0};
	int n = 0, total = hlit + hdist;
	while (n < total) {
		int sym = huff_decode(b, &clc);
		if (b->err || sym < 0) return -3;
		if (sym < 16) {
			lengths[n++] = (uint8_t)sym;
		} else if (sym == 16) {
			if (n == 0) return -4;         /* nothing to repeat */
			int rep = 3 + (int)bits(b, 2);
			uint8_t prev = lengths[n - 1];
			while (rep-- && n < total) lengths[n++] = prev;
		} else if (sym == 17) {
			int rep = 3 + (int)bits(b, 3);
			while (rep-- && n < total) lengths[n++] = 0;
		} else {                           /* sym == 18 */
			int rep = 11 + (int)bits(b, 7);
			while (rep-- && n < total) lengths[n++] = 0;
		}
	}
	if (lengths[256] == 0) return -5;      /* end-of-block must be codeable */

	if (huff_build(lh, lengths, hlit) < 0) return -6;
	if (huff_build(dh, lengths + hlit, hdist) < 0) return -7;
	return 0;
}

/* ---- top-level raw DEFLATE ---------------------------------------------- */
long inflate_raw(const uint8_t *src, uint32_t srclen, uint8_t *dst, uint32_t dstcap)
{
	struct bitr b = { src, srclen, 0, 0, 0, 0 };
	uint32_t out = 0;
	int final;

	do {
		final = (int)bits(&b, 1);
		int type = (int)bits(&b, 2);
		if (b.err) return -10;

		if (type == 0) {                   /* stored */
			b.bitbuf = 0; b.bitcnt = 0;    /* align to byte boundary */
			if (b.pos + 4 > b.len) return -11;
			uint32_t len  = b.src[b.pos] | (b.src[b.pos + 1] << 8);
			uint32_t nlen = b.src[b.pos + 2] | (b.src[b.pos + 3] << 8);
			b.pos += 4;
			if ((len ^ 0xFFFF) != nlen) return -12;
			if (b.pos + len > b.len) return -13;
			if (out + len > dstcap) return -14;
			for (uint32_t i = 0; i < len; i++) dst[out++] = b.src[b.pos++];
		} else if (type == 1 || type == 2) {
			struct huff lh, dh;
			if (type == 1) {
				build_fixed(&lh, &dh);
			} else {
				if (build_dynamic(&b, &lh, &dh) != 0) return -15;
			}
			long r = inflate_block(&b, &lh, &dh, dst, dstcap, out);
			if (r < 0) return r;
			out = (uint32_t)r;
		} else {
			return -16;                    /* BTYPE=11 reserved */
		}
	} while (!final);

	return (long)out;
}

/* ---- CRC32 (gzip/zlib polynomial 0xEDB88320, reflected) ----------------- */
static uint32_t crc32(const uint8_t *p, uint32_t n)
{
	uint32_t crc = 0xFFFFFFFFu;
	for (uint32_t i = 0; i < n; i++) {
		crc ^= p[i];
		for (int k = 0; k < 8; k++)
			crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1)));
	}
	return crc ^ 0xFFFFFFFFu;
}

/* ---- gzip container (RFC 1952) ------------------------------------------ */
int is_gzip(const uint8_t *src, uint32_t srclen)
{
	return srclen >= 2 && src[0] == 0x1f && src[1] == 0x8b;
}

uint32_t gzip_isize(const uint8_t *src, uint32_t srclen)
{
	if (srclen < 4) return 0;
	const uint8_t *t = src + srclen - 4;    /* last 4 bytes = ISIZE (LE) */
	return t[0] | (t[1] << 8) | (t[2] << 16) | ((uint32_t)t[3] << 24);
}

long gunzip(const uint8_t *src, uint32_t srclen, uint8_t *dst, uint32_t dstcap)
{
	if (!is_gzip(src, srclen) || srclen < 18)   /* 10 hdr + min + 8 trailer */
		return -20;
	if (src[2] != 8)                        /* CM must be deflate */
		return -21;
	uint8_t flg = src[3];
	uint32_t p = 10;                        /* past the fixed 10-byte header */

	if (flg & 0x04) {                       /* FEXTRA: XLEN (LE) + XLEN bytes */
		if (p + 2 > srclen) return -22;
		uint32_t xlen = src[p] | (src[p + 1] << 8);
		p += 2 + xlen;
	}
	if (flg & 0x08)                         /* FNAME: NUL-terminated */
		while (p < srclen && src[p++]) ;
	if (flg & 0x10)                         /* FCOMMENT: NUL-terminated */
		while (p < srclen && src[p++]) ;
	if (flg & 0x02)                         /* FHCRC: 2 bytes */
		p += 2;
	if (p + 8 > srclen)                     /* need room for the trailer */
		return -23;

	/* DEFLATE payload is [p, srclen-8); the last 8 bytes are CRC32 + ISIZE. */
	long n = inflate_raw(src + p, srclen - 8 - p, dst, dstcap);
	if (n < 0)
		return n;

	/* sanity: output length should match the trailer ISIZE (mod 2^32). */
	uint32_t isize = gzip_isize(src, srclen);
	if ((uint32_t)n != isize)
		return -24;

	/* integrity: verify the CRC32 of the decompressed data against the trailer
	 * (bytes srclen-8..srclen-4, LE). Catches corruption a length-only check
	 * would miss — the same check Linux's decompressor does. */
	const uint8_t *tc = src + srclen - 8;
	uint32_t stored_crc = tc[0] | (tc[1] << 8) | (tc[2] << 16) | ((uint32_t)tc[3] << 24);
	if (crc32(dst, (uint32_t)n) != stored_crc)
		return -25;
	return n;
}
