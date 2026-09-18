/* engine.h — the gameboy-v3 2D game engine (the Unity/Godot-2D analog),
 * a plain C library (no scripting VM). A game hands eng_run() three callbacks and the
 * engine owns everything below: connecting to the compositor, the ~60 Hz loop,
 * per-frame input state, frame pacing, and presenting. The game just writes
 * init/update/draw + high-level draw calls — no pixels, no sockets, no loop.
 *
 * Layer: sits ON TOP of the canvas SDK (libcanvas). game.c -> engine -> libcanvas
 * -> the compositor. (See src/README.md for the layer stack.)
 *
 * IMPLEMENTED: an immediate layer (loop + shapes + input + antialiased text) AND a
 * retained SCENE GRAPH (nodes with a transform + sprite + per-frame behavior — the
 * Unity/Godot-2D model), with PNG sprite loading. NEXT: tilemaps, audio.
 */
#ifndef GV3GAME_H
#define GV3GAME_H

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t eng_color;                 /* 0x00RRGGBB (XRGB8888, matches the compositor) */
#define ENG_RGB(r, g, b) ((eng_color)(((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b)))
#define ENG_BLACK ENG_RGB(0, 0, 0)
#define ENG_WHITE ENG_RGB(255, 255, 255)

/* Canonical gamepad — same order as the compositor's RetroPad (canvas_button), so a
 * game speaks ENG_* and never touches canvas internals. */
typedef enum {
	ENG_UP, ENG_DOWN, ENG_LEFT, ENG_RIGHT,
	ENG_A,  ENG_B,    ENG_X,    ENG_Y,
	ENG_L,  ENG_R,    ENG_START, ENG_SELECT,
	ENG_COUNT
} eng_button;

/* A game = these callbacks. init() runs once; then each frame: update(dt), the scene nodes'
 * own updates, then rendering: draw_background() [immediate, behind everything] -> the scene
 * LAYERS (BACKGROUND, then WORLD offset by the camera, then HUD) -> draw_overlay() [immediate,
 * on top of everything]. Put most content in scene layers; the two immediate hooks are for a
 * procedural backdrop and quick screen-space drawing. Any callback may be NULL. */
typedef struct {
	const char *title;               /* app name (unused yet; forward-looking) */
	void (*init)(void);
	void (*update)(float dt);        /* global per-frame logic; dt = seconds since last frame */
	void (*fixed_update)(float fdt); /* physics tick at a FIXED dt, run 0..N times/frame (may be NULL) —
	                                    put motion/collision here for stable, framerate-independent
	                                    physics (like Unity FixedUpdate / Godot _physics_process).
	                                    Read HELD input (eng_pressed) here — NOT eng_just_pressed:
	                                    an edge would re-fire on every sub-step; handle edges in update(). */
	void (*draw_background)(void);   /* immediate, drawn BEFORE the scene layers */
	void (*draw_overlay)(void);      /* immediate, drawn AFTER the scene layers (screen-space) */
} eng_game;

int  eng_run(const eng_game *g);       /* connect + own the loop; returns on quit (0 ok, 1 = no compositor) */
void eng_quit(void);
int  eng_width(void);
int  eng_height(void);

/* Input — the engine samples it once per frame; you query state (no event drain). */
bool eng_pressed(eng_button b);        /* held down this frame */
bool eng_just_pressed(eng_button b);   /* went down THIS frame (edge) */

/* Pointer / touch — one contact, coords in frame pixels (0,0 = top-left), sampled once per frame
 * like the buttons. On a button-only device these never fire and the position stays -1. Read the
 * *_just_* edges in update(), NOT fixed_update() (an edge would re-fire on every sub-step). */
bool eng_pointer_pressed(void);        /* contact held this frame */
bool eng_pointer_just_pressed(void);   /* contact began THIS frame (edge — a tap/click) */
bool eng_pointer_just_released(void);  /* contact ended THIS frame (edge) */
void eng_pointer(int *x, int *y);      /* current position (last known if not down); -1 if none yet */
float eng_wheel(void);                 /* accumulated scroll/pinch-zoom delta since last call, then clears (0 if none) */

