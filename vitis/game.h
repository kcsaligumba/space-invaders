// game.h
//
// Game state and per-frame update functions.  All positions are in LOGICAL
// 320x240 playfield coordinates; pixel_mux.sv doubles to 640x480 on screen.
//
// Sprite-engine register writes are done via game_commit_to_hardware(),
// which packs the bool grid into the three ALIEN_ALIVE words and pushes
// every relevant sprite-state register.

#ifndef GAME_H
#define GAME_H

#include "xil_types.h"

// Geometry (must match localparams in pixel_mux.sv)
#define ALIEN_ROWS        5
#define ALIEN_COLS       11
#define ALIEN_COUNT      (ALIEN_ROWS * ALIEN_COLS)
#define ALIEN_W          16
#define ALIEN_H           8
#define ALIEN_STRIDE_X   20    // 16-wide cell + 4 gap
#define ALIEN_STRIDE_Y   14    // 8-high cell + 6 gap

#define PLAYER_W     16
#define PLAYER_H      8                         // logical height of cannon (matches pixel_mux.sv)
#define PLAYER_Y    220                         // logical y of cannon top  (matches pixel_mux.sv)
#define PLAY_X_MIN    0
#define PLAY_X_MAX   (320 - PLAYER_W)
#define PLAYFIELD_H 240                         // logical screen height

// Centers the 11-column grid (effective width 10*ALIEN_STRIDE_X + ALIEN_W = 216)
// on the 320-wide playfield: (320 - 216)/2 = 52.  Aligns column index 5
// (the 6th from left, the true middle column) with the centered cannon.
#define GRID_X_INIT  52
#define GRID_Y_INIT  32

// Player projectile geometry (matches pixel_mux.sv localparams).
#define PROJ_W            2
#define PROJ_H            6
#define PROJ_SPEED        4                     // logical pixels/frame, upward

// Alien projectiles (3 slots wired in HDL; Step 11 spawns slot 0 only,
// Step 14 stretch fills slots 1 and 2).
#define ALIEN_PROJ_COUNT  3
#define ALIEN_PROJ_SPEED  2                     // logical pixels/frame, downward

typedef struct {
    int active;
    int x, y;                                   // logical top-left
} Projectile;

typedef struct {
    int        player_x;                          // logical 0..PLAY_X_MAX
    int        grid_x, grid_y;                    // logical top-left of grid
    int        grid_step;                         // animation phase, 0/1
    int        grid_dir;                          // +1 = moving right, -1 = moving left
    int        step_timer;                        // frames until next grid step
    int        alien_alive[ALIEN_ROWS][ALIEN_COLS];
    int        alive_count;                       // cached; recomputed when changed
    Projectile player_proj;
    Projectile alien_proj[ALIEN_PROJ_COUNT];
    int        score;
    int        lives;
    uint8_t    state;                             // STATE_START / PLAYING / GAMEOVER
} GameState;

// Reset to a fresh round: player centered, full alien grid, score=0, lives=3,
// state=PLAYING.  Call once at boot and again on game restart.
void game_reset(GameState *g);

// Advance one frame.  Caller is responsible for applying input deltas to
// g->player_x (clamped to [PLAY_X_MIN, PLAY_X_MAX]) before calling, since
// input handling lives in input.{c,h}.
//
// Step 7: advances the player projectile if active and deactivates it when
// it leaves the top of the screen.  Later steps add alien-grid movement,
// collisions, and state-machine transitions.
void game_tick(GameState *g);

// Spawn a player projectile centered horizontally on the cannon and just
// above it, but only if no projectile is currently in flight (one bullet
// at a time, classic Space Invaders).  Call on SPACE press-edge.
void game_player_fire(GameState *g);

// Commit the entire visible state to the sprite-engine AXI registers in
// hardware, including packing alien_alive into the three 32-bit words.
void game_commit_to_hardware(const GameState *g);

#endif // GAME_H
