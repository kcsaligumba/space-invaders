// hud.c
//
// VRAM byte writes go through the AXI bus with byte-strobe writes; the IP's
// AXI module honours WSTRB to update individual VRAM bytes.  Each on-screen
// character is two consecutive bytes:
//   even byte: (fg_idx << 4) | bg_idx
//   odd  byte: 7-bit ASCII char code (high bit unused / inverse-flag = 0)
// The HDL text pipeline reads this with the layout in hdmi_text_controller.h.
//
// For the score-advance-table sprites on the title screen, pixel_mux.sv
// renders the actual alien/UFO sprites at fixed (TITLE_X, TITLE_*_Y)
// positions; we just leave columns 32-35 blank in those text rows so the
// sprites have empty space to draw into.

#include <stdio.h>          // snprintf
#include <string.h>         // strlen

#include "hud.h"
#include "xparameters.h"
#include "sprite_engine.h"  // STATE_START / PLAYING / GAMEOVER

#define HUD_BASE_ADDR  XPAR_HDMI_TEXT_CONTROLLER_0_AXI_BASEADDR
#define HUD_VRAM       ((volatile uint8_t  *)(HUD_BASE_ADDR + 0x0000))
#define HUD_PALETTE    ((volatile uint32_t *)(HUD_BASE_ADDR + 0x2000))

#define COLUMNS  80
#define ROWS     30

// Palette indices we use (programmed by hud_init).
#define HUD_FG  15
#define HUD_BG   0

// Title-screen layout (text rows; HDL renders sprites alongside).
#define ROW_PLAY            8
#define ROW_GAME_TITLE      9   // "SPACE INVADERS"
#define ROW_TABLE_HEADER    13   // "*SCORE ADVANCE TABLE*"
#define ROW_TABLE_UFO       15
#define ROW_TABLE_OCTOPUS   17
#define ROW_TABLE_CRAB     19
#define ROW_TABLE_SQUID    21
#define ROW_PRESS_ENTER    27

// Score table text starts at this column (HDL sprites occupy cols 32-35).
#define TABLE_TEXT_COL     37

// GAME OVER overlay row (above the always-on SCORE/LIVES line on row 1).
#define ROW_GAME_OVER       0

// Persistent SCORE/LIVES line.
#define ROW_SCORE_LIVES     1

#define BLINK_FRAMES       30   // frames per blink half-cycle (~0.5 s)

// -----------------------------------------------------------------------
// Low-level VRAM/palette helpers
// -----------------------------------------------------------------------

static void hud_set_palette(int idx, uint8_t r, uint8_t g, uint8_t b)
{
    int reg = idx >> 1;
    uint32_t cur = HUD_PALETTE[reg];
    uint32_t v   = ((uint32_t)(r & 0xF) << 8)
                 | ((uint32_t)(g & 0xF) << 4)
                 |  (uint32_t)(b & 0xF);
    if (idx & 1) {
        cur = (cur & 0x0000FFFFu) | (v << 16);
    } else {
        cur = (cur & 0xFFFF0000u) | v;
    }
    HUD_PALETTE[reg] = cur;
}

static void hud_clear_vram(void)
{
    for (int i = 0; i < ROWS * COLUMNS * 2; i++) {
        HUD_VRAM[i] = 0;
    }
}

static void hud_write_char(int col, int row, char ch, uint8_t fg, uint8_t bg)
{
    if ((unsigned)col >= COLUMNS || (unsigned)row >= ROWS) return;
    int byte_addr = (row * COLUMNS + col) * 2;
    HUD_VRAM[byte_addr]     = (uint8_t)(((fg & 0xF) << 4) | (bg & 0xF));
    HUD_VRAM[byte_addr + 1] = (uint8_t)ch;
}

static void clear_row(int row)
{
    for (int x = 0; x < COLUMNS; x++) {
        hud_write_char(x, row, ' ', HUD_FG, HUD_BG);
    }
}

static void draw_text_at(const char *str, int col, int row)
{
    int i = 0;
    while (str[i] != 0 && (col + i) < COLUMNS) {
        hud_write_char(col + i, row, str[i], HUD_FG, HUD_BG);
        i++;
    }
}

