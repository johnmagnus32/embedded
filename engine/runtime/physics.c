/* engine/physics.c — minimal tile collision: move a node's AABB against the active solid
 * tilemap, resolving each axis and reporting which sides it hit. This is the guts of Godot's
 * CharacterBody2D.move_and_slide(): the game sets the collision world once (eng_set_tilemap),
 * then each frame sets a velocity and calls eng_move_and_collide — no hand-rolled snapping in
 * the game. Grid AABB only (no slopes/rotation yet). */
#include "engine.h"
#include <math.h>

static eng_tilemap *g_world;   /* the active solid tilemap (set by the game) */

void eng_set_tilemap(eng_tilemap *m) { g_world = m; }

/* Does the AABB centered at (cx,cy), size w x h, overlap any solid tile? */
static int box_hits(float cx, float cy, float w, float h)
{
	int tp = eng_tilemap_px(g_world);
	if (tp < 1) return 0;
	int c0 = (int)floorf((cx - w / 2) / tp), c1 = (int)floorf((cx + w / 2 - 1) / tp);
	int r0 = (int)floorf((cy - h / 2) / tp), r1 = (int)floorf((cy + h / 2 - 1) / tp);
	for (int r = r0; r <= r1; r++)
		for (int c = c0; c <= c1; c++)
			if (eng_tilemap_solid_at(g_world, c * tp + tp * 0.5f, r * tp + tp * 0.5f)) return 1;
	return 0;
}

int eng_move_and_collide(eng_node *n, float dx, float dy)
{
	if (!n) return 0;
	if (!g_world) { n->x += dx; n->y += dy; return 0; }   /* no world set -> free move */

	float w = (float)n->w, h = (float)n->h;
	int tp = eng_tilemap_px(g_world), flags = 0;

	n->x += dx;                                            /* horizontal, then resolve */
	if (box_hits(n->x, n->y, w, h)) {
		if (dx > 0)      { n->x = floorf((n->x + w / 2) / tp) * tp - w / 2.0f - 0.01f;       flags |= ENG_COL_RIGHT; }
		else if (dx < 0) { n->x = (floorf((n->x - w / 2) / tp) + 1) * tp + w / 2.0f + 0.01f; flags |= ENG_COL_LEFT;  }
	}
	n->y += dy;                                            /* vertical, then resolve */
	if (box_hits(n->x, n->y, w, h)) {
		if (dy > 0)      { n->y = floorf((n->y + h / 2) / tp) * tp - h / 2.0f - 0.01f;       flags |= ENG_COL_DOWN; }  /* landed */
		else if (dy < 0) { n->y = (floorf((n->y - h / 2) / tp) + 1) * tp + h / 2.0f + 0.01f; flags |= ENG_COL_UP;   }
	}
	return flags;
}

/* ---- 2D collision primitives (circle/segment) — see engine.h ------------------------- */

bool eng_circle_segment(float cx, float cy, float r, float ax, float ay, float bx, float by,
                        float *nx, float *ny, float *pen)
{
	float ex = bx - ax, ey = by - ay;              /* segment direction */
	float len2 = ex * ex + ey * ey;
	float t = 0.0f;
	if (len2 > 1e-6f) {                            /* project the center onto the segment, clamped */
		t = ((cx - ax) * ex + (cy - ay) * ey) / len2;
		if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
	}
	float px = ax + t * ex, py = ay + t * ey;      /* closest point on the segment */
	float dx = cx - px, dy = cy - py;
	float d2 = dx * dx + dy * dy;
	if (d2 >= r * r) return false;                 /* no overlap */
	float d = sqrtf(d2);
	if (d > 1e-6f) { *nx = dx / d; *ny = dy / d; } /* normal: segment -> center */
	else if (len2 > 1e-6f) {                       /* center exactly on the line: use its perpendicular */
		float el = sqrtf(len2);
		*nx = -ey / el; *ny = ex / el;
	} else { *nx = 0.0f; *ny = -1.0f; }            /* degenerate zero-length segment: arbitrary unit normal */
	*pen = r - d;
	return true;
}

/* Circle A (center ax,ay, radius ar) vs circle B (bx,by,br). Returns true on overlap; on hit sets
 * *nx,*ny = UNIT normal from B toward A (push A out along it) and *pen = penetration depth. */
bool eng_circle_circle(float ax, float ay, float ar, float bx, float by, float br,
                       float *nx, float *ny, float *pen)
{
	float dx = ax - bx, dy = ay - by, rr = ar + br;
	float d2 = dx * dx + dy * dy;
	if (d2 >= rr * rr) return false;
	float d = sqrtf(d2);
	if (d > 1e-6f) { *nx = dx / d; *ny = dy / d; } /* normal: B -> A */
	else           { *nx = 0.0f;  *ny = -1.0f; }   /* coincident centers: arbitrary unit normal */
	*pen = rr - d;
	return true;
}

void eng_reflect(float *vx, float *vy, float nx, float ny, float e)
{
	float vn = (*vx) * nx + (*vy) * ny;            /* velocity along the normal */
	if (vn >= 0.0f) return;                        /* already separating -> don't reflect (no sticking) */
	float j = (1.0f + e) * vn;
	*vx -= j * nx;
	*vy -= j * ny;
}

/* Resolve a collision between two EQUAL-MASS moving circles A and B (each radius r): separate them
 * (split the overlap) and exchange momentum along the contact normal with restitution e. Unlike
 * eng_reflect (which bounces one body off an immovable surface), this shares the impulse between
 * both movable bodies. Returns true if they were overlapping. */
bool eng_resolve_circles(float *ax, float *ay, float *avx, float *avy,
                         float *bx, float *by, float *bvx, float *bvy, float r, float e)
{
	float nx, ny, pen;
	if (!eng_circle_circle(*ax, *ay, r, *bx, *by, r, &nx, &ny, &pen)) return false;
	float hx = nx * pen * 0.5f, hy = ny * pen * 0.5f;    /* positional correction, split evenly */
	*ax += hx; *ay += hy; *bx -= hx; *by -= hy;
	float rvn = (*avx - *bvx) * nx + (*avy - *bvy) * ny; /* relative velocity along the normal */
	if (rvn >= 0.0f) return true;                        /* separating -> positions fixed, no impulse */
	float j = -(1.0f + e) * rvn * 0.5f;                  /* equal mass m=1 -> divide by (1/m + 1/m) = 2 */
	*avx += j * nx; *avy += j * ny;
	*bvx -= j * nx; *bvy -= j * ny;
	return true;
}
