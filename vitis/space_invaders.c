#include "space_invaders.h"
#include "usb_hid.h"

static uint32_t pack_projectile(projectile_t proj)
{
    if (!proj.active) {
        return 0U;
    }
    return (1U << 20) | ((uint32_t)(proj.x & 0x3FFU) << 10) |
           (uint32_t)(proj.y & 0x3FFU);
}

static uint32_t alien_index(uint32_t row, uint32_t col)
{
    return (row * ALIEN_COLS) + col;
}

static uint32_t alien_score_for_row(uint32_t row)
{
    if (row == 0U) return 30U;
    if (row <= 2U) return 20U;
    return 10U;
}

static void award_alien_kill_score(game_t *game, uint32_t row)
{
    game->score += alien_score_for_row(row);
    publish_score(game);
}

static uint16_t clamp_player_x(uint16_t x)
{
    uint16_t max_x = SCREEN_W - PLAYER_W;
    return (x > max_x) ? max_x : x;
}

uint32_t score_to_bcd(uint32_t score)
{
    uint32_t bcd = 0U;
    uint32_t digit;

    if (score > 999999U) {
        score = 999999U;
    }

    for (digit = 0U; digit < 6U; ++digit) {
        bcd |= (score % 10U) << (digit * 4U);
        score /= 10U;
    }

    return bcd;
}

void publish_score(const game_t *game)
{
    uint32_t score_bcd = score_to_bcd(game->score);

    /*
     * HEX display integration point:
     * When the FPGA seven-segment/HEX display hardware is added,
     * write score_bcd to that memory-mapped peripheral here.
     *
     * Expected format:
     *   bits [3:0]   = ones digit
     *   bits [7:4]   = tens digit
     *   bits [11:8]  = hundreds digit
     *   bits [15:12] = thousands digit
     *   bits [19:16] = ten-thousands digit
     *   bits [23:20] = hundred-thousands digit
     */
#ifdef XPAR_SCORE_DISPLAY_0_BASEADDR
    Xil_Out32(XPAR_SCORE_DISPLAY_0_BASEADDR, score_bcd);
#else
    (void)score_bcd;
#endif
}

void reset_game(game_t *game)
{
    uint32_t i;

    game->state = STATE_START;
    game->player_x = PLAYER_START_X;
    game->player_projectile.active = 0U;
    game->player_projectile.x = 0U;
    game->player_projectile.y = PROJECTILE_OFFSCREEN_Y;
    game->grid_x = 38U;
    game->grid_y = 28U;
    game->alien_anim_phase = 0U;
    game->alien_dir = 1;
    game->score = 0U;
    publish_score(game);
    game->lives = 3U;
    game->last_frame_counter = Xil_In32(SPRITE_ENGINE_BASEADDR + REG_FRAME_COUNTER);
    game->alien_step_timer = 0U;

    for (i = 0; i < 3U; ++i) {
        game->alien_alive[i] = 0U;
    }
    for (i = 0; i < TOTAL_ALIENS; ++i) {
        game->alien_alive[i / 32U] |= (1U << (i % 32U));
    }

    write_sprite_registers(game);
}

void wait_for_frame(game_t *game)
{
    uint32_t frame_counter;
    do {
        frame_counter = Xil_In32(SPRITE_ENGINE_BASEADDR + REG_FRAME_COUNTER);
    } while (frame_counter == game->last_frame_counter);
    game->last_frame_counter = frame_counter;
}

keyboard_state_t read_keyboard(void)
{
    keyboard_state_t keys = {0, 0, 0, 0, 0};

    usb_hid_poll();
    keys.left_held       = usb_hid_left_held();
    keys.right_held      = usb_hid_right_held();
    keys.space_pressed   = usb_hid_space_pressed();
    keys.enter_pressed   = usb_hid_enter_pressed();
    keys.restart_pressed = usb_hid_r_pressed();

    return keys;
}

void update_player(game_t *game, keyboard_state_t keys)
{
    if (keys.left_held && !keys.right_held) {
        if (game->player_x > PLAYER_SPEED) {
            game->player_x -= PLAYER_SPEED;
        } else {
            game->player_x = 0U;
        }
    } else if (keys.right_held && !keys.left_held) {
        game->player_x = clamp_player_x(game->player_x + PLAYER_SPEED);
    }

    if (keys.space_pressed && !game->player_projectile.active) {
        game->player_projectile.active = 1U;
        game->player_projectile.x = game->player_x + (PLAYER_W / 2U);
        game->player_projectile.y = PLAYER_Y - 1U;
    }
}

void update_projectile(game_t *game)
{
    if (!game->player_projectile.active) return;

    if (game->player_projectile.y > PROJECTILE_SPEED) {
        game->player_projectile.y -= PROJECTILE_SPEED;
    } else {
        game->player_projectile.active = 0U;
        game->player_projectile.x = 0U;
        game->player_projectile.y = PROJECTILE_OFFSCREEN_Y;
    }
}

uint32_t count_alive_aliens(const game_t *game)
{
    uint32_t count = 0U;
    uint32_t i;
    uint32_t word;

    for (i = 0; i < 3U; ++i) {
        word = game->alien_alive[i];
        while (word != 0U) {
            word &= (word - 1U);
            ++count;
        }
    }
    return count;
}

uint8_t get_alive_bit(const game_t *game, uint32_t row, uint32_t col)
{
    uint32_t index = alien_index(row, col);
    return (uint8_t)((game->alien_alive[index / 32U] >> (index % 32U)) & 0x1U);
}

