// game.c

#include <stdlib.h>     // rand()

#include "game.h"
#include "sprite_engine.h"
#include "collide.h"
#include "xil_printf.h"

// Arcade-classic point values, indexed by alien grid row.
//   row 0   -> C / octopus / 30
//   rows 1-2 -> B / crab    / 20
//   rows 3-4 -> A / squid   / 10
static const int POINT_VALUE[ALIEN_ROWS] = { 30, 20, 20, 10, 10 };

void game_reset(GameState *g)
{
    g->player_x   = 152;
    g->grid_x     = GRID_X_INIT;
    g->grid_y     = GRID_Y_INIT;
    g->grid_step  = 0;
    g->grid_dir   = +1;                         // start sweeping right
    g->step_timer = 30;                         // matches period at full grid (55 alive)

    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            g->alien_alive[r][c] = 1;
        }
    }
    g->alive_count = ALIEN_COUNT;

    g->player_proj.active = 0;
    g->player_proj.x      = 0;
    g->player_proj.y      = 0;

    for (int i = 0; i < ALIEN_PROJ_COUNT; i++) {
        g->alien_proj[i].active = 0;
        g->alien_proj[i].x      = 0;
        g->alien_proj[i].y      = 0;
    }

    g->score = 0;
    g->lives = 3;
    g->state = STATE_START;                     // wait for ENTER to begin
}

void game_player_fire(GameState *g)
{
    if (g->player_proj.active) return;          // one bullet at a time
    g->player_proj.active = 1;
    g->player_proj.x      = g->player_x + (PLAYER_W - PROJ_W) / 2;  // center on cannon
    g->player_proj.y      = PLAYER_Y - PROJ_H;                       // just above cannon
}

// Advance the alien grid by one "step": horizontal +-2 logical px, or a
// drop+flip if the next horizontal move would push the leftmost or
// rightmost live alien past the playfield edge.  Toggles the animation
// phase on every step (drop or horizontal).  No-op if no aliens are alive.
static void advance_grid(GameState *g)
{
    int leftmost_col = -1, rightmost_col = -1;
    for (int c = 0; c < ALIEN_COLS; c++) {
        int col_alive = 0;
        for (int r = 0; r < ALIEN_ROWS; r++) {
            if (g->alien_alive[r][c]) { col_alive = 1; break; }
        }
        if (col_alive) {
            if (leftmost_col < 0) leftmost_col = c;
            rightmost_col = c;
        }
    }
    if (leftmost_col < 0) return;               // no aliens; nothing to advance

    int next_grid_x = g->grid_x + 2 * g->grid_dir;
    int next_left   = next_grid_x + leftmost_col  * ALIEN_STRIDE_X;
    int next_right  = next_grid_x + rightmost_col * ALIEN_STRIDE_X + ALIEN_W;

    int wall_hit = (g->grid_dir > 0 && next_right > 320) ||
                   (g->grid_dir < 0 && next_left  < 0);

    if (wall_hit) {
        g->grid_y  += ALIEN_STRIDE_Y;           // drop one row stride
        g->grid_dir = -g->grid_dir;             // flip; next step goes the other way
    } else {
        g->grid_x = next_grid_x;
    }

    g->grid_step ^= 1;                          // two-frame animation phase
}

// Spawn an alien projectile in slot 0 from the bottom-most live alien in
// a randomly-chosen live column.  No-op if all aliens are dead or slot 0
// is already active.
static void spawn_alien_proj_slot0(GameState *g)
{
    if (g->alien_proj[0].active) return;

    // Collect columns that have at least one live alien.
    int alive_cols[ALIEN_COLS];
    int n_alive = 0;
    for (int c = 0; c < ALIEN_COLS; c++) {
        for (int r = 0; r < ALIEN_ROWS; r++) {
            if (g->alien_alive[r][c]) {
                alive_cols[n_alive++] = c;
                break;
            }
        }
    }
    if (n_alive == 0) return;

    int col = alive_cols[rand() % n_alive];

    // Bottom-most live alien in that column = highest row index.
    int bottom_row = -1;
    for (int r = ALIEN_ROWS - 1; r >= 0; r--) {
        if (g->alien_alive[r][col]) { bottom_row = r; break; }
    }
    if (bottom_row < 0) return;

    int alien_x = g->grid_x + col * ALIEN_STRIDE_X;
    int alien_y = g->grid_y + bottom_row * ALIEN_STRIDE_Y;

    g->alien_proj[0].active = 1;
    g->alien_proj[0].x      = alien_x + (ALIEN_W - PROJ_W) / 2;  // centered on alien
    g->alien_proj[0].y      = alien_y + ALIEN_H;                 // just below alien
}

