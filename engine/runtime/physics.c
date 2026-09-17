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
