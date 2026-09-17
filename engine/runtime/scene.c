/* engine/scene.c — the retained SCENE GRAPH: the Unity/Godot-esque layer on top of the
 * engine's immediate drawing. Content lives in three LAYERS (BACKGROUND, WORLD, HUD) that
 * render back-to-front; each layer is a tree of nodes. A node carries a transform (its
 * center), an optional sprite and/or text label, an optional per-frame update() (Godot's
 * _process), a tag (groups), and an AABB. The WORLD layer is offset by the camera
 * (eng_camera_set) so it scrolls; BACKGROUND and HUD are pinned to the screen — the
 * CanvasLayer / sorting-layer idea. Each frame eng_run() calls:
 *     eng__scene_update(dt)  — run every node's update, then reap queue_free'd nodes,
 *     eng__scene_render()    — draw each layer's visible sprites/labels, z-sorted, via eng__blend.
 * Textures are RGBA in memory, decoded from PNG at load (stb_image); a relative path
 * resolves under $CANVAS_ASSETS (else /usr/share/canvas).
 */
#include "engine.h"
#include "engine_internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG                 /* we only ship PNGs; keeps the decoder small */
/* STBI_ONLY_PNG leaves a couple of stb's overflow-check helpers unused (they belong to the
 * decoders we compiled out) and they're static, so scope-suppress just that one warning for
 * the vendored header — our own code below stays fully -Wall -Wextra checked. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "stb_image.h"
#pragma GCC diagnostic pop

struct eng_image { int w, h; uint32_t *argb; };   /* 0xAARRGGBB, row-major */
/* cell 0 = empty. If `tileset` is set, a non-empty cell is drawn as tile (cell-1) from the
 * atlas (atlas_cols per row); otherwise it's a flat pal[cell] color. */
struct eng_tilemap { int cols, rows, tile_px; unsigned char *cell; eng_color pal[16];
                     eng_image *tileset; int atlas_cols; };

/* ---- layers -------------------------------------------------------------------------- */
/* Three fixed layer roots, rendered back-to-front. Each is a position-only node owning its
 * subtree. WORLD is offset by the camera; BACKGROUND and HUD are pinned to the screen. */
#define NLAYERS 3
static eng_node g_layer[NLAYERS];
static int      g_ready;
static float    g_cam_x, g_cam_y;      /* WORLD render offset (0,0 => world coords == screen) */

static void ensure(void)
{
	if (g_ready) return;
	for (int i = 0; i < NLAYERS; i++) { g_layer[i].visible = true; g_layer[i].tint = ENG_WHITE; }
	g_ready = 1;
}

static eng_node *layer_root(eng_layer_id id)
{
	ensure();
	if (id < 0 || id >= NLAYERS) id = ENG_LAYER_WORLD;
	return &g_layer[id];
}

eng_node *eng_layer(eng_layer_id id) { return layer_root(id); }
eng_node *eng_root(void)             { return layer_root(ENG_LAYER_WORLD); }   /* default layer */

void eng_camera_set(float cx, float cy)   /* center the WORLD layer on world point (cx,cy) */
{
	g_cam_x = eng_width()  / 2.0f - cx;
	g_cam_y = eng_height() / 2.0f - cy;
}

/* ---- textures ------------------------------------------------------------------------ */

/* Load a PNG into an RGBA texture. An absolute `path` is used as-is; a relative one
 * resolves under $CANVAS_ASSETS (else /usr/share/canvas), so a game says "shmup/ship.png"
 * and the sim vs the device just point CANVAS_ASSETS at the right place. NULL on failure. */