/* Drawing — call inside draw(). No pixel loops in game code. */
void eng_clear(eng_color c);
void eng_rect_fill(int x, int y, int w, int h, eng_color c);
void eng_rect(int x, int y, int w, int h, eng_color c);   /* 1px outline */
void eng_circle_fill(int cx, int cy, int r, eng_color c); /* filled disc centered at (cx,cy) */
void eng_line(int x0, int y0, int x1, int y1, eng_color c); /* 1px line (Bresenham) */
void eng_post_aa(void);   /* one-pass edge anti-aliasing over the finished frame (call before HUD text) */

/* Text — antialiased TrueType (via stb_truetype), any font/size. `px` is the font pixel
 * size (like a point size), `y` is the TOP of the text, '\n' starts a new line. Font from
 * $CANVAS_FONT, else the installed default; no font loaded -> text is a no-op. */
void eng_text(int x, int y, int px, eng_color c, const char *s);
int  eng_text_width(int px, const char *s);   /* pixel width of one line (for centering) */

/* Draw one line horizontally aligned relative to `x` — mirrors the HTML-canvas textAlign anchor
 * model / Godot's HorizontalAlignment: LEFT starts at x, CENTER centers on x, RIGHT ends at x.
 * Plain eng_text() is the LEFT case. (e.g. center on screen: x = eng_width()/2, ENG_ALIGN_CENTER.) */
typedef enum { ENG_ALIGN_LEFT, ENG_ALIGN_CENTER, ENG_ALIGN_RIGHT } eng_align;
void eng_text_aligned(int x, int y, int px, eng_color c, eng_align a, const char *s);

/* ---- Audio (a per-game software mixer; the mixed stream feeds the soundd audio server) ----
 * Load 16-bit PCM WAVs (mono/stereo, at 44.1kHz), then fire one-shot SFX or a looping music bed.
 * `vol` is a linear gain (~0..1). On a build with no audio backend/soundd these are safe no-ops
 * (mixed but never heard). Loading resolves paths under $CANVAS_ASSETS, like eng_image_from_png. */
typedef struct eng_sound eng_sound;
eng_sound *eng_sound_load(const char *path);        /* NULL on failure */
void       eng_sound_free(eng_sound *s);
void       eng_sound_play(eng_sound *s, float vol); /* fire-and-forget one-shot */
void       eng_music_play(eng_sound *s, float vol); /* looping bed; replaces any current music */
void       eng_music_stop(void);
double     eng_music_pos(void);        /* music playback position in seconds (audio clock; 0 = none) */

/* ---- Scene graph (the Unity/Godot-esque retained layer) -----------------------------
 * Instead of redrawing everything by hand, build a tree of NODES. A node has a transform
 * (its CENTER position, like Godot's Sprite2D), an optional sprite (a texture the engine
 * draws automatically), an optional per-frame `update` callback (Godot's _process / Unity's
 * Update), a `tag` for grouping (Godot "groups"), and a `w,h` AABB hitbox. Each frame the
 * engine runs every node's update, reaps freed nodes, then renders visible sprites by `z`.
 * Positions are relative to the parent; child of an invisible node is hidden too.
 * Nodes live in one of three LAYERS drawn back-to-front — BACKGROUND, WORLD, HUD (like
 * Godot CanvasLayers / Unity sorting layers). The WORLD layer scrolls with the camera
 * (eng_camera_set); BACKGROUND and HUD stay pinned to the screen. Default layer = WORLD. */

typedef struct eng_image   eng_image;    /* a texture (opaque; RGBA held in memory) */
typedef struct eng_tilemap eng_tilemap;  /* a grid of solid tiles (opaque) */
typedef struct eng_node    eng_node;

