/* engine/text.c — text rendering for the engine.
 *
 * PRIMARY path: runtime TrueType rasterization via stb_truetype (third_party/) — the same
 * kind of vector-font rasterizer Godot/Unity use (they use FreeType). Given the font file
 * bytes + a pixel size, stb_truetype produces an antialiased 8-bit coverage bitmap per
 * glyph; we cache each rasterized glyph and alpha-blend it onto the frame in the requested
 * color. Any .ttf, any size, real kerning. The font is loaded from $CANVAS_FONT, else the
 * installed default (/usr/share/canvas/common/ui.ttf).
 *
 * If no font file loads, eng_font_init logs an error and text becomes a no-op (there is no
 * bitmap fallback — the font is bundled and installed, so a missing font is misconfig).
 *
 * `px` in eng_text/eng_text_width is the font PIXEL SIZE (like a real engine's point size),
 * NOT the old integer cell-scale. `y` is the top of the text; we place glyphs on a baseline
 * derived from the font's ascent so callers can keep positioning by top-left.
 */
#define _GNU_SOURCE
#include "engine.h"
#include "engine_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* NB: no STBTT_STATIC — that marks stb's many unused helpers `static`, which then trip
 * -Wunused-function. Left extern, they're just unreferenced globals (no warning); only
 * this TU defines the implementation, so nothing collides. */
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

/* ---- loaded font ---- */
static struct {
	unsigned char   *ttf;            /* font file bytes (must outlive `info`) */
	stbtt_fontinfo   info;
	int              loaded;
} g_font;

/* ---- glyph cache: (codepoint, pixel-size) -> rasterized coverage bitmap + metrics ---- */
#define GCACHE 1024
static struct glyph {
	int              cp, px;         /* key; cp = -1 marks an empty slot */
	unsigned char   *bmp;            /* stb-allocated coverage (w*h), NULL for blank glyphs */
	int              w, h, xoff, yoff;
	float            adv;            /* horizontal advance in pixels */
} gc[GCACHE];
static int gc_n;

void eng_font_init(void)
{
	const char *path = getenv("CANVAS_FONT");
	if (!path || !*path) path = "/usr/share/canvas/common/ui.ttf";

	FILE *f = fopen(path, "rb");
	if (!f) { fprintf(stderr, "engine: no font at %s -> text disabled\n", path); return; }
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	if (n <= 0) { fclose(f); return; }
	g_font.ttf = malloc((size_t)n);
	if (!g_font.ttf) { fclose(f); return; }
	size_t got = fread(g_font.ttf, 1, (size_t)n, f);
	fclose(f);
	if (got != (size_t)n) { free(g_font.ttf); g_font.ttf = NULL; return; }

	if (!stbtt_InitFont(&g_font.info, g_font.ttf, stbtt_GetFontOffsetForIndex(g_font.ttf, 0))) {
		free(g_font.ttf); g_font.ttf = NULL; return;
	}
	g_font.loaded = 1;
	for (int i = 0; i < GCACHE; i++) gc[i].cp = -1;
	fprintf(stderr, "engine: font loaded (%s)\n", path);
}

/* Fetch a glyph from the cache, rasterizing + inserting it on first use. */
static struct glyph *glyph_get(int cp, int px, float scale)
{
	for (int i = 0; i < gc_n; i++)
		if (gc[i].cp == cp && gc[i].px == px) return &gc[i];
	if (gc_n >= GCACHE) return NULL;             /* full (won't happen for ASCII): skip caching */

	struct glyph *g = &gc[gc_n];
	g->bmp = stbtt_GetCodepointBitmap(&g_font.info, scale, scale, cp,
					  &g->w, &g->h, &g->xoff, &g->yoff);
	int adv, lsb;
	stbtt_GetCodepointHMetrics(&g_font.info, cp, &adv, &lsb);
	g->adv = adv * scale;
	g->cp = cp; g->px = px;
	gc_n++;
	return g;
}

void eng_text(int x, int y, int px, eng_color c, const char *s)
{
	canvas_frame *f = eng__frame();
	if (!f || !s || px < 1) return;

	if (!g_font.loaded) return;                  /* no font -> nothing to draw */

	float scale = stbtt_ScaleForPixelHeight(&g_font.info, (float)px);
	int asc, desc, gap;
	stbtt_GetFontVMetrics(&g_font.info, &asc, &desc, &gap);
	float penx = (float)x;
	float baseline = (float)y + asc * scale;     /* y = top of text; place on the baseline */

	for (int i = 0; s[i]; i++) {
		if (s[i] == '\n') { penx = (float)x; baseline += (asc - desc + gap) * scale; continue; }
		int cp = (unsigned char)s[i];
		struct glyph *g = glyph_get(cp, px, scale);
		if (g && g->bmp) {
			int ox = (int)(penx + 0.5f) + g->xoff;
			int oy = (int)(baseline + 0.5f) + g->yoff;
			for (int j = 0; j < g->h; j++)
				for (int k = 0; k < g->w; k++)
					eng__blend(f, ox + k, oy + j, c, g->bmp[j * g->w + k]);
		}
		penx += g ? g->adv : 0.0f;
		if (s[i + 1] && s[i + 1] != '\n')        /* kerning to the next glyph */
			penx += stbtt_GetCodepointKernAdvance(&g_font.info, cp, (unsigned char)s[i + 1]) * scale;
	}
}

int eng_text_width(int px, const char *s)
{
	if (!s || px < 1 || !g_font.loaded) return 0;
	float scale = stbtt_ScaleForPixelHeight(&g_font.info, (float)px);
	float w = 0.0f;
	for (int i = 0; s[i] && s[i] != '\n'; i++) {
		int cp = (unsigned char)s[i], adv, lsb;
		stbtt_GetCodepointHMetrics(&g_font.info, cp, &adv, &lsb);
		w += adv * scale;
		if (s[i + 1] && s[i + 1] != '\n')
			w += stbtt_GetCodepointKernAdvance(&g_font.info, cp, (unsigned char)s[i + 1]) * scale;
	}
	return (int)(w + 0.5f);
}