eng_image *eng_image_from_png(const char *path)
{
	char full[512];
	const char *p = path;
	if (path && path[0] != '/') {
		const char *base = getenv("CANVAS_ASSETS");
		if (!base || !*base) base = "/usr/share/canvas";
		snprintf(full, sizeof full, "%s/%s", base, path);
		p = full;
	}

	int w, h, comp;
	unsigned char *rgba = stbi_load(p, &w, &h, &comp, 4);   /* force RGBA (4 channels) */
	if (!rgba) { fprintf(stderr, "engine: can't load image %s\n", p ? p : "(null)"); return NULL; }

	eng_image *im = malloc(sizeof *im);
	if (im) { im->w = w; im->h = h; im->argb = malloc((size_t)w * h * sizeof(uint32_t)); }
	if (!im || !im->argb) { free(im); stbi_image_free(rgba); return NULL; }

	for (int i = 0; i < w * h; i++) {                       /* R,G,B,A bytes -> 0xAARRGGBB */
		const unsigned char *q = rgba + (size_t)i * 4;
		im->argb[i] = ((uint32_t)q[3] << 24) | ((uint32_t)q[0] << 16) |
		              ((uint32_t)q[1] << 8)  |  (uint32_t)q[2];
	}
	stbi_image_free(rgba);
	return im;
}

void eng_image_free(eng_image *im)      { if (im) { free(im->argb); free(im); } }
int  eng_image_w(const eng_image *im)   { return im ? im->w : 0; }
int  eng_image_h(const eng_image *im)   { return im ? im->h : 0; }

/* ---- tilemaps ------------------------------------------------------------------------ */

eng_tilemap *eng_tilemap_new(const char *const *rows, int tile_px,
                             const char *keys, const eng_color *colors)
{
	if (tile_px < 1) tile_px = 1;
	int rh = 0; while (rows[rh]) rh++;
	int rw = 0;                                       /* widest row (robust to trimmed spaces) */
	for (int r = 0; r < rh; r++) { int l = (int)strlen(rows[r]); if (l > rw) rw = l; }
	if (rw <= 0 || rh <= 0) return NULL;

	eng_tilemap *m = malloc(sizeof *m);
	if (!m) return NULL;
	m->cols = rw; m->rows = rh; m->tile_px = tile_px;
	m->cell = calloc((size_t)rw * rh, 1);            /* 0 = empty everywhere to start */
	if (!m->cell) { free(m); return NULL; }
	for (int i = 0; i < 16; i++) m->pal[i] = 0;
	m->tileset = NULL; m->atlas_cols = 0;
	int nk = (int)strlen(keys); if (nk > 15) nk = 15; /* tile ids 1..15 */
	for (int i = 0; i < nk; i++) m->pal[i + 1] = colors[i];

	for (int r = 0; r < rh; r++)
		for (int c = 0; c < rw && rows[r][c]; c++) {
			char ch = rows[r][c];
			const char *k = (ch == ' ') ? NULL : strchr(keys, ch);
			if (k && (k - keys) < nk) m->cell[r * rw + c] = (unsigned char)(k - keys + 1);
		}
	return m;
}

/* Build a tilemap directly from an id grid (used by the Tiled loader): cell = ids[i] (0 =
 * empty, 1..ncolors = solid tile colored by colors[id-1]). ids is copied. */
eng_tilemap *eng_tilemap_from_ids(int cols, int rows, int tile_px,
                                  const unsigned char *ids, const eng_color *colors, int ncolors)
{
	if (cols <= 0 || rows <= 0 || tile_px < 1) return NULL;
	eng_tilemap *m = malloc(sizeof *m);
	if (!m) return NULL;
	m->cols = cols; m->rows = rows; m->tile_px = tile_px;
	m->cell = malloc((size_t)cols * rows);
	if (!m->cell) { free(m); return NULL; }
	memcpy(m->cell, ids, (size_t)cols * rows);
	for (int i = 0; i < 16; i++) m->pal[i] = 0;
	m->tileset = NULL; m->atlas_cols = 0;
	if (ncolors > 15) ncolors = 15;
	for (int i = 0; i < ncolors; i++) m->pal[i + 1] = colors[i];
	return m;
}

void eng_tilemap_free(eng_tilemap *m)       { if (m) { eng_image_free(m->tileset); free(m->cell); free(m); } }

/* Give the tilemap an atlas image (tiles drawn from it instead of flat colors). Takes
 * ownership of `img` (freed by eng_tilemap_free). `atlas_cols` = tiles per atlas row. */