static void draw_centered(const char *str, int row)
{
    int n = (int)strlen(str);
    if (n > COLUMNS) n = COLUMNS;
    int leading = (COLUMNS - n) / 2;
    if (leading < 0) leading = 0;
    for (int i = 0; i < n; i++) {
        hud_write_char(leading + i, row, str[i], HUD_FG, HUD_BG);
    }
}

// Re-render the SCORE/LIVES line.  Always rewrites the whole row so a
// digit-count change doesn't leave stale glyphs behind.
static void draw_score_lives(int score, int lives)
{
    if (score < 0) score = 0;
    if (lives < 0) lives = 0;

    char buf[64];
    int n = snprintf(buf, sizeof(buf), "SCORE %d  LIVES %d", score, lives);
    if (n < 0) return;
    if (n > COLUMNS) n = COLUMNS;

    int leading = (COLUMNS - n) / 2;
    if (leading < 0) leading = 0;

    for (int x = 0; x < leading; x++) {
        hud_write_char(x, ROW_SCORE_LIVES, ' ', HUD_FG, HUD_BG);
    }
    for (int i = 0; i < n; i++) {
        hud_write_char(leading + i, ROW_SCORE_LIVES, buf[i], HUD_FG, HUD_BG);
    }
    for (int x = leading + n; x < COLUMNS; x++) {
        hud_write_char(x, ROW_SCORE_LIVES, ' ', HUD_FG, HUD_BG);
    }
}

// -----------------------------------------------------------------------
// Title-screen / GAME-OVER templates
// -----------------------------------------------------------------------

static void draw_title_screen(void)
{
    draw_centered("PLAY",                  ROW_PLAY);
    draw_centered("SPACE INVADERS",        ROW_GAME_TITLE);
    draw_centered("*SCORE ADVANCE TABLE*", ROW_TABLE_HEADER);

    // Score table -- pixel_mux renders the alien/UFO sprite at cols 32-35
    // for these rows when game_state == STATE_START.  We only fill in the
    // text portion to the right of the sprite.
    draw_text_at("= ? MYSTERY", TABLE_TEXT_COL, ROW_TABLE_UFO);
    draw_text_at("= 30 POINTS", TABLE_TEXT_COL, ROW_TABLE_OCTOPUS);
    draw_text_at("= 20 POINTS", TABLE_TEXT_COL, ROW_TABLE_CRAB);
    draw_text_at("= 10 POINTS", TABLE_TEXT_COL, ROW_TABLE_SQUID);
}

static void draw_game_over(void)
{
    draw_centered("GAME OVER", ROW_GAME_OVER);
}

// -----------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------

void hud_init(void)
{
    hud_clear_vram();
    hud_set_palette(HUD_BG, 0x0, 0x0, 0x0);   // black
    hud_set_palette(HUD_FG, 0xF, 0xF, 0xF);   // white
}

void hud_update(uint8_t state, int score, int lives)
{
    static uint8_t  last_state       = 0xFF;
    static int      last_score       = -1;
    static int      last_lives       = -1;
    static uint32_t local_frame      = 0;
    static int      last_blink_phase = -1;

    int state_changed = (state != last_state);
    local_frame++;

    if (state_changed) {
        // Wipe everything and redraw the static template for this state.
        hud_clear_vram();
        last_state       = state;
        last_score       = -1;       // force score/lives re-draw
        last_lives       = -1;
        last_blink_phase = -1;       // force PRESS ENTER re-draw

        if (state == STATE_START) {
            draw_title_screen();
        } else if (state == STATE_GAMEOVER) {
            draw_game_over();
        }
    }

    // SCORE/LIVES line is always shown, regardless of state.
    if (score != last_score || lives != last_lives) {
        draw_score_lives(score, lives);
        last_score = score;
        last_lives = lives;
    }

    // Blink "PRESS ENTER" during STATE_START.
    if (state == STATE_START) {
        int blink_phase = (int)((local_frame / BLINK_FRAMES) & 1u);
        if (blink_phase != last_blink_phase) {
            if (blink_phase == 0) {
                draw_centered("PRESS ENTER", ROW_PRESS_ENTER);
            } else {
                clear_row(ROW_PRESS_ENTER);
            }
            last_blink_phase = blink_phase;
        }
    }
}
