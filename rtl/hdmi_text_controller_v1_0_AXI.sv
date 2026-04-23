`timescale 1 ns / 1 ps

// AXI4-Lite slave for Space Invaders sprite engine.
// Register map (word offsets, byte addresses = offset*4):
//   reg[ 0] 0x00  player_x          [9:0]
//   reg[ 1] 0x04  player_proj       {active, x[9:0], y[9:0]}
//   reg[ 2] 0x08  grid_x            [9:0]
//   reg[ 3] 0x0C  grid_y            [9:0]
//   reg[ 4] 0x10  grid_step         [0]    animation phase
//   reg[ 5] 0x14  alien_alive[0]    32 bits
//   reg[ 6] 0x18  alien_alive[1]    32 bits
//   reg[ 7] 0x1C  alien_alive[2]    32 bits (55 aliens total, low bits)
//   reg[ 8] 0x20  alien_proj[0]     {active, x[9:0], y[9:0]}
//   reg[ 9] 0x24  alien_proj[1]
//   reg[10] 0x28  alien_proj[2]
//   reg[11] 0x2C  alien_proj[3]
//   reg[12] 0x30  alien_proj[4]
//   reg[13] 0x34  alien_proj[5]
//   reg[14] 0x38  alien_proj[6]
//   reg[15] 0x3C  alien_proj[7]
//   reg[16] 0x40  shield_damage[0]  [15:0]
//   reg[17] 0x44  shield_damage[1]  [15:0]
//   reg[18] 0x48  shield_damage[2]  [15:0]
//   reg[19] 0x4C  shield_damage[3]  [15:0]
//   reg[20] 0x50  ufo               {active, x[9:0]}
//   reg[21] 0x54  game_state        [1:0]
//   reg[22] 0x58  collision_flags   [hw-written, software read-clear]
//   reg[23] 0x5C  frame_counter     [hw-written, read-only]

module hdmi_text_controller_v1_0_AXI #
(
    parameter integer C_S_AXI_DATA_WIDTH = 32,
    parameter integer C_S_AXI_ADDR_WIDTH = 8
)
(
    // Sprite engine register exports (SW-written regs visible to HW)
    output logic [31:0] sprite_regs[24],

    // Hardware-driven status inputs (HW overrides these register slots)
    input  logic [31:0] frame_counter_in,
    input  logic [31:0] collision_flags_in,

    // AXI4-Lite
    input  logic S_AXI_ACLK,
    input  logic S_AXI_ARESETN,
    input  logic [C_S_AXI_ADDR_WIDTH-1 : 0] S_AXI_AWADDR,
    input  logic [2 : 0] S_AXI_AWPROT,
    input  logic S_AXI_AWVALID,
    output logic S_AXI_AWREADY,
    input  logic [C_S_AXI_DATA_WIDTH-1 : 0] S_AXI_WDATA,
    input  logic [(C_S_AXI_DATA_WIDTH/8)-1 : 0] S_AXI_WSTRB,
    input  logic S_AXI_WVALID,
    output logic S_AXI_WREADY,
    output logic [1 : 0] S_AXI_BRESP,
    output logic S_AXI_BVALID,
    input  logic S_AXI_BREADY,
    input  logic [C_S_AXI_ADDR_WIDTH-1 : 0] S_AXI_ARADDR,
    input  logic [2 : 0] S_AXI_ARPROT,
    input  logic S_AXI_ARVALID,
    output logic S_AXI_ARREADY,
    output logic [C_S_AXI_DATA_WIDTH-1 : 0] S_AXI_RDATA,
    output logic [1 : 0] S_AXI_RRESP,
    output logic S_AXI_RVALID,
    input  logic S_AXI_RREADY
);

localparam integer ADDR_LSB = 2;              // 32-bit words
localparam integer NUM_REGS = 24;
localparam integer OPT_MEM_ADDR_BITS = $clog2(NUM_REGS) - 1;  // 4 -> indexes 0..23 using 5 bits

// AXI4LITE internals
logic [C_S_AXI_ADDR_WIDTH-1 : 0] axi_awaddr;
logic axi_awready;
logic axi_wready;
logic [1 : 0] axi_bresp;
logic axi_bvalid;
logic [C_S_AXI_ADDR_WIDTH-1 : 0] axi_araddr;
logic axi_arready;
logic [C_S_AXI_DATA_WIDTH-1 : 0] axi_rdata;
logic [1 : 0] axi_rresp;
logic axi_rvalid;

logic [C_S_AXI_DATA_WIDTH-1:0] slv_regs [NUM_REGS];
logic slv_reg_rden;
logic slv_reg_wren;
logic [C_S_AXI_DATA_WIDTH-1:0] reg_data_out;
integer byte_index;
logic aw_en;

assign S_AXI_AWREADY = axi_awready;
assign S_AXI_WREADY  = axi_wready;
assign S_AXI_BRESP   = axi_bresp;
assign S_AXI_BVALID  = axi_bvalid;
assign S_AXI_ARREADY = axi_arready;
assign S_AXI_RDATA   = axi_rdata;
assign S_AXI_RRESP   = axi_rresp;
assign S_AXI_RVALID  = axi_rvalid;

// Expose all SW-written registers to sprite engine
genvar gi;
generate
    for (gi = 0; gi < NUM_REGS; gi = gi + 1) begin : g_export
        assign sprite_regs[gi] = slv_regs[gi];
    end
endgenerate

// ---- Write address channel ----
always_ff @( posedge S_AXI_ACLK ) begin
    if ( S_AXI_ARESETN == 1'b0 ) begin
        axi_awready <= 1'b0;
        aw_en <= 1'b1;
    end else begin
        if (~axi_awready && S_AXI_AWVALID && S_AXI_WVALID && aw_en) begin
            axi_awready <= 1'b1;
            aw_en <= 1'b0;
        end else if (S_AXI_BREADY && axi_bvalid) begin
            aw_en <= 1'b1;
            axi_awready <= 1'b0;
        end else begin
            axi_awready <= 1'b0;
        end
    end
end

always_ff @( posedge S_AXI_ACLK ) begin
    if ( S_AXI_ARESETN == 1'b0 )
        axi_awaddr <= 0;
    else if (~axi_awready && S_AXI_AWVALID && S_AXI_WVALID && aw_en)
        axi_awaddr <= S_AXI_AWADDR;
end

// ---- Write data channel ----
always_ff @( posedge S_AXI_ACLK ) begin
    if ( S_AXI_ARESETN == 1'b0 )
        axi_wready <= 1'b0;
    else if (~axi_wready && S_AXI_WVALID && S_AXI_AWVALID && aw_en)
        axi_wready <= 1'b1;
    else
        axi_wready <= 1'b0;
end

assign slv_reg_wren = axi_wready && S_AXI_WVALID && axi_awready && S_AXI_AWVALID;

// ---- Register write + HW updates ----
// SW write lands into slv_regs[index]. Hardware updates frame_counter/collision_flags
// unconditionally (SW writes to those slots are treated as read-clear / ignored).
logic [OPT_MEM_ADDR_BITS:0] wr_idx;
assign wr_idx = axi_awaddr[ADDR_LSB+OPT_MEM_ADDR_BITS:ADDR_LSB];

always_ff @( posedge S_AXI_ACLK ) begin
    if ( S_AXI_ARESETN == 1'b0 ) begin
        for (integer i = 0; i < NUM_REGS; i++) slv_regs[i] <= '0;
    end else begin
        // Hardware-driven regs refresh every cycle
        slv_regs[22] <= collision_flags_in;
        slv_regs[23] <= frame_counter_in;

        if (slv_reg_wren && wr_idx < NUM_REGS && wr_idx != 5'd23) begin
            // SW write to reg 22 acts as write-to-clear via collision_flags_in path in engine;
            // here we just let the HW assign above take precedence next cycle.
            for ( byte_index = 0; byte_index <= (C_S_AXI_DATA_WIDTH/8)-1; byte_index = byte_index+1 )
                if ( S_AXI_WSTRB[byte_index] == 1 )
                    slv_regs[wr_idx][(byte_index*8) +: 8] <= S_AXI_WDATA[(byte_index*8) +: 8];
        end
    end
end

// ---- Write response ----
always_ff @( posedge S_AXI_ACLK ) begin
    if ( S_AXI_ARESETN == 1'b0 ) begin
        axi_bvalid <= 0;
        axi_bresp  <= 2'b0;
    end else begin
        if (axi_awready && S_AXI_AWVALID && ~axi_bvalid && axi_wready && S_AXI_WVALID) begin
            axi_bvalid <= 1'b1;
            axi_bresp  <= 2'b0;
        end else if (S_AXI_BREADY && axi_bvalid) begin
            axi_bvalid <= 1'b0;
        end
    end
end

// ---- Read address channel ----
always_ff @( posedge S_AXI_ACLK ) begin
    if ( S_AXI_ARESETN == 1'b0 ) begin
        axi_arready <= 1'b0;
        axi_araddr  <= '0;
    end else if (~axi_arready && S_AXI_ARVALID) begin
        axi_arready <= 1'b1;
        axi_araddr  <= S_AXI_ARADDR;
    end else begin
        axi_arready <= 1'b0;
    end
end

always_ff @( posedge S_AXI_ACLK ) begin
    if ( S_AXI_ARESETN == 1'b0 ) begin
        axi_rvalid <= 0;
        axi_rresp  <= 0;
    end else begin
        if (axi_arready && S_AXI_ARVALID && ~axi_rvalid) begin
            axi_rvalid <= 1'b1;
            axi_rresp  <= 2'b0;
        end else if (axi_rvalid && S_AXI_RREADY) begin
            axi_rvalid <= 1'b0;
        end
    end
end

assign slv_reg_rden = axi_arready & S_AXI_ARVALID & ~axi_rvalid;

logic [OPT_MEM_ADDR_BITS:0] rd_idx;
assign rd_idx = axi_araddr[ADDR_LSB+OPT_MEM_ADDR_BITS:ADDR_LSB];

always_comb begin
    if (rd_idx < NUM_REGS)
        reg_data_out = slv_regs[rd_idx];
    else
        reg_data_out = 32'h0;
end

always_ff @( posedge S_AXI_ACLK ) begin
    if ( S_AXI_ARESETN == 1'b0 )
        axi_rdata <= 0;
    else if (slv_reg_rden)
        axi_rdata <= reg_data_out;
end

endmodule