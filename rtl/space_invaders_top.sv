`timescale 1 ns / 1 ps

module space_invaders_top (
    // 200 MHz differential DDR reference clock (pins C1/B1)
    input  logic        sys_clk_p,
    input  logic        sys_clk_n,

    // Reset (active-low)
    input  logic        cpu_resetn,

    // HDMI TX (make these External in the block design first)
    output logic        hdmi_clk_p,
    output logic        hdmi_clk_n,
    output logic [2:0]  hdmi_tx_p,
    output logic [2:0]  hdmi_tx_n,

    // UART
    input  logic        uart_rxd,
    output logic        uart_txd,

    // GPIO (32-bit tristate — AXI GPIO IP)
    inout  wire [31:0]  gpio_0,
    inout  wire [31:0]  gpio_1,

    // USB SPI (MAX3421E)
    output logic        usb_sclk,
    output logic        usb_mosi,
    input  logic        usb_miso,
    output logic        usb_ss_n
);

    design_1_wrapper u_bd (
        .diff_clock_rtl_0_clk_p (sys_clk_p),
        .diff_clock_rtl_0_clk_n (sys_clk_n),
        .reset_rtl_0             (~cpu_resetn),

        .hdmi_clk_p_0  (hdmi_clk_p),
        .hdmi_clk_n_0  (hdmi_clk_n),
        .hdmi_tx_p_0   (hdmi_tx_p),
        .hdmi_tx_n_0   (hdmi_tx_n),

        .uart_rtl_0_rxd (uart_rxd),
        .uart_rtl_0_txd (uart_txd),

        .gpio_rtl_0_tri_io (gpio_0),
        .gpio_rtl_1_tri_io (gpio_1),

        .usb_sclk  (usb_sclk),
        .usb_mosi  (usb_mosi),
        .usb_miso  (usb_miso),
        .usb_ss_n  (usb_ss_n)
    );

endmodule
