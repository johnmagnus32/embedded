/* engine/level.c — load a level (a solid tilemap + an optional non-solid decoration tilemap
 * + placed objects) from a Tiled JSON map (.tmj), so level design lives in a data file you
 * paint in the Tiled editor, not in source. JSON is parsed with cJSON (the repo's vendored
 * library, third_party/cJSON.{c,h}). Paths resolve under $CANVAS_ASSETS (else
 * /usr/share/canvas), like fonts and sprites.
 */
#include "engine.h"
#include "engine_internal.h"
#include "cJSON.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_OBJECTS 256

struct eng_level {
	eng_tilemap *map;        /* solid collision layer */
	eng_tilemap *bg;         /* optional non-solid decoration layer (drawn, not collided) */
	eng_object   obj[MAX_OBJECTS];
	int          nobj;
};

eng_tilemap      *eng_level_map(eng_level *l)                    { return l ? l->map : NULL; }
eng_tilemap      *eng_level_bg(eng_level *l)                     { return l ? l->bg : NULL; }
const eng_object *eng_level_objects(eng_level *l, int *count)    { if (count) *count = l ? l->nobj : 0;
                                                                   return l ? l->obj : NULL; }
void              eng_level_free(eng_level *l)                   { if (l) { eng_tilemap_free(l->map);
                                                                   eng_tilemap_free(l->bg); free(l); } }

/* ---- read a whole file (path resolved under $CANVAS_ASSETS) into a NUL-terminated buffer ---- */
static char *read_file(const char *path)
{
	char full[512];
	const char *p = path;
	if (path && path[0] != '/') {
		const char *base = getenv("CANVAS_ASSETS");
		if (!base || !*base) base = "/usr/share/canvas";
		snprintf(full, sizeof full, "%s/%s", base, path);
		p = full;
	}
	FILE *f = fopen(p, "rb");
	if (!f) { fprintf(stderr, "engine: can't open level %s\n", p ? p : "(null)"); return NULL; }
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	if (n < 0) { fclose(f); return NULL; }
	char *buf = malloc((size_t)n + 1);
	if (!buf) { fclose(f); return NULL; }
	size_t got = fread(buf, 1, (size_t)n, f);
	fclose(f);
	buf[got] = 0;
	return buf;
}

/* ---- small cJSON accessors ----------------------------------------------------------- */
static double num(const cJSON *v, double dflt) { return cJSON_IsNumber(v) ? v->valuedouble : dflt; }
static const char *str(const cJSON *v)         { return (cJSON_IsString(v) && v->valuestring) ? v->valuestring : ""; }
#define GET(o, k) cJSON_GetObjectItemCaseSensitive((o), (k))

/* read a tile layer's `data` array into a fresh id grid (gid -> 1-based tile index), or NULL. */
static unsigned char *read_tile_ids(const cJSON *layer, int w, int h, int firstgid)
{
	const cJSON *data = GET(layer, "data");
	if (!cJSON_IsArray(data) || cJSON_GetArraySize(data) < w * h) return NULL;
	unsigned char *ids = calloc((size_t)w * h, 1);
	if (!ids) return NULL;
	for (int i = 0; i < w * h; i++) {
		int gid = (int)num(cJSON_GetArrayItem(data, i), 0);
		int cell = gid ? gid - firstgid + 1 : 0;
		ids[i] = (unsigned char)(cell < 0 ? 0 : cell > 255 ? 255 : cell);
	}
	return ids;
}

/* build a tilemap from an id grid and, if given, attach its own copy of the atlas image. */
static eng_tilemap *build_map(int w, int h, int tw, const unsigned char *ids,
                              const eng_color *colors, int ncolors, const char *tileset_img, int atlas_cols)
{
	if (!ids) return NULL;
	eng_tilemap *m = eng_tilemap_from_ids(w, h, tw, ids, colors, ncolors);
	if (m && tileset_img) {
		eng_image *timg = eng_image_from_png(tileset_img);   /* one atlas copy per map */
		if (timg) eng_tilemap_set_tileset(m, timg, atlas_cols);
	}
	return m;
}

/* ---- Tiled .tmj loader (cJSON) ------------------------------------------------------- */
static eng_level *load_tiled(const char *text, int tile_px, const char *keys, const eng_color *colors)
{
	cJSON *root = cJSON_Parse(text);
	eng_level *L = calloc(1, sizeof *L);
	if (!root || !cJSON_IsObject(root) || !L) { cJSON_Delete(root); free(L); return NULL; }

	int w  = (int)num(GET(root, "width"), 0);
	int h  = (int)num(GET(root, "height"), 0);
	int tw = (int)num(GET(root, "tilewidth"), tile_px);   /* .tmj carries tilewidth; tile_px = fallback */

	/* optional embedded tileset: gid range (firstgid) + atlas image + columns */
	int firstgid = 1, atlas_cols = 1;
	const char *tileset_img = NULL;
	const cJSON *tilesets = GET(root, "tilesets");
	if (cJSON_IsArray(tilesets) && cJSON_GetArraySize(tilesets) > 0) {
		const cJSON *tset = cJSON_GetArrayItem(tilesets, 0);
		firstgid   = (int)num(GET(tset, "firstgid"), 1);
		atlas_cols = (int)num(GET(tset, "columns"), 1);
		const cJSON *im = GET(tset, "image");
		if (cJSON_IsString(im)) tileset_img = im->valuestring;
	}

	unsigned char *solid_ids = NULL, *bg_ids = NULL;
	const cJSON *layers = GET(root, "layers");
	if (w > 0 && h > 0 && cJSON_IsArray(layers)) {
		const cJSON *ly;
		cJSON_ArrayForEach(ly, layers) {
			const char *ts = str(GET(ly, "type"));
			if (!strcmp(ts, "tilelayer")) {
				unsigned char *g = read_tile_ids(ly, w, h, firstgid);
				if (g) {
					if (!strcmp(str(GET(ly, "name")), "bg") && !bg_ids) bg_ids = g;   /* decorations */
					else if (!solid_ids)                                solid_ids = g; /* collision */
					else free(g);
				}
			} else if (!strcmp(ts, "objectgroup")) {
				const cJSON *objs = GET(ly, "objects"), *ob;
				if (cJSON_IsArray(objs))
					cJSON_ArrayForEach(ob, objs) {
						if (L->nobj >= MAX_OBJECTS) break;
						eng_object *e = &L->obj[L->nobj++];
						strncpy(e->name, str(GET(ob, "name")), sizeof e->name - 1);
						e->name[sizeof e->name - 1] = 0;
						e->x = (float)num(GET(ob, "x"), 0);
						e->y = (float)num(GET(ob, "y"), 0);
					}
			}
		}
	}

	int ncol = (int)strlen(keys);
	L->map = build_map(w, h, tw, solid_ids, colors, ncol, tileset_img, atlas_cols);
	L->bg  = build_map(w, h, tw, bg_ids,    colors, ncol, tileset_img, atlas_cols);
	free(solid_ids); free(bg_ids);
	cJSON_Delete(root);
	if (!L->map) { eng_tilemap_free(L->bg); free(L); return NULL; }
	return L;
}

/* ---- public entry -------------------------------------------------------------------- */
eng_level *eng_level_load(const char *path, int tile_px, const char *keys, const eng_color *colors)
{
	char *text = read_file(path);
	if (!text) return NULL;
	eng_level *L = load_tiled(text, tile_px, keys, colors);
	free(text);
	if (L) fprintf(stderr, "engine: level %s %dx%d, %d objects\n", path,
	               eng_tilemap_cols(L->map), eng_tilemap_rows(L->map), L->nobj);
	return L;
}
