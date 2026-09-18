/* engine/particles.c — a tiny SCREEN-SPACE particle pool, reusable across games (drift sparks,
 * grass dust, boost trails, hit-stars, exhaust, explosions). The game emits particles at projected
 * screen positions with a velocity + gravity; the engine ages them, lerps color c0->c1 over life,
 * shrinks the square, and draws. No allocation — a fixed pool; compacted on death. */
#include "engine.h"

#define PMAX 640
static struct { float x, y, vx, vy, gy, age, life, sz; eng_color c0, c1; } ps[PMAX];
static int pn;

void eng_particles_reset(void) { pn = 0; }

void eng_particle(float x, float y, float vx, float vy, float gy, float life, float sz, eng_color c0, eng_color c1)
{
	if (pn >= PMAX || life <= 0) return;
	ps[pn].x = x; ps[pn].y = y; ps[pn].vx = vx; ps[pn].vy = vy; ps[pn].gy = gy;
	ps[pn].age = 0; ps[pn].life = life; ps[pn].sz = sz; ps[pn].c0 = c0; ps[pn].c1 = c1; pn++;
}

void eng_particles_update(float dt)
{
	for (int i = 0; i < pn; ) {
		ps[i].age += dt;
		if (ps[i].age >= ps[i].life) { ps[i] = ps[--pn]; continue; }
		ps[i].vy += ps[i].gy * dt; ps[i].x += ps[i].vx * dt; ps[i].y += ps[i].vy * dt; i++;
	}
}

void eng_particles_draw(void)
{
	for (int i = 0; i < pn; i++) {
		float t = ps[i].age / ps[i].life;
		int r0 = (ps[i].c0 >> 16) & 0xff, g0 = (ps[i].c0 >> 8) & 0xff, b0 = ps[i].c0 & 0xff;
		int r1 = (ps[i].c1 >> 16) & 0xff, g1 = (ps[i].c1 >> 8) & 0xff, b1 = ps[i].c1 & 0xff;
		int r = r0 + (int)((r1 - r0) * t), g = g0 + (int)((g1 - g0) * t), b = b0 + (int)((b1 - b0) * t);
		int s = (int)(ps[i].sz * (1.0f - 0.55f * t)); if (s < 1) s = 1;
		eng_rect_fill((int)ps[i].x - s/2, (int)ps[i].y - s/2, s, s, ENG_RGB(r, g, b));
	}
}
