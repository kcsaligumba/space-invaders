// hud.h
//
// State-aware on-screen HUD.  Renders into the IP's 80x30-char text-mode
// VRAM (8x16 font, palette-coloured) wired through pixel_mux.  Layout:
//   STATE_START    -- Title screen: PLAY / SPACE INVADERS / *SCORE
//                     ADVANCE TABLE* / 4 score-table rows (HDL renders
//                     alien sprites at fixed positions next to each line)
//                     / blinking PRESS ENTER.  SCORE/LIVES still on row 1.
//   STATE_PLAYING  -- Just the SCORE/LIVES line on row 1.
//   STATE_GAMEOVER -- GAME OVER on row 0, SCORE/LIVES on row 1.

#ifndef HUD_H
#define HUD_H

#include "xil_types.h"

// Clear the entire VRAM and program palette colours 0 (black) and 15
// (white).  Call once at boot, before any hud_update() call.
void hud_init(void);

// Render the HUD for the current game state.  On state transitions the
// whole VRAM is wiped and the appropriate template is drawn.  The score
// and lives line on row 1 is always re-rendered when those values change.
// During STATE_START the "PRESS ENTER" prompt blinks at ~1 Hz.
//
// Cheap to call every frame -- only writes the bytes that actually need
// to change.
void hud_update(uint8_t state, int score, int lives);

#endif // HUD_H
