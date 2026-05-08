// collide.h
//
// Collision detection in logical 320x240 playfield space.  Helpers report
// which alien (if any) the active player projectile is overlapping.  Each
// alien is treated as a solid ALIEN_W x ALIEN_H rectangle (no pixel-perfect
// testing against the sprite ROM bits).

#ifndef COLLIDE_H
#define COLLIDE_H

#include "game.h"

typedef struct {
    int row;
    int col;
} HitCell;

// Returns 1 if the player projectile is active and overlaps a live alien
// in the grid; fills *out_hit with that alien's row/col.  Returns 0
// otherwise (no projectile active, projectile outside the grid, projectile
// in a horizontal/vertical inter-cell gap, or only dead aliens overlap).
int collide_player_proj_vs_aliens(const GameState *g, HitCell *out_hit);

// Returns 1 if alien projectile slot `idx` is active and its rect overlaps
// the player's PLAYER_W x PLAYER_H rect at (player_x, PLAYER_Y).  Pure AABB.
int collide_alien_proj_vs_player(const GameState *g, int idx);

// Returns 1 if the player projectile is active and its rect overlaps the
// active UFO at (ufo.x, UFO_Y, UFO_W, UFO_H).  Pure AABB.
int collide_player_proj_vs_ufo(const GameState *g);

#endif // COLLIDE_H
