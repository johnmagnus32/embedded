/*
 * version.c — version scripts: `VERSION { ... }` in a linker script, or --version-script FILE. Each node
 *     [NAME] { global: PATTERN; ... local: PATTERN; ... } [PARENT ...];
 * puts the shared object's symbols it matches in version NAME (exported as sym@@NAME), or — by a local: pattern —
 * makes them local (not exported, local in .symtab). A pattern is a name or a glob (* ? [...]); "quoted" is literal.
 * Which node a symbol answers to is bfd_find_version_for_sym's order: an exact global match in any node, then an
 * exact local one, then a glob global, then a glob local; none: exported, unversioned. Named nodes become version
 * definitions (dynamic.c: .gnu.version_d, .gnu.version, an absolute dynamic symbol per version); an anonymous node
 * only hides.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>
#include "ld.h"

typedef struct { char *pat; int literal; } Pat;
typedef struct { const char *name; Pat *glob; int nglob; Pat *loc; int nloc; const char **deps; int ndep; } VerNode;
static VerNode *vn; static int nvn, vncap;

/* ---- parsing --------------------------------------------------------------------------------------- */
static const char *vs, *vwhat;
static char vtok[1024]; static int vquoted;
static int vnext(void) {   /* the next token into vtok; 0 at the end */
	for (;;) {
		while (*vs == ' ' || *vs == '\t' || *vs == '\n' || *vs == '\r') vs++;
		if (vs[0] == '/' && vs[1] == '*') { const char *e = strstr(vs + 2, "*/"); if (!e) die("%s: unterminated comment", vwhat); vs = e + 2; continue; }
		if (*vs == '#') { while (*vs && *vs != '\n') vs++; continue; }
		break;
	}
	vquoted = 0;
	if (!*vs) return 0;
	if (strchr("{};:", *vs)) { vtok[0] = *vs++; vtok[1] = 0; return 1; }
	int n = 0;
	if (*vs == '"') {
		vs++; vquoted = 1;
		while (*vs && *vs != '"') { if (n < (int)sizeof vtok - 1) vtok[n++] = *vs; vs++; }
		if (*vs++ != '"') die("%s: unterminated string", vwhat);
	} else while (*vs && !strchr(" \t\r\n{};:", *vs)) { if (n < (int)sizeof vtok - 1) vtok[n++] = *vs; vs++; }
	vtok[n] = 0;
	return 1;
}
static void vexpect(const char *t) { if (!vnext() || strcmp(vtok, t)) die("%s: expected '%s' in a version script, got '%s'", vwhat, t, vtok); }
static void add_pat(Pat **v, int *n) {
	*v = realloc(*v, (*n + 1) * sizeof **v);
	(*v)[*n].pat = strdup(vtok); (*v)[*n].literal = vquoted || !strpbrk(vtok, "*?["); (*n)++;
}

/* The text between VERSION's braces (or a --version-script file's). */
void version_script(const char *text, const char *what) {
	vs = text; vwhat = what;
	while (vnext()) {
		if (!shared) die("%s: a version script outside -shared: unsupported", what);
		vn = grow(vn, nvn, &vncap, sizeof *vn);
		VerNode *node = &vn[nvn++]; memset(node, 0, sizeof *node);
		if (strcmp(vtok, "{")) { node->name = strdup(vtok); vexpect("{"); }
		for (int i = 0; i < nvn - 1; i++) if (node->name && vn[i].name && !strcmp(vn[i].name, node->name)) die("%s: version %s defined twice", what, node->name);
		int local = 0;
		for (;;) {
			if (!vnext()) die("%s: unterminated version node", what);
			if (!vquoted && !strcmp(vtok, "}")) break;
			if (!vquoted && !strcmp(vtok, ";")) continue;
			if (!vquoted && (!strcmp(vtok, "global") || !strcmp(vtok, "local")) && *vs == ':') { local = vtok[0] == 'l'; vexpect(":"); continue; }
			if (!vquoted && !strcmp(vtok, "extern")) die("%s: extern \"C++\"/\"Java\" version patterns: unsupported", what);
			if (local) add_pat(&node->loc, &node->nloc); else add_pat(&node->glob, &node->nglob);
			if (!vnext() || strcmp(vtok, ";")) { if (!strcmp(vtok, "}")) break; die("%s: expected ';' after a version pattern, got '%s'", what, vtok); }
		}
		for (;;) {   /* the parents this version inherits from, up to the ';' */
			if (!vnext()) die("%s: expected ';' after a version node", what);
			if (!strcmp(vtok, ";")) break;
			if (!node->name) die("%s: an anonymous version node can't have parents", what);
			node->deps = realloc(node->deps, (node->ndep + 1) * sizeof *node->deps); node->deps[node->ndep++] = strdup(vtok);
		}
		if (!node->name && nvn > 1) die("%s: an anonymous version node must be the only one", what);
	}
}

/* ---- matching ---------------------------------------------------------------------------------------- */
static int match(const Pat *v, int n, const char *sym, int literal) {
	for (int i = 0; i < n; i++) if (v[i].literal == literal && (literal ? !strcmp(v[i].pat, sym) : !fnmatch(v[i].pat, sym, 0))) return 1;
	return 0;
}
static int lookup(const char *sym, int *hide) {   /* the node (-1: none), and whether it hides the symbol */
	*hide = 0;
	for (int lit = 1; lit >= 0; lit--) {
		for (int i = 0; i < nvn; i++) if (match(vn[i].glob, vn[i].nglob, sym, lit)) return i;
		for (int i = 0; i < nvn; i++) if (match(vn[i].loc, vn[i].nloc, sym, lit)) { *hide = 1; return i; }
	}
	return -1;
}
int version_hidden(const char *sym) { int h; if (!nvn) return 0; lookup(sym, &h); return h; }
int version_count(void) { return nvn && vn[0].name ? nvn : 0; }   /* named versions (definitions 2, 3, …) */
int version_of(const char *sym) {   /* an exported symbol's version index: 2 + its node; 1 = unversioned (global) */
	int h, i = nvn ? lookup(sym, &h) : -1;
	return i >= 0 && !h && version_count() ? 2 + i : 1;
}
const char *version_name(int i) { return vn[i].name; }
int version_parent(const char *name) {   /* a parent's definition index */
	for (int i = 0; i < nvn; i++) if (vn[i].name && !strcmp(vn[i].name, name)) return 2 + i;
	die("version script: unknown parent version %s", name);
}
const char **version_deps(int i, int *n) { *n = vn[i].ndep; return vn[i].deps; }
