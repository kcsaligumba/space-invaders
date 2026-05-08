`timescale 1ns / 1ps

// Baseline pixel color multiplexer for Space Invaders.
// Logical playfield: 320x240, doubled to 640x480 on screen.
// Draws (priority high -> low): player projectile, player, alien grid, background.
// Extend with UFO / alien projectiles / shields in later weeks.
//
// Sprite ROMs are instantiated here for the player and one alien frame.
// ROM reads are registered (1 cycle); drawX/drawY are registered to match,
// so pixel output lags vga_controller by 1 clock -- HDMI TX IP tolerates this.

module pixel_mux (
    input  logic        pixel_clk,
    input  logic        reset,

    // From vga_controller
    input  logic [9:0]  drawX,
    input  logic [9:0]  drawY,
    input  logic        active,

    // Sprite engine state (from AXI register file)
    input  logic [9:0]  player_x,           // logical (0..319)
    input  logic        player_proj_active,
    input  logic [9:0]  player_proj_x,
    input  logic [9:0]  player_proj_y,
    input  logic [9:0]  grid_x,
    input  logic [9:0]  grid_y,
    input  logic        grid_step,
    input  logic [95:0] alien_alive,        // alien_alive[2:0] concatenated
    input  logic [1:0]  game_state,

    // Alien projectiles, 3 slots; same per-slot packing as the player proj.
    // Packed { slot2, slot1, slot0 } in [29:20] / [19:10] / [9:0].
    input  logic [2:0]  alien_proj_active,
    input  logic [29:0] alien_proj_x,
    input  logic [29:0] alien_proj_y,

    // HUD text overlay (Lab 7-style: 80x30 chars, 8x16 font, palette-coloured).
    input  logic [31:0] palette_regs [8],
    input  logic [31:0] vram_rd_data,
    output logic [10:0] vram_rd_addr,

    output logic [7:0]  red,
    output logic [7:0]  green,
    output logic [7:0]  blue
);

    // --- Logical coordinates (pixel doubling) ---
    logic [9:0] lx, ly;
    assign lx = drawX >> 1;
    assign ly = drawY >> 1;

    // --- Geometry constants ---
    localparam int PLAYER_W = 16;
    localparam int PLAYER_H = 8;
    localparam int PLAYER_Y = 220;              // logical y of player cannon

    localparam int ALIEN_W  = 16;
    localparam int ALIEN_H  = 8;
    localparam int ALIEN_STRIDE_X = 20;         // 16 + 4 gap
    localparam int ALIEN_STRIDE_Y = 14;
    localparam int GRID_COLS = 11;
    localparam int GRID_ROWS = 5;

    localparam int PROJ_W = 2;
    localparam int PROJ_H = 6;

    // --- Player sprite ROM ---
    logic [$clog2(PLAYER_H)-1:0] player_row;
    logic [PLAYER_W-1:0]         player_bits;
    sprite_rom #(.WIDTH(PLAYER_W), .HEIGHT(PLAYER_H), .INIT_FILE("player.mem"))
        u_player_rom (.clk(pixel_clk), .row(player_row), .row_bits(player_bits));

    // --- Alien sprite ROMs ---
    // Three types (A,B,C) x two animation frames (1,2) = 6 parallel ROMs.
    // All read with the same alien_row index; type selection happens after
    // the registered hit-test (see below) so the ROM output latency aligns.
    //   Type A = squid    (bottom rows 3-4, 10 pts)
    //   Type B = crab     (middle rows 1-2, 20 pts)
    //   Type C = octopus  (top row 0,      30 pts)
    logic [$clog2(ALIEN_H)-1:0] alien_row;
    logic [ALIEN_W-1:0]         alien_bits_a1, alien_bits_a2;
    logic [ALIEN_W-1:0]         alien_bits_b1, alien_bits_b2;
    logic [ALIEN_W-1:0]         alien_bits_c1, alien_bits_c2;
    logic [ALIEN_W-1:0]         alien_bits;

    sprite_rom #(.WIDTH(ALIEN_W), .HEIGHT(ALIEN_H), .INIT_FILE("alienA1.mem"))
        u_alien_rom_a1 (.clk(pixel_clk), .row(alien_row), .row_bits(alien_bits_a1));
    sprite_rom #(.WIDTH(ALIEN_W), .HEIGHT(ALIEN_H), .INIT_FILE("alienA2.mem"))
        u_alien_rom_a2 (.clk(pixel_clk), .row(alien_row), .row_bits(alien_bits_a2));
    sprite_rom #(.WIDTH(ALIEN_W), .HEIGHT(ALIEN_H), .INIT_FILE("alienB1.mem"))
        u_alien_rom_b1 (.clk(pixel_clk), .row(alien_row), .row_bits(alien_bits_b1));
    sprite_rom #(.WIDTH(ALIEN_W), .HEIGHT(ALIEN_H), .INIT_FILE("alienB2.mem"))
        u_alien_rom_b2 (.clk(pixel_clk), .row(alien_row), .row_bits(alien_bits_b2));
    sprite_rom #(.WIDTH(ALIEN_W), .HEIGHT(ALIEN_H), .INIT_FILE("alienC1.mem"))
        u_alien_rom_c1 (.clk(pixel_clk), .row(alien_row), .row_bits(alien_bits_c1));
    sprite_rom #(.WIDTH(ALIEN_W), .HEIGHT(ALIEN_H), .INIT_FILE("alienC2.mem"))
        u_alien_rom_c2 (.clk(pixel_clk), .row(alien_row), .row_bits(alien_bits_c2));

    // --- Player hit test (combinational address into ROM) ---
    logic        in_player;
    logic [3:0]  player_dx;
    always_comb begin
        in_player = 1'b0;
        player_dx = '0;
        player_row = '0;
        if (lx >= player_x && lx < (player_x + PLAYER_W) &&
            ly >= PLAYER_Y && ly < (PLAYER_Y + PLAYER_H)) begin
            in_player  = 1'b1;
            player_dx  = PLAYER_W - 1 - (lx - player_x);
            player_row = ly - PLAYER_Y;
        end
    end

    // --- Alien grid hit test ---
    logic        in_alien_cell;
    logic [3:0]  alien_col;
    logic [2:0]  alien_grid_row;
    logic [1:0]  alien_type;          // 0=A (bottom), 1=B (middle), 2=C (top)
    logic [3:0]  alien_dx;
    logic [6:0]  alien_idx;
    logic [9:0]  alien_rx, alien_ry, alien_cx, alien_cy;
    always_comb begin
        in_alien_cell  = 1'b0;
        alien_col      = '0;
        alien_grid_row = '0;
        alien_dx       = '0;
        alien_row      = '0;
        alien_idx      = '0;
        alien_rx       = '0;
        alien_ry       = '0;
        alien_cx       = '0;
        alien_cy       = '0;
        if (lx >= grid_x && ly >= grid_y) begin
            alien_rx       = lx - grid_x;
            alien_ry       = ly - grid_y;
            alien_col      = alien_rx / ALIEN_STRIDE_X;
            alien_grid_row = alien_ry / ALIEN_STRIDE_Y;
            if (alien_col < GRID_COLS && alien_grid_row < GRID_ROWS) begin
                alien_cx = alien_rx - alien_col * ALIEN_STRIDE_X;
                alien_cy = alien_ry - alien_grid_row * ALIEN_STRIDE_Y;
                if (alien_cx < ALIEN_W && alien_cy < ALIEN_H) begin
                    in_alien_cell = 1'b1;
                    alien_dx  = ALIEN_W - 1 - alien_cx;
                    alien_row = alien_cy[$clog2(ALIEN_H)-1:0];
                    alien_idx = alien_grid_row * GRID_COLS + alien_col;
                end
            end
        end
    end

    // --- Alien type decode (which sprite ROM pair to use for this row) ---
    // Arcade-classic mapping:
    //   row 0       -> C (octopus, 30 pts)
    //   rows 1-2    -> B (crab,    20 pts)
    //   rows 3-4    -> A (squid,   10 pts)
    always_comb begin
        unique case (alien_grid_row)
            3'd0:                 alien_type = 2'd2;   // C
            3'd1, 3'd2:           alien_type = 2'd1;   // B
            3'd3, 3'd4:           alien_type = 2'd0;   // A
            default:              alien_type = 2'd0;
        endcase
    end

    // --- Player projectile hit test (no ROM, solid color) ---
    logic in_player_proj;
    always_comb begin
        in_player_proj = player_proj_active &&
                         (lx >= player_proj_x) && (lx < player_proj_x + PROJ_W) &&
                         (ly >= player_proj_y) && (ly < player_proj_y + PROJ_H);
    end

    // --- Alien projectile hit tests (3 slots, no ROM, solid color) ---
    // Unpack the three per-slot {x, y} pairs from the packed AXI inputs.
    logic [9:0] alien_proj_x_u [3];
    logic [9:0] alien_proj_y_u [3];
    assign alien_proj_x_u[0] = alien_proj_x[9:0];
    assign alien_proj_x_u[1] = alien_proj_x[19:10];
    assign alien_proj_x_u[2] = alien_proj_x[29:20];
    assign alien_proj_y_u[0] = alien_proj_y[9:0];
    assign alien_proj_y_u[1] = alien_proj_y[19:10];
    assign alien_proj_y_u[2] = alien_proj_y[29:20];

    logic [2:0] in_alien_proj;
    always_comb begin
        for (int i = 0; i < 3; i++) begin
            in_alien_proj[i] = alien_proj_active[i] &&
                (lx >= alien_proj_x_u[i]) && (lx < alien_proj_x_u[i] + PROJ_W) &&
                (ly >= alien_proj_y_u[i]) && (ly < alien_proj_y_u[i] + PROJ_H);
        end
    end

    // -------------------------------------------------------------------
    // HUD text overlay (Lab 7 80x30 / 8x16 font / palette pipeline).
    // Only the top HUD_HEIGHT lines actually display text; outside that
    // the priority mux ignores the text RGB.
    // -------------------------------------------------------------------
    localparam int HUD_HEIGHT = 32;     // 2 char rows = 32 screen px

    logic [5:0]  char_row;
    logic [6:0]  char_col;
    logic [3:0]  pixel_row;
    logic [2:0]  pixel_col;
    logic        half_sel;

    assign char_col  = drawX[9:3];
    assign char_row  = drawY[9:4];
    assign pixel_col = drawX[2:0];
    assign pixel_row = drawY[3:0];
    assign half_sel  = char_col[0];

    // word_addr = char_row * 40 + char_col / 2  (1200 32-bit words = 80*30/2)
    assign vram_rd_addr = (char_row * 7'd40) + char_col[6:1];

    // 1-cycle pipeline registers to align with BRAM read latency.
    logic       half_sel_reg;
    logic [2:0] pixel_col_reg;
    logic [3:0] pixel_row_reg;
    always_ff @(posedge pixel_clk) begin
        half_sel_reg  <= half_sel;
        pixel_col_reg <= pixel_col;
        pixel_row_reg <= pixel_row;
    end

    // Decode the 16-bit half-word that vram_rd_data delivers (registered).
    //   [15]=inverse, [14:8]=char_code, [7:4]=fg_idx, [3:0]=bg_idx
    logic [6:0] char_code;
    logic       inverse;
    logic [3:0] fgd_idx;
    logic [3:0] bkg_idx;
    always_comb begin
        if (half_sel_reg == 1'b0) begin
            char_code = vram_rd_data[14:8];
            inverse   = vram_rd_data[15];
            fgd_idx   = vram_rd_data[7:4];
            bkg_idx   = vram_rd_data[3:0];
        end else begin
            char_code = vram_rd_data[30:24];
            inverse   = vram_rd_data[31];
            fgd_idx   = vram_rd_data[23:20];
            bkg_idx   = vram_rd_data[19:16];
        end
    end

    // Combinational 8x16 font ROM.
    logic [10:0] font_addr;
    logic [7:0]  font_data;
    logic        font_pixel;
    assign font_addr  = {char_code, pixel_row_reg};
    font_rom u_font (.addr(font_addr), .data(font_data));
    assign font_pixel = font_data[7 - pixel_col_reg];

    // Palette decode: 2 colours per 32-bit register, 12-bit RGB each.
    //   even idx -> [11:8]=R, [7:4]=G, [3:0]=B
    //   odd  idx -> [27:24]=R, [23:20]=G, [19:16]=B
    logic [3:0] fg_r, fg_g, fg_b;
    logic [3:0] bg_r, bg_g, bg_b;
    always_comb begin
        if (fgd_idx[0] == 1'b0) begin
            fg_r = palette_regs[fgd_idx[3:1]][11:8];
            fg_g = palette_regs[fgd_idx[3:1]][7:4];
            fg_b = palette_regs[fgd_idx[3:1]][3:0];
        end else begin
            fg_r = palette_regs[fgd_idx[3:1]][27:24];
            fg_g = palette_regs[fgd_idx[3:1]][23:20];
            fg_b = palette_regs[fgd_idx[3:1]][19:16];
        end
    end
    always_comb begin
        if (bkg_idx[0] == 1'b0) begin
            bg_r = palette_regs[bkg_idx[3:1]][11:8];
            bg_g = palette_regs[bkg_idx[3:1]][7:4];
            bg_b = palette_regs[bkg_idx[3:1]][3:0];
        end else begin
            bg_r = palette_regs[bkg_idx[3:1]][27:24];
            bg_g = palette_regs[bkg_idx[3:1]][23:20];
            bg_b = palette_regs[bkg_idx[3:1]][19:16];
        end
    end

    // Pre-pick foreground vs background per pixel (registered drawY check).
    logic       in_hud;
    assign in_hud = (drawY < HUD_HEIGHT);

    // --- Register hit-test results to align with 1-cycle ROM latency ---
    logic       in_player_r, in_alien_cell_r, in_player_proj_r, active_r;
    logic [3:0] player_dx_r, alien_dx_r;
    logic [6:0] alien_idx_r;
    logic [1:0] alien_type_r;
    logic [1:0] game_state_r;
    logic [2:0] in_alien_proj_r;
    logic       in_hud_r;

    always_ff @(posedge pixel_clk) begin
        in_player_r      <= in_player;
        in_alien_cell_r  <= in_alien_cell;
        in_player_proj_r <= in_player_proj;
        in_alien_proj_r  <= in_alien_proj;
        in_hud_r         <= in_hud;
        player_dx_r      <= player_dx;
        alien_dx_r       <= alien_dx;
        alien_idx_r      <= alien_idx;
        alien_type_r     <= alien_type;
        active_r         <= active;
        game_state_r     <= game_state;
    end

    // --- Select alien sprite bits based on registered type + animation phase ---
    // grid_step is per-frame (stable for ~16 ms), so combinational select is fine.
    always_comb begin
        unique case (alien_type_r)
            2'd0:    alien_bits = grid_step ? alien_bits_a2 : alien_bits_a1;
            2'd1:    alien_bits = grid_step ? alien_bits_b2 : alien_bits_b1;
            2'd2:    alien_bits = grid_step ? alien_bits_c2 : alien_bits_c1;
            default: alien_bits = '0;
        endcase
    end

    // --- Pick pixel bit from selected ROM row ---
    logic player_pix;
    logic alien_pix;
    logic alien_alive_bit;

    assign player_pix      = player_bits[player_dx_r];
    assign alien_pix       = alien_bits[alien_dx_r];
    assign alien_alive_bit = alien_alive[alien_idx_r];

    // --- Priority mux ---
    localparam bit [1:0] STATE_START    = 2'd0;
    localparam bit [1:0] STATE_PLAYING  = 2'd1;
    localparam bit [1:0] STATE_GAMEOVER = 2'd2;

    always_comb begin
        red   = 8'h00;
        green = 8'h00;
        blue  = 8'h00;
        if (active_r) begin
            // Background tint on GAME OVER.
            if (game_state_r == STATE_GAMEOVER) begin
                red   = 8'h20;
            end

            // HUD text overlay (top HUD_HEIGHT lines).  Only paint the
            // foreground glyph pixels and leave HUD-background pixels as
            // the default screen colour -- so during STATE_GAMEOVER the
            // red tint (set above) shows through the HUD area too,
            // matching the rest of the screen.  Replicate the 4-bit
            // palette colour into the upper 4 bits of each 8-bit channel
            // so hdmi_tx_0's high-nibble extraction keeps it intact.
            if (in_hud_r && (font_pixel ^ inverse)) begin
                red   = {fg_r, fg_r};
                green = {fg_g, fg_g};
                blue  = {fg_b, fg_b};
            end

            // Sprite layers (override HUD where they overlap).
            // Priority: player_proj > alien_proj > player > aliens.
            if (in_player_proj_r) begin
                red = 8'hFF; green = 8'hFF; blue = 8'hFF;
            end else if (|in_alien_proj_r) begin
                red = 8'hFF; green = 8'hFF; blue = 8'hFF;
            end else if (in_player_r && player_pix && game_state_r == STATE_PLAYING) begin
                red = 8'h00; green = 8'hFF; blue = 8'h40;
            end else if (in_alien_cell_r && alien_alive_bit && alien_pix) begin
                red = 8'hFF; green = 8'hFF; blue = 8'hFF;
            end
        end
    end

endmodule
