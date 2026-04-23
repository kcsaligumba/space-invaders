#ifndef SPACE_INVADERS_H
#define SPACE_INVADERS_H

#include <stdint.h>

#include "xil_io.h"
#include "xparameters.h"

#define SCREEN_W 320U
#define SCREEN_H 240U

#define PLAYER_Y 220U
#define PLAYER_W 16U
#define PLAYER_H 8U

#define ALIEN_ROWS 5U
#define ALIEN_COLS 11U
#define ALIEN_W 16U
#define ALIEN_H 8U
#define ALIEN_STRIDE_X 22U
#define ALIEN_STRIDE_Y 16U
#define TOTAL_ALIENS 55U

#define PLAYER_SPEED 2U
#define PROJECTILE_SPEED 4U
#define BASE_ALIEN_STEP_PERIOD 30U
#define ALIEN_STEP_PIXELS 2U
#define ALIEN_DROP_PIXELS 8U

#define PLAYER_START_X ((SCREEN_W - PLAYER_W) / 2U)
#define PROJECTILE_OFFSCREEN_Y 0x3FFU

#define REG_PLAYER_X              0x00U
#define REG_PLAYER_PROJECTILE     0x04U
#define REG_GRID_X                0x08U
#define REG_GRID_Y                0x0CU
#define REG_GRID_STEP             0x10U
#define REG_ALIEN_ALIVE_WORD0     0x14U
#define REG_ALIEN_ALIVE_WORD1     0x18U
#define REG_ALIEN_ALIVE_WORD2     0x1CU
#define REG_ALIEN_PROJECTILE0     0x20U
#define REG_ALIEN_PROJECTILE7     0x3CU
#define REG_SHIELD_DAMAGE0        0x40U
#define REG_SHIELD_DAMAGE3        0x4CU
#define REG_UFO                   0x50U
#define REG_GAME_STATE            0x54U
#define REG_COLLISION_FLAGS       0x58U
#define REG_FRAME_COUNTER         0x5CU

#define SPRITE_ENGINE_BASEADDR XPAR_SPRITE_ENGINE_0_S00_AXI_BASEADDR

/*
 * Replace this base address with the keyboard or USB-HID peripheral generated
 * in your design once that IP is available in xparameters.h.
 */
#ifndef XPAR_KEYBOARD_0_S00_AXI_BASEADDR
#define XPAR_KEYBOARD_0_S00_AXI_BASEADDR 0U
#endif

#define KEYBOARD_BASEADDR XPAR_KEYBOARD_0_S00_AXI_BASEADDR
#define REG_KEYBOARD_HELD         0x00U
#define REG_KEYBOARD_PRESSED      0x04U

#define KEY_LEFT_MASK   (1U << 0)
#define KEY_RIGHT_MASK  (1U << 1)
#define KEY_SPACE_MASK  (1U << 2)
#define KEY_ENTER_MASK  (1U << 3)
#define KEY_R_MASK      (1U << 4)

typedef enum {
    STATE_START = 0,
    STATE_PLAYING = 1,
    STATE_GAMEOVER = 2
} game_state_t;

typedef struct {
    uint8_t left_held;
    uint8_t right_held;
    uint8_t space_pressed;
    uint8_t enter_pressed;
    uint8_t restart_pressed;
} keyboard_state_t;

typedef struct {
    uint8_t active;
    uint16_t x;
    uint16_t y;
} projectile_t;

typedef struct {
    game_state_t state;
    uint16_t player_x;
    projectile_t player_projectile;
    uint16_t grid_x;
    uint16_t grid_y;
    uint8_t alien_anim_phase;
    int8_t alien_dir;
    uint32_t alien_alive[3];
    uint32_t score;
    uint8_t lives;
    uint32_t last_frame_counter;
    uint32_t alien_step_timer;
} game_t;

void reset_game(game_t *game);
void wait_for_frame(game_t *game);
keyboard_state_t read_keyboard(void);
void update_player(game_t *game, keyboard_state_t keys);
void update_projectile(game_t *game);
void update_aliens(game_t *game);
void check_projectile_alien_collision(game_t *game);
void write_sprite_registers(const game_t *game);
uint32_t count_alive_aliens(const game_t *game);
uint8_t get_alive_bit(const game_t *game, uint32_t row, uint32_t col);
void clear_alive_bit(game_t *game, uint32_t row, uint32_t col);

#endif
