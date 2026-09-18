/* games/racer/racer.c — a pseudo-3D racer (OutRun / Pole-Position style). The road is a stack of
 * SEGMENTS projected by distance (near = wide, far = narrow) with eased CURVES + HILLS; roadside
 * trees are billboards scaled by distance via eng_tex_column. This is a NEW rendering technique for
 * the engine — perspective road projection + a painter's near->far pass with hill occlusion —
 * distinct from the raycaster's grid casting.
 *
 * Controls: A / UP / hold-touch = accelerate, B / DOWN = brake, LEFT/RIGHT (or touch x) = steer.
 * Off the road you slow down. The track loops. TAP / A / START to begin.
 */
#include "engine.h"
#include <stdio.h>
#include <math.h>

enum { S_TITLE, S_PLAY };

#define NSEG    2200
#define SEGLEN  200.0f
#define RUMBLE  4                 /* segments per road/grass colour block (the moving stripes) */
#define ROADW   1000.0f           /* half road width, world units */
#define CAMH    1200.0f           /* camera height above the road */
#define CAMD    0.84f             /* camera depth = 1/tan(fov/2) */
#define DRAW    140               /* draw distance, segments */
#define MAXSPD  (SEGLEN * 80.0f)
#define ACCEL   (MAXSPD / 4.0f)
#define BRAKEA  (MAXSPD / 2.0f)
#define DECEL   (MAXSPD / 6.0f)
#define OFFSPD  (MAXSPD / 4.0f)   /* speed cap while off-road */

#define C_SKY     ENG_RGB(112, 170, 236)
#define C_GRASS_L ENG_RGB(60, 160, 74)
#define C_GRASS_D ENG_RGB(52, 146, 66)
#define C_ROAD_L  ENG_RGB(104, 104, 112)
#define C_ROAD_D  ENG_RGB(96, 96, 104)
#define C_RUMB_L  ENG_RGB(232, 232, 232)
#define C_RUMB_D  ENG_RGB(200, 60, 60)
#define C_LANE    ENG_RGB(235, 235, 235)
#define C_HUD     ENG_RGB(255, 255, 255)
#define C_SHAD    ENG_RGB(40, 60, 40)

typedef struct { float y1, y2, curve; int tree; float toff; } Seg;
static Seg   seg[NSEG];
static int   W, H, st, nbuilt;
static float pos, speed, player_x, steer, g_t, dist, lastY;
static eng_image *img_car, *img_tree;
static eng_sound *mus;

/* ---- track build (eased curves + hills), the Jake-Gordon road builder ---- */
static float ez_io(float a, float b, float p) { return a + (b - a) * (-cosf(p * 3.14159265f) / 2 + 0.5f); }
static float ez_i (float a, float b, float p) { return a + (b - a) * p * p; }

static void add_seg(float curve, float y)
{
	if (nbuilt >= NSEG) return;
	Seg *s = &seg[nbuilt];
	s->y1 = lastY; s->y2 = y; s->curve = curve;
	s->tree = (nbuilt % 9 == 0);                          /* a tree every ~9 segments */
	s->toff = ((nbuilt / 9) % 2) ? 1.7f : -1.7f;          /* alternate sides, off the road */
	lastY = y; nbuilt++;
}
static void add_road(int enter, int hold, int leave, float curve, float dy)
{
	float startY = lastY, endY = lastY + dy, total = (float)(enter + hold + leave);
	for (int n = 0; n < enter; n++) add_seg(ez_i (0, curve, (float)n / enter),  ez_io(startY, endY, (float)n / total));
	for (int n = 0; n < hold;  n++) add_seg(curve,                              ez_io(startY, endY, (float)(enter + n) / total));
	for (int n = 0; n < leave; n++) add_seg(ez_io(curve, 0, (float)n / leave),  ez_io(startY, endY, (float)(enter + hold + n) / total));
}
static void build_track(void)
{
	lastY = 0; nbuilt = 0;
	add_road(30, 40, 30,  0.0f,     0);
	add_road(30, 60, 30,  2.5f,  1500);
	add_road(30, 60, 30, -3.0f, -1500);
	add_road(20, 40, 20,  0.0f,  2200);
	add_road(30, 80, 30,  4.0f,     0);
	add_road(30, 50, 30, -2.0f, -1200);
	add_road(20, 40, 20,  0.0f,     0);
	while (nbuilt < NSEG) add_seg(0, lastY);
}