// Returns the highest row index containing any live alien, or -1 if grid empty.
static int lowest_live_row(const GameState *g)
{
    for (int r = ALIEN_ROWS - 1; r >= 0; r--) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            if (g->alien_alive[r][c]) return r;
        }
    }
    return -1;
}

// Clear every projectile slot.  Called when transitioning out of PLAYING
// so a stray bullet doesn't keep rendering during STATE_GAMEOVER.
static void clear_all_projectiles(GameState *g)
{
    g->player_proj.active = 0;
    for (int i = 0; i < ALIEN_PROJ_COUNT; i++) {
        g->alien_proj[i].active = 0;
    }
}

// Trim fully-dead leftmost columns from the alien grid: shift the bitmap
// left by one column and bump grid_x by one stride so every live alien
// stays at the same on-screen pixel.  Required because the AXI grid_x
// register is 10-bit *unsigned* -- without this, advance_grid would let
// grid_x go negative as left columns are wiped out, the cast to uint16
// would wrap to ~1000, and pixel_mux's `lx >= grid_x` test would hide
// the entire grid until grid_x climbed back to >= 0.
static void compact_grid_left(GameState *g)
{
    while (1) {
        // Column 0 still occupied?  Then no compaction needed.
        int col0_alive = 0;
        for (int r = 0; r < ALIEN_ROWS; r++) {
            if (g->alien_alive[r][0]) { col0_alive = 1; break; }
        }
        if (col0_alive) return;

        // Column 0 is empty.  If the whole grid is empty, stop --
        // there's nothing left to shift in.
        int any_alive = 0;
        for (int c = 1; c < ALIEN_COLS && !any_alive; c++) {
            for (int r = 0; r < ALIEN_ROWS && !any_alive; r++) {
                if (g->alien_alive[r][c]) any_alive = 1;
            }
        }
        if (!any_alive) return;

        // Shift the bitmap left one column; rightmost becomes empty.
        for (int r = 0; r < ALIEN_ROWS; r++) {
            for (int c = 0; c < ALIEN_COLS - 1; c++) {
                g->alien_alive[r][c] = g->alien_alive[r][c + 1];
            }
            g->alien_alive[r][ALIEN_COLS - 1] = 0;
        }
        g->grid_x += ALIEN_STRIDE_X;
    }
}

