Refactor the game rendering in `projects/gameboy/src/game.c` to eliminate flicker using dirty-rectangle overdraw instead of erase-then-redraw. Read the file before making changes. Build with `make` from `projects/gameboy/`.

## Problem

The game flickers because it erases each object (fills with sky color) then redraws it at the new position. If the display refreshes between the erase and redraw, the object disappears for one frame. This happens on real ILI9341 hardware too — it's not an emulator bug.

Current approach (flickers):
```c
// Erase entire player column
display_fill_rect(display, PLAYER_X, 0, PLAYER_W, GROUND_Y, SKY);
// Draw player at new position
display_fill_rect(display, PLAYER_X, player_y, PLAYER_W, PLAYER_H, YELLOW);
```

Between these two calls, the player is invisible. Same for obstacles — the entire obstacle column is erased then redrawn.

## Solution: overdraw with dirty rectangles

Never erase an object to background then redraw it. Instead, only erase the pixels the object no longer covers, and draw the object at its new position. Every pixel write is the final color — there's no intermediate "erased" state.

## Player rendering

The player moves vertically (jumping). Track the previous Y position and only erase the difference:

```c
static int prev_player_y = GROUND_Y - PLAYER_H;

// Erase only the strip the player vacated
if (player_y > prev_player_y) {
    // Moving down: erase strip above new position
    int strip_h = player_y - prev_player_y;
    display_fill_rect(display, PLAYER_X, prev_player_y, PLAYER_W, strip_h, SKY);
} else if (player_y < prev_player_y) {
    // Moving up: erase strip below new position
    int strip_h = prev_player_y - player_y;
    display_fill_rect(display, PLAYER_X, player_y + PLAYER_H, PLAYER_W, strip_h, SKY);
}

// Draw player at new position (always — covers any remaining old pixels)
display_fill_rect(display, PLAYER_X, player_y, PLAYER_W, PLAYER_H, YELLOW);
display_fill_rect(display, PLAYER_X + 10, player_y + 5, 3, 3, BLACK);  // eye

prev_player_y = player_y;
```

When the player is stationary (on ground, not jumping), `player_y == prev_player_y` so no erase happens — just the redraw, which overwrites the same pixels with the same color. Zero flicker.

## Obstacle rendering

Obstacles scroll left by `SCROLL_SPEED` pixels per frame. Instead of erasing the entire column and redrawing:

```c
static int prev_obs_x[MAX_OBS];

for (int i = 0; i < MAX_OBS; i++) {
    int old_x = prev_obs_x[i];
    int new_x = obs_x[i];

    if (old_x >= 0 && old_x < SCR_W) {
        // Erase only the strip on the right that the obstacle vacated
        int trail_x = new_x + OBS_W;
        int trail_w = old_x + OBS_W - trail_x;
        if (trail_w > 0 && trail_x >= 0 && trail_x < SCR_W) {
            display_fill_rect(display, trail_x, GROUND_Y - obs_gap[i], trail_w, obs_gap[i], SKY);
        }
    }

    // Draw obstacle at new position
    if (new_x >= 0 && new_x < SCR_W) {
        display_fill_rect(display, new_x, GROUND_Y - obs_gap[i], OBS_W, obs_gap[i], RED);
    }

    // When obstacle wraps off-screen left, erase its last position fully
    if (new_x < -OBS_W && old_x >= 0) {
        display_fill_rect(display, old_x, GROUND_Y - obs_gap[i], OBS_W, obs_gap[i], SKY);
    }

    prev_obs_x[i] = new_x;
}
```

The obstacle moves left by `SCROLL_SPEED` (3 pixels). Each frame, only a 3-pixel-wide strip on the right is erased (where the obstacle used to be but no longer is). The obstacle itself is redrawn at the new position, overlapping most of its old pixels. No full-column erase.

## Score bar

The green score bar at the top grows rightward. Only draw the new pixels:

```c
static int prev_bar_w = 0;
int bar_w = score * 4;
if (bar_w > SCR_W) bar_w = SCR_W;
if (bar_w > prev_bar_w) {
    display_fill_rect(display, prev_bar_w, 0, bar_w - prev_bar_w, 3, GREEN);
    prev_bar_w = bar_w;
}
```

## Ground line

The ground never changes — draw it once at game start, never redraw.

## Reset on new game

When the game restarts (after game over), reset all `prev_*` state and do one full-screen clear:

```c
// At start of each game round:
prev_player_y = GROUND_Y - PLAYER_H;
for (int i = 0; i < MAX_OBS; i++) prev_obs_x[i] = obs_x[i];
prev_bar_w = 0;
// Full screen clear (only happens once per game)
display_fill_rect(display, 0, 0, SCR_W, GROUND_Y, SKY);
display_fill_rect(display, 0, GROUND_Y, SCR_W, SCR_H - GROUND_Y, GROUND);
```

## What this achieves

- Zero flicker: every pixel write is the final color, no intermediate "erased" state
- Less SPI traffic: only changed pixels are written instead of full columns
- Faster frame rendering: fewer pixels to push over SPI
- Works correctly on real ILI9341 hardware with tearing — mid-refresh captures always show valid content

## Testing

Run the game in the emulator. The character should jump without flickering. Obstacles should scroll smoothly without their columns flashing to sky color. Capture frames with ffmpeg and verify no frame shows a missing player or obstacle.
