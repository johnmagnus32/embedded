/*
 * ar.c — our from-scratch archiver, the GNU `ar` equivalent, byte for byte. An archive (.a) is a container: a magic
 * line, then members — each a 60-byte header (name, date, uid, gid, mode, size) + its bytes, padded to even length.
 * The GNU/SysV variant:
 *   "!<arch>\n"
 *   member "/"   — the symbol index: for every symbol a member DEFINES (global, weak, common), the offset of that
 *                  member's header, so a linker finds "which member defines foo" without reading them all.
 *                  Big-endian count + offsets, then the names.
 *   member "//"  — long names: each "name/\n"; a member whose name is longer than 15 bytes is called "/offset".
 *   members ...
 * A THIN archive ("!<thin>\n", modifier T) stores only those two: each member is a file of its own, named — always
 * through "//" — by its path relative to the archive's directory. A thin archive given as a member is flattened into
 * its members (the kernel's built-in.a of built-in.a's).
 *
 * Run as `ranlib` — argv[0] ending in "ranlib" — it rewrites each named archive's symbol index and nothing else,
 * the same work as `ar s`, which is all GNU's ranlib is (musl's build and the autotools ones call it by that name).
 *
 * Usage: ar [-]{d|m|p|q|r|s|t|x}[abcDiPsSTuUv] [relpos] archive [member...]   — GNU's keys and modifiers:
 *   r replace or add (after/before relpos with a / b|i; u: only if newer), q append, d delete, m move, t list,
 *   x extract, p print, s just index; c quietly create; D deterministic (date/uid/gid 0, mode 644) — U (the default
 *   here, as GNU's build) records each file's mtime/uid/gid/mode and the index's time; P full path names; S no index;
 *   s an index (the default unless S); T thin. Members that are not ELF relocatables are archived, not indexed.
 */
#define _XOPEN_SOURCE 700   /* realpath */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>
#include "elfread.h"
#include "arread.h"

static const char *tool = "ar";
void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fprintf(stderr, "%s: ", tool); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}

typedef struct {
	char *name;                 /* as stored (thin: the path relative to the archive's directory) */
	char *path;                 /* thin: its file (for reading it); else NULL */
	u8 *data; long size;        /* regular: its bytes (thin: read only to index it) */
	long date; long uid, gid; unsigned mode;
	int fresh;                  /* added by THIS command: a later file on the same line does not replace it */
} Mem;
static Mem *mem; static int nmem, memcap;
static int key, verbose, quiet_create, deterministic, full_path, no_index, want_index, thin, update, pos_mode;   /* pos_mode: 'a' or 'b' */
static const char *relpos;

static void *grow(void *v, int n, int *cap, size_t esz) {
	if (n < *cap) return v;
	*cap = *cap ? *cap * 2 : 64;
	if (!(v = realloc(v, (size_t)*cap * esz))) die("out of memory");
	return v;
}
static u8 *slurp(const char *path, long *size) {
	FILE *f = fopen(path, "rb"); if (!f) die("%s: No such file or directory", path);
	fseek(f, 0, SEEK_END); *size = ftell(f); fseek(f, 0, SEEK_SET);
	u8 *b = malloc(*size ? *size : 1); if (fread(b, 1, *size, f) != (size_t)*size) die("%s: read failed", path);
	fclose(f); return b;
}
static const char *base_of(const char *p) { const char *s = strrchr(p, '/'); return s ? s + 1 : p; }

