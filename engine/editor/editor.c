/* engine/editor/editor.c — a 2D WORLD BUILDER (E2b).
 *
 * Paints Tiled .tmj tile maps (a solid collision layer + a `bg` decoration layer) and places named
 * entity objects, then saves a map that eng_level_load() runs unchanged — so the loop is
 * paint -> save -> run the game -> play your level. Runs as a native app on the SDL host platform
 * (platform/sdl). Tiles are drawn as flat palette colors (a loaded map's tileset block is preserved
 * verbatim so the GAME still renders it textured).
 *
 * Run: ./build/editor <path-to.tmj>   (loads it if present, else starts a blank map; saves back to it)
 * Controls: Pan/Paint/Object tools + palette in the toolbar; drag=pan, scroll/pinch/Up-Down=zoom. */
#include "engine.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAXW 200
#define MAXH 200
#define MAXOBJ 256
#define TBH 54                    /* toolbar height (px) */

/* ---- editable map model (id grid; id 0 = empty, else 1-based tile index == Tiled gid) ---- */
static int  mapw = 40, maph = 23, tp = 32;
static unsigned char solid[MAXW * MAXH];
static unsigned char bg[MAXW * MAXH];
static struct { char name[24]; float x, y; } obj[MAXOBJ];
static int  nobj;
static char mappath[512];
static cJSON *g_tilesets;          /* preserved verbatim from a loaded map (keeps its textures) */

/* ---- palette: sel 0 = eraser, 1..NPAL = tile index. Colors are the EDITOR's view only; the game
 *      maps the same index to its own TILE_COLORS, so only the index we store matters. ---- */
static const struct { const char *name; eng_color col; } PAL[] = {
	{ "grass", ENG_RGB( 84, 120,  64) },
	{ "wood",  ENG_RGB(150, 124,  96) },
	{ "stone", ENG_RGB(120, 124, 130) },
	{ "sand",  ENG_RGB(214, 196, 140) },
	{ "water", ENG_RGB( 60, 110, 180) },
	{ "path",  ENG_RGB(170, 150, 110) },
	{ "tree",  ENG_RGB( 40,  92,  56) },
	{ "wall",  ENG_RGB( 96,  84,  76) },
};
#define NPAL ((int)(sizeof PAL / sizeof PAL[0]))
static const char *OBJTYPES[] = { "player", "enemy", "coin", "exit" };
#define NOBJT ((int)(sizeof OBJTYPES / sizeof OBJTYPES[0]))

/* ---- view + tool state ---- */
enum { TOOL_PAN, TOOL_PAINT, TOOL_OBJ };
static int   W, H;
static float camx, camy, zoom = 1.0f;   /* camx/camy = world-px at screen top-left */
static int   tool = TOOL_PAN, layer, sel = 1, selobj;
static int   pdown, dpx, dpy, dragging;
static int   save_flash;

/* ---- small cJSON accessors (mirror level.c) ---- */
static double num(const cJSON *v, double d) { return cJSON_IsNumber(v) ? v->valuedouble : d; }
static const char *str(const cJSON *v)      { return (cJSON_IsString(v) && v->valuestring) ? v->valuestring : ""; }
#define GET(o, k) cJSON_GetObjectItemCaseSensitive((o), (k))

static void clamp_zoom(void) { if (zoom < 0.15f) zoom = 0.15f; if (zoom > 4.0f) zoom = 4.0f; }

/* zoom by `f` keeping the world point under (ax,ay) fixed on screen */
static void zoom_at(float f, int ax, int ay)
{
	float wx = ax / zoom + camx, wy = ay / zoom + camy;
	zoom *= f; clamp_zoom();
	camx = wx - ax / zoom; camy = wy - ay / zoom;
}

static int hit(int x, int y, int w, int h, int mx, int my) { return mx >= x && mx < x + w && my >= y && my < y + h; }

/* screen (sx,sy) -> map cell; returns 1 and fills *cx,*cy if inside the map */
static int cell_at(int sx, int sy, int *cx, int *cy)
{
	int wx = (int)(sx / zoom + camx), wy = (int)(sy / zoom + camy);
	if (wx < 0 || wy < 0) return 0;
	int c = wx / tp, r = wy / tp;
	if (c < 0 || c >= mapw || r < 0 || r >= maph) return 0;
	*cx = c; *cy = r; return 1;
}

