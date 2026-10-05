/* common/arread.c — see arread.h. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arread.h"

static long field(const char *h, int at, int len) { char b[16]; memcpy(b, h + at, len); b[len] = 0; return strtol(b, NULL, 10); }
static int special(const char *h) { return !memcmp(h, "/ ", 2) || !memcmp(h, "//", 2); }   /* the index or the long names */
/* A header's bytes IN the archive: a thin archive stores only its index and long-name table. */
static long stored(const Archive *ar, const char *h) { return !ar->thin || special(h) ? field(h, 48, 10) : 0; }

int ar_is_archive(const u8 *d, long size) { return size >= 8 && (!memcmp(d, "!<arch>\n", 8) || !memcmp(d, "!<thin>\n", 8)); }

void ar_open(Archive *ar, const char *path, u8 *data, long size) {
	memset(ar, 0, sizeof *ar);
	if (!ar_is_archive(data, size)) die("%s: not an archive", path);
	ar->path = path; ar->data = data; ar->size = size; ar->thin = !memcmp(data, "!<thin>\n", 8);
	for (long p = 8; p < size; ) {
		if (p + 60 > size) die("%s: truncated archive member header at %ld", path, p);
		const char *h = (const char *)(data + p);
		long n = stored(ar, h);
		if (memcmp(h + 58, "`\n", 2) || field(h, 48, 10) < 0 || n > size - p - 60) die("%s: corrupt archive member header at %ld", path, p);
		if (!memcmp(h, "/ ", 2)) { ar->index = data + p + 60; ar->index_size = n; }
		else if (!memcmp(h, "//", 2)) { ar->longtab = (const char *)(data + p + 60); ar->longsz = n; }
		else if (!memcmp(h, "/SYM64/", 7) || !memcmp(h, "__.SYMDEF", 9)) die("%s: unsupported archive index format", path);
		p += 60 + n + (n & 1);
	}
}

int ar_next(const Archive *ar, long *pos, ArMember *m) {
	for (;;) {
		long p = *pos;
		if (p >= ar->size) return 0;
		const char *h = (const char *)(ar->data + p);
		long n = stored(ar, h);
		*pos = p + 60 + n + (n & 1);
		if (special(h)) continue;
		int k = 0;
		if (h[0] == '/' && h[1] >= '0' && h[1] <= '9') {   /* "/off": a long name, ending at "/\n" (a thin member's path has '/'s) */
			long at = atol(h + 1);
			if (!ar->longtab || at < 0 || at >= ar->longsz) die("%s: member name outside the // table", ar->path);
			const char *nm = ar->longtab + at;
			while (k < (int)sizeof m->name - 1 && at + k < ar->longsz && nm[k] != '\n'
			       && !(nm[k] == '/' && (at + k + 1 >= ar->longsz || nm[k + 1] == '\n'))) { m->name[k] = nm[k]; k++; }
		} else while (k < 16 && h[k] != '/' && h[k] != ' ') { m->name[k] = h[k]; k++; }
		m->name[k] = 0;
		m->off = p; m->size = field(h, 48, 10);
		m->data = ar->thin ? NULL : ar->data + p + 60;
		return 1;
	}
}

char *ar_member_path(const Archive *ar, const ArMember *m) {
	const char *slash = strrchr(ar->path, '/');
	char *fp = malloc(strlen(ar->path) + strlen(m->name) + 2);
	if (m->name[0] == '/' || !slash) strcpy(fp, m->name); else sprintf(fp, "%.*s/%s", (int)(slash - ar->path), ar->path, m->name);
	return fp;
}
