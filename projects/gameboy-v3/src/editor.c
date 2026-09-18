/* editor.c — the kart TRACK editor (E1: top-down map view), a canvas client viewable in the browser.
 *
 * Runs on the engine like a game (eng_run) via the canvas platform + canvasd, so you can edit on the
 * cloud desktop and view from a local browser. E1 is a READ-ONLY top-down map: it loads a track JSON
 * and draws the racing-line spline (coloured by biome), waypoints, and markers (ramp/boxes/bridge).
 * E2 adds the 3D WYSIWYG viewport + terrain sculpting (needs the shared kart-world extraction).
 *
 * Controls (current web input): L/R cycle track, UP/DOWN zoom, arrow/drag pans, A recenters.
 */
#include "engine.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define NSEG 240
#define MAXWP 40
#define MAXREG 12
#define MAXTBOX 12
enum { REG_GRASS, REG_MOUNT, REG_BEACH };

typedef struct {
	char name[32];
	int nwp, nregions, ramp_seg, nbox, has_bridge, bridge_from, bridge_to;
	float wp[MAXWP][2];
	struct { int from, to, type; } region[MAXREG];
	struct { int seg; } box[MAXTBOX];
} track;

static track T[8]; static int nT, sel;
static float px[NSEG], pz[NSEG];           /* resampled centreline (x,z only — top-down) */
static unsigned char sreg[NSEG];
static float camx, camz, zoom = 1.6f;      /* world->screen: screen = center + (world-cam)*zoom */
static int W, H, dragging, drag_px, drag_pz;

/* ---- minimal asset/JSON load (self-contained; cJSON is in libengine.a) ---- */
static char *slurp(const char *rel){
	const char *base = getenv("CANVAS_ASSETS"); if (!base || !*base) base = "/usr/share/canvas";
	char full[512]; snprintf(full, sizeof full, "%s/%s", base, rel);
	FILE *f = fopen(full, "rb"); if (!f) return NULL;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); if (n < 0) n = 0;
	char *b = malloc((size_t)n + 1); if (!b) { fclose(f); return NULL; }
	size_t rd = fread(b, 1, (size_t)n, f); b[rd] = 0; fclose(f); return b;
}
static double jnum(const cJSON *o, const char *k, double d){ const cJSON *x = o ? cJSON_GetObjectItem(o, k) : NULL; return (x && cJSON_IsNumber(x)) ? x->valuedouble : d; }

static int load_one(const char *file, track *t){
	char rel[160]; snprintf(rel, sizeof rel, "kart/tracks/%s", file);
	char *txt = slurp(rel); if (!txt) return 0;
	cJSON *j = cJSON_Parse(txt); free(txt); if (!j) return 0;
	memset(t, 0, sizeof *t);
	const cJSON *nm = cJSON_GetObjectItem(j, "name");
	snprintf(t->name, sizeof t->name, "%s", (nm && cJSON_IsString(nm)) ? nm->valuestring : "TRACK");
	const cJSON *wp = cJSON_GetObjectItem(j, "waypoints"), *pt;
	if (wp && cJSON_IsArray(wp)) cJSON_ArrayForEach(pt, wp){ if (t->nwp >= MAXWP) break;
		const cJSON *a = cJSON_GetArrayItem(pt, 0), *b = cJSON_GetArrayItem(pt, 1);
		if (a && b){ t->wp[t->nwp][0] = (float)a->valuedouble; t->wp[t->nwp][1] = (float)b->valuedouble; t->nwp++; } }
	const cJSON *rg = cJSON_GetObjectItem(j, "regions"), *rr;
	if (rg && cJSON_IsArray(rg)) cJSON_ArrayForEach(rr, rg){ if (t->nregions >= MAXREG) break;
		const cJSON *ty = cJSON_GetObjectItem(rr, "type"); const char *ts = (ty && cJSON_IsString(ty)) ? ty->valuestring : "grass";
		int tid = REG_GRASS; if (!strcmp(ts, "mountain")) tid = REG_MOUNT; else if (!strcmp(ts, "beach")) tid = REG_BEACH;
		t->region[t->nregions].from = (int)jnum(rr, "from", 0); t->region[t->nregions].to = (int)jnum(rr, "to", 0); t->region[t->nregions].type = tid; t->nregions++; }
	const cJSON *bx = cJSON_GetObjectItem(j, "boxes"), *b;
	if (bx && cJSON_IsArray(bx)) cJSON_ArrayForEach(b, bx){ if (t->nbox >= MAXTBOX) break; t->box[t->nbox].seg = (int)jnum(b, "seg", 0); t->nbox++; }
	const cJSON *rp = cJSON_GetObjectItem(j, "ramp"); t->ramp_seg = (int)jnum(rp, "seg", -1);
	const cJSON *br = cJSON_GetObjectItem(j, "bridge"); if (br && cJSON_IsObject(br)){ t->has_bridge = 1; t->bridge_from = (int)jnum(br, "from", 0); t->bridge_to = (int)jnum(br, "to", 0); }
	cJSON_Delete(j);
	return t->nwp >= 3;
}
static void load_tracks(void){
	nT = 0; char *txt = slurp("kart/tracks/index.json");
	if (txt){ cJSON *j = cJSON_Parse(txt); free(txt);
		if (j && cJSON_IsArray(j)){ const cJSON *e; cJSON_ArrayForEach(e, j){ if (nT >= 8) break; if (cJSON_IsString(e) && load_one(e->valuestring, &T[nT])) nT++; } }
		if (j) cJSON_Delete(j); }
}

