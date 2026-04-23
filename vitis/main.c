#include "platform.h"

#include "space_invaders.h"


/*When playing the game, the controls are:
*Left Arrow: move player left while held
*Right Arrow: move player right while held
*Space: fire one projectile on press transition
*Enter: start the game from the start screen
*R: restart after game over
*/

int main(void)
{
    game_t game; // creates an instance of the game

    init_platform(); // intializes the platform
    reset_game(&game); // calls the function to reset the game

    while (1) { // start of the game loop
        keyboard_state_t keys;

        wait_for_frame(&game);
        keys = read_keyboard(); // this sets keys equal to a keyboard_state_t

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

            if (game.lives == 0U) { // check for death
                game.state = STATE_GAMEOVER;
            }
        } else {
            if (keys.restart_pressed) { // restart the game is the restart button is pressed
                reset_game(&game);
            }
        }
        write_sprite_registers(&game); // writes the sprite
    } // end of the game loop

    cleanup_platform(); // clears the platform (Part of the platform file)
    return 0;
}
