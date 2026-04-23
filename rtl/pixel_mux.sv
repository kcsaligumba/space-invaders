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

    // --- Alien sprite ROMs (one per animation frame) ---
    logic [$clog2(ALIEN_H)-1:0] alien_row;
    logic [ALIEN_W-1:0]         alien_bits_a, alien_bits_b, alien_bits;
    sprite_rom #(.WIDTH(ALIEN_W), .HEIGHT(ALIEN_H), .INIT_FILE("alienA1.mem"))
        u_alien_rom_a (.clk(pixel_clk), .row(alien_row), .row_bits(alien_bits_a));
    sprite_rom #(.WIDTH(ALIEN_W), .HEIGHT(ALIEN_H), .INIT_FILE("alienA2.mem"))
        u_alien_rom_b (.clk(pixel_clk), .row(alien_row), .row_bits(alien_bits_b));
    assign alien_bits = grid_step ? alien_bits_b : alien_bits_a;

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
    logic [3:0]  alien_dx;
    logic [6:0]  alien_idx;
    always_comb begin
        in_alien_cell  = 1'b0;
        alien_col      = '0;
        alien_grid_row = '0;
        alien_dx       = '0;
        alien_row      = '0;
        alien_idx      = '0;
        if (lx >= grid_x && ly >= grid_y) begin
            logic [9:0] rx, ry;
            rx = lx - grid_x;
            ry = ly - grid_y;
            alien_col      = rx / ALIEN_STRIDE_X;
            alien_grid_row = ry / ALIEN_STRIDE_Y;
            if (alien_col < GRID_COLS && alien_grid_row < GRID_ROWS) begin
                logic [9:0] cx, cy;
                cx = rx - alien_col * ALIEN_STRIDE_X;
                cy = ry - alien_grid_row * ALIEN_STRIDE_Y;
                if (cx < ALIEN_W && cy < ALIEN_H) begin
                    in_alien_cell = 1'b1;
                    alien_dx  = ALIEN_W - 1 - cx;
                    alien_row = cy;
                    alien_idx = alien_grid_row * GRID_COLS + alien_col;
                end
            end
        end
    end

    // --- Player projectile hit test (no ROM, solid color) ---
    logic in_player_proj;
    always_comb begin
        in_player_proj = player_proj_active &&
                         (lx >= player_proj_x) && (lx < player_proj_x + PROJ_W) &&
                         (ly >= player_proj_y) && (ly < player_proj_y + PROJ_H);
    end

    // --- Register hit-test results to align with 1-cycle ROM latency ---
    logic in_player_r, in_alien_cell_r, in_player_proj_r, active_r;
    logic [3:0] player_dx_r, alien_dx_r;
    logic [6:0] alien_idx_r;
    logic [1:0] game_state_r;

    always_ff @(posedge pixel_clk) begin
        in_player_r      <= in_player;
        in_alien_cell_r  <= in_alien_cell;
        in_player_proj_r <= in_player_proj;
        player_dx_r      <= player_dx;
        alien_dx_r       <= alien_dx;
        alien_idx_r      <= alien_idx;
        active_r         <= active;
        game_state_r     <= game_state;
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
            // background stays black; tint slightly on GAME OVER
            if (game_state_r == STATE_GAMEOVER) begin
                red   = 8'h20;
            end
            if (in_player_proj_r) begin
                red = 8'hFF; green = 8'hFF; blue = 8'hFF;
            end else if (in_player_r && player_pix && game_state_r != STATE_GAMEOVER) begin
                red = 8'h00; green = 8'hFF; blue = 8'h40;
            end else if (in_alien_cell_r && alien_alive_bit && alien_pix) begin
                red = 8'hFF; green = 8'hFF; blue = 8'hFF;
            end
        end
    end

endmodule