/* ---- build the resampled centreline + per-segment biome for the current track (top-down) ---- */
static float cr1(float a, float b, float c, float e, float t){ float t2 = t * t, t3 = t2 * t; return 0.5f * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - e) * t2 + (-a + 3 * b - 3 * c + e) * t3); }
static void build(void){
	track *t = &T[sel]; int nwp = t->nwp; if (nwp < 3) return;
	static float dx[2600], dz[2600], cum[2601]; int n = 0, steps = 2400 / nwp;
	for (int w = 0; w < nwp; w++){ int i0 = (w - 1 + nwp) % nwp, i1 = w, i2 = (w + 1) % nwp, i3 = (w + 2) % nwp;
		for (int s = 0; s < steps; s++){ float u = (float)s / steps;
			dx[n] = cr1(t->wp[i0][0], t->wp[i1][0], t->wp[i2][0], t->wp[i3][0], u);
			dz[n] = cr1(t->wp[i0][1], t->wp[i1][1], t->wp[i2][1], t->wp[i3][1], u); n++; } }
	cum[0] = 0; for (int i = 0; i < n; i++){ int j = (i + 1) % n; float a = dx[j] - dx[i], b = dz[j] - dz[i]; cum[i + 1] = cum[i] + sqrtf(a * a + b * b); }
	float total = cum[n]; int di = 0;
	for (int i = 0; i < NSEG; i++){ float tg = total * i / NSEG; while (di < n - 1 && cum[di + 1] < tg) di++;
		float sl = cum[di + 1] - cum[di], f = sl > 1e-4f ? (tg - cum[di]) / sl : 0; int j = (di + 1) % n;
		px[i] = dx[di] + (dx[j] - dx[di]) * f; pz[i] = dz[di] + (dz[j] - dz[di]) * f; }
	for (int i = 0; i < NSEG; i++) sreg[i] = REG_GRASS;
	for (int r = 0; r < t->nregions; r++){ int a = t->region[r].from, b = t->region[r].to, ty = t->region[r].type;
		if (a < 0) a = 0;
		if (b > NSEG) b = NSEG;
		if (a <= b) for (int i = a; i < b; i++) sreg[i] = (unsigned char)ty;
		else { for (int i = a; i < NSEG; i++) sreg[i] = (unsigned char)ty; for (int i = 0; i < b; i++) sreg[i] = (unsigned char)ty; } }
	camx = 0; camz = 0;   /* recenter on the track's rough middle (waypoint 0 area) */
}

static void sx_sy(float wx, float wz, int *sx, int *sy){ *sx = W / 2 + (int)((wx - camx) * zoom); *sy = H / 2 + (int)((wz - camz) * zoom); }

