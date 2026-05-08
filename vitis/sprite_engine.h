// sprite_engine.h
//
// Software-side view of the sprite-state register region inside the
// hdmi_text_controller IP.  Lives at base + 0x4000 (region select bit
// addr[14]==1 inside the IP's AXI register file).
//
// The hardware pixel_mux.sv reads these registers directly each pixel.
// Software writes them at most once per frame, between vsync events.

#ifndef SPRITE_ENGINE_H
#define SPRITE_ENGINE_H

#include "xil_types.h"
#include "xparameters.h"

// -------------------------------------------------------------------------
// Memory-mapped layout (must match the SystemVerilog assigns in
// hdmi_text_controller_v1_0_AXI.sv).  Word offsets within the sprite region:
//
//   0x00 PLAYER_X       [9:0]   logical x, 0..319-PLAYER_W
//   0x04 PLAYER_PROJ    [31]=active, [25:16]=y, [9:0]=x
//   0x08 GRID_XY        [28]=step, [25:16]=y, [9:0]=x
//   0x0C (reserved)
//   0x10 ALIEN_ALIVE[0] bits 0..31  (alien r*11+c, row-major)
//   0x14 ALIEN_ALIVE[1] bits 32..63
//   0x18 ALIEN_ALIVE[2] bits 64..95 (only bits 64..86 meaningful for 5x11)
//   0x1C (reserved)
//   0x20 ALIEN_PROJ[0]  same packing as PLAYER_PROJ
//   0x24 ALIEN_PROJ[1]
//   0x28 ALIEN_PROJ[2]
//   0x2C (reserved)
//   0x30 SHIELD_DAMAGE[0]  [15:0] one bit per chunk; set => destroyed
//   0x34 SHIELD_DAMAGE[1]
//   0x38 SHIELD_DAMAGE[2]
//   0x3C SHIELD_DAMAGE[3]
//   0x40 UFO            [31]=active, [9:0]=x
//   0x44 GAME_STATE     [1:0]
// -------------------------------------------------------------------------

#define SPRITE_REGION_OFFSET 0x4000u

struct SPRITE_STATE {
    uint32_t PLAYER_X;          // 0x00
    uint32_t PLAYER_PROJ;       // 0x04
    uint32_t GRID_XY;           // 0x08
    uint32_t _pad0;             // 0x0C
    uint32_t ALIEN_ALIVE[3];    // 0x10..0x18
    uint32_t _pad1;             // 0x1C
    uint32_t ALIEN_PROJ[3];     // 0x20..0x28
    uint32_t _pad2;             // 0x2C
    uint32_t SHIELD_DAMAGE[4];  // 0x30..0x3C
    uint32_t UFO;               // 0x40
    uint32_t GAME_STATE;        // 0x44
};

// Game state codes (must match pixel_mux.sv localparams).
#define STATE_START     0u
#define STATE_PLAYING   1u
#define STATE_GAMEOVER  2u

// -------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------

// Zero out the entire sprite-state region and set sane initial values
// (player centered, GAME_STATE=PLAYING, all aliens dead, no projectiles).
void sprite_init(void);

// Individual setters.  Each performs a single 32-bit AXI write.
void sprite_set_player_x      (uint16_t x);
void sprite_set_player_proj   (int active, uint16_t x, uint16_t y);
void sprite_set_grid          (uint16_t x, uint16_t y, int step);
void sprite_set_alien_alive_word(int idx, uint32_t bits);   // idx in 0..2
void sprite_set_alien_proj    (int idx, int active, uint16_t x, uint16_t y); // idx in 0..2
void sprite_set_shield_damage (int idx, uint16_t damage);   // idx in 0..3
void sprite_set_ufo           (int active, uint16_t x);
void sprite_set_game_state    (uint8_t state);

#endif // SPRITE_ENGINE_H
