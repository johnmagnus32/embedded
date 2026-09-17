/* engine/engine_internal.h — shared across the engine's translation units (engine.c,
 * text.c, scene.c), NOT part of the public engine API (games never include this). */
#ifndef ENGINE_INTERNAL_H
#define ENGINE_INTERNAL_H

#include "engine.h"
#include "canvas.h"

/* The frame currently being drawn (valid only during rendering), else NULL. (engine.c) */
canvas_frame *eng__frame(void);

/* One gamma-correct src-over-dst pixel: composite `rgb` over the frame at (x,y) with
 * coverage/alpha a in [0,255]. Used by both text.c and scene.c. (engine.c) */
void eng__blend(canvas_frame *f, int x, int y, eng_color rgb, unsigned a);

/* Called once by eng_run() at startup to load the vector font. (text.c) */
void eng_font_init(void);

/* Per-frame scene-graph hooks driven by eng_run(). (scene.c) */
void eng__scene_update(float dt);   /* run node update callbacks, then reap freed nodes */
void eng__scene_render(void);       /* draw visible sprites into the current frame, by z */

/* Build a tilemap from an id grid (0 = empty, 1..ncolors = solid). Used by level.c's Tiled
 * loader; ids is copied. (scene.c) */
eng_tilemap *eng_tilemap_from_ids(int cols, int rows, int tile_px,
                                  const unsigned char *ids, const eng_color *colors, int ncolors);

/* Attach an atlas image to a tilemap (draw tiles from it instead of flat colors); takes
 * ownership of img. Used by level.c's Tiled loader. (scene.c) */
void eng_tilemap_set_tileset(eng_tilemap *m, eng_image *img, int atlas_cols);

#endif /* ENGINE_INTERNAL_H */