/* ---- load an existing .tmj into the editable model (best-effort; leaves a blank map on failure) ---- */
static char *slurp(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	if (n < 0) { fclose(f); return NULL; }
	char *b = malloc((size_t)n + 1);
	if (!b) { fclose(f); return NULL; }
	size_t got = fread(b, 1, (size_t)n, f); fclose(f); b[got] = 0;
	return b;
}
static void read_layer(const cJSON *ly, int firstgid)
{
	const cJSON *data = GET(ly, "data");
	if (!cJSON_IsArray(data)) return;
	int isbg = !strcmp(str(GET(ly, "name")), "bg");
	unsigned char *g = isbg ? bg : solid;
	int n = mapw * maph, i = 0;
	const cJSON *v;
	cJSON_ArrayForEach(v, data) {
		if (i >= n) break;
		int gid = (int)num(v, 0), cell = gid ? gid - firstgid + 1 : 0;
		g[i++] = (unsigned char)(cell < 0 ? 0 : cell > 255 ? 255 : cell);
	}
}
static void load_map(const char *path)
{
	char *text = slurp(path);
	if (!text) { fprintf(stderr, "editor: new map (no file at %s)\n", path); return; }
	cJSON *root = cJSON_Parse(text); free(text);
	if (!root) { fprintf(stderr, "editor: %s is not valid JSON — starting blank\n", path); return; }

	int w = (int)num(GET(root, "width"), mapw), h = (int)num(GET(root, "height"), maph);
	mapw = w < 1 ? 1 : w > MAXW ? MAXW : w;
	maph = h < 1 ? 1 : h > MAXH ? MAXH : h;
	tp = (int)num(GET(root, "tilewidth"), tp);

	int firstgid = 1;
	const cJSON *tsets = GET(root, "tilesets");
	if (cJSON_IsArray(tsets) && cJSON_GetArraySize(tsets) > 0) {
		g_tilesets = cJSON_Duplicate(tsets, 1);           /* keep textures for the game */
		firstgid = (int)num(GET(cJSON_GetArrayItem(tsets, 0), "firstgid"), 1);
	}
	const cJSON *layers = GET(root, "layers"), *ly;
	if (cJSON_IsArray(layers)) cJSON_ArrayForEach(ly, layers) {
		const char *t = str(GET(ly, "type"));
		if (!strcmp(t, "tilelayer")) {
			read_layer(ly, firstgid);
		} else if (!strcmp(t, "objectgroup")) {
			const cJSON *objs = GET(ly, "objects"), *ob;
			if (cJSON_IsArray(objs)) cJSON_ArrayForEach(ob, objs) {
				if (nobj >= MAXOBJ) break;
				strncpy(obj[nobj].name, str(GET(ob, "name")), sizeof obj[nobj].name - 1);
				obj[nobj].x = (float)num(GET(ob, "x"), 0);
				obj[nobj].y = (float)num(GET(ob, "y"), 0);
				nobj++;
			}
		}
	}
	cJSON_Delete(root);
	fprintf(stderr, "editor: loaded %s  %dx%d  %d objects\n", path, mapw, maph, nobj);
}

