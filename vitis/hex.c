// hex.c
//
// The AXI GPIO that drives the HEX displays is the same one Lab 6 used for
// keycodes (XPAR_GPIO_USB_KEYCODE_BASEADDR), channel 1: data at offset
// 0x00, tri-state at 0x04.  Writing 0 to TRI sets all bits as outputs.
// Each nibble of the 32-bit data word lights one HEX digit, with bits
// [31:28] going to the leftmost display (HEX[7]) and bits [3:0] to the
// rightmost (HEX[0]) -- so the natural left-to-right reading of the hex
// representation matches what appears on the board.

#include "hex.h"
#include "xparameters.h"

#define HEX_GPIO_BASE      XPAR_GPIO_USB_KEYCODE_BASEADDR
#define HEX_DATA_OFFSET    0x00u
#define HEX_TRI_OFFSET     0x04u

static volatile uint32_t * const HEX_DATA =
    (uint32_t *)(HEX_GPIO_BASE + HEX_DATA_OFFSET);
static volatile uint32_t * const HEX_TRI  =
    (uint32_t *)(HEX_GPIO_BASE + HEX_TRI_OFFSET);

#define SCORE_MAX  9999u        // 4 digits, fits entirely in HexA
#define LIVES_MAX  99u          // 2 digits, sits in the rightmost two HexB digits

void hex_init(void)
{
    *HEX_TRI  = 0u;     // configure all 32 bits as outputs
    *HEX_DATA = 0u;     // start displays at "00000000"
}

static uint32_t pack_bcd(uint32_t value, int digits)
{
    uint32_t packed = 0u;
    for (int d = 0; d < digits; d++) {
        packed |= (value % 10u) << (4 * d);
        value /= 10u;
    }
    return packed;
}

void hex_write_score_lives(int score, int lives)
{
    uint32_t s = (score < 0) ? 0u : (uint32_t)score;
    uint32_t l = (lives < 0) ? 0u : (uint32_t)lives;
    if (s > SCORE_MAX) s = SCORE_MAX;
    if (l > LIVES_MAX) l = LIVES_MAX;

    uint32_t score_bcd = pack_bcd(s, 4);    // HEX[7..4] = HexA
    uint32_t lives_bcd = pack_bcd(l, 2);    // HEX[1..0] = right side of HexB

    // HEX[3..2] are intentionally left at 0 ("00") as a visual gap.
    *HEX_DATA = (score_bcd << 16) | lives_bcd;
}
