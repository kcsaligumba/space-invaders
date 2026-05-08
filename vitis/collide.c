// collide.c

#include "collide.h"

int collide_player_proj_vs_aliens(const GameState *g, HitCell *out_hit)
{
    if (!g->player_proj.active) return 0;

    int bx_lo = g->player_proj.x;
    int bx_hi = bx_lo + PROJ_W;
    int by_lo = g->player_proj.y;
    int by_hi = by_lo + PROJ_H;

    // Quick reject: bullet fully outside the grid bounding box.
    int grid_x_end = g->grid_x + (ALIEN_COLS - 1) * ALIEN_STRIDE_X + ALIEN_W;
    int grid_y_end = g->grid_y + (ALIEN_ROWS - 1) * ALIEN_STRIDE_Y + ALIEN_H;
    if (bx_hi <= g->grid_x || bx_lo >= grid_x_end) return 0;
    if (by_hi <= g->grid_y || by_lo >= grid_y_end) return 0;

    // The bullet (PROJ_W=2) is narrower than the column gap
    // (ALIEN_STRIDE_X-ALIEN_W=4), so it can overlap at most one column.
    // Use the bullet center to locate it.
    int bx_center = bx_lo + PROJ_W / 2;
    int col = (bx_center - g->grid_x) / ALIEN_STRIDE_X;
    if (col < 0 || col >= ALIEN_COLS) return 0;

    // Reject if bullet x is in the horizontal gap between columns.
    int cell_x = g->grid_x + col * ALIEN_STRIDE_X;
    if (bx_hi <= cell_x || bx_lo >= cell_x + ALIEN_W) return 0;

    // Iterate rows top-to-bottom; with PROJ_H == ALIEN_STRIDE_Y - ALIEN_H
    // (6 px = 14 - 8) the bullet overlaps at most one row at any time.
    for (int r = 0; r < ALIEN_ROWS; r++) {
        int cell_y = g->grid_y + r * ALIEN_STRIDE_Y;
        if (by_lo >= cell_y + ALIEN_H) continue;       // bullet below this row
        if (by_hi <= cell_y) break;                    // bullet above this and all lower rows
        if (g->alien_alive[r][col]) {
            out_hit->row = r;
            out_hit->col = col;
            return 1;
        }
    }
    return 0;
}

int collide_alien_proj_vs_player(const GameState *g, int idx)
{
    if (idx < 0 || idx >= ALIEN_PROJ_COUNT) return 0;
    if (!g->alien_proj[idx].active) return 0;

    int bx_lo = g->alien_proj[idx].x;
    int bx_hi = bx_lo + PROJ_W;
    int by_lo = g->alien_proj[idx].y;
    int by_hi = by_lo + PROJ_H;

    int px_lo = g->player_x;
    int px_hi = px_lo + PLAYER_W;
    int py_lo = PLAYER_Y;
    int py_hi = py_lo + PLAYER_H;

    if (bx_hi <= px_lo || bx_lo >= px_hi) return 0;
    if (by_hi <= py_lo || by_lo >= py_hi) return 0;
    return 1;
}
