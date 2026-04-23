// Top-level for Space Invaders on the Urbana Board.
// Instantiates the MicroBlaze block design wrapper (design_1_wrapper) and
// routes board-level I/O (system clock/reset, HDMI TX pins, USB HID, HEX display).
// The HDMI controller + AXI sprite engine live inside the block design as the
// custom "hdmi_text_controller" IP.

`timescale 1 ns / 1 ps

module space_invaders_top (
    // Board I/O
    input  logic        clk_100mhz,
    input  logic        cpu_resetn,   // active-low push button

    // HDMI TX
    output logic        hdmi_clk_p,
    output logic        hdmi_clk_n,
    output logic [2:0]  hdmi_tx_p,
    output logic [2:0]  hdmi_tx_n,

    // UART
    input  logic        uart_rxd,
    output logic        uart_txd,

    // HEX display (score / lives) - 8 digits x 7 segments + 8 anodes
    output logic [7:0]  hex_seg,
    output logic [7:0]  hex_an,

    // USB HID (Lab 6.2 style: SPI / custom interface - pin names match your XDC)
    inout  wire  [7:0]  usb_data,
    output logic        usb_ss,
    output logic        usb_sclk,
    output logic        usb_mosi,
    input  logic        usb_miso,
    input  logic        usb_int,

    // Switches / buttons (fallback input)
    input  logic [15:0] sw,
    input  logic [3:0]  btn
);

    // The Vivado-generated block design wrapper exposes ports for every external
    // pin we want to reach from software. Names below match the plan's block
    // design: rename here if your design_1 uses different port names.
    design_1_wrapper u_bd (
        .clk_100MHz   (clk_100mhz),
        .reset_rtl_0  (~cpu_resetn),

        // HDMI TX (from hdmi_text_controller inside the BD)
        .hdmi_clk_p   (hdmi_clk_p),
        .hdmi_clk_n   (hdmi_clk_n),
        .hdmi_tx_p    (hdmi_tx_p),
        .hdmi_tx_n    (hdmi_tx_n),

        // UART
        .uart_rxd     (uart_rxd),
        .uart_txd     (uart_txd),

        // GPIO: HEX display
        .hex_seg      (hex_seg),
        .hex_an       (hex_an),

        // GPIO: buttons / switches
        .sw           (sw),
        .btn          (btn),

        // USB HID (SPI-like interface from Lab 6.2)
        .usb_ss       (usb_ss),
        .usb_sclk     (usb_sclk),
        .usb_mosi     (usb_mosi),
        .usb_miso     (usb_miso),
        .usb_int      (usb_int),
        .usb_data     (usb_data)
    );

endmodule