/* bfd's adjust_relative_path: `path` relative to the directory of `ref` (both resolved, as lrealpath does). */
static char *relative_to(const char *path, const char *ref) {
	char lp[PATH_MAX], rp[PATH_MAX + 256], dir[PATH_MAX];   /* (the archive itself may not exist yet: resolve its directory) */
	const char *p = realpath(path, lp) ? lp : path, *slash = strrchr(ref, '/'), *r = ref;
	snprintf(dir, sizeof dir, "%.*s", slash ? (int)(slash - ref) : 1, slash ? ref : ".");
	char rd[PATH_MAX]; if (realpath(slash && slash == ref ? "/" : dir, rd)) { snprintf(rp, sizeof rp, "%s/%s", strcmp(rd, "/") ? rd : "", slash ? slash + 1 : ref); r = rp; }
	for (;;) {   /* drop the common leading directories */
		const char *e1 = p, *e2 = r;
		while (*e1 && *e1 != '/') e1++;
		while (*e2 && *e2 != '/') e2++;
		if (!*e1 || !*e2 || e1 - p != e2 - r || strncmp(p, r, e1 - p)) break;
		p = e1 + 1; r = e2 + 1;
	}
	int up = 0; for (; *r; r++) if (*r == '/') up++;
	char *out = malloc(3 * up + strlen(p) + 1), *o = out;
	while (up--) { memcpy(o, "../", 3); o += 3; }
	strcpy(o, p);
	return out;
}

/* A member's header fields from its file, as GNU records them (D: deterministic zeros). */
static void stamp(Mem *m, const char *file) {
	struct stat st;
	if (deterministic || stat(file, &st)) { m->date = 0; m->uid = m->gid = 0; m->mode = 0644; return; }
	m->date = st.st_mtime; m->uid = st.st_uid; m->gid = st.st_gid; m->mode = st.st_mode;
}

static long field(const char *h, int at, int len, int base) { char b[16]; memcpy(b, h + at, len); b[len] = 0; return strtol(b, NULL, base); }
static void load_archive(const char *path, int *was_thin) {
	long size; u8 *d = slurp(path, &size);
	Archive ar; ar_open(&ar, path, d, size);
	*was_thin = ar.thin;
	ArMember am; long pos = 8;
	while (ar_next(&ar, &pos, &am)) {
		mem = grow(mem, nmem, &memcap, sizeof *mem);
		Mem *m = &mem[nmem++]; memset(m, 0, sizeof *m);
		const char *h = (const char *)ar.data + am.off;
		m->name = strdup(am.name); m->size = am.size;
		m->date = field(h, 16, 12, 10); m->uid = field(h, 28, 6, 10); m->gid = field(h, 34, 6, 10); m->mode = (unsigned)field(h, 40, 8, 8);
		if (ar.thin) m->path = ar_member_path(&ar, &am);
		else { m->data = malloc(am.size ? am.size : 1); memcpy(m->data, am.data, am.size); }
	}
}

static int find(const char *name) {   /* a member by name: the full stored name with P (and in a thin archive), else the basename */
	for (int i = 0; i < nmem; i++) {
		if (full_path || thin ? !strcmp(mem[i].name, name) : !strcmp(mem[i].name, base_of(name))) return i;
	}
	return -1;
}
/* What `r` may replace: a member that was already in the archive when the command started. Two files with the
 * same member name on one command line BOTH land (GNU's replace_members, "the same file added multiple times"),
 * which is how musl's libc.a carries two free.o's — src/malloc/free.c and mallocng/free.c. */
static int find_replaceable(const char *name) {
	int at = find(name);
	return at >= 0 && mem[at].fresh ? -1 : at;
}
static int place(void) {   /* where a / b|i put new or moved members: the index to insert at (nmem = the end) */
	if (!pos_mode) return nmem;
	for (int at = 0; at < nmem; at++)   /* relpos names a member exactly (GNU's get_pos_bfd: no basename) */
		if (!strcmp(mem[at].name, relpos)) return pos_mode == 'a' ? at + 1 : at;
	return nmem;   /* (missing: the end, as GNU) */
}
static void insert_at(int at, Mem m) {
	mem = grow(mem, nmem, &memcap, sizeof *mem);
	memmove(&mem[at + 1], &mem[at], (nmem - at) * sizeof *mem); mem[at] = m; nmem++;
}

