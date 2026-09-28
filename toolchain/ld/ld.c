/*
 * ld.c — the linker FRONT-END (architecture- and format-generic linking algorithm) + driver.
 *
 * Produces an ELF executable (static, PIE, or dynamically linked) or a shared object from relocatable objects,
 * archives and shared libraries — the inverse of the assembler: `as` emitted section-relative bytes +
 * relocations; `ld` gives them real addresses and patches them in. The phases, in order:
 *
 *   load       objects, archive indexes, shared-library exports                       (elf.c)
 *   resolve    the global symbol table; archive members pulled for strong references  (here)
 *   script     read the linker script (-T, or the built-in default)                     (script.c)
 *   match      input sections -> output sections                                        (script.c)
 *   scan       one relocation pass: imports, PLT, GOT, dynamic relocations              (dynamic.c)
 *   layout     orphans, addresses, segments                                             (script.c)
 *   fill       the linker-made tables' contents                                         (dynamic.c)
 *   relocate   patch every relocation (encodings in arm.c)                              (here)
 *   write      the output file                                                          (elf.c)
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "ld.h"

Obj **objs; int nobj; static int objcap;
Obj *obj_new(void) {
	objs = grow(objs, nobj, &objcap, sizeof *objs);
	return objs[nobj++] = calloc(1, sizeof(Obj));
}
u32 load_base = 0x00010000u;               /* default layout: image base; -Ttext <addr> overrides (bare-metal 0x40000000) */
int pie = 0;                               /* -pie: ET_DYN, base 0, absolute refs become load-bias fixups */
int shared = 0;                            /* -shared: emit a .so — exported .dynsym/.hash, no required entry */
const char *soname = NULL;                 /* -soname NAME -> DT_SONAME (default: the output basename) */
const char *entry_sym = NULL;              /* -e; else the script's ENTRY(); else _start */
int bsymbolic = 0;                         /* -Bsymbolic */
const char *interp_path = "/lib/ld.so.1";  /* --dynamic-linker: the PT_INTERP a dynamic program names */
int stack_override;                        /* -z noexecstack / execstack: PT_GNU_STACK's flags, else from the inputs */
int norelro;                               /* -z norelro */
int warn_rwx = 1;                          /* --no-warn-rwx-segments turns the RWX-segment warning off */

/* die() is tool-specific (its own "ld:" prefix); rd32/wr32/alignup/Strtab are shared (common/elfutil). */
void die(const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	fputs("ld: ", stderr); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); exit(1);
}
void *grow(void *v, int n, int *cap, size_t esz) {
	if (n < *cap) return v;
	*cap = *cap ? *cap * 2 : 64;
	if (!(v = realloc(v, (size_t)*cap * esz))) die("out of memory");
	return v;
}

/* ---- global symbol table ------------------------------------------------------------------------- */
/* ELF symbol binding: a STRONG (STB_GLOBAL) definition overrides a WEAK one; two strong ones are an error; of
 * two weak ones the first stays. An undefined WEAK reference resolves to 0 (see resolve()); only a STRONG
 * reference pulls an archive member (a member whose definition is weak still satisfies it, as in GNU ld). */
static StrMap gsyms;
GSym *gsym_find(const char *name) { return strmap_get(&gsyms, name); }
GSym *gsym_get(const char *name) {
	GSym *g = strmap_get(&gsyms, name);
	if (!g) { g = calloc(1, sizeof *g); g->name = name; strmap_put(&gsyms, name, g); }
	return g;
}
static const char **pending; static int npending, pendcap;   /* strongly referenced, not yet defined: archive candidates */
/* Enter one object's global/weak symbols: its definitions (binding rules) and its undefined references. */
static void add_symbols(Obj *o) {
	for (int k = 0; k < o->nsym; k++) {
		Elf32_Sym *s = &o->sym[k]; int b = ELF32_ST_BIND(s->st_info);
		if ((b != STB_GLOBAL && b != STB_WEAK) || !s->st_name) continue;
		const char *name = o->strtab + s->st_name;
		GSym *g = gsym_get(name);
		if (s->st_shndx == SHN_COMMON) die("%s: COMMON symbol '%s' (compile with -fno-common)", o->path, name);
		if (s->st_shndx == SHN_UNDEF) {
			if (b == STB_GLOBAL && !g->strong_ref) {
				g->strong_ref = 1;
				if (!g->defined) { pending = grow(pending, npending, &pendcap, sizeof *pending); pending[npending++] = name; }
			}
			continue;
		}
		int weak = b == STB_WEAK;
		if (g->defined) {
			if (!weak && !g->weak) die("duplicate definition of '%s' (in %s and %s)", name, g->obj->path, o->path);
			if (weak || !g->weak) continue;              /* the existing (strong, or first weak) one stays */
		}
		g->defined = 1; g->weak = weak; g->obj = o; g->symidx = k;
		g->hidden = ELF32_ST_VISIBILITY(s->st_other) != STV_DEFAULT;
	}
}
/* Symbol resolution: enter every command-line object, then pull archive members for strongly referenced,
 * still-undefined symbols until nothing new is needed (resolution across all archives at once, like
 * --start-group). Unresolved references are reported when the relocation scan meets them. */