/* ---- perspective projection: world (x=0 centerline, y=hill, z) -> screen ---- */
typedef struct { float sx, sy, sw, cz; } Proj;
static Proj project(float worldZ, float worldY, float camX, float camY, float camZ)
{
	Proj p; p.cz = worldZ - camZ;
	float cz = p.cz < 0.0001f ? 0.0001f : p.cz, scale = CAMD / cz;
	p.sx = W / 2.0f - scale * camX * (W / 2.0f);          /* camX carries player + accumulated curve */
	p.sy = H / 2.0f - scale * (worldY - camY) * (H / 2.0f);
	p.sw = scale * ROADW * (W / 2.0f);
	return p;
}

static void span(int y, float xl, float xr, eng_color c)
{
	if (y < 0 || y >= H) return;
	int a = xl < 0 ? 0 : (int)xl, b = xr > W ? W : (int)(xr + 0.5f);
	if (b > a) eng_rect_fill(a, y, b - a, 1, c);
}
static void render_seg(Proj *nr, Proj *fr, int dark)
{
	eng_color grass = dark ? C_GRASS_D : C_GRASS_L, road = dark ? C_ROAD_D : C_ROAD_L, rumb = dark ? C_RUMB_D : C_RUMB_L;
	int y2 = (int)fr->sy, y1 = (int)nr->sy;
	for (int y = y2; y < y1; y++) {
		if (y < 0) continue;
		if (y >= H) break;
		float t = (y1 == y2) ? 0 : (float)(y - y2) / (float)(y1 - y2);
		float cx = fr->sx + (nr->sx - fr->sx) * t, hw = fr->sw + (nr->sw - fr->sw) * t;
		span(y, 0, W, grass);
		span(y, cx - hw, cx + hw, road);
		float rw = hw * 0.12f; span(y, cx - hw, cx - hw + rw, rumb); span(y, cx + hw - rw, cx + hw, rumb);
		if (!dark) { float lw = hw * 0.018f; span(y, cx - lw, cx + lw, C_LANE); }
	}
}
static void draw_tree(float sx, float baseY, float scale, int clipY)
{
	if (!img_tree || baseY > clipY) return;               /* ground point hidden behind a hill */
	float destW = scale * ROADW * (W / 2.0f) * 1.1f;
	if (destW < 2 || destW > 4000) return;
	int iw = eng_image_w(img_tree), ih = eng_image_h(img_tree);
	float destH = destW * (float)ih / (float)iw;
	int x0 = (int)(sx - destW / 2), n = (int)destW, y0 = (int)(baseY - destH), y1 = (int)baseY;
	for (int c = 0; c < n; c++) {
		int scx = x0 + c; if (scx < 0 || scx >= W) continue;
		eng_tex_column(img_tree, scx, (int)(c * (float)iw / destW), y0, y1, 256, 1);
	}
}

static void on_init(void)
{
	W = eng_width(); H = eng_height(); st = S_TITLE;
	img_car  = eng_image_from_png("racer/car.png");
	img_tree = eng_image_from_png("racer/tree.png");
	mus      = eng_sound_load("sfx/music.wav");
	build_track();
	pos = 0; speed = 0; player_x = 0; dist = 0;
}

static void on_update(float dt)
{
	g_t += dt;
	if (st == S_TITLE &&
	    (eng_just_pressed(ENG_A) || eng_just_pressed(ENG_START) || eng_pointer_just_pressed())) {
		st = S_PLAY; eng_music_play(mus, 0.3f);
	}
}