struct eng_node {
	float       x, y;        /* position of the node's CENTER, relative to its parent */
	float       rot;         /* rotation in RADIANS about the center (0 = upright) */
	float       scale;       /* uniform scale (1.0 = native size); eng_node_new sets it to 1 */
	int         z;           /* draw order: higher renders on top */
	bool        visible;     /* false hides this node's sprite AND its children */
	int         w, h;        /* size / AABB hitbox (a sprite node defaults to the image size) */
	eng_color   tint;        /* image/text modulate (multiply); ENG_WHITE = unchanged */
	const char *text;        /* if non-NULL, draw this string as a label at (x,y) [top-left] */
	int         text_px;     /* label pixel size (see eng_label_new) */
	eng_tilemap *tiles;      /* if non-NULL, draw this tilemap with (x,y) as its top-left */
	eng_image  *sheet;       /* the node's texture; drawn as cell (frame,row) of fw×fh — a plain
	                            sprite is just a 1-frame sheet (see eng_sprite_new / eng_anim_new) */
	int         fw, fh;      /* frame cell size within `sheet` */
	int         frame, row;  /* current frame (column) and row (e.g. facing dir) in the sheet */
	int         nframes;     /* frames per row = the loop length (1 = a static sprite) */
	float       fps;         /* auto-advance rate; 0 = hold on the current `frame` (idle) */
	int         tag;         /* your grouping id (Godot "groups"); 0 = untagged */
	void      (*update)(eng_node *self, float dt);  /* per-frame behavior (may be NULL) */
	void       *user;        /* your per-node data */

	/* --- engine-managed; do not touch --- */
	eng_node   *_parent, *_child, *_sibling;
	float       _atime;      /* animation frame-timer accumulator */
	bool        _freed;
};

/* Textures. Load a PNG (decoded to RGBA via stb_image). A relative `path` resolves under
 * $CANVAS_ASSETS (else /usr/share/canvas); an absolute path is used as-is. NULL on error. */
eng_image *eng_image_from_png(const char *path);
void       eng_image_free(eng_image *img);
int        eng_image_w(const eng_image *img);
int        eng_image_h(const eng_image *img);

/* Draw one VERTICAL screen column `sx`, rows [y0,y1), sampling texture column `tex_x` of `tex`
 * with the texture's full height mapped onto the span (nearer = taller span = magnified) — the
 * core operation of a RAYCASTER (Wolfenstein-style walls) and billboard sprites. `shade` 0..256
 * scales brightness (distance/side shading; 256 = full). alpha!=0 skips transparent texels (for
 * sprites); else opaque (walls). Clipped to the frame. */
void eng_tex_column(const eng_image *tex, int sx, int tex_x, int y0, int y1, int shade, int alpha);

/* Textured WALL column for a SECTOR/portal renderer (Doom-style). Like eng_tex_column, the
 * texture's full height maps onto the wall's TRUE projected extent [wy0,wy1) — but only rows
 * inside the per-column clip window [clip0,clip1) are written, so a wall seen through a narrow
 * portal keeps its texture v-coordinate tied to the whole wall (not the visible sliver). Opaque.
 * (eng_tex_column is the special case clip==[wy0,wy1].) */
void eng_wall_column(const eng_image *tex, int sx, int tex_x, int wy0, int wy1,
                     int clip0, int clip1, int shade);

/* Perspective-textured horizontal PLANE, one screen column — the floor/ceiling caster behind a
 * Doom-style renderer (and Mode-7). Fills rows [y0,y1) of column `sx`; for each row it
 * back-projects to a world distance  rowDist = numer / |y - horizon|,  samples `tex` at world
 * point (px + rowDist*rdx, py + rowDist*rdy) scaled by `texscale` texels/world-unit (wrapped),
 * and darkens with distance by `fog` (shade drops fog units per world unit; 0 = none). `numer`
 * folds the plane's height from the eye and the vertical projection:
 *     numer = fabsf(eye_z - plane_z) * eng_height()
 * `rdx,rdy` = this column's world-space ray direction (dir + plane*camx, like the wall caster).
 * Opaque writes; nothing outside the frame. */
void eng_floor_column(const eng_image *tex, int sx, int y0, int y1, int horizon,
                      float numer, float rdx, float rdy, float px, float py,
                      float texscale, int fog);

/* ---- Software 3D: a flat-shaded, z-buffered TRIANGLE rasterizer (a tiny software GPU) --------
 * The step past the column tricks (raycaster/portal/racer): fill ARBITRARY projected triangles
 * with a real PER-PIXEL depth buffer, so 3D polygons occlude each other correctly regardless of
 * draw order. The game does the 3D math (rotate + project its model vertices, compute a lit face
 * color) and hands screen-space triangles here; the engine owns the pixels + the z-test. This is
 * the exact GPU pipeline (transform -> project -> rasterize -> depth-test) done on the CPU. */

