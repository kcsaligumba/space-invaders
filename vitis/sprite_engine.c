// sprite_engine.c
//
// Thin shims that pack scalar arguments into the bit layout the AXI
// register file expects, then issue a 32-bit store.

#include "sprite_engine.h"

static volatile struct SPRITE_STATE * const sprite =
    (struct SPRITE_STATE *)(XPAR_HDMI_TEXT_CONTROLLER_0_AXI_BASEADDR
                            + SPRITE_REGION_OFFSET);

#define MASK10  0x3FFu

void sprite_init(void)
{
    sprite->PLAYER_X         = 152u;            // logical screen-center
    sprite->PLAYER_PROJ      = 0u;
    sprite->GRID_XY          = 0u;
    sprite->_pad0            = 0u;
    sprite->ALIEN_ALIVE[0]   = 0u;
    sprite->ALIEN_ALIVE[1]   = 0u;
    sprite->ALIEN_ALIVE[2]   = 0u;
    sprite->_pad1            = 0u;
    sprite->ALIEN_PROJ[0]    = 0u;
    sprite->ALIEN_PROJ[1]    = 0u;
    sprite->ALIEN_PROJ[2]    = 0u;
    sprite->_pad2            = 0u;
    sprite->SHIELD_DAMAGE[0] = 0u;
    sprite->SHIELD_DAMAGE[1] = 0u;
    sprite->SHIELD_DAMAGE[2] = 0u;
    sprite->SHIELD_DAMAGE[3] = 0u;
    sprite->UFO              = 0u;
    sprite->GAME_STATE       = STATE_PLAYING;
}

void sprite_set_player_x(uint16_t x)
{
    sprite->PLAYER_X = ((uint32_t)x) & MASK10;
}

void sprite_set_player_proj(int active, uint16_t x, uint16_t y)
{
    uint32_t v = (((uint32_t)y) & MASK10) << 16
               | (((uint32_t)x) & MASK10);
    if (active) v |= (1u << 31);
    sprite->PLAYER_PROJ = v;
}

void sprite_set_grid(uint16_t x, uint16_t y, int step)
{
    uint32_t v = (((uint32_t)y) & MASK10) << 16
               | (((uint32_t)x) & MASK10);
    if (step) v |= (1u << 28);
    sprite->GRID_XY = v;
}

void sprite_set_alien_alive_word(int idx, uint32_t bits)
{
    if ((unsigned)idx < 3u) sprite->ALIEN_ALIVE[idx] = bits;
}

void sprite_set_alien_proj(int idx, int active, uint16_t x, uint16_t y)
{
    if ((unsigned)idx >= 3u) return;
    uint32_t v = (((uint32_t)y) & MASK10) << 16
               | (((uint32_t)x) & MASK10);
    if (active) v |= (1u << 31);
    sprite->ALIEN_PROJ[idx] = v;
}

void sprite_set_shield_damage(int idx, uint16_t damage)
{
    if ((unsigned)idx < 4u) sprite->SHIELD_DAMAGE[idx] = (uint32_t)damage;
}

void sprite_set_ufo(int active, uint16_t x)
{
    uint32_t v = ((uint32_t)x) & MASK10;
    if (active) v |= (1u << 31);
    sprite->UFO = v;
}

void sprite_set_game_state(uint8_t state)
{
    sprite->GAME_STATE = (uint32_t)(state & 0x3u);
}