/* ---- save the model back out as a Tiled .tmj that eng_level_load() reads ---- */
static void add_tilelayer(cJSON *layers, const char *name, const unsigned char *g, int id)
{
	cJSON *L = cJSON_CreateObject();
	cJSON_AddStringToObject(L, "name", name);
	cJSON_AddStringToObject(L, "type", "tilelayer");
	cJSON_AddNumberToObject(L, "width", mapw);
	cJSON_AddNumberToObject(L, "height", maph);
	cJSON_AddNumberToObject(L, "x", 0);
	cJSON_AddNumberToObject(L, "y", 0);
	cJSON_AddNumberToObject(L, "opacity", 1);
	cJSON_AddBoolToObject(L, "visible", 1);
	cJSON_AddNumberToObject(L, "id", id);
	int n = mapw * maph, *d = malloc((size_t)n * sizeof *d);
	if (d) {
		for (int i = 0; i < n; i++) d[i] = g[i];   /* gid == our index (firstgid 1) */
		cJSON_AddItemToObject(L, "data", cJSON_CreateIntArray(d, n));
		free(d);
	}
	cJSON_AddItemToArray(layers, L);
}
static void save_map(void)
{
	if (!mappath[0]) return;
	cJSON *root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "width", mapw);
	cJSON_AddNumberToObject(root, "height", maph);
	cJSON_AddNumberToObject(root, "tilewidth", tp);
	cJSON_AddNumberToObject(root, "tileheight", tp);
	cJSON_AddStringToObject(root, "orientation", "orthogonal");
	cJSON_AddStringToObject(root, "type", "map");
	cJSON_AddBoolToObject(root, "infinite", 0);
	cJSON_AddStringToObject(root, "version", "1.10");
	cJSON_AddNumberToObject(root, "nextlayerid", 4);
	cJSON_AddNumberToObject(root, "nextobjectid", nobj + 1);
	if (g_tilesets) cJSON_AddItemToObject(root, "tilesets", cJSON_Duplicate(g_tilesets, 1));

	cJSON *layers = cJSON_CreateArray();
	cJSON_AddItemToObject(root, "layers", layers);
	add_tilelayer(layers, "bg", bg, 1);
	add_tilelayer(layers, "solid", solid, 2);

	cJSON *og = cJSON_CreateObject();
	cJSON_AddStringToObject(og, "name", "entities");
	cJSON_AddStringToObject(og, "type", "objectgroup");
	cJSON_AddNumberToObject(og, "id", 3);
	cJSON *objs = cJSON_CreateArray();
	cJSON_AddItemToObject(og, "objects", objs);
	for (int i = 0; i < nobj; i++) {
		cJSON *o = cJSON_CreateObject();
		cJSON_AddNumberToObject(o, "id", i + 1);
		cJSON_AddStringToObject(o, "name", obj[i].name);
		cJSON_AddNumberToObject(o, "x", obj[i].x);
		cJSON_AddNumberToObject(o, "y", obj[i].y);
		cJSON_AddNumberToObject(o, "width", 0);
		cJSON_AddNumberToObject(o, "height", 0);
		cJSON_AddItemToArray(objs, o);
	}
	cJSON_AddItemToArray(layers, og);

	char *txt = cJSON_Print(root);
	FILE *f = fopen(mappath, "wb");
	if (f && txt) { fwrite(txt, 1, strlen(txt), f); fclose(f); fprintf(stderr, "editor: saved %s\n", mappath); }
	else fprintf(stderr, "editor: SAVE FAILED for %s\n", mappath);
	if (txt) free(txt);
	cJSON_Delete(root);
}

/* ---- lifecycle ---- */
static void on_init(void)
{
	W = eng_width(); H = eng_height();
	float zx = (float)W / (mapw * tp), zy = (float)(H - TBH) / (maph * tp);
	zoom = (zx < zy ? zx : zy) * 0.92f; clamp_zoom();
	camx = mapw * tp / 2.0f - (W / zoom) / 2.0f;
	camy = maph * tp / 2.0f - (H / zoom) / 2.0f;
}

static void toolbar_click(int mx, int my)
{
	if      (hit(  8, 6, 40, 20, mx, my)) tool = TOOL_PAN;
	else if (hit( 52, 6, 48, 20, mx, my)) tool = TOOL_PAINT;
	else if (hit(104, 6, 56, 20, mx, my)) tool = TOOL_OBJ;
	else if (hit(168, 6, 92, 20, mx, my)) layer ^= 1;
	else if (hit(W - 64, 6, 56, 20, mx, my)) { save_map(); save_flash = 90; }
	else if (my >= 28 && my < 48) {           /* palette / object-type row */
		if (tool == TOOL_OBJ) {
			for (int j = 0; j < NOBJT; j++) if (hit(8 + j * 78, 28, 74, 20, mx, my)) selobj = j;
		} else {
			for (int j = 0; j <= NPAL; j++) if (hit(8 + j * 34, 28, 30, 20, mx, my)) sel = j;
		}
	}
}