/* One screen-space vertex: pixel (x,y) + INVERSE depth iz = 1/z_camera (nearer = larger). 1/z is
 * linear across a triangle in screen space, so the engine interpolates it for a correct z-test. */
typedef struct { float x, y, iz; } eng_vtx;

void eng_zclear(void);   /* reset the depth buffer to far; call ONCE per frame before the 3D pass */
/* Rasterize triangle (a,b,c) into the current frame, flat-shaded with `color` (already lit by the
 * game). Per covered pixel, keeps + writes the fragment only where its interpolated iz is nearer
 * than the z-buffer. Winding-agnostic (cull back-faces yourself if you want the speed). */
void eng_tri(eng_vtx a, eng_vtx b, eng_vtx c, eng_color color);
/* Distance fog: lerp triangles toward `color` as they recede (near..far in world units); opt-in per game. */
void eng_fog(eng_color color, float near_dist, float far_dist);
void eng_fog_off(void);

/* Gouraud-shaded flat-colour triangle: ia/ib/ic are per-vertex light intensities (0..1),
 * interpolated across the face so lighting varies smoothly instead of per-triangle. Same z-buffer. */
void eng_tri_gouraud(eng_vtx a, eng_vtx b, eng_vtx c, eng_color color, float ia, float ib, float ic);

/* Textured vertex: screen (x,y), iz=1/z_camera, and texture coords (u,v) in TILES (0..1 spans the
 * image once; >1 or <0 wraps — so a road/wall can repeat a tile along its length). */
typedef struct { float x, y, iz, u, v; } eng_vtx_tex;

/* Perspective-correct TEXTURED triangle (the last core GPU concept). Same z-buffered rasterizer as
 * eng_tri, but it interpolates u/z, v/z and 1/z across the triangle and divides per pixel to
 * recover the true (u,v) — so the texture stays glued to the surface with no affine warping (the
 * wobble PS1 games had). Samples `tex` (wrapped), scaled by `shade` 0..256 (distance fog / light).
 * Shares the same depth buffer as eng_tri, so flat + textured tris interleave correctly. */
void eng_tri_tex(eng_vtx_tex a, eng_vtx_tex b, eng_vtx_tex c, const eng_image *tex, int shade);
/* Textured + Gouraud-lit triangle: like eng_tri_tex but scaled by a per-vertex light intensity
 * (ia/ib/ic, 0..1) instead of one flat shade — the rasterizer behind textured meshes. */
void eng_tri_tex_lit(eng_vtx_tex a, eng_vtx_tex b, eng_vtx_tex c, const eng_image *tex, float ia, float ib, float ic, eng_color base);

/* ---- 3D scene layer: a camera + meshes on top of eng_tri (folds the per-game transform/projection
 * boilerplate into the engine). Per frame: eng_zclear(); eng_camera_look(...); then eng_mesh_draw()
 * for geometry and/or eng_project() to place your own sprites/effects. Flat-shaded by eng_light. */
typedef struct { float x, y, z; } eng_vec3;
static inline eng_vec3 eng_v3(float x, float y, float z) { eng_vec3 v = { x, y, z }; return v; }

/* Aim a look-at camera: from `eye` toward `target`, `up` = world-up hint, `fov_deg` = vertical FOV.
 * Persists until changed; consumed by eng_project + eng_mesh_draw. */
void eng_camera_look(eng_vec3 eye, eng_vec3 target, eng_vec3 up, float fov_deg);
/* Global directional light for mesh shading: `dir` points toward the light, `ambient` in [0,1]. */
void eng_light(eng_vec3 dir, float ambient);
/* Project a world point through the active camera -> screen (sx,sy) + inverse depth iz. Returns 1
 * if in front of the near plane (drawable), 0 if behind. For placing billboards / effects. */
int  eng_project(eng_vec3 w, float *sx, float *sy, float *iz);
/* Near-plane-clipped textured triangle in WORLD space (clips vs the camera near plane so quads at the
 * camera's feet fill in instead of being discarded). Uses the eng_camera_look camera. */
void eng_tri_tex_world(eng_vec3 wa, eng_vec3 wb, eng_vec3 wc, const eng_image *tex, float ua, float va, float ub, float vb, float uc, float vc, int shade);