static void resolve_symbols(void) {
	for (int i = 0; i < nobj; i++) add_symbols(objs[i]);
	for (int i = 0; i < npending; i++) {                 /* the list grows as pulled members add references */
		if (gsym_find(pending[i])->defined) continue;
		Obj *m = ar_pull(pending[i]);
		if (m) add_symbols(m);
	}
}
static int discarded(const Obj *o, int shndx) {
	return shndx != SHN_ABS && shndx < 0xff00 && (!o->sec_out[shndx] || o->sec_out[shndx] == &os_discard);
}
/* After layout: every object-defined global's final address (one in a discarded section no longer defines it). */
static void build_globals(void) {
	for (size_t i = 0; i < gsyms.cap; i++) {
		GSym *g = gsyms.vals[i];
		if (!g || !g->defined || !g->obj) continue;
		if (discarded(g->obj, g->obj->sym[g->symidx].st_shndx)) { g->defined = 0; continue; }
		g->vaddr = sym_addr(g->obj, g->symidx);
	}
}
int defined_locally(const char *name) { GSym *g = gsym_find(name); return g && (g->defined || g->linker); }
u32 sym_addr(const Obj *o, int symidx) {
	const Elf32_Sym *s = &o->sym[symidx];
	return s->st_shndx == SHN_ABS ? s->st_value : o->sec_vaddr[s->st_shndx] + s->st_value;
}
int sym_is_abs(const Obj *o, int symidx) {
	const Elf32_Sym *s = &o->sym[symidx];
	int b = ELF32_ST_BIND(s->st_info);
	if (s->st_shndx != SHN_UNDEF && (b == STB_LOCAL || !s->st_name)) return s->st_shndx == SHN_ABS;   /* global: the winner's */
	GSym *g = gsym_find(o->strtab + s->st_name);
	if (!g || !(g->defined || g->linker)) return 1;      /* undefined weak: 0 at any load address */
	return g->obj ? g->obj->sym[g->symidx].st_shndx == SHN_ABS : g->abs;
}

/* A relocation's symbol, resolved to its final address. A GLOBAL or WEAK name — even one this object defines —
 * means the link's winning definition (a weak one here may have lost to a strong one elsewhere: musl's
 * weak_alias(dummy, __stdout_used) is overridden by stdout.o's), so it's looked up by name; only a LOCAL symbol is
 * this object's own. */
static u32 resolve(Obj *o, int symidx) {
	Elf32_Sym *s = &o->sym[symidx];
	int b = ELF32_ST_BIND(s->st_info);
	if (s->st_shndx == SHN_UNDEF || (s->st_name && (b == STB_GLOBAL || b == STB_WEAK))) {
		const char *nm = o->strtab + s->st_name;
		GSym *g = gsym_find(nm);
		if (g && g->defined) return g->vaddr;
		if (s->st_shndx == SHN_UNDEF && b == STB_WEAK) return 0;   /* an undefined weak reference is 0 */
		if (g && g->obj) die("'%s' (referenced in %s) is defined in a discarded section of %s", nm, o->path, g->obj->path);
		die("undefined symbol '%s' (referenced in %s)", nm, o->path);
	}
	if (discarded(o, s->st_shndx))
		die("%s: reference to %s in discarded section %s", o->path,
		    s->st_name ? o->strtab + s->st_name : "a symbol", sec_name(o, s->st_shndx));
	return sym_addr(o, symidx);
}

