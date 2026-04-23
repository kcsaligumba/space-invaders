#include "platform.h"

#include "space_invaders.h"

int main(void)
{
    game_t game;

    init_platform();
    reset_game(&game);

    while (1) {
        keyboard_state_t keys;

        wait_for_frame(&game);
        keys = read_keyboard();

        if (game.state == STATE_START) {
            if (keys.enter_pressed) {
                game.state = STATE_PLAYING;
            }
        } else if (game.state == STATE_PLAYING) {
            /*
             * One simulation tick per video frame. Hardware renders from the
             * registers written at the end of this block.
             */
            update_player(&game, keys);
            update_projectile(&game);
            check_projectile_alien_collision(&game);
            update_aliens(&game);

            if (game.lives == 0U) {
                game.state = STATE_GAMEOVER;
            }
        } else {
            if (keys.restart_pressed) {
                reset_game(&game);
            }
        }

        write_sprite_registers(&game);
    }

    cleanup_platform();
    return 0;
}
