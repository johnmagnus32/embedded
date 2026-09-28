/* common/strmap.c — see strmap.h. Linear probing; the table doubles at 70% load, so probes stay short. */
#include <stdlib.h>
#include <string.h>
#include "strmap.h"

static size_t hash(const char *s) { size_t h = 1469598103934665603ULL; while (*s) { h ^= (unsigned char)*s++; h *= 1099511628211ULL; } return h; }

static size_t slot(const StrMap *m, const char *key) {   /* key's slot, or the empty slot it would take */
	size_t i = hash(key) & (m->cap - 1);
	while (m->keys[i] && strcmp(m->keys[i], key)) i = (i + 1) & (m->cap - 1);
	return i;
}

void *strmap_get(const StrMap *m, const char *key) {
	if (!m->cap) return NULL;
	size_t i = slot(m, key);
	return m->keys[i] ? m->vals[i] : NULL;
}

static void grow(StrMap *m) {
	StrMap g = { calloc(m->cap ? m->cap * 2 : 64, sizeof *g.keys), NULL, m->cap ? m->cap * 2 : 64, 0 };
	g.vals = calloc(g.cap, sizeof *g.vals);
	if (!g.keys || !g.vals) abort();
	for (size_t i = 0; i < m->cap; i++) if (m->keys[i]) { size_t j = slot(&g, m->keys[i]); g.keys[j] = m->keys[i]; g.vals[j] = m->vals[i]; g.n++; }
	free(m->keys); free(m->vals); *m = g;
}

void strmap_put(StrMap *m, const char *key, void *val) {
	if ((m->n + 1) * 10 > m->cap * 7) grow(m);
	size_t i = slot(m, key);
	if (!m->keys[i]) { m->keys[i] = key; m->n++; }
	m->vals[i] = val;
}

void strmap_clear(StrMap *m) { free(m->keys); free(m->vals); m->keys = NULL; m->vals = NULL; m->cap = m->n = 0; }