/* A file to add: for a thin archive output, a thin archive's members (flattened); else the file itself. */
static void add_file(const char *archive, const char *file, int replace) {
	long size; u8 *d = slurp(file, &size);
	if (thin && size >= 8 && !memcmp(d, "!<thin>\n", 8)) {
		Archive ar; ar_open(&ar, file, d, size);
		ArMember am; long pos = 8;
		while (ar_next(&ar, &pos, &am)) { char *p = ar_member_path(&ar, &am); add_file(archive, p, replace); free(p); }
		free(d); return;
	}
	if (thin && ar_is_archive(d, size)) die("%s: a regular archive can't be a member of a thin archive", file);
	Mem m; memset(&m, 0, sizeof m);
	m.name = thin ? relative_to(file, archive) : strdup(full_path ? file : base_of(file));
	m.size = size; stamp(&m, file);
	if (thin) { m.path = strdup(file); free(d); } else m.data = d;
	int at = replace ? find_replaceable(thin ? m.name : file) : -1;
	m.fresh = 1;
	if (at >= 0 && update && m.date <= mem[at].date) { free(m.name); free(m.data); free(m.path); return; }   /* u: not newer */
	if (at >= 0) { free(mem[at].name); free(mem[at].data); free(mem[at].path); mem[at] = m; return; }
	insert_at(replace ? place() : nmem, m);
}

/* ---- writing ---------------------------------------------------------------------------------------------- */
typedef struct { const char *name; int member; } Sym;
static Sym *sym; static int nsym, symcap;
static void index_members(void) {   /* GNU's armap: each object member's defined global/weak/common symbols, in order */
	for (int i = 0; i < nmem; i++) {
		long size = mem[i].size; u8 *d = mem[i].data, *own = NULL;
		if (!d) { d = own = slurp(mem[i].path, &size); if (size != mem[i].size) die("%s: changed size since it was archived", mem[i].path); }
		else if ((size_t)d & 3) { own = malloc(size); memcpy(own, d, size); d = own; }   /* (elf_open wants it aligned) */
		if (size >= 18 && !memcmp(d, "\177ELF", 4) && (d[16] | d[17] << 8) == ET_REL) {
			ElfFile f; elf_open(&f, mem[i].path ? mem[i].path : mem[i].name, d, size, ET_REL, 0);
			for (int k = 1; k < f.nsym; k++) {
				int b = ELF32_ST_BIND(f.sym[k].st_info);
				if ((b != STB_GLOBAL && b != STB_WEAK && b != 10 /* GNU_UNIQUE */) || f.sym[k].st_shndx == SHN_UNDEF || !f.sym[k].st_name) continue;
				sym = grow(sym, nsym, &symcap, sizeof *sym);
				sym[nsym].name = strdup(f.strtab + f.sym[k].st_name); sym[nsym++].member = i;
			}
		}
		free(own);
	}
}
static void put(FILE *f, const char *s, int width) {   /* a header field: left-justified, space-padded, truncated (bfd's spacepad) */
	int n = (int)strlen(s); for (int i = 0; i < width; i++) fputc(i < n ? s[i] : ' ', f);
}
static void header(FILE *f, const char *name, const char *date, const char *uid, const char *gid, const char *mode, long size) {
	char sz[16]; snprintf(sz, sizeof sz, "%ld", size);
	put(f, name, 16); put(f, date, 12); put(f, uid, 6); put(f, gid, 6); put(f, mode, 8); put(f, sz, 10); fputs("`\n", f);
}
static void be32(FILE *f, u32 v) { fputc(v >> 24, f); fputc(v >> 16, f); fputc(v >> 8, f); fputc(v, f); }
static void write_archive(const char *path) {
	int index = !no_index;
	nsym = 0; if (index) index_members();
	char *longtab = NULL; long longlen = 0, *loff = calloc(nmem ? nmem : 1, sizeof *loff);
	for (int i = 0; i < nmem; i++) {
		size_t L = strlen(mem[i].name);
		loff[i] = -1;
		if (thin || L > 15) {
			longtab = realloc(longtab, longlen + L + 3); loff[i] = longlen;
			longlen += sprintf(longtab + longlen, "%s/\n", mem[i].name);
		}
	}
	long symsize = 4 + 4L * nsym; for (int i = 0; i < nsym; i++) symsize += (long)strlen(sym[i].name) + 1;
	long pos = 8 + (index ? 60 + symsize + (symsize & 1) : 0) + (longlen ? 60 + longlen + (longlen & 1) : 0);
	long *hoff = calloc(nmem ? nmem : 1, sizeof *hoff);
	for (int i = 0; i < nmem; i++) { hoff[i] = pos; pos += 60 + (thin ? 0 : mem[i].size + (mem[i].size & 1)); }

	char *tmp = malloc(strlen(path) + 16); sprintf(tmp, "%s.arXXXX", path);
	FILE *f = fopen(tmp, "wb"); if (!f) die("%s: cannot create", tmp);
	fputs(thin ? "!<thin>\n" : "!<arch>\n", f);
	if (index) {
		char now[16]; snprintf(now, sizeof now, "%ld", deterministic ? 0L : (long)time(NULL));
		header(f, "/", now, "0", "0", "0", symsize + (symsize & 1));   /* (its size counts its pad — a NUL, as GNU's) */
		be32(f, (u32)nsym);
		for (int i = 0; i < nsym; i++) be32(f, (u32)hoff[sym[i].member]);
		for (int i = 0; i < nsym; i++) fwrite(sym[i].name, 1, strlen(sym[i].name) + 1, f);
		if (symsize & 1) fputc('\0', f);
	}
	if (longlen) { header(f, "//", "", "", "", "", longlen + (longlen & 1)); fwrite(longtab, 1, longlen, f); if (longlen & 1) fputc('\n', f); }
	for (int i = 0; i < nmem; i++) {
		char nm[24], date[24], uid[24], gid[24], mode[24];
		if (loff[i] >= 0) {   /* "/offset" — and, in a thin archive, bfd's leftover '/' after a 15-byte basename */
			snprintf(nm, sizeof nm, "/%-14ld%c", loff[i], thin && strlen(base_of(mem[i].name)) == 15 ? '/' : ' ');
		} else snprintf(nm, sizeof nm, "%s/", mem[i].name);
		snprintf(date, sizeof date, "%ld", mem[i].date); snprintf(uid, sizeof uid, "%ld", mem[i].uid);
		snprintf(gid, sizeof gid, "%ld", mem[i].gid); snprintf(mode, sizeof mode, "%o", mem[i].mode);
		header(f, nm, date, uid, gid, mode, mem[i].size);
		if (!thin) { fwrite(mem[i].data, 1, mem[i].size, f); if (mem[i].size & 1) fputc('\n', f); }
	}
	if (fclose(f)) die("%s: write failed", tmp);
	if (rename(tmp, path)) die("%s: cannot rename to %s", tmp, path);
	free(tmp); free(longtab); free(loff); free(hoff);
}

