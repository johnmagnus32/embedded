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

/* Drawing — call inside draw(). No pixel loops in game code. */
void eng_clear(eng_color c);
void eng_rect_fill(int x, int y, int w, int h, eng_color c);
void eng_rect(int x, int y, int w, int h, eng_color c);   /* 1px outline */

/* Text — antialiased TrueType (via stb_truetype), any font/size. `px` is the font pixel
 * size (like a point size), `y` is the TOP of the text, '\n' starts a new line. Font from
 * $CANVAS_FONT, else the installed default; no font loaded -> text is a no-op. */
void eng_text(int x, int y, int px, eng_color c, const char *s);
int  eng_text_width(int px, const char *s);   /* pixel width of one line (for centering) */

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