static void on_update(float dt)
{
	int down = eng_pointer_pressed(), tx, ty;
	eng_pointer(&tx, &ty);
	int just = down && !pdown;

	float wd = eng_wheel();                       /* scroll / trackpad pinch -> zoom at cursor */
	if (wd != 0.0f) zoom_at(1.0f - wd * 0.001f, tx < 0 ? W / 2 : tx, ty < 0 ? H / 2 : ty);
	if (eng_pressed(ENG_UP))   zoom_at(1.0f + dt * 1.5f, W / 2, H / 2);
	if (eng_pressed(ENG_DOWN)) zoom_at(1.0f - dt * 1.5f, W / 2, H / 2);

	if (just) { dpx = tx; dpy = ty; }
	if (just && ty < TBH) {                       /* toolbar takes the click; no map action */
		toolbar_click(tx, ty);
		dragging = 0;
	} else if (tool == TOOL_PAN && down && ty >= TBH) {
		if (dragging) { camx -= (tx - dpx) / zoom; camy -= (ty - dpy) / zoom; }
		dpx = tx; dpy = ty; dragging = 1;
	} else if (tool == TOOL_PAINT && down && ty >= TBH) {
		int cx, cy;
		if (cell_at(tx, ty, &cx, &cy)) (layer ? bg : solid)[cy * mapw + cx] = (unsigned char)sel;
	} else if (tool == TOOL_OBJ && just && ty >= TBH && nobj < MAXOBJ) {
		int cx, cy;
		if (cell_at(tx, ty, &cx, &cy)) {
			snprintf(obj[nobj].name, sizeof obj[nobj].name, "%s", OBJTYPES[selobj]);
			obj[nobj].x = cx * tp + tp / 2.0f; obj[nobj].y = cy * tp + tp / 2.0f;
			nobj++;
		}
	}
	if (!down) dragging = 0;

	if (eng_just_pressed(ENG_START)) { save_map(); save_flash = 90; }   /* Enter = save */
	if (eng_just_pressed(ENG_A)) on_init();                            /* recenter/fit */
	if (save_flash > 0) save_flash--;
	pdown = down;
}

