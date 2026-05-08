// hud.h
//
// On-screen score/lives HUD.  Renders into the IP's 80x30-char text-mode
// VRAM (8x16 font, palette-coloured) wired through pixel_mux.  Visible
// only inside the top HUD region (drawY < 32 px = top 2 char rows); we
// write to row 1 specifically, leaving row 0 blank for a clean top edge.

#ifndef HUD_H
#define HUD_H

#include "xil_types.h"

// Clear the entire VRAM and program palette colours 0 (black) and 15
// (white).  Call once at boot before any hud_write_score_lives() call.
void hud_init(void);

// Update the centred "SCORE n  LIVES m" string on row 1 of the HUD.
// Numbers are variable-width with no leading zeros (e.g. "SCORE 0",
// "SCORE 12345").  Re-centres each call so the text shifts left/right
// as widths change.  No-op when neither score nor lives changed.
void hud_write_score_lives(int score, int lives);

#endif // HUD_H
