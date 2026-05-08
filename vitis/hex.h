// hex.h
//
// Drive the on-board HEX displays via the GPIO_USB_KEYCODE channel-1 path
// already wired into space_invaders_top.sv.  Layout (left to right):
//   HEX[7..4] (HexA) -- 4-digit BCD score, max 9999
//   HEX[3..2]        -- always "00" (visual gap between score and lives)
//   HEX[1..0] (HexB) -- 2-digit BCD lives, max 99
// Score and lives both display with leading zeros.

#ifndef HEX_H
#define HEX_H

#include "xil_types.h"

// Configure the GPIO channel as output and zero the display.  Call once
// at boot before any hex_write_score_lives() call.
void hex_init(void);

// Pack score (clamped 0..9999) + lives (clamped 0..99) into BCD and write
// to the HEX display register.  Cheap -- a few mod/divs and one memory-
// mapped store -- safe to call every frame.
void hex_write_score_lives(int score, int lives);

#endif // HEX_H
