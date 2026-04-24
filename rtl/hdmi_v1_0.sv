// HDMI text/sprite controller - Space Invaders adaptation of Lab 7.
// Instantiates:
//   - Clocking Wizard (clk_wiz_0): 25 MHz pixel clock and 125 MHz TMDS serial clock.
//   - vga_controller: 640x480@60 timing generator.
//   - pixel_mux:      sprite compositing.
//   - rgb2dvi (or hdmi_tx) IP: serializes RGB + syncs to TMDS HDMI output.
//   - AXI slave:      MicroBlaze-facing sprite engine register file.
//
// Note: clk_wiz_0 and rgb2dvi_0 are Vivado IP-catalog cores. Instantiate them
// in the IP integrator / IP catalog with matching names before synthesis.

`timescale 1 ns / 1 ps

module hdmi_v1_0 #
(
    parameter integer C_AXI_DATA_WIDTH = 32,
    parameter integer C_AXI_ADDR_WIDTH = 8
)
(
    // HDMI differential outputs
    output logic        hdmi_clk_n,
    output logic        hdmi_clk_p,
    output logic [2:0]  hdmi_tx_n,
    output logic [2:0]  hdmi_tx_p,

    // Frame interrupt (pulses for 1 AXI clk at start of VSYNC)
    output logic        frame_irq,

    // AXI4-Lite slave
    input  logic axi_aclk,
    input  logic axi_aresetn,
    input  logic [C_AXI_ADDR_WIDTH-1 : 0] axi_awaddr,
    input  logic [2 : 0] axi_awprot,
    input  logic axi_awvalid,
    output logic axi_awready,
    input  logic [C_AXI_DATA_WIDTH-1 : 0] axi_wdata,
    input  logic [(C_AXI_DATA_WIDTH/8)-1 : 0] axi_wstrb,
    input  logic axi_wvalid,
    output logic axi_wready,
    output logic [1 : 0] axi_bresp,
    output logic axi_bvalid,
    input  logic axi_bready,
    input  logic [C_AXI_ADDR_WIDTH-1 : 0] axi_araddr,
    input  logic [2 : 0] axi_arprot,
    input  logic axi_arvalid,
    output logic axi_arready,
    output logic [C_AXI_DATA_WIDTH-1 : 0] axi_rdata,
    output logic [1 : 0] axi_rresp,
    output logic axi_rvalid,
    input  logic axi_rready
);

    // ---- Clocks ----
    // Assumes clk_wiz_0 is configured with:
    //   clk_in1  = axi_aclk (100 MHz)
    //   clk_out1 = 25 MHz  (pixel clock)
    //   clk_out2 = 125 MHz (TMDS serial, 5x pixel)
    logic pixel_clk;
    logic serial_clk;
    logic clk_wiz_locked;

    clk_wiz_0 u_clk_wiz (
        .clk_in1 (axi_aclk),
        .reset   (~axi_aresetn),
        .clk_out1(pixel_clk),
        .clk_out2(serial_clk),
        .locked  (clk_wiz_locked)
    );

    logic pixel_reset;
    assign pixel_reset = ~clk_wiz_locked;

    // ---- VGA timing ----
    logic        hsync, vsync, vga_active, vga_sync;
    logic [9:0]  drawX, drawY;
    vga_controller u_vga (
        .pixel_clk    (pixel_clk),
        .reset        (pixel_reset),
        .hs           (hsync),
        .vs           (vsync),
        .active_nblank(vga_active),
        .sync         (vga_sync),
        .drawX        (drawX),
        .drawY        (drawY)
    );

    // ---- AXI register file -> sprite engine state ----
    logic [31:0] sprite_regs [24];
    logic [31:0] frame_counter;
    logic [31:0] collision_flags;

    hdmi_v1_0_AXI #(
        .C_S_AXI_DATA_WIDTH(C_AXI_DATA_WIDTH),
        .C_S_AXI_ADDR_WIDTH(C_AXI_ADDR_WIDTH)
    ) u_axi (
        .sprite_regs        (sprite_regs),
        .frame_counter_in   (frame_counter),
        .collision_flags_in (collision_flags),
        .S_AXI_ACLK   (axi_aclk),
        .S_AXI_ARESETN(axi_aresetn),
        .S_AXI_AWADDR (axi_awaddr),
        .S_AXI_AWPROT (axi_awprot),
        .S_AXI_AWVALID(axi_awvalid),
        .S_AXI_AWREADY(axi_awready),
        .S_AXI_WDATA  (axi_wdata),
        .S_AXI_WSTRB  (axi_wstrb),
        .S_AXI_WVALID (axi_wvalid),
        .S_AXI_WREADY (axi_wready),
        .S_AXI_BRESP  (axi_bresp),
        .S_AXI_BVALID (axi_bvalid),
        .S_AXI_BREADY (axi_bready),
        .S_AXI_ARADDR (axi_araddr),
        .S_AXI_ARPROT (axi_arprot),
        .S_AXI_ARVALID(axi_arvalid),
        .S_AXI_ARREADY(axi_arready),
        .S_AXI_RDATA  (axi_rdata),
        .S_AXI_RRESP  (axi_rresp),
        .S_AXI_RVALID (axi_rvalid),
        .S_AXI_RREADY (axi_rready)
    );

    // Unpack the register map used by the baseline
    logic [9:0]  player_x;
    logic        player_proj_active;
    logic [9:0]  player_proj_x, player_proj_y;
    logic [9:0]  grid_x, grid_y;
    logic        grid_step;
    logic [95:0] alien_alive;
    logic [1:0]  game_state;

    assign player_x           = sprite_regs[0][9:0];
    assign player_proj_y      = sprite_regs[1][9:0];
    assign player_proj_x      = sprite_regs[1][19:10];
    assign player_proj_active = sprite_regs[1][20];
    assign grid_x             = sprite_regs[2][9:0];
    assign grid_y             = sprite_regs[3][9:0];
    assign grid_step          = sprite_regs[4][0];
    assign alien_alive        = {sprite_regs[7], sprite_regs[6], sprite_regs[5]};
    assign game_state         = sprite_regs[21][1:0];

    // ---- Pixel color multiplexer ----
    logic [7:0] pix_r, pix_g, pix_b;
    pixel_mux u_mux (
        .pixel_clk          (pixel_clk),
        .reset              (pixel_reset),
        .drawX              (drawX),
        .drawY              (drawY),
        .active             (vga_active),
        .player_x           (player_x),
        .player_proj_active (player_proj_active),
        .player_proj_x      (player_proj_x),
        .player_proj_y      (player_proj_y),
        .grid_x             (grid_x),
        .grid_y             (grid_y),
        .grid_step          (grid_step),
        .alien_alive        (alien_alive),
        .game_state         (game_state),
        .red                (pix_r),
        .green              (pix_g),
        .blue               (pix_b)
    );

    // ---- RGB -> HDMI/TMDS ----
    // rgb2dvi_0 is a Digilent/Xilinx IP that handles TMDS encoding + serialization.
    // If the course uses a different core (e.g. hdmi_tx_0), swap the instance.
    rgb2dvi_0 u_rgb2dvi (
        .TMDS_Clk_p   (hdmi_clk_p),
        .TMDS_Clk_n   (hdmi_clk_n),
        .TMDS_Data_p  (hdmi_tx_p),
        .TMDS_Data_n  (hdmi_tx_n),
        .aRst         (pixel_reset),
        .vid_pData    ({pix_r, pix_g, pix_b}),
        .vid_pVDE     (vga_active),
        .vid_pHSync   (hsync),
        .vid_pVSync   (vsync),
        .PixelClk     (pixel_clk),
        .SerialClk    (serial_clk)
    );

    // ---- Frame counter and VSYNC interrupt ----
    // Detect VSYNC falling edge (active-low assertion) on pixel_clk, synchronize to AXI clock.
    logic vsync_r, vsync_fall_pclk;
    always_ff @(posedge pixel_clk) begin
        vsync_r <= vsync;
        vsync_fall_pclk <= vsync_r & ~vsync;
    end

    // CDC: 2-FF synchronizer + edge detect on axi_aclk
    logic vs_sync0, vs_sync1, vs_sync2;
    always_ff @(posedge axi_aclk) begin
        if (!axi_aresetn) begin
            vs_sync0 <= 0; vs_sync1 <= 0; vs_sync2 <= 0;
        end else begin
            vs_sync0 <= vsync_fall_pclk;
            vs_sync1 <= vs_sync0;
            vs_sync2 <= vs_sync1;
        end
    end
    assign frame_irq = vs_sync1 & ~vs_sync2;

    always_ff @(posedge axi_aclk) begin
        if (!axi_aresetn)
            frame_counter <= 32'h0;
        else if (frame_irq)
            frame_counter <= frame_counter + 1;
    end

    // Placeholder for hardware collision detection (Option B in plan).
    // Baseline uses Option A (software AABB), so leave zero for now.
    assign collision_flags = 32'h0;

endmodule