static eng_color dim(eng_color c)   /* darken for the bg layer */
{
	int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
	return ENG_RGB(r * 6 / 10, g * 6 / 10, b * 6 / 10);
}
static void draw_layer(const unsigned char *grid, int isbg)
{
	int c0 = (int)(camx / tp) - 1, c1 = (int)((camx + W / zoom) / tp) + 1;
	int r0 = (int)(camy / tp) - 1, r1 = (int)((camy + H / zoom) / tp) + 1;
	if (c0 < 0) c0 = 0;
	if (r0 < 0) r0 = 0;
	if (c1 >= mapw) c1 = mapw - 1;
	if (r1 >= maph) r1 = maph - 1;
	int s = (int)(tp * zoom) + 1;
	for (int r = r0; r <= r1; r++) for (int c = c0; c <= c1; c++) {
		int id = grid[r * mapw + c];
		if (!id) continue;
		eng_color col = id <= NPAL ? PAL[id - 1].col : ENG_RGB(200, 60, 200);
		int sx = (int)((c * tp - camx) * zoom), sy = (int)((r * tp - camy) * zoom);
		eng_rect_fill(sx, sy, s, s, isbg ? dim(col) : col);
	}
}
static void draw_toolbar(void)
{
	eng_rect_fill(0, 0, W, TBH, ENG_RGB(30, 34, 42));
	eng_line(0, TBH, W, TBH, ENG_RGB(80, 88, 100));
	const char *tn[] = { "PAN", "PAINT", "OBJ" };
	int bx[] = { 8, 52, 104 }, bw[] = { 40, 48, 56 };
	for (int i = 0; i < 3; i++) {
		eng_rect_fill(bx[i], 6, bw[i], 20, tool == i ? ENG_RGB(70, 110, 170) : ENG_RGB(48, 54, 66));
		eng_rect(bx[i], 6, bw[i], 20, ENG_RGB(90, 98, 112));
		eng_text(bx[i] + 6, 9, 14, ENG_RGB(230, 235, 245), tn[i]);
	}
	char lb[24]; snprintf(lb, sizeof lb, "layer: %s", layer ? "bg" : "solid");
	eng_rect_fill(168, 6, 92, 20, ENG_RGB(48, 54, 66)); eng_rect(168, 6, 92, 20, ENG_RGB(90, 98, 112));
	eng_text(174, 9, 14, ENG_RGB(230, 235, 245), lb);
	eng_rect_fill(W - 64, 6, 56, 20, ENG_RGB(60, 130, 80)); eng_rect(W - 64, 6, 56, 20, ENG_RGB(90, 160, 110));
	eng_text(W - 56, 9, 14, ENG_RGB(240, 245, 240), "SAVE");

	if (tool == TOOL_OBJ) {
		for (int j = 0; j < NOBJT; j++) {
			eng_rect_fill(8 + j * 78, 28, 74, 20, selobj == j ? ENG_RGB(70, 110, 170) : ENG_RGB(48, 54, 66));
			eng_rect(8 + j * 78, 28, 74, 20, ENG_RGB(90, 98, 112));
			eng_text(8 + j * 78 + 6, 31, 14, ENG_RGB(230, 235, 245), OBJTYPES[j]);
		}
	} else {
		for (int j = 0; j <= NPAL; j++) {
			int x = 8 + j * 34;
			eng_rect_fill(x, 28, 30, 20, j == 0 ? ENG_RGB(24, 26, 30) : PAL[j - 1].col);
			eng_rect(x, 28, 30, 20, sel == j ? ENG_RGB(255, 220, 80) : ENG_RGB(90, 98, 112));
			if (j == 0) eng_text(x + 8, 31, 14, ENG_RGB(200, 120, 120), "X");
		}
	}
}
static void on_draw(void)
{
	eng_clear(ENG_RGB(18, 20, 26));
	/* map extent backdrop */
	int mx0 = (int)((0 - camx) * zoom), my0 = (int)((0 - camy) * zoom);
	eng_rect_fill(mx0, my0, (int)(mapw * tp * zoom), (int)(maph * tp * zoom), ENG_RGB(28, 32, 40));

	draw_layer(bg, 1);
	draw_layer(solid, 0);

	if (tp * zoom > 5) {   /* grid lines when cells are big enough to matter */
		for (int c = 0; c <= mapw; c++) {
			int sx = (int)((c * tp - camx) * zoom);
			eng_line(sx, my0, sx, my0 + (int)(maph * tp * zoom), ENG_RGB(44, 50, 62));
		}
		for (int r = 0; r <= maph; r++) {
			int sy = (int)((r * tp - camy) * zoom);
			eng_line(mx0, sy, mx0 + (int)(mapw * tp * zoom), sy, ENG_RGB(44, 50, 62));
		}
	}

	for (int i = 0; i < nobj; i++) {
		int sx = (int)((obj[i].x - camx) * zoom), sy = (int)((obj[i].y - camy) * zoom);
		eng_circle_fill(sx, sy, 6, ENG_RGB(255, 180, 60));
		eng_text(sx + 8, sy - 8, 14, ENG_RGB(255, 220, 150), obj[i].name);
	}

	int tx, ty;   /* hovered-cell highlight */
	eng_pointer(&tx, &ty);
	int cx, cy;
	if (tx >= 0 && ty >= TBH && cell_at(tx, ty, &cx, &cy)) {
		int sx = (int)((cx * tp - camx) * zoom), sy = (int)((cy * tp - camy) * zoom), s = (int)(tp * zoom);
		eng_rect(sx, sy, s, s, ENG_RGB(255, 220, 80));
	}

	draw_toolbar();

	char buf[128];
	snprintf(buf, sizeof buf, "%s   %dx%d  tile %d   zoom %.2fx   drag=pan  scroll/pinch=zoom  Enter=save  A=fit",
	         mappath[0] ? mappath : "(no file)", mapw, maph, tp, zoom);
	eng_text(8, H - 20, 14, ENG_RGB(150, 200, 255), buf);
	if (save_flash > 0) eng_text(W / 2 - 30, H / 2, 22, ENG_RGB(120, 240, 140), "saved");
}

int main(int argc, char **argv)
{
	if (argc > 1) {
		snprintf(mappath, sizeof mappath, "%s", argv[1]);
		load_map(mappath);
	} else {
		fprintf(stderr, "editor: no map path given — blank map, SAVE will be a no-op.\n"
		                "        usage: %s <path-to.tmj>\n", argv[0]);
	}
	static const eng_game g = { .title = "World Builder", .init = on_init, .update = on_update, .draw_background = on_draw };
	return eng_run(&g);
}