/* For each REL section, patch its target section's bytes now that addresses are known. */
static void relocate(void) {
	for (int i = 0; i < nobj; i++) {
		Obj *o = objs[i];
		if (!o->active || o == linker_obj) continue;
		for (int j = 0; j < o->nsh; j++) {
			Elf32_Shdr *rs = &o->sh[j];
			if (rs->sh_type != SHT_REL) continue;
			int t = (int)rs->sh_info;
			Elf32_Shdr *ts = &o->sh[t];                          /* the section being patched */
			if (!(ts->sh_flags & SHF_ALLOC) || o->sec_out[t] == &os_discard) continue;
			if (ts->sh_type == SHT_NOBITS) die("%s: relocations against NOBITS section %s", o->path, sec_name(o, t));
			Elf32_Rel *rel = (Elf32_Rel *)(o->data + rs->sh_offset);
			for (u32 r = 0; r < rs->sh_size / sizeof *rel; r++) {
				u32 type = ELF32_R_TYPE(rel[r].r_info); int sidx = (int)ELF32_R_SYM(rel[r].r_info);
				if (!md_is_marker(type) && (rel[r].r_offset > ts->sh_size || ts->sh_size - rel[r].r_offset < 4))   /* a 4-byte field (no underflow) */
					die("%s: relocation offset %#x outside %s", o->path, rel[r].r_offset, sec_name(o, t));
				u32 S; int tk = md_tls_kind(type);
				if (tk == TLS_LE) S = tls_tpoff(resolve(o, sidx));                 /* from the thread pointer */
				else if (tk == TLS_LDO) S = resolve(o, sidx) - tls_vaddr;          /* inside this module's block */
				else if (!dyn_target(o, sidx, type, &S)) S = resolve(o, sidx);   /* GOT slot / PLT stub / copy, else the symbol */
				md_apply_reloc(o, type, o->data + ts->sh_offset + rel[r].r_offset, S, o->sec_vaddr[t] + rel[r].r_offset);
			}
		}
	}
}

/* An input file's kind, from its first bytes: an archive (ar magic), a shared object (ELF ET_DYN, used as a
 * provider), else a relocatable object (elf_load validates it). */
enum { IN_OBJECT, IN_ARCHIVE, IN_SHARED };
static int input_kind(const char *path) {
	FILE *f = fopen(path, "rb"); if (!f) die("cannot open %s", path);
	u8 h[18]; size_t n = fread(h, 1, sizeof h, f); fclose(f);
	if (n >= 8 && !memcmp(h, "!<arch>\n", 8)) return IN_ARCHIVE;
	if (n >= 18 && !memcmp(h, "\177ELF", 4) && (h[16] | h[17] << 8) == ET_DYN) return IN_SHARED;
	return IN_OBJECT;
}
static int file_exists(const char *p) { FILE *f = fopen(p, "rb"); if (f) { fclose(f); return 1; } return 0; }
static int link_static;                                   /* -static: no shared objects at all */
static void load_input(const char *path, int kind) {
	if (kind == IN_ARCHIVE) ar_load(path);               /* lazy members, pulled on demand */
	else if (kind == IN_SHARED) {
		if (link_static) die("%s: a shared object in a -static link", path);
		load_shared(path);                               /* a provider: its exports, not its sections */
	} else elf_load(path);                               /* always-linked object */
}
/* -z KEYWORD: the ones with a meaning here; any other is an error (never silently ignored). */
static void z_option(const char *k) {
	if (!strcmp(k, "relro")) norelro = 0;
	else if (!strcmp(k, "norelro")) norelro = 1;
	else if (!strcmp(k, "now")) ;                        /* our loader always binds at startup (eager) */
	else if (!strcmp(k, "noexecstack")) stack_override = PF_R | PF_W;
	else if (!strcmp(k, "execstack")) stack_override = PF_R | PF_W | PF_X;
	else if (!strncmp(k, "max-page-size=", 14) || !strncmp(k, "common-page-size=", 17)) {
		if (strtoul(strchr(k, '=') + 1, NULL, 0) != PAGE) die("-z %s: this linker lays out %#x-byte pages only", k, PAGE);
	} else die("unsupported -z %s", k);
}