void eng_tilemap_set_tileset(eng_tilemap *m, eng_image *img, int atlas_cols)
{
	if (!m) return;
	m->tileset = img;
	m->atlas_cols = atlas_cols > 0 ? atlas_cols : 1;
}
int  eng_tilemap_cols(const eng_tilemap *m) { return m ? m->cols : 0; }
int  eng_tilemap_rows(const eng_tilemap *m) { return m ? m->rows : 0; }
int  eng_tilemap_px(const eng_tilemap *m)   { return m ? m->tile_px : 0; }

bool eng_tilemap_solid_at(const eng_tilemap *m, float x, float y)   /* world (x,y) -> solid cell? */
{
	if (!m || x < 0 || y < 0) return false;
	int c = (int)(x / m->tile_px), r = (int)(y / m->tile_px);
	if (c < 0 || c >= m->cols || r < 0 || r >= m->rows) return false;
	return m->cell[r * m->cols + c] != 0;
}

/* ---- nodes --------------------------------------------------------------------------- */

eng_node *eng_node_new(eng_node *parent)
{
	ensure();
	eng_node *n = calloc(1, sizeof *n);
	if (!n) return NULL;
	n->visible = true;
	n->tint = ENG_WHITE;
	eng_node *p = parent ? parent : eng_root();   /* default parent = the WORLD layer */
	n->_parent = p;
	n->_sibling = p->_child;                       /* prepend to the parent's child list */
	p->_child = n;
	return n;
}

eng_node *eng_sprite_new(eng_node *parent, eng_image *img)   /* a whole image = a 1-frame sheet */
{
	return eng_anim_new(parent, img, img ? img->w : 0, img ? img->h : 0, 1, 0.0f);
}

eng_node *eng_anim_new(eng_node *parent, eng_image *sheet, int fw, int fh, int nframes, float fps)
{
	eng_node *n = eng_node_new(parent);
	if (!n) return NULL;
	n->sheet = sheet; n->fw = fw; n->fh = fh;
	n->nframes = nframes > 0 ? nframes : 1;
	n->fps = fps; n->frame = 0; n->row = 0;
	n->w = fw; n->h = fh;                         /* hitbox defaults to one frame */
	return n;
}

eng_node *eng_label_new(eng_node *parent, int px)   /* set n->text yourself; drawn at (x,y) */
{
	eng_node *n = eng_node_new(parent);
	if (n) n->text_px = px;
	return n;
}

eng_node *eng_tilemap_node(eng_node *parent, eng_tilemap *m)   /* node (x,y) = the map's top-left */
{
	eng_node *n = eng_node_new(parent);
	if (n) n->tiles = m;
	return n;
}

void eng_node_free(eng_node *n) { if (n) n->_freed = true; }   /* deferred (Godot queue_free) */

eng_node *eng_first_child(eng_node *n) { return (n ? n : eng_root())->_child; }   /* NULL = WORLD */
eng_node *eng_next(eng_node *n)        { return n ? n->_sibling : NULL; }

bool eng_overlap(const eng_node *a, const eng_node *b)
{
	float ax = a->x - a->w / 2.0f, ay = a->y - a->h / 2.0f;
	float bx = b->x - b->w / 2.0f, by = b->y - b->h / 2.0f;
	return ax < bx + b->w && ax + a->w > bx && ay < by + b->h && ay + a->h > by;
}

/* Next WORLD-layer node tagged `tag` overlapping `probe`, after `after` (NULL = start). Skips
 * `probe` and freed nodes so it's safe to eng_node_free() a returned node inside the loop. */
eng_node *eng_overlap_next(const eng_node *probe, int tag, eng_node *after)
{
	ensure();
	eng_node *c = after ? after->_sibling : g_layer[ENG_LAYER_WORLD]._child;
	for (; c; c = c->_sibling)
		if (!c->_freed && c != probe && c->tag == tag && eng_overlap(c, probe))
			return c;
	return NULL;
}

static void free_subtree(eng_node *n)
{
	eng_node *c = n->_child;
	while (c) { eng_node *next = c->_sibling; free_subtree(c); c = next; }
	free(n);
}

