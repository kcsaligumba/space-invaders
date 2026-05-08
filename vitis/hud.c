// hud.c
//
// VRAM byte writes go through the AXI bus with byte-strobe writes; the IP's
// AXI module honours WSTRB to update individual VRAM bytes.  Each on-screen
// character is two consecutive bytes:
//   even byte: (fg_idx << 4) | bg_idx
//   odd  byte: 7-bit ASCII char code (high bit unused / inverse-flag = 0)
// The HDL text pipeline reads this with the layout in hdmi_text_controller.h.

#include <stdio.h>          // snprintf

#include "hud.h"
#include "xparameters.h"

#define HUD_BASE_ADDR  XPAR_HDMI_TEXT_CONTROLLER_0_AXI_BASEADDR
#define HUD_VRAM       ((volatile uint8_t  *)(HUD_BASE_ADDR + 0x0000))
#define HUD_PALETTE    ((volatile uint32_t *)(HUD_BASE_ADDR + 0x2000))

#define COLUMNS  80
#define ROWS     30
#define HUD_ROW   1   // text row (drawY 16..31)

// Palette indices we use (idx 0 + idx 15 are programmed by hud_init).
#define HUD_FG  15
#define HUD_BG   0

static int last_score = -1;
static int last_lives = -1;

static void hud_set_palette(int idx, uint8_t r, uint8_t g, uint8_t b)
{
    // Each 32-bit palette register holds two 12-bit colours.  Lower 16
    // bits = even idx, upper 16 bits = odd idx.
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

void hud_init(void)
{
    hud_clear_vram();
    hud_set_palette(HUD_BG, 0x0, 0x0, 0x0);   // black
    hud_set_palette(HUD_FG, 0xF, 0xF, 0xF);   // white
    last_score = -1;
    last_lives = -1;
}

void hud_write_score_lives(int score, int lives)
{
    if (score == last_score && lives == last_lives) return;
    last_score = score;
    last_lives = lives;

    if (score < 0) score = 0;
    if (lives < 0) lives = 0;

    // Buffer big enough for "SCORE 2147483647  LIVES 2147483647" + NUL.
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "SCORE %d  LIVES %d", score, lives);
    if (n < 0) return;
    if (n > COLUMNS) n = COLUMNS;

    int leading = (COLUMNS - n) / 2;
    if (leading < 0) leading = 0;

    // Rewrite the whole row each call so a digit-count change (e.g. 99 -> 100,
    // which shifts the centre by one column) doesn't leave stale glyphs.
    for (int x = 0; x < leading; x++) {
        hud_write_char(x, HUD_ROW, ' ', HUD_FG, HUD_BG);
    }
    for (int i = 0; i < n; i++) {
        hud_write_char(leading + i, HUD_ROW, buf[i], HUD_FG, HUD_BG);
    }
    for (int x = leading + n; x < COLUMNS; x++) {
        hud_write_char(x, HUD_ROW, ' ', HUD_FG, HUD_BG);
    }
}