/* A mesh = flat-shaded triangles built in code. A tri flagged `tint` multiplies by the per-draw
 * tint (a recolorable body); un-flagged tris keep their own color (glass, trim, wheels). */
typedef struct eng_mesh eng_mesh;
eng_mesh *eng_mesh_new(void);
/* Load geometry authored as DATA: a Wavefront .obj (+.mtl for per-material colour). Positions+faces
 * only (normals recomputed via the crease pass); a material named *tint* -> recolourable by eng_mesh_draw's
 * tint. Path resolves under $CANVAS_ASSETS. This is the real asset-pipeline path vs building meshes in code. */
eng_mesh *eng_mesh_from_obj(const char *path);
/* Raw world-space triangle access (for a track surface: draw ranges yourself, multi-textured/windowed). */
int  eng_mesh_ntris(const eng_mesh *m);
void eng_mesh_get_tri(const eng_mesh *m, int i, eng_vec3 *a, eng_vec3 *b, eng_vec3 *c, float *uv6);
void      eng_mesh_free(eng_mesh *m);
void      eng_mesh_tri (eng_mesh *m, eng_vec3 a, eng_vec3 b, eng_vec3 c, eng_color col, int tint);
void      eng_mesh_quad(eng_mesh *m, eng_vec3 a, eng_vec3 b, eng_vec3 c, eng_vec3 d, eng_color col, int tint);
void      eng_mesh_box (eng_mesh *m, eng_vec3 lo, eng_vec3 hi, eng_color col, int tint);
/* A UV sphere centred at `center`, radius `r`; `subdiv` sets resolution (rings=4*subdiv, segs=8*subdiv).
 * Curved geometry: its shallow face angles fall under the smoothing crease, so eng_mesh_draw shades it
 * smoothly (Gouraud). Box/hard-edged meshes stay faceted. */
void      eng_mesh_sphere(eng_mesh *m, eng_vec3 center, float r, int subdiv, eng_color col, int tint);
/* Draw a mesh: model transform = uniform `scale`, then yaw (about +Y), then pitch (about +X), placed
 * at world `pos`; `tint` recolors the tint-flagged tris. Uses the active camera + light + z-buffer. */
void      eng_mesh_draw(const eng_mesh *m, eng_vec3 pos, float yaw, float pitch, float roll, float scale, eng_color tint);

/* Immediate whole-image blit centered at (cx,cy), alpha-blended; `tint` MODULATES (ENG_WHITE =
 * unchanged, so a greyscale sprite can be recolored). For HUD / 2D sprites drawn outside the
 * scene graph. */
void eng_draw_image(const eng_image *img, int cx, int cy, eng_color tint);

/* Immediate blit of ONE cell (col,row) of a sprite SHEET (cells fw x fh) centered at (cx,cy),
 * alpha-blended, tint modulates. The sheet form of eng_draw_image — for frame-animated immediate
 * effects (hit bursts, pickups) drawn outside the scene graph. */
void eng_draw_image_cell(const eng_image *sheet, int cx, int cy, int fw, int fh, int col, int row, eng_color tint);

/* Immediate ROTATED + uniformly-scaled centered blit (rot in radians), alpha-blended, tint
 * modulates. For billboards/effects needing rotation outside the scene graph (e.g. top-down karts). */
void eng_draw_image_rot(const eng_image *img, int cx, int cy, float rot, float scale, eng_color tint);
/* Immediate blit of sheet cell (col,row) of fw×fh, centered at (cx,cy), ROTATED (rad) + SCALED,
 * alpha-blended, tint modulates. The scalable+rotatable form of eng_draw_image_cell (billboards). */
void eng_draw_sprite(const eng_image *sheet, int cx, int cy, int fw, int fh, int col, int row, float rot, float scale, eng_color tint);
/* Depth-tested billboard (iz = 1/z_camera from eng_project): z-tested + written vs the shared depth
 * buffer so it's occluded by nearer 3D geometry. For world sprites (trees, rivals) in a 3D scene. */
void eng_draw_sprite_z(const eng_image *sheet, int cx, int cy, int fw, int fh, int col, int row, float rot, float scale, eng_color tint, float iz);

