/* common/elfutil.c — see elfutil.h. Format-level helpers shared across the toolchain. */
#include <stdlib.h>
#include <string.h>
#include "elfutil.h"

u32  rd32(const u8 *p) { return p[0] | p[1]<<8 | p[2]<<16 | (u32)p[3]<<24; }
void wr32(u8 *p, u32 v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }
u32  alignup(u32 x, u32 a) { return a > 1 ? (x + a - 1) & ~(a - 1) : x; }

u32 str_add(Strtab *s, const char *name) {
	if (!name) name = "";   /* always WRITE (incl. the leading "" at offset 0) — a strtab must start with \0
	                           so the null section/symbol (sh_name/st_name = 0) reads as the empty string. */
	u32 off = s->len; size_t n = strlen(name) + 1;
	if (s->len + n > s->cap) { s->cap = (s->len + n) * 2 + 64; s->b = realloc(s->b, s->cap); }
	memcpy(s->b + s->len, name, n); s->len += n; return off;
}
