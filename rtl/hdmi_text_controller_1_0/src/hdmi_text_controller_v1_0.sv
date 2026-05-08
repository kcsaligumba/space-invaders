// hdmi_text_controller_v1_0.sv
//
// Sprite-mode top-level for the HDMI text/sprite controller IP.
//
// Step 4: the font/VRAM/palette pixel pipeline that previously drove
// red/green/blue has been REPLACED by pixel_mux.sv, which composites
// player + alien grid + projectile sprites from sprite_rom BRAMs.
//
// The font/VRAM/palette regions in the AXI register file are kept available
// for a future on-screen HUD overlay (Step 16, stretch).  Until then the
// VRAM display read port is tied off and the palette outputs are unused.

`timescale 1 ns / 1 ps

module hdmi_text_controller_v1_0 #
(
    parameter integer C_AXI_DATA_WIDTH = 32,
    parameter integer C_AXI_ADDR_WIDTH = 16
)
(
    // HDMI output ports (unchanged from Lab 7)
    output logic       hdmi_clk_n,
    output logic       hdmi_clk_p,
    output logic [2:0] hdmi_tx_n,
    output logic [2:0] hdmi_tx_p,

    // AXI4-Lite slave bus (unchanged from Lab 7)
    input  logic                              axi_aclk,
    input  logic                              axi_aresetn,
    input  logic [C_AXI_ADDR_WIDTH-1 : 0]     axi_awaddr,
    input  logic [2 : 0]                      axi_awprot,
    input  logic                              axi_awvalid,
    output logic                              axi_awready,
    input  logic [C_AXI_DATA_WIDTH-1 : 0]     axi_wdata,
    input  logic [(C_AXI_DATA_WIDTH/8)-1 : 0] axi_wstrb,
    input  logic                              axi_wvalid,
    output logic                              axi_wready,
    output logic [1 : 0]                      axi_bresp,
    output logic                              axi_bvalid,
    input  logic                              axi_bready,
    input  logic [C_AXI_ADDR_WIDTH-1 : 0]     axi_araddr,
    input  logic [2 : 0]                      axi_arprot,
    input  logic                              axi_arvalid,
    output logic                              axi_arready,
    output logic [C_AXI_DATA_WIDTH-1 : 0]     axi_rdata,
    output logic [1 : 0]                      axi_rresp,
    output logic                              axi_rvalid,
    input  logic                              axi_rready
);

// -------------------------------------------------------------------------
// Clocks, resets
// -------------------------------------------------------------------------
logic clk_25MHz, clk_125MHz;
logic locked;
logic reset_ah;

// In hardware the AXI-side reset is sufficient; pixel-domain reset is held
// low so the VGA timer free-runs.  Match the previous hardware setting.
assign reset_ah = 1'b0;

// -------------------------------------------------------------------------
// VGA timing
// -------------------------------------------------------------------------
logic [9:0] drawX, drawY;
logic       hsync, vsync, vde;

// -------------------------------------------------------------------------
// Frame counter (vsync edge in AXI domain so software can poll it)
// -------------------------------------------------------------------------
logic        vsync_prev;
logic [31:0] frame_counter;
always_ff @(posedge axi_aclk) begin
    if (!axi_aresetn) begin
        frame_counter <= 32'h0;
        vsync_prev    <= 1'b1;
    end else begin
        vsync_prev <= vsync;
        if (vsync_prev && !vsync) begin
            frame_counter <= frame_counter + 32'h1;
        end
    end
end

// -------------------------------------------------------------------------
// Sprite-state wires (driven by AXI register file, consumed by pixel_mux)
// -------------------------------------------------------------------------
logic [9:0]  player_x_w;
logic        player_proj_active_w;
logic [9:0]  player_proj_x_w;
logic [9:0]  player_proj_y_w;
logic [9:0]  grid_x_w;
logic [9:0]  grid_y_w;
logic        grid_step_w;
logic [95:0] alien_alive_w;
logic [1:0]  game_state_w;
logic [2:0]  alien_proj_active_w;
logic [29:0] alien_proj_x_w;
logic [29:0] alien_proj_y_w;
logic        ufo_active_w;
logic [9:0]  ufo_x_w;

// -------------------------------------------------------------------------
// HUD text-overlay path: palette + VRAM live in the AXI module; pixel_mux
// drives vram_rd_addr from drawX/drawY and consumes vram_rd_data + the 8
// palette registers to render an 8x16 font HUD on the top of the screen.
// -------------------------------------------------------------------------
logic [C_AXI_DATA_WIDTH-1:0] palette_regs_w [8];
logic [C_AXI_DATA_WIDTH-1:0] vram_rd_data_w;
logic [10:0]                 vram_rd_addr_w;

// -------------------------------------------------------------------------
// AXI register file
// -------------------------------------------------------------------------
hdmi_text_controller_v1_0_AXI # (
    .C_S_AXI_DATA_WIDTH (C_AXI_DATA_WIDTH),
    .C_S_AXI_ADDR_WIDTH (C_AXI_ADDR_WIDTH)
) hdmi_text_controller_v1_0_AXI_inst (
    .frame_counter        (frame_counter),
    .current_draw_x       ({22'b0, drawX}),
    .current_draw_y       ({22'b0, drawY}),

    // VRAM display-side read port: driven by pixel_mux for HUD overlay.
    .vram_rd_addr         (vram_rd_addr_w),
    .vram_rd_data         (vram_rd_data_w),

    // Palette registers: consumed by pixel_mux for the HUD overlay.
    .palette_regs_out     (palette_regs_w),

    // Sprite-state outputs
    .player_x_out         (player_x_w),
    .player_proj_active_out(player_proj_active_w),
    .player_proj_x_out    (player_proj_x_w),
    .player_proj_y_out    (player_proj_y_w),
    .grid_x_out           (grid_x_w),
    .grid_y_out           (grid_y_w),
    .grid_step_out        (grid_step_w),
    .alien_alive_out      (alien_alive_w),
    .game_state_out       (game_state_w),
    .alien_proj_active_out(alien_proj_active_w),
    .alien_proj_x_out     (alien_proj_x_w),
    .alien_proj_y_out     (alien_proj_y_w),
    .ufo_active_out       (ufo_active_w),
    .ufo_x_out            (ufo_x_w),

    .pixel_clk            (clk_25MHz),

    .S_AXI_ACLK           (axi_aclk),
    .S_AXI_ARESETN        (axi_aresetn),
    .S_AXI_AWADDR         (axi_awaddr),
    .S_AXI_AWPROT         (axi_awprot),
    .S_AXI_AWVALID        (axi_awvalid),
    .S_AXI_AWREADY        (axi_awready),
    .S_AXI_WDATA          (axi_wdata),
    .S_AXI_WSTRB          (axi_wstrb),
    .S_AXI_WVALID         (axi_wvalid),
    .S_AXI_WREADY         (axi_wready),
    .S_AXI_BRESP          (axi_bresp),
    .S_AXI_BVALID         (axi_bvalid),
    .S_AXI_BREADY         (axi_bready),
    .S_AXI_ARADDR         (axi_araddr),
    .S_AXI_ARPROT         (axi_arprot),
    .S_AXI_ARVALID        (axi_arvalid),
    .S_AXI_ARREADY        (axi_arready),
    .S_AXI_RDATA          (axi_rdata),
    .S_AXI_RRESP          (axi_rresp),
    .S_AXI_RVALID         (axi_rvalid),
    .S_AXI_RREADY         (axi_rready)
);

// -------------------------------------------------------------------------
// Clocking, VGA timing, HDMI TX (carried over from previous version)
// -------------------------------------------------------------------------
clk_wiz_0 clk_wiz_inst (
    .clk_out1 (clk_25MHz),
    .clk_out2 (clk_125MHz),
    .reset    (reset_ah),
    .locked   (locked),
    .clk_in1  (axi_aclk)
);

vga_controller vga (
    .pixel_clk     (clk_25MHz),
    .reset         (reset_ah),
    .hs            (hsync),
    .vs            (vsync),
    .active_nblank (vde),
    .drawX         (drawX),
    .drawY         (drawY)
);

// -------------------------------------------------------------------------
// Sprite-mode pixel mux: composites player + aliens + projectile.
// Outputs 8-bit RGB; we feed the high nibble into the 4-bit HDMI encoder.
// -------------------------------------------------------------------------
logic [7:0] mux_red, mux_green, mux_blue;

pixel_mux u_pixel_mux (
    .pixel_clk         (clk_25MHz),
    .reset             (reset_ah),
    .drawX             (drawX),
    .drawY             (drawY),
    .active            (vde),

    .player_x          (player_x_w),
    .player_proj_active(player_proj_active_w),
    .player_proj_x     (player_proj_x_w),
    .player_proj_y     (player_proj_y_w),
    .grid_x            (grid_x_w),
    .grid_y            (grid_y_w),
    .grid_step         (grid_step_w),
    .alien_alive       (alien_alive_w),
    .game_state        (game_state_w),
    .alien_proj_active (alien_proj_active_w),
    .alien_proj_x      (alien_proj_x_w),
    .alien_proj_y      (alien_proj_y_w),
    .ufo_active        (ufo_active_w),
    .ufo_x             (ufo_x_w),

    .palette_regs      (palette_regs_w),
    .vram_rd_data      (vram_rd_data_w),
    .vram_rd_addr      (vram_rd_addr_w),

    .red               (mux_red),
    .green             (mux_green),
    .blue              (mux_blue)
);

logic [3:0] red4, green4, blue4;
assign red4   = mux_red  [7:4];
assign green4 = mux_green[7:4];
assign blue4  = mux_blue [7:4];

// -------------------------------------------------------------------------
// HDMI TMDS encoder (RD HDMI IP)
// -------------------------------------------------------------------------
hdmi_tx_0 vga_to_hdmi (
    .pix_clk        (clk_25MHz),
    .pix_clkx5      (clk_125MHz),
    .pix_clk_locked (locked),
    .rst            (reset_ah),

    .red            (red4),
    .green          (green4),
    .blue           (blue4),
    .hsync          (hsync),
    .vsync          (vsync),
    .vde            (vde),

    .aux0_din       (4'b0),
    .aux1_din       (4'b0),
    .aux2_din       (4'b0),
    .ade            (1'b0),

    .TMDS_CLK_P     (hdmi_clk_p),
    .TMDS_CLK_N     (hdmi_clk_n),
    .TMDS_DATA_P    (hdmi_tx_p),
    .TMDS_DATA_N    (hdmi_tx_n)
);

endmodule