/* ---- Screen-space particle pool (reusable: sparks, dust, trails, stars, exhaust) -------------
 * Emit at pixel (x,y) with velocity (px/s) + gravity gy (px/s^2); the square's color lerps c0->c1
 * and it shrinks over `life` seconds. reset() clears all; call update(dt) each frame and draw()
 * in draw_overlay (screen-space). */
void eng_particles_reset(void);
void eng_particle(float x, float y, float vx, float vy, float gy, float life, float sz, eng_color c0, eng_color c1);
void eng_particles_update(float dt);
void eng_particles_draw(void);

/* Tilemaps. Build from ASCII `rows` (equal-length, NULL-terminated): each char found in
 * `keys` becomes a SOLID tile colored by the parallel `colors[]`; ' ' and unknown chars are
 * empty (non-solid). Attach it to a node (see eng_tilemap_node); the node's (x,y) is the
 * map's top-left. Put that node on the WORLD layer at (0,0) so the camera scrolls it and
 * world coordinates line up with eng_tilemap_solid_at. `tile_px` = pixel size of each cell. */
eng_tilemap *eng_tilemap_new(const char *const *rows, int tile_px,
                             const char *keys, const eng_color *colors);
void         eng_tilemap_free(eng_tilemap *m);
bool         eng_tilemap_solid_at(const eng_tilemap *m, float x, float y);  /* world pt in a solid cell? */
int          eng_tilemap_cols(const eng_tilemap *m);
int          eng_tilemap_rows(const eng_tilemap *m);
int          eng_tilemap_px(const eng_tilemap *m);

/* Grid pathfinding (BFS flow field). Given a cols x rows grid where blocked[r*cols+c] != 0 is a
 * wall, fills next[r*cols+c] with the INDEX of the next cell on a shortest 4-connected path to the
 * goal: the goal points to itself, and blocked/unreachable cells are -1. `next` must hold
 * cols*rows ints. Reusable for tower-defense creeps, pursuit AI, etc. */
void eng_flow_field(int cols, int rows, const unsigned char *blocked, int goalc, int goalr, int *next);

/* Levels load from a Tiled JSON map (.tmj) under $CANVAS_ASSETS, so level design lives in
 * data you paint in the Tiled editor, not in code. A level = a solid tilemap + an optional
 * non-solid decoration tilemap + placed objects: the tile layer named "bg" becomes the
 * decoration layer (eng_level_bg), any other tile layer is the solid/collision layer, and
 * the object layer becomes objects (each carries its Tiled name + position). If the map
 * embeds a tileset, tiles draw from that atlas image, else from `tile_colors` (gid n ->
 * tile_colors[n-1]). `tile_px` is only a fallback if the map omits its tilewidth. NULL on
 * failure. */
typedef struct eng_level eng_level;
typedef struct { char name[24]; float x, y; } eng_object;   /* entity placement; (x,y) = world center */

eng_level        *eng_level_load(const char *path, int tile_px,
                                 const char *tile_keys, const eng_color *tile_colors);
eng_tilemap      *eng_level_map(eng_level *l);              /* solid collision tilemap (borrowed) */
eng_tilemap      *eng_level_bg(eng_level *l);              /* non-solid decoration tilemap, or NULL */
const eng_object *eng_level_objects(eng_level *l, int *count);  /* placed entities to spawn */
void              eng_level_free(eng_level *l);            /* frees the level AND its tilemaps */

/* ---- Physics: move a node's AABB against the active collision tilemap ------------------
 * Set the solid world once (eng_set_tilemap), then each frame set a velocity and call
 * eng_move_and_collide — the guts of Godot's move_and_slide(). It moves the node's (x,y) by
 * (dx,dy), resolves its w x h box against solid tiles per axis, and returns which sides it
 * hit (bitmask; ENG_COL_DOWN = landed on the floor). Grid AABB only — no slopes yet. */
enum { ENG_COL_LEFT = 1, ENG_COL_RIGHT = 2, ENG_COL_UP = 4, ENG_COL_DOWN = 8 };
void eng_set_tilemap(eng_tilemap *m);                 /* the solid world for collisions */
int  eng_move_and_collide(eng_node *n, float dx, float dy);

/* ---- 2D collision primitives (ball/segment physics: Pong, pinball, …) ------------------
 * Pure geometry — the game owns its bodies (position + velocity) and calls these each physics
 * step. Run the motion in the game's fixed_update (fixed dt), substepping fast bodies so they
 * can't tunnel through thin segments. */