static void on_init(void){ W = eng_width(); H = eng_height(); load_tracks(); if (nT) build(); }

static void on_update(float dt){ (void)dt;
	if (eng_just_pressed(ENG_RIGHT)) { sel = (sel + 1) % (nT ? nT : 1); build(); }
	if (eng_just_pressed(ENG_LEFT))  { sel = (sel - 1 + (nT ? nT : 1)) % (nT ? nT : 1); build(); }
	if (eng_pressed(ENG_UP))   zoom *= 1.0f + dt * 1.5f;
	if (eng_pressed(ENG_DOWN)) zoom *= 1.0f - dt * 1.5f;
	if (zoom < 0.3f) zoom = 0.3f;
	if (zoom > 8.0f) zoom = 8.0f;
	if (eng_just_pressed(ENG_A)) { camx = camz = 0; }
	int tx, ty, down = eng_pointer_pressed(); eng_pointer(&tx, &ty);   /* drag to pan */
	if (down && tx >= 0){
		if (dragging){ camx -= (tx - drag_px) / zoom; camz -= (ty - drag_pz) / zoom; }
		drag_px = tx; drag_pz = ty; dragging = 1;
	} else dragging = 0;
}

static eng_color reg_col(int r){ return r == REG_MOUNT ? ENG_RGB(150,150,158) : r == REG_BEACH ? ENG_RGB(224,206,158) : ENG_RGB(86,170,86); }

static void on_draw_background(void){
	eng_clear(ENG_RGB(28, 40, 56));
	if (!nT) return;
	track *t = &T[sel];
	for (int i = 0; i < NSEG; i++){ int j = (i + 1) % NSEG; int ax, ay, bx, by; sx_sy(px[i], pz[i], &ax, &ay); sx_sy(px[j], pz[j], &bx, &by);
		eng_color c = t->has_bridge && ((t->bridge_from <= t->bridge_to) ? (i >= t->bridge_from && i < t->bridge_to) : (i >= t->bridge_from || i < t->bridge_to)) ? ENG_RGB(70,140,210) : reg_col(sreg[i]);
		/* thick line = a few offset 1px lines */
		for (int o = -3; o <= 3; o++){ eng_line(ax, ay + o, bx, by + o, c); eng_line(ax + o, ay, bx + o, by, c); }
	}
	for (int w = 0; w < t->nwp; w++){ int sx, sy; sx_sy(t->wp[w][0], t->wp[w][1], &sx, &sy); eng_circle_fill(sx, sy, 4, ENG_RGB(240,240,250)); }
	for (int b = 0; b < t->nbox; b++){ int s = t->box[b].seg % NSEG, sx, sy; sx_sy(px[s], pz[s], &sx, &sy); eng_rect_fill(sx-4, sy-4, 8, 8, ENG_RGB(240,210,70)); }
	if (t->ramp_seg >= 0){ int s = t->ramp_seg % NSEG, sx, sy; sx_sy(px[s], pz[s], &sx, &sy); eng_circle_fill(sx, sy, 6, ENG_RGB(240,140,40)); }
	{ int sx, sy; sx_sy(px[0], pz[0], &sx, &sy); eng_circle_fill(sx, sy, 6, ENG_RGB(255,255,255)); eng_circle_fill(sx, sy, 3, ENG_RGB(30,30,30)); }
}

static void on_draw_overlay(void){
	eng_text(14, 12, 30, ENG_RGB(255,255,255), "TRACK EDITOR");
	char buf[96]; snprintf(buf, sizeof buf, "%s   (%d/%d)", nT ? T[sel].name : "no tracks", nT ? sel + 1 : 0, nT);
	eng_text(14, 48, 24, ENG_RGB(255,210,60), buf);
	eng_text_aligned(W - 14, 12, 20, ENG_RGB(150,170,200), ENG_ALIGN_RIGHT, "L/R track   UP/DOWN zoom   drag pan   A center");
}

int main(void){
	static const eng_game g = { .title = "Track Editor", .init = on_init, .update = on_update, .draw_background = on_draw_background, .draw_overlay = on_draw_overlay };
	return eng_run(&g);
}