void eng_layer_clear(eng_layer_id id)   /* free every node in one layer; the root survives */
{
	eng_node *r = layer_root(id);
	eng_node *c = r->_child;
	while (c) { eng_node *next = c->_sibling; free_subtree(c); c = next; }
	r->_child = NULL;
}

void eng_scene_clear(void)              /* free every node in all layers */
{
	for (int i = 0; i < NLAYERS; i++) eng_layer_clear((eng_layer_id)i);
}

/* ---- per-frame update + reap --------------------------------------------------------- */

#define MAX_NODES 4096
static eng_node *s_walk[MAX_NODES];         /* snapshot of nodes for the update pass */

/* preorder-collect every node under `n` (excluding `n`) into s_walk, from index `cnt` */
static int collect(eng_node *n, int cnt)
{
	for (eng_node *c = n->_child; c; c = c->_sibling) {
		if (cnt < MAX_NODES) s_walk[cnt++] = c;
		cnt = collect(c, cnt);
	}
	return cnt;
}

/* unlink + free any _freed node (and its subtree) from `parent`'s child list */
static void reap(eng_node *parent)
{
	eng_node **link = &parent->_child;
	while (*link) {
		eng_node *n = *link;
		if (n->_freed) { *link = n->_sibling; free_subtree(n); }
		else           { reap(n); link = &n->_sibling; }
	}
}

void eng__scene_update(float dt)
{
	ensure();
	int n = 0;
	for (int i = 0; i < NLAYERS; i++) n = collect(&g_layer[i], n);   /* snapshot ALL layers first */
	for (int i = 0; i < n; i++) {
		eng_node *nd = s_walk[i];
		if (nd->_freed) continue;                                    /* freed by an earlier update */
		if (nd->update) nd->update(nd, dt);
		if (!nd->_freed && nd->sheet && nd->fps > 0.0f && nd->nframes > 1) {   /* advance animation */
			nd->_atime += dt;
			float period = 1.0f / nd->fps;
			while (nd->_atime >= period) { nd->_atime -= period; nd->frame = (nd->frame + 1) % nd->nframes; }
		}
	}
	for (int i = 0; i < NLAYERS; i++) reap(&g_layer[i]);             /* freed nodes vanish pre-render */
}

/* ---- per-frame render ---------------------------------------------------------------- */

struct draw_item { eng_node *n; int x, y; };
static struct draw_item s_draw[MAX_NODES];

/* collect visible sprite/label nodes with their world (accumulated) center position */
static int collect_draw(eng_node *n, float ox, float oy, int cnt)
{
	for (eng_node *c = n->_child; c; c = c->_sibling) {
		if (!c->visible) continue;           /* invisible hides this node AND its children */
		float wx = ox + c->x, wy = oy + c->y;
		if ((c->sheet || c->text || c->tiles) && cnt < MAX_NODES) {
			s_draw[cnt].n = c;
			s_draw[cnt].x = (int)(wx + 0.5f);
			s_draw[cnt].y = (int)(wy + 0.5f);
			cnt++;
		}
		cnt = collect_draw(c, wx, wy, cnt);
	}
	return cnt;
}

static int cmp_z(const void *a, const void *b)
{
	int za = ((const struct draw_item *)a)->n->z;
	int zb = ((const struct draw_item *)b)->n->z;
	return (za > zb) - (za < zb);
}

/* blit a sub-rectangle (sx,sy,sw,sh) of `im`, centered at screen (cx,cy), modulated by tint.
 * A plain sprite passes its whole extent; an animated node passes the current frame's cell. */
static void blit_region(canvas_frame *f, const eng_image *im, int cx, int cy,
                        int sx, int sy, int sw, int sh, eng_color t)
{
	int x0 = cx - sw / 2, y0 = cy - sh / 2;
	int plain = (t == ENG_WHITE);
	unsigned tr = (t >> 16) & 0xff, tg = (t >> 8) & 0xff, tb = t & 0xff;

	for (int j = 0; j < sh; j++)
		for (int i = 0; i < sw; i++) {
			int ax = sx + i, ay = sy + j;
			if (ax < 0 || ay < 0 || ax >= im->w || ay >= im->h) continue;   /* frame outside sheet */
			uint32_t p = im->argb[ay * im->w + ax];
			unsigned a = p >> 24;
			if (!a) continue;                /* transparent pixel */
			unsigned r = (p >> 16) & 0xff, g = (p >> 8) & 0xff, b = p & 0xff;
			if (!plain) { r = r * tr / 255; g = g * tg / 255; b = b * tb / 255; }
			eng__blend(f, x0 + i, y0 + j, (r << 16) | (g << 8) | b, a);
		}
}