int main(int argc, char **argv) {
	const char *base = strrchr(argv[0], '/'); base = base ? base + 1 : argv[0];
	size_t bl = strlen(base);
	if (bl >= 6 && !strcmp(base + bl - 6, "ranlib")) {   /* ranlib [-D|-U|-t] archive... — just the index */
		tool = "ranlib"; key = 's';
		int done = 0;
		for (int i = 1; i < argc; i++) {
			const char *a = argv[i];
			if (!strcmp(a, "-D")) { deterministic = 1; continue; }
			if (!strcmp(a, "-U")) { deterministic = 0; continue; }
			if (!strcmp(a, "-t")) continue;              /* "update the index" — what we do anyway */
			if (a[0] == '-' && a[1]) die("unknown option '%s'", a);
			int was_thin = 0;
			struct stat ast; if (stat(a, &ast)) die("%s: No such file or directory", a);
			load_archive(a, &was_thin); thin = was_thin;
			write_archive(a);
			nmem = 0; done = 1;
		}
		if (!done) die("usage: ranlib [-D|-U] archive...");
		return 0;
	}
	if (argc < 3) die("usage: ar [-]{d|m|p|q|r|s|t|x}[abcDiPsSTuUv] [relpos] archive [member...]");
	int ai = 1;
	for (const char *c = argv[ai][0] == '-' ? argv[ai] + 1 : argv[ai]; *c; c++)
		switch (*c) {
		case 'd': case 'm': case 'p': case 'q': case 'r': case 's': case 't': case 'x':
			if (*c == 's' && key) { want_index = 1; break; }   /* (a modifier after a key) */
			if (key) die("two operations: '%c' and '%c'", key, *c);
			key = *c; break;
		case 'a': pos_mode = 'a'; break;
		case 'b': case 'i': pos_mode = 'b'; break;
		case 'c': quiet_create = 1; break;
		case 'D': deterministic = 1; break;
		case 'U': deterministic = 0; break;
		case 'P': full_path = 1; break;
		case 'S': no_index = 1; break;
		case 'T': thin = 1; break;
		case 'u': update = 1; break;
		case 'v': verbose = 1; break;
		case 'o': case 'f': case 'l': break;   /* (extract keeps dates; truncate names; -l libraries): no effect here */
		default: die("unknown modifier '%c'", *c);
		}
	ai++;
	if (!key) die("no operation specified");
	if (want_index && no_index) die("both s and S");
	if (pos_mode) { if (ai >= argc) die("%s needs a member name", pos_mode == 'a' ? "a" : "b/i"); relpos = argv[ai++]; }
	if (ai >= argc) die("no archive specified");
	const char *archive = argv[ai++];
	char **files = argv + ai; int nfiles = argc - ai;

	struct stat st; int exists = !stat(archive, &st), was_thin = 0;
	if (exists) load_archive(archive, &was_thin);
	if (exists && was_thin) thin = 1;               /* an existing thin archive stays thin */
	else if (exists && thin && nmem) die("%s: T on an existing regular archive: unsupported", archive);

	switch (key) {
	case 't':
		if (!exists) die("%s: No such file or directory", archive);
		if (verbose) die("tv: unsupported");
		for (int i = 0; i < nmem; i++) if (!nfiles || find(files[0]) >= 0) {
			if (nfiles) { int show = 0; for (int k = 0; k < nfiles; k++) if (!strcmp(mem[i].name, full_path ? files[k] : base_of(files[k]))) show = 1; if (!show) continue; }
			puts(mem[i].name);
		}
		return 0;
	case 'p': case 'x':
		if (!exists) die("%s: No such file or directory", archive);
		for (int i = 0; i < nmem; i++) {
			int want = !nfiles; for (int k = 0; k < nfiles; k++) if (!strcmp(mem[i].name, full_path ? files[k] : base_of(files[k]))) want = 1;
			if (!want) continue;
			long size = mem[i].size; u8 *d = mem[i].data ? mem[i].data : slurp(mem[i].path, &size);
			if (key == 'p') { fwrite(d, 1, size, stdout); continue; }
			if (thin) die("%s: extracting from a thin archive: its members are files already", archive);
			FILE *f = fopen(mem[i].name, "wb"); if (!f) die("%s: cannot create", mem[i].name);
			if (fwrite(d, 1, size, f) != (size_t)size || fclose(f)) die("%s: write failed", mem[i].name);
			chmod(mem[i].name, mem[i].mode & 07777);
		}
		return 0;
	case 'd':
		if (!exists) die("%s: No such file or directory", archive);
		for (int k = 0; k < nfiles; k++) {
			int at = find(files[k]);
			if (at < 0) { if (verbose) fprintf(stderr, "No member named '%s'\n", files[k]); continue; }
			memmove(&mem[at], &mem[at + 1], (nmem - at - 1) * sizeof *mem); nmem--;
		}
		break;
	case 'm':
		if (!exists) die("%s: No such file or directory", archive);
		for (int k = 0; k < nfiles; k++) {   /* each named member, in argument order, to the end (or relpos) */
			int at = find(files[k]);
			if (at < 0) die("no entry %s in archive %s", files[k], archive);
			Mem m = mem[at]; memmove(&mem[at], &mem[at + 1], (nmem - at - 1) * sizeof *mem); nmem--;
			insert_at(place(), m);
		}
		break;
	case 'q': case 'r':
		if (!exists && !quiet_create) fprintf(stderr, "%s: creating %s\n", tool, archive);
		for (int k = 0; k < nfiles; k++) add_file(archive, files[k], key == 'r');
		break;
	case 's':
		if (!exists) die("%s: No such file or directory", archive);
		break;
	}
	write_archive(archive);
	return 0;
}