void clear_alive_bit(game_t *game, uint32_t row, uint32_t col)
{
    uint32_t index = alien_index(row, col);
    game->alien_alive[index / 32U] &= ~(1U << (index % 32U));
}

void check_projectile_alien_collision(game_t *game)
{
    uint32_t row, col;
    uint16_t proj_x, proj_y;

    if (!game->player_projectile.active) return;

    proj_x = game->player_projectile.x;
    proj_y = game->player_projectile.y;

    for (row = 0; row < ALIEN_ROWS; ++row) {
        uint16_t alien_y = game->grid_y + (uint16_t)(row * ALIEN_STRIDE_Y);

        if ((proj_y + 1U) < alien_y || proj_y > (alien_y + ALIEN_H)) continue;

        for (col = 0; col < ALIEN_COLS; ++col) {
            uint16_t alien_x;

            if (!get_alive_bit(game, row, col)) continue;

            alien_x = game->grid_x + (uint16_t)(col * ALIEN_STRIDE_X);

            if (proj_x >= alien_x && proj_x <= (alien_x + ALIEN_W)) {
                clear_alive_bit(game, row, col);
                game->player_projectile.active = 0U;
                game->player_projectile.x = 0U;
                game->player_projectile.y = PROJECTILE_OFFSCREEN_Y;
                award_alien_kill_score(game, row);
                return;
            }
        }
    }
}

void update_aliens(game_t *game)
{
    uint32_t alive_count;
    uint32_t step_period;
    uint32_t row, col;
    uint16_t leftmost  = SCREEN_W;
    uint16_t rightmost = 0U;
    uint16_t bottommost = 0U;

    alive_count = count_alive_aliens(game);
    if (alive_count == 0U) {
        game->state = STATE_GAMEOVER;
        return;
    }

    step_period = BASE_ALIEN_STEP_PERIOD;
    if (alive_count < TOTAL_ALIENS) {
        uint32_t speedup = (TOTAL_ALIENS - alive_count) / 4U;
        step_period = (speedup < (BASE_ALIEN_STEP_PERIOD - 5U)) ?
                      (BASE_ALIEN_STEP_PERIOD - speedup) : 5U;
    }

    game->alien_step_timer++;
    if (game->alien_step_timer < step_period) return;

    game->alien_step_timer = 0U;
    game->alien_anim_phase ^= 1U;

    for (row = 0; row < ALIEN_ROWS; ++row) {
        for (col = 0; col < ALIEN_COLS; ++col) {
            uint16_t alien_x, alien_y;

            if (!get_alive_bit(game, row, col)) continue;

            alien_x = game->grid_x + (uint16_t)(col * ALIEN_STRIDE_X);
            alien_y = game->grid_y + (uint16_t)(row * ALIEN_STRIDE_Y);

            if (alien_x < leftmost)            leftmost  = alien_x;
            if ((alien_x + ALIEN_W) > rightmost) rightmost = alien_x + ALIEN_W;
            if ((alien_y + ALIEN_H) > bottommost) bottommost = alien_y + ALIEN_H;
        }
    }

    if (game->alien_dir > 0) {
        if ((rightmost + ALIEN_STEP_PIXELS) >= SCREEN_W) {
            game->alien_dir = -1;
            game->grid_y += ALIEN_DROP_PIXELS;
        } else {
            game->grid_x += ALIEN_STEP_PIXELS;
        }
    } else {
        if (leftmost <= ALIEN_STEP_PIXELS) {
            game->alien_dir = 1;
            game->grid_y += ALIEN_DROP_PIXELS;
        } else {
            game->grid_x -= ALIEN_STEP_PIXELS;
        }
    }

    if (bottommost >= PLAYER_Y) {
        game->state = STATE_GAMEOVER;
    }
}

void write_sprite_registers(const game_t *game)
{
    uint32_t offset;

    Xil_Out32(SPRITE_ENGINE_BASEADDR + REG_PLAYER_X,
              game->player_x);
    Xil_Out32(SPRITE_ENGINE_BASEADDR + REG_PLAYER_PROJECTILE,
              pack_projectile(game->player_projectile));
    Xil_Out32(SPRITE_ENGINE_BASEADDR + REG_GRID_X,  game->grid_x);
    Xil_Out32(SPRITE_ENGINE_BASEADDR + REG_GRID_Y,  game->grid_y);
    Xil_Out32(SPRITE_ENGINE_BASEADDR + REG_GRID_STEP, game->alien_anim_phase);
    Xil_Out32(SPRITE_ENGINE_BASEADDR + REG_ALIEN_ALIVE_WORD0, game->alien_alive[0]);
    Xil_Out32(SPRITE_ENGINE_BASEADDR + REG_ALIEN_ALIVE_WORD1, game->alien_alive[1]);
    Xil_Out32(SPRITE_ENGINE_BASEADDR + REG_ALIEN_ALIVE_WORD2, game->alien_alive[2]);

    for (offset = REG_ALIEN_PROJECTILE0; offset <= REG_ALIEN_PROJECTILE7; offset += 4U) {
        Xil_Out32(SPRITE_ENGINE_BASEADDR + offset, 0U);
    }
    for (offset = REG_SHIELD_DAMAGE0; offset <= REG_SHIELD_DAMAGE3; offset += 4U) {
        Xil_Out32(SPRITE_ENGINE_BASEADDR + offset, 0U);
    }

    Xil_Out32(SPRITE_ENGINE_BASEADDR + REG_UFO,        0U);
    Xil_Out32(SPRITE_ENGINE_BASEADDR + REG_GAME_STATE, (uint32_t)game->state);
}
