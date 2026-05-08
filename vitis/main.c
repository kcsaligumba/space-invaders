// main.c -- Space Invaders, Step 11: alien projectile, player death,
//                                    state machine (START / PLAYING / GAMEOVER).
//
// Centralizes game state in GameState (see game.h) and commits to hardware
// each frame.  Steps 5-10 carry over.  This step:
//   - boots into STATE_START (player input frozen, ENTER -> PLAYING),
//   - spawns one alien projectile (slot 0) per frame from a random live
//     column's bottom-most alien (game.c::spawn_alien_proj_slot0),
//   - on alien-proj-vs-player AABB hit, decrements lives,
//   - transitions to STATE_GAMEOVER when lives hit 0 OR when the lowest
//     live alien row descends to the player's y,
//   - R from STATE_GAMEOVER calls game_reset() -> STATE_START.

#include <stdio.h>

#include "platform.h"
#include "xparameters.h"
#include "xil_printf.h"

#include "lw_usb/GenericMacros.h"
#include "lw_usb/GenericTypeDefs.h"
#include "lw_usb/MAX3421E.h"
#include "lw_usb/USB.h"

#include "sprite_engine.h"
#include "input.h"
#include "game.h"
#include "hex.h"

// -------------------------------------------------------------------------
// Tuning
// -------------------------------------------------------------------------
#define PLAYER_SPEED  1     // logical pixels per frame

// FRAME_COUNT register lives in the IP's palette/sync region at byte 0x2020
#define FRAME_COUNT_OFFSET   0x2020u
static volatile uint32_t * const FRAME_COUNT_REG =
    (uint32_t *)(XPAR_HDMI_TEXT_CONTROLLER_0_AXI_BASEADDR + FRAME_COUNT_OFFSET);

static int clamp_int(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

int main(void)
{
    init_platform();

    xil_printf("Step 11: alien projectile + state machine\r\n");

    // Initialize hardware sprite registers and game state.
    sprite_init();
    hex_init();

    GameState gs;
    game_reset(&gs);
    game_commit_to_hardware(&gs);
    hex_write_score_lives(gs.score, gs.lives);

    // USB host stack
    xil_printf("initializing MAX3421E...\r\n");
    MAX3421E_init();
    xil_printf("initializing USB...\r\n");
    USB_init();
    input_init();
    xil_printf("Init complete.  Press ENTER to start; LEFT/RIGHT (or A/D) moves, SPACE fires; R resets after game over.\r\n");

    uint32_t last_frame   = *FRAME_COUNT_REG;
    BYTE     running_flag = 0;

    while (1) {
        input_poll();

        // USB drop / re-init handling
        BYTE state = GetUsbTaskState();
        if (state == USB_STATE_RUNNING) {
            if (!running_flag) {
                running_flag = 1;
                xil_printf("USB enumeration complete; controls active\r\n");
            }
        } else if (running_flag) {
            running_flag = 0;
            xil_printf("USB state dropped (state=%x); re-init\r\n", state);
            MAX3421E_init();
            USB_init();
            input_init();
        }

        // Frame edge?
        uint32_t cur_frame = *FRAME_COUNT_REG;
        if (cur_frame == last_frame) continue;
        last_frame = cur_frame;

        input_snapshot();

        // State-machine transitions on press-edges.
        if (gs.state == STATE_START && input_pressed(KEY_ENTER)) {
            gs.state = STATE_PLAYING;
            xil_printf("PLAY!\r\n");
        } else if (gs.state == STATE_GAMEOVER && input_pressed(KEY_R)) {
            game_reset(&gs);
            xil_printf("Reset; press ENTER to start.\r\n");
        }

        // Player input applies only during PLAYING.
        if (gs.state == STATE_PLAYING) {
            int dx = 0;
            if (input_held(KEY_RIGHT_ARROW) || input_held(KEY_D)) dx += PLAYER_SPEED;
            if (input_held(KEY_LEFT_ARROW)  || input_held(KEY_A)) dx -= PLAYER_SPEED;
            gs.player_x = clamp_int(gs.player_x + dx, PLAY_X_MIN, PLAY_X_MAX);

            // Fire on SPACE press-edge (one bullet at a time).
            if (input_pressed(KEY_SPACE)) {
                game_player_fire(&gs);
            }
        }

        // Per-frame game logic (no-op outside STATE_PLAYING).
        game_tick(&gs);

        // Push state to hardware.
        game_commit_to_hardware(&gs);
        hex_write_score_lives(gs.score, gs.lives);
    }

    cleanup_platform();
    return 0;
}