/* Circle (center cx,cy, radius r) vs line segment (ax,ay)-(bx,by). Returns true on overlap; on
 * hit sets *nx,*ny = UNIT normal from the segment toward the circle center, and *pen = penetration
 * depth (push the circle out by pen along the normal to separate them). */
bool eng_circle_segment(float cx, float cy, float r, float ax, float ay, float bx, float by,
                        float *nx, float *ny, float *pen);
/* Circle A vs circle B (bumpers, ball-vs-ball). Returns true on overlap; on hit sets *nx,*ny =
 * UNIT normal from B toward A (push A out along it) and *pen = penetration depth. */
bool eng_circle_circle(float ax, float ay, float ar, float bx, float by, float br,
                       float *nx, float *ny, float *pen);
/* Reflect velocity (*vx,*vy) about UNIT normal (nx,ny) with restitution e in [0,1]:
 * v' = v - (1+e)(v·n)n. No-op if already moving away from the surface (v·n >= 0). */
void eng_reflect(float *vx, float *vy, float nx, float ny, float e);

/* Resolve two EQUAL-MASS moving circles (radius r): separate the overlap and exchange momentum
 * along the contact normal with restitution e (the two-movable-bodies version of eng_reflect —
 * for billiards/marbles). Returns true if they overlapped. */
bool eng_resolve_circles(float *ax, float *ay, float *avx, float *avy,
                         float *bx, float *by, float *bvx, float *bvy, float r, float e);

/* The three render layers, drawn back-to-front. A node attaches to a layer as its parent. */
typedef enum { ENG_LAYER_BACKGROUND, ENG_LAYER_WORLD, ENG_LAYER_HUD } eng_layer_id;
eng_node *eng_layer(eng_layer_id id);        /* the layer's root node (pass as a parent) */

/* Nodes. parent == NULL means "under the WORLD layer" (the default gameplay layer). */
eng_node *eng_root(void);                    /* == eng_layer(ENG_LAYER_WORLD) */
eng_node *eng_node_new(eng_node *parent);
eng_node *eng_sprite_new(eng_node *parent, eng_image *img);  /* whole image (a 1-frame sheet); hitbox = image size */
/* Animated sprite from a SHEET: a grid of frames, each fw x fh, `nframes` frames across a row.
 * The engine auto-advances the current frame at `fps` (0 = hold). Set n->row to pick the clip
 * (e.g. facing direction), n->frame to jump, n->fps=0 + n->frame=0 for an idle pose. */
eng_node *eng_anim_new(eng_node *parent, eng_image *sheet, int fw, int fh, int nframes, float fps);
eng_node *eng_label_new(eng_node *parent, int px);           /* text node; set n->text yourself */
eng_node *eng_tilemap_node(eng_node *parent, eng_tilemap *m);/* node that draws a tilemap */
void      eng_node_free(eng_node *n);        /* deferred: reaped after this frame's updates */
void      eng_layer_clear(eng_layer_id id);  /* free every node in one layer */
void      eng_scene_clear(void);             /* free every node in all layers (e.g. on restart) */

/* Camera: scroll the WORLD layer so world point (cx,cy) sits at screen center. Default
 * (unset) = no offset (world coords == screen coords). BACKGROUND and HUD ignore it. */
void      eng_camera_set(float cx, float cy);

/* Iterate a node's children (for collisions etc.); pass NULL for the root's children. */
eng_node *eng_first_child(eng_node *n);
eng_node *eng_next(eng_node *n);             /* next sibling; NULL at the end */

/* AABB overlap of two nodes using their centered (x,y,w,h) boxes. */
bool eng_overlap(const eng_node *a, const eng_node *b);

/* Iterate the WORLD-layer nodes tagged `tag` whose box overlaps `probe` (skips `probe` itself
 * and freed nodes). Start with after == NULL; pass the previous result to continue; NULL ends.
 * Calling eng_node_free() on a returned node inside the loop is safe. Replaces the hand-rolled
 * "walk children, match tag, test overlap" pattern. */
eng_node *eng_overlap_next(const eng_node *probe, int tag, eng_node *after);

#endif /* GV3GAME_H */
