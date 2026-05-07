#include "platform.h"
#include "space_invaders.h"
#include "usb_hid.h"
#include "xil_printf.h"

static game_t game;
static uint32_t last_usb_status = 0xFFFFFFFFu;

static void print_usb_status_if_changed(void)
{
    uint32_t status = usb_hid_status_word();

    if (status != last_usb_status) {
        last_usb_status = status;
        xil_printf("USB HID status: 0x%x\r\n", (unsigned int)status);
    }
}

int main(void)
{
    init_platform();
    usb_hid_init();
    print_usb_status_if_changed();
    reset_game(&game);

    while (1) {
        keyboard_state_t keys;
        uint32_t i;

        /* Temporary: fixed delay instead of frame-sync, to diagnose HDMI */
        for (i = 0u; i < 2000000u; i++) { (void)i; }
        keys = read_keyboard();
        print_usb_status_if_changed();

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