static void on_fixed(float fdt)
{
	if (st != S_PLAY) return;
	int base = (int)(pos / SEGLEN) % NSEG;
	float curve = seg[base].curve;

	int accel = eng_pressed(ENG_A) || eng_pressed(ENG_UP) || eng_pointer_pressed();
	int brake = eng_pressed(ENG_B) || eng_pressed(ENG_DOWN);
	steer = 0;
	if (eng_pressed(ENG_LEFT))       steer = -1;
	else if (eng_pressed(ENG_RIGHT)) steer =  1;
	else if (eng_pointer_pressed())  { int px, py; eng_pointer(&px, &py); steer = ((float)px - W / 2) / (W / 2.0f); }

	if (accel)      speed += ACCEL * fdt;
	else if (brake) speed -= BRAKEA * fdt;
	else            speed -= DECEL * fdt;
	if (speed < 0) speed = 0;
	if (speed > MAXSPD) speed = MAXSPD;

	float spct = speed / MAXSPD;
	player_x += steer * fdt * 2.2f * spct;                /* steering scales with speed */
	player_x -= curve * spct * spct * 0.0006f;            /* centrifugal push out of the curve */
	if ((player_x < -1.0f || player_x > 1.0f) && speed > OFFSPD) {   /* off-road drag */
		speed -= DECEL * 2.5f * fdt; if (speed < OFFSPD) speed = OFFSPD;
	}
	if (player_x < -2.2f) player_x = -2.2f;
	if (player_x > 2.2f) player_x = 2.2f;

	pos += speed * fdt; dist += speed * fdt;
	if (pos >= NSEG * SEGLEN) pos -= NSEG * SEGLEN;        /* loop the track */
}

static void on_draw_background(void)
{
	eng_clear(C_SKY);
	int base = (int)(pos / SEGLEN);
	float basePct = fmodf(pos, SEGLEN) / SEGLEN;
	float camY = CAMH + ez_io(seg[base % NSEG].y1, seg[base % NSEG].y2, basePct);
	float x = 0, dx = -(seg[base % NSEG].curve * basePct), maxy = H;

	float vsy[DRAW], vsc[DRAW], vcamx[DRAW]; int vclip[DRAW], vidx[DRAW], vN = 0;
	for (int n = 0; n < DRAW; n++) {
		int idx = (base + n) % NSEG; Seg *sg = &seg[idx];
		float camX = player_x * ROADW - x;
		float zn = (float)(base + n) * SEGLEN, zf = (float)(base + n + 1) * SEGLEN;
		Proj p1 = project(zn, sg->y1, camX, camY, pos);
		Proj p2 = project(zf, sg->y2, player_x * ROADW - x - dx, camY, pos);
		x += dx; dx += sg->curve;
		vsy[n] = p1.sy; vcamx[n] = camX; vidx[n] = idx; vclip[n] = (int)maxy;
		vsc[n] = CAMD / (zn - pos > 0.0001f ? zn - pos : 0.0001f); vN = n + 1;
		if (p1.cz <= 0 || p2.sy >= maxy) continue;
		render_seg(&p1, &p2, (idx / RUMBLE) % 2);
		maxy = p2.sy;
	}
	for (int n = vN - 1; n >= 0; n--) {                   /* trees: far -> near so nearer overdraw */
		Seg *sg = &seg[vidx[n]];
		if (!sg->tree) continue;
		float tsx = W / 2.0f - vsc[n] * (vcamx[n] - sg->toff * ROADW) * (W / 2.0f);
		draw_tree(tsx, vsy[n], vsc[n], vclip[n]);
	}

	if (img_car) {                                        /* player car — small steer lean + bob */
		int ch = eng_image_h(img_car);
		int cx = W / 2 + (int)(steer * 14);
		int cy = H - ch / 2 - 12 + (int)(sinf(g_t * 22) * (speed > 1 ? 2 : 0));
		eng_draw_image(img_car, cx, cy, ENG_WHITE);
	}
}

static void on_draw_overlay(void)
{
	char buf[32];
	snprintf(buf, sizeof buf, "%d km/h", (int)(speed / MAXSPD * 220));
	eng_text(16, 12, 28, C_HUD, buf);
	snprintf(buf, sizeof buf, "%d m", (int)(dist / 100));
	eng_text_aligned(W - 16, 12, 26, C_HUD, ENG_ALIGN_RIGHT, buf);
	if (st == S_TITLE) {
		eng_text_aligned(W / 2, H / 2 - 70, 74, C_HUD, ENG_ALIGN_CENTER, "RACER");
		eng_text_aligned(W / 2, H / 2 + 6, 24, C_HUD, ENG_ALIGN_CENTER, "A / UP / hold = gas   LEFT/RIGHT = steer");
		eng_text_aligned(W / 2, H / 2 + 44, 28, C_HUD, ENG_ALIGN_CENTER, "TAP / A TO START");
	}
}

int main(void)
{
	static const eng_game game = {
		.title = "Racer", .init = on_init, .update = on_update,
		.fixed_update = on_fixed, .draw_background = on_draw_background, .draw_overlay = on_draw_overlay,
	};
	return eng_run(&game);
}