/* copy one tp x tp tile from the atlas (source top-left sx,sy) to screen (dx,dy) */
static void blit_tile(canvas_frame *f, const eng_image *im, int dx, int dy, int sx, int sy, int tp)
{
	if (sx < 0 || sy < 0 || sx + tp > im->w || sy + tp > im->h) return;   /* invalid tile index */
	for (int j = 0; j < tp; j++) {
		int py = dy + j; if (py < 0 || py >= (int)f->height) continue;
		for (int i = 0; i < tp; i++) {
			int pxx = dx + i; if (pxx < 0 || pxx >= (int)f->width) continue;
			uint32_t p = im->argb[(sy + j) * im->w + (sx + i)];
			unsigned a = p >> 24;
			if (!a) continue;
			if (a == 255) {                      /* opaque: write straight through (fast path) */
				uint32_t *d = (uint32_t *)((char *)f->pixels + (size_t)py * f->stride) + pxx;
				*d = p & 0x00ffffff;
			} else {
				eng__blend(f, pxx, py, p & 0x00ffffff, a);
			}
		}
	}
}

/* draw the visible part of a tilemap whose top-left is at screen (ox,oy) */
static void draw_tilemap(canvas_frame *f, const eng_tilemap *m, int ox, int oy)
{
	int tp = m->tile_px, fw = (int)f->width, fh = (int)f->height;
	int c0 = ox < 0 ? (-ox) / tp : 0;            /* skip columns/rows fully off the left/top */
	int r0 = oy < 0 ? (-oy) / tp : 0;
	for (int r = r0; r < m->rows; r++) {
		int sy = oy + r * tp;
		if (sy >= fh) break;
		for (int c = c0; c < m->cols; c++) {
			int sx = ox + c * tp;
			if (sx >= fw) break;
			unsigned char id = m->cell[r * m->cols + c];
			if (!id) continue;
			if (m->tileset) {                    /* draw tile (id-1) from the atlas */
				int local = id - 1;
				blit_tile(f, m->tileset, sx, sy,
				          (local % m->atlas_cols) * tp, (local / m->atlas_cols) * tp, tp);
			} else {
				eng_rect_fill(sx, sy, tp, tp, m->pal[id]);   /* flat color fallback */
			}
		}
	}
}

/* draw one layer's tree, z-sorted, at base offset (ox,oy) — so layer order dominates z */
static void render_layer(canvas_frame *f, eng_node *rootn, float ox, float oy)
{
	int n = collect_draw(rootn, ox, oy, 0);
	qsort(s_draw, (size_t)n, sizeof s_draw[0], cmp_z);
	for (int i = 0; i < n; i++) {
		eng_node *nd = s_draw[i].n;
		if (nd->tiles)  draw_tilemap(f, nd->tiles, s_draw[i].x, s_draw[i].y);
		if (nd->sheet)  blit_region(f, nd->sheet, s_draw[i].x, s_draw[i].y,
		                            nd->frame * nd->fw, nd->row * nd->fh, nd->fw, nd->fh, nd->tint);
		if (nd->text)   eng_text(s_draw[i].x, s_draw[i].y, nd->text_px, nd->tint, nd->text);
	}
}

void eng__scene_render(void)
{
	canvas_frame *f = eng__frame();
	if (!f) return;
	ensure();
	render_layer(f, &g_layer[ENG_LAYER_BACKGROUND], 0.0f, 0.0f);
	render_layer(f, &g_layer[ENG_LAYER_WORLD], g_cam_x, g_cam_y);   /* camera scrolls WORLD */
	render_layer(f, &g_layer[ENG_LAYER_HUD], 0.0f, 0.0f);
}
