#include "platform.h"
#include "space_invaders.h"
#include "usb_hid.h"

static game_t game;

int main(void)
{
    uint32_t debug_loop = 0U;

    init_platform();
    usb_hid_init();
    usb_hid_print_diag("after-init");
    reset_game(&game);

    while (1) {
        keyboard_state_t keys;
        uint32_t i;

        /* Temporary: fixed delay instead of frame-sync, to diagnose HDMI */
        for (i = 0u; i < 2000000u; i++) { (void)i; }
        keys = read_keyboard();
        debug_loop++;

        if ((debug_loop & 0x7FU) == 0U) {
            usb_hid_print_diag("poll");
        }

        if (game.state == STATE_START) {
            if (keys.enter_pressed) {
                game.state = STATE_PLAYING;
            }
        } else if (game.state == STATE_PLAYING) {
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