int main(int argc, char **argv) {
	const char *out = "a.out", *script_path = NULL;
	/* Inputs are processed in command-line order once every option is read (-L applies to every -l, wherever it
	 * appears, as in GNU ld): a file, or -l<name> searched per -L dir as lib<name>.so then lib<name>.a — .a only
	 * under -Bstatic/-static. */
	struct { const char *name; int lib, bstatic; } *in = NULL; int nin = 0, incap = 0;
	const char **libdirs = NULL; int nlibdir = 0, dircap = 0;
	int ttext = 0, bstatic = 0;
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (!strcmp(a, "-o") && i + 1 < argc) out = argv[++i];
		else if (!strcmp(a, "-T") && i + 1 < argc) script_path = argv[++i];
		else if (!strcmp(a, "-Ttext") && i + 1 < argc) { load_base = strtoul(argv[++i], NULL, 0); ttext = 1; }
		else if (!strncmp(a, "-Ttext=", 7)) { load_base = strtoul(a + 7, NULL, 0); ttext = 1; }
		else if ((!strcmp(a, "-e") || !strcmp(a, "--entry")) && i + 1 < argc) entry_sym = argv[++i];
		else if (!strcmp(a, "-pie") || !strcmp(a, "--pie")) { pie = 1; load_base = 0; }
		else if (!strcmp(a, "-shared") || !strcmp(a, "--shared")) { shared = 1; load_base = 0; }
		else if (!strcmp(a, "-Bsymbolic")) bsymbolic = 1;
		else if (!strcmp(a, "--no-warn-rwx-segments")) warn_rwx = 0;
		else if (!strcmp(a, "--warn-rwx-segments")) warn_rwx = 1;
		else if (!strcmp(a, "--build-id=none") || !strcmp(a, "--start-group") || !strcmp(a, "--end-group")) ;   /* we emit no build-id;
		                                                     * archives already resolve as one group (see resolve_symbols) */
		else if (!strncmp(a, "--dynamic-linker=", 17)) interp_path = a + 17;
		else if (!strcmp(a, "--dynamic-linker") && i + 1 < argc) interp_path = argv[++i];
		else if (!strcmp(a, "-z") && i + 1 < argc) z_option(argv[++i]);
		else if (!strncmp(a, "-z", 2) && a[2]) z_option(a + 2);
		else if (!strcmp(a, "-soname") && i + 1 < argc) soname = argv[++i];
		else if (!strncmp(a, "-soname=", 8)) soname = a + 8;
		else if (!strcmp(a, "-static")) link_static = bstatic = 1;
		else if (!strcmp(a, "-Bstatic") || !strcmp(a, "-dn") || !strcmp(a, "-non_shared")) bstatic = 1;
		else if (!strcmp(a, "-Bdynamic") || !strcmp(a, "-dy") || !strcmp(a, "-call_shared")) { if (link_static) die("-Bdynamic after -static"); bstatic = 0; }
		else if (!strncmp(a, "-l", 2)) {
			if (!a[2] && i + 1 == argc) die("-l needs a name");
			in = grow(in, nin, &incap, sizeof *in); in[nin].name = a[2] ? a + 2 : argv[++i]; in[nin].lib = 1; in[nin++].bstatic = bstatic;
		} else if (!strncmp(a, "-L", 2)) {
			if (!a[2] && i + 1 == argc) die("-L needs a directory");
			libdirs = grow(libdirs, nlibdir, &dircap, sizeof *libdirs); libdirs[nlibdir++] = a[2] ? a + 2 : argv[++i];
		}
		else if (a[0] == '-') die("unknown option '%s'", a);
		else { in = grow(in, nin, &incap, sizeof *in); in[nin].name = a; in[nin].lib = 0; in[nin++].bstatic = bstatic; }
	}
	for (int k = 0; k < nin; k++) {
		if (!in[k].lib) { load_input(in[k].name, input_kind(in[k].name)); continue; }
		char path[512]; int found = 0;
		for (int d = 0; d < nlibdir && !found; d++)
			for (int t = in[k].bstatic; t < 2 && !found; t++) {   /* lib<name>.so (unless static), then lib<name>.a */
				snprintf(path, sizeof path, "%s/lib%s.%s", libdirs[d], in[k].name, t ? "a" : "so");
				if (file_exists(path)) { load_input(strdup(path), t ? IN_ARCHIVE : IN_SHARED); found = 1; }
			}
		if (!found) die("cannot find -l%s (searched %d -L dir(s) for lib%s%s.a)", in[k].name, nlibdir, in[k].name, in[k].bstatic ? "" : ".so / lib");
	}
	if (!nobj) die("usage: ld [-o out] [-T script | -Ttext addr] [-e sym] [-pie | -shared [-soname name] [-Bsymbolic]] [-z kw] [--dynamic-linker path] [-L dir] [-l name] obj.o|lib.a ...");
	if (script_path && ttext) die("-Ttext and -T both place the image: use one");

	resolve_symbols();
	if (shared && !soname) { const char *b = strrchr(out, '/'); soname = b ? b + 1 : out; }

	script_read(script_path);                            /* declares the script's symbols */
	if (!entry_sym) entry_sym = "_start";
	dyn_init();
	for (int i = 0; i < nobj; i++) if (objs[i]->active && objs[i] != linker_obj) layout_match(objs[i]);
	dyn_scan();
	dyn_size();
	layout_match(linker_obj);
	layout_run();
	build_globals();
	dyn_fill();
	relocate();

	u32 entry = 0;                                       /* a plain .so has none; ld.so IS a .so WITH an entry */
	GSym *start = gsym_find(entry_sym);
	if (start && start->defined) entry = start->vaddr;
	else if (!shared) die("no '%s' symbol (entry point)", entry_sym);
	elf_write(out, entry);
	return 0;
}
