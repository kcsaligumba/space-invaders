#ifndef USB_HID_H
#define USB_HID_H

#include <stdint.h>

/*
 * Minimal USB HID keyboard driver for the MAX3421E USB host controller.
 * The MAX3421E communicates with MicroBlaze via an AXI Quad SPI peripheral.
 *
 * Required Vivado block design changes (do once, then re-export XSA):
 *   1. Add "AXI Quad SPI" IP. Set: Master mode, Standard SPI, 8-bit,
 *      FIFO depth 16, clock ratio such that SPI clk <= 26 MHz.
 *   2. Connect its IO ports to the block design's USB SPI external pins:
 *        io0_o  -> usb_mosi
 *        io1_i  <- usb_miso
 *        sck_o  -> usb_sclk
 *        ss_o   -> usb_ss_n
 *   3. The usb_int pin can be left as a GPIO input or unconnected for now
 *      (the driver polls; it does not use the interrupt line).
 *   4. Connect AXI Quad SPI's S_AXI to the AXI interconnect and assign an
 *      address. Run Connection Automation, Validate, re-generate bitstream.
 *
 * If XPAR_SPI_USB_BASEADDR or XPAR_SPI_0_BASEADDR is not in xparameters.h,
 * usb_hid_init() returns -1 and all key accessors return 0, so the rest of
 * the game is unaffected.
 */

typedef struct {
    uint8_t  compiled_in;
    uint8_t  spi_dead;
    uint8_t  osc_ok;
    uint8_t  revision;
    uint8_t  usbirq_initial;
    uint8_t  usbirq_after_reset;
    uint8_t  hrsl_initial;
    uint8_t  hrsl_after_wait;
    uint8_t  blind_connect_rc;
    uint8_t  last_poll_rc;
    uint8_t  last_hirq;
    uint8_t  last_rcvbc;
    uint8_t  last_report[8];
    uint32_t spi_base;
    uint32_t poll_count;
    uint32_t report_count;
} usb_hid_diag_t;

/* Call once from main() before the game loop. Returns 0 on success. */
int usb_hid_init(void);

/*
 * Call once per game frame (inside read_keyboard).
 * Attempts one USB HID IN transfer and updates internal key state.
 * Returns immediately on NAK or if no keyboard is connected.
 */
void usb_hid_poll(void);

/* Held-key state -- valid after the most recent usb_hid_poll(). */
uint8_t usb_hid_left_held(void);
uint8_t usb_hid_right_held(void);

/*
 * Edge-detected press -- set on the first frame the key goes down,
 * automatically cleared after this function returns 1.
 */
uint8_t usb_hid_space_pressed(void);
uint8_t usb_hid_enter_pressed(void);
uint8_t usb_hid_r_pressed(void);

/* Temporary hardware debug helpers for bring-up. */
void usb_hid_get_diag(usb_hid_diag_t *diag);
uint32_t usb_hid_status_word(void);

#endif /* USB_HID_H */
