/*
 * inflate.h — in-kernel gunzip: DEFLATE (RFC 1951) + gzip wrapper (RFC 1952).
 *
 * This is what lets the kernel consume the bootloader's REAL initramfs, which
 * is `initramfs.cpio.gz`. Linux does the same: init/initramfs.c feeds the
 * compressed image through a built-in decompressor (lib/decompress_inflate.c,
 * derived from zlib inflate) before the cpio parser runs. Ours is a puff-style
 * inflater — it writes decoded bytes straight into the output buffer, so LZ77
 * back-references just index earlier output (no separate 32 KB window).
 *
 * Scope: single-shot, whole-buffer in / whole-buffer out (the initramfs fits in
 * RAM). No streaming. gzip container only (magic 1f 8b, CM=8).
 */
#ifndef GV3K_INFLATE_H
#define GV3K_INFLATE_H

#include <stdint.h>

/* Raw DEFLATE: decode `srclen` bytes at `src` into `dst` (capacity `dstcap`).
 * Returns the number of output bytes, or negative on error. */
long inflate_raw(const uint8_t *src, uint32_t srclen, uint8_t *dst, uint32_t dstcap);

/* gzip: validate the header/trailer, inflate the enclosed DEFLATE stream into
 * `dst`. Returns output bytes (should equal the trailer ISIZE) or negative. */
long gunzip(const uint8_t *src, uint32_t srclen, uint8_t *dst, uint32_t dstcap);

/* True if `src` (>=2 bytes) begins with the gzip magic 0x1f 0x8b. */
int is_gzip(const uint8_t *src, uint32_t srclen);

/* Read the gzip trailer's ISIZE (uncompressed size mod 2^32, LE) — used to size
 * the output buffer before calling gunzip(). 0 if srclen < 4. */
uint32_t gzip_isize(const uint8_t *src, uint32_t srclen);

#endif /* GV3K_INFLATE_H */