void game_tick(GameState *g)
{
    // STATE_START: frozen until ENTER (handled in main.c).
    // STATE_GAMEOVER: frozen until R (handled in main.c).
    // Only PLAYING runs game logic.
    if (g->state != STATE_PLAYING) return;

    // Player projectile rises until it leaves the top of the playfield.
    if (g->player_proj.active) {
        g->player_proj.y -= PROJ_SPEED;
        if (g->player_proj.y < 0) {
            g->player_proj.active = 0;
        }
    }

    // Player projectile vs alien collision.  AABB on each live alien's
    // ALIEN_W x ALIEN_H cell; on hit, clear the alien and credit the row's
    // point value.
    if (g->player_proj.active) {
        HitCell hit;
        if (collide_player_proj_vs_aliens(g, &hit)) {
            g->alien_alive[hit.row][hit.col] = 0;
            g->alive_count--;
            g->player_proj.active = 0;
            g->score += POINT_VALUE[hit.row];
            xil_printf("score: %d\r\n", g->score);
            // Keep grid_x >= 0 by trimming any newly-empty leftmost columns.
            compact_grid_left(g);
        }
    }

    // Alien grid sweep: step at most once per frame.  Period scales with
    // live alien count so the grid speeds up as aliens are killed:
    //   period = max(1, 30 * alive_count / ALIEN_COUNT)
    // i.e. ~30 frames (~0.5 s) at 55 alive, down to 1 frame at 1-2 alive.
    if (g->alive_count > 0) {
        if (--g->step_timer <= 0) {
            advance_grid(g);
            int period = (30 * g->alive_count) / ALIEN_COUNT;
            if (period < 1) period = 1;
            g->step_timer = period;
        }
    }

    // Alien projectile (slot 0): respawn whenever the slot is free.
    spawn_alien_proj_slot0(g);

    // Advance every active alien projectile downward; deactivate when off
    // the bottom of the playfield.
    for (int i = 0; i < ALIEN_PROJ_COUNT; i++) {
        if (g->alien_proj[i].active) {
            g->alien_proj[i].y += ALIEN_PROJ_SPEED;
            if (g->alien_proj[i].y >= PLAYFIELD_H) {
                g->alien_proj[i].active = 0;
            }
        }
    }

    // Alien projectile vs player collision.  On hit, deactivate the
    // bullet, decrement lives, and end the game when lives hits zero.
    for (int i = 0; i < ALIEN_PROJ_COUNT; i++) {
        if (collide_alien_proj_vs_player(g, i)) {
            g->alien_proj[i].active = 0;
            g->lives--;
            xil_printf("lives: %d\r\n", g->lives);
            if (g->lives <= 0) {
                g->state = STATE_GAMEOVER;
                xil_printf("GAME OVER (lives = 0)\r\n");
                clear_all_projectiles(g);
                return;
            }
        }
    }

    // Game-over if the lowest live alien row has descended to the player y.
    int max_row = lowest_live_row(g);
    if (max_row >= 0) {
        int alive_grid_bottom = g->grid_y + max_row * ALIEN_STRIDE_Y + ALIEN_H;
        if (alive_grid_bottom >= PLAYER_Y) {
            g->state = STATE_GAMEOVER;
            xil_printf("GAME OVER (aliens reached player)\r\n");
            clear_all_projectiles(g);
            return;
        }
    }
}

// Pack the 5x11 bool grid into the three 32-bit words pixel_mux.sv reads.
// Bit ordering: alien_idx = row * ALIEN_COLS + col, row-major.
static void pack_alien_alive(const GameState *g, uint32_t out[3])
{
    out[0] = 0u;
    out[1] = 0u;
    out[2] = 0u;
    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            if (g->alien_alive[r][c]) {
                int idx = r * ALIEN_COLS + c;        // 0..54
                int word = idx >> 5;                 // 0..1
                int bit  = idx & 31;
                out[word] |= ((uint32_t)1u) << bit;
            }
        }
    }
}

void game_commit_to_hardware(const GameState *g)
{
    sprite_set_player_x((uint16_t)g->player_x);

    sprite_set_grid((uint16_t)g->grid_x,
                    (uint16_t)g->grid_y,
                    g->grid_step);

    uint32_t alive[3];
    pack_alien_alive(g, alive);
    sprite_set_alien_alive_word(0, alive[0]);
    sprite_set_alien_alive_word(1, alive[1]);
    sprite_set_alien_alive_word(2, alive[2]);

    sprite_set_player_proj(g->player_proj.active,
                           (uint16_t)g->player_proj.x,
                           (uint16_t)g->player_proj.y);

    for (int i = 0; i < ALIEN_PROJ_COUNT; i++) {
        sprite_set_alien_proj(i,
                              g->alien_proj[i].active,
                              (uint16_t)g->alien_proj[i].x,
                              (uint16_t)g->alien_proj[i].y);
    }

    sprite_set_game_state(g->state);
}
