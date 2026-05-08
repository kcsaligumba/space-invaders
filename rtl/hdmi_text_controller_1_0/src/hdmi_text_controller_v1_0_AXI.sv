`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: ECE-Illinois
// Engineer: Zuofu Cheng
// 
// Create Date: 06/08/2023 12:21:05 PM
// Design Name: 
// Module Name: hdmi_text_controller_v1_0_AXI
// Project Name: ECE 385 - hdmi_text_controller
// Target Devices: 
// Tool Versions: 
// Description: 
// This is a modified version of the Vivado template for an AXI4-Lite peripheral,
// rewritten into SystemVerilog for use with ECE 385.
// 
// Dependencies: 
// 
// Revision:
// Revision 0.02 - File modified to be more consistent with generated template
// Revision 11/18 - Made comments less confusing
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


`timescale 1 ns / 1 ps

module hdmi_text_controller_v1_0_AXI #
(

    parameter integer C_S_AXI_DATA_WIDTH = 32,
    parameter integer C_S_AXI_ADDR_WIDTH = 16
)
(
    // Users to add ports here
    
    input logic [31:0] frame_counter,
    input logic [31:0] current_draw_x,
    input logic [31:0] current_draw_y,
    
    // Display-side VRAM read port (directly connected to BRAM port B)
    input  logic [10:0]                   vram_rd_addr, // Expanded to 11 bits for up to 1200 words
    output logic [C_S_AXI_DATA_WIDTH-1:0] vram_rd_data,
    
    // Palette registers output (replaces single ctrl_reg_out from Week 1)
    output logic [C_S_AXI_DATA_WIDTH-1:0] palette_regs_out[8],

    // Sprite-state outputs (Step 4+, sprite-mode rendering).
    // Live in a new region selected by axi_addr[14] == 1.  See struct
    // SPRITE_STATE in sprite_engine.h for the C-side memory map.
    output logic [9:0]  player_x_out,
    output logic        player_proj_active_out,
    output logic [9:0]  player_proj_x_out,
    output logic [9:0]  player_proj_y_out,
    output logic [9:0]  grid_x_out,
    output logic [9:0]  grid_y_out,
    output logic        grid_step_out,
    output logic [95:0] alien_alive_out,
    output logic [1:0]  game_state_out,
    // Alien projectiles, 3 slots (Step 11 uses slot 0; Step 14 uses 1-2).
    // Packed as { slot2, slot1, slot0 } in [29:20] / [19:10] / [9:0].
    output logic [2:0]  alien_proj_active_out,
    output logic [29:0] alien_proj_x_out,
    output logic [29:0] alien_proj_y_out,
    // UFO bonus alien (Step 15).  Same per-slot packing as PLAYER_PROJ but
    // y is fixed in pixel_mux, so we only pass active and x.
    output logic        ufo_active_out,
    output logic [9:0]  ufo_x_out,

    input logic pixel_clk,

    // User ports ends

    // Global Clock Signal
    input logic  S_AXI_ACLK,
    // Global Reset Signal. This Signal is Active LOW
    input logic  S_AXI_ARESETN,
    // Write address (issued by master, acceped by Slave)
    input logic [C_S_AXI_ADDR_WIDTH-1 : 0] S_AXI_AWADDR,
    // Write channel Protection type. This signal indicates the
        // privilege and security level of the transaction, and whether
        // the transaction is a data access or an instruction access.
    input logic [2 : 0] S_AXI_AWPROT,
    // Write address valid. This signal indicates that the master signaling
        // valid write address and control information.
    input logic  S_AXI_AWVALID,
    // Write address ready. This signal indicates that the slave is ready
        // to accept an address and associated control signals.
    output logic  S_AXI_AWREADY,
    // Write data (issued by master, acceped by Slave) 
    input logic [C_S_AXI_DATA_WIDTH-1 : 0] S_AXI_WDATA,
    // Write strobes. This signal indicates which byte lanes hold
        // valid data. There is one write strobe bit for each eight
        // bits of the write data bus.    
    input logic [(C_S_AXI_DATA_WIDTH/8)-1 : 0] S_AXI_WSTRB,
    // Write valid. This signal indicates that valid write
        // data and strobes are available.
    input logic  S_AXI_WVALID,
    // Write ready. This signal indicates that the slave
        // can accept the write data.
    output logic  S_AXI_WREADY,
    // Write response. This signal indicates the status
        // of the write transaction.
    output logic [1 : 0] S_AXI_BRESP,
    // Write response valid. This signal indicates that the channel
        // is signaling a valid write response.
    output logic  S_AXI_BVALID,
    // Response ready. This signal indicates that the master
        // can accept a write response.
    input logic  S_AXI_BREADY,
    // Read address (issued by master, acceped by Slave)
    input logic [C_S_AXI_ADDR_WIDTH-1 : 0] S_AXI_ARADDR,
    // Protection type. This signal indicates the privilege
        // and security level of the transaction, and whether the
        // transaction is a data access or an instruction access.
    input logic [2 : 0] S_AXI_ARPROT,
    // Read address valid. This signal indicates that the channel
        // is signaling valid read address and control information.
    input logic  S_AXI_ARVALID,
    // Read address ready. This signal indicates that the slave is
        // ready to accept an address and associated control signals.
    output logic  S_AXI_ARREADY,
    // Read data (issued by slave)
    output logic [C_S_AXI_DATA_WIDTH-1 : 0] S_AXI_RDATA,
    // Read response. This signal indicates the status of the
        // read transfer.
    output logic [1 : 0] S_AXI_RRESP,
    // Read valid. This signal indicates that the channel is
        // signaling the required read data.
    output logic  S_AXI_RVALID,
    // Read ready. This signal indicates that the master can
        // accept the read data and response information.
    input logic  S_AXI_RREADY
);

// AXI4LITE signals
logic  [C_S_AXI_ADDR_WIDTH-1 : 0] axi_awaddr;
logic  axi_awready;
logic  axi_wready;
logic  [1 : 0] 	axi_bresp;
logic  axi_bvalid;
logic  [C_S_AXI_ADDR_WIDTH-1 : 0] axi_araddr;
logic  axi_arready;
logic  [C_S_AXI_DATA_WIDTH-1 : 0] axi_rdata;
logic  [1 : 0] 	axi_rresp;
logic  	axi_rvalid;

// Example-specific design signals
// local parameter for addressing 32 bit / 64 bit C_S_AXI_DATA_WIDTH
// ADDR_LSB is used for addressing 32/64 bit registers/memories
// ADDR_LSB = 2 for 32 bits (n downto 2)
// ADDR_LSB = 3 for 64 bits (n downto 3)

// Byte address bits 1:0 are ignored (32-bit aligned)
localparam integer ADDR_LSB = 2;

// localparam integer OPT_MEM_ADDR_BITS = 9; // Expanded from 1 -> 9 because 601 registers = 10 index bits
//----------------------------------------------
//-- Signals for user logic register space example
//------------------------------------------------
//-- Number of Slave Registers 4
//logic [C_S_AXI_DATA_WIDTH-1:0]	slv_reg0;
//logic [C_S_AXI_DATA_WIDTH-1:0]	slv_reg1;
//logic [C_S_AXI_DATA_WIDTH-1:0]	slv_reg2;
//logic [C_S_AXI_DATA_WIDTH-1:0]	slv_reg3;
//
//Note: the provided Verilog template had the registered declared as above, but in order to give 
//students a hint we have replaced the 4 individual registers with an unpacked array of packed logic. 
//Note that you as the student will still need to extend this to the full register set needed for the lab.
// (* ram_style = "block" *) logic [C_S_AXI_DATA_WIDTH-1:0] slv_regs[604]; // Extended from 4 -> 604
logic	 slv_reg_rden;
logic	 slv_reg_wren;
// logic [C_S_AXI_DATA_WIDTH-1:0] reg_data_out;
integer	 byte_index;
logic	 aw_en;

// I/O Connections assignments

assign S_AXI_AWREADY = axi_awready;
assign S_AXI_WREADY	= axi_wready;
assign S_AXI_BRESP	= axi_bresp;
assign S_AXI_BVALID	= axi_bvalid;
assign S_AXI_ARREADY = axi_arready;
assign S_AXI_RDATA	= axi_rdata;
assign S_AXI_RRESP	= axi_rresp;
assign S_AXI_RVALID	= axi_rvalid;

// BRAM for VRAM (1200 x 32-bit words)
// Dual-port: Port A for AXI access, Port B for display logic

// Infer dual-port BRAM using ram_style attribute
(* ram_style = "block" *) logic [C_S_AXI_DATA_WIDTH-1:0] vram[1200];

// Port A signals (AXI side, clocked by S_AXI_ACLK)
logic [10:0] bram_addr_a; // AXI word address into VRAM
logic [C_S_AXI_DATA_WIDTH-1:0] bram_rd_data_a; // data read from port A
logic bram_we_a; // write enable for port A
logic bram_en_a; // chip enable for port A (prevents spurious writes)

// Palette registers (8 x 32-bit, kept in FPGA registers for fast access)
// Each register holds 2 packed 12-bit colors:
// [27:24]=Cn+1_R [23:20]=Cn+1_G [19:16]=Cn+1_B  [11:8]=Cn_R [7:4]=Cn_G [3:0]=Cn_B
logic [C_S_AXI_DATA_WIDTH-1:0] palette_regs[8];

// Output palette registers directly to display logic
assign palette_regs_out = palette_regs;

// Sprite-state register file (Step 4+).  32 word slots; only 0..17 currently
// meaningful.  Selected by axi_*addr[14]==1; word index = axi_*addr[6:2].
logic [C_S_AXI_DATA_WIDTH-1:0] sprite_regs[32];

// Sprite-state outputs are direct slices of the corresponding registers.
// See sprite_engine.h for the canonical bit-packing.
assign player_x_out           = sprite_regs[5'h00][9:0];
assign player_proj_active_out = sprite_regs[5'h01][31];
assign player_proj_x_out      = sprite_regs[5'h01][9:0];
assign player_proj_y_out      = sprite_regs[5'h01][25:16];
assign grid_x_out             = sprite_regs[5'h02][9:0];
assign grid_y_out             = sprite_regs[5'h02][25:16];
assign grid_step_out          = sprite_regs[5'h02][28];
assign alien_alive_out[31:0]  = sprite_regs[5'h04];
assign alien_alive_out[63:32] = sprite_regs[5'h05];
assign alien_alive_out[95:64] = sprite_regs[5'h06];
assign game_state_out         = sprite_regs[5'h11][1:0];

// Alien projectiles: ALIEN_PROJ[0..2] live at sprite_regs[8..10].
// Same per-register packing as PLAYER_PROJ: [31]=active, [25:16]=y, [9:0]=x.
assign alien_proj_active_out[0] = sprite_regs[5'h08][31];
assign alien_proj_active_out[1] = sprite_regs[5'h09][31];
assign alien_proj_active_out[2] = sprite_regs[5'h0A][31];
assign alien_proj_x_out[9:0]    = sprite_regs[5'h08][9:0];
assign alien_proj_x_out[19:10]  = sprite_regs[5'h09][9:0];
assign alien_proj_x_out[29:20]  = sprite_regs[5'h0A][9:0];
assign alien_proj_y_out[9:0]    = sprite_regs[5'h08][25:16];
assign alien_proj_y_out[19:10]  = sprite_regs[5'h09][25:16];
assign alien_proj_y_out[29:20]  = sprite_regs[5'h0A][25:16];

// UFO bonus alien lives at sprite_regs[5'h10] (byte offset 0x40 in the
// sprite-state region).  Packing: [31]=active, [9:0]=x.
assign ufo_active_out = sprite_regs[5'h10][31];
assign ufo_x_out      = sprite_regs[5'h10][9:0];

// Implement axi_awready generation
// axi_awready is asserted for one S_AXI_ACLK clock cycle when both
// S_AXI_AWVALID and S_AXI_WVALID are asserted. axi_awready is
// de-asserted when reset is low.

always_ff @( posedge S_AXI_ACLK )
begin
  if ( S_AXI_ARESETN == 1'b0 )
    begin
      axi_awready <= 1'b0;
      aw_en <= 1'b1;
    end 
  else
    begin    
      if (~axi_awready && S_AXI_AWVALID && S_AXI_WVALID && aw_en)
        begin
          // slave is ready to accept write address when 
          // there is a valid write address and write data
          // on the write address and data bus. This design 
          // expects no outstanding transactions. 
          axi_awready <= 1'b1;
          aw_en <= 1'b0;
        end
        else if (S_AXI_BREADY && axi_bvalid)
            begin
              aw_en <= 1'b1;
              axi_awready <= 1'b0;
            end
      else           
        begin
          axi_awready <= 1'b0;
        end
    end 
end       

// Implement axi_awaddr latching
// This process is used to latch the address when both 
// S_AXI_AWVALID and S_AXI_WVALID are valid. 

always_ff @( posedge S_AXI_ACLK )
begin
  if ( S_AXI_ARESETN == 1'b0 )
    begin
      axi_awaddr <= 0;
    end 
  else
    begin    
      if (~axi_awready && S_AXI_AWVALID && S_AXI_WVALID && aw_en)
        begin
          // Write Address latching 
          axi_awaddr <= S_AXI_AWADDR;
        end
    end 
end       

// Implement axi_wready generation
// axi_wready is asserted for one S_AXI_ACLK clock cycle when both
// S_AXI_AWVALID and S_AXI_WVALID are asserted. axi_wready is 
// de-asserted when reset is low. 

always_ff @( posedge S_AXI_ACLK )
begin
  if ( S_AXI_ARESETN == 1'b0 )
    begin
      axi_wready <= 1'b0;
    end 
  else
    begin    
      if (~axi_wready && S_AXI_WVALID && S_AXI_AWVALID && aw_en )
        begin
          // slave is ready to accept write data when 
          // there is a valid write address and write data
          // on the write address and data bus. This design 
          // expects no outstanding transactions. 
          axi_wready <= 1'b1;
        end
      else
        begin
          axi_wready <= 1'b0;
        end
    end 
end       

// Write logic, routes writes to either BRAM (VRAM) or palette registers
// Uses bit 13 of byte address to distinguish:
// axi_awaddr[13] == 0 -> write to VRAM (BRAM)
// axi_awaddr[13] == 1 -> write to palette registers (if in range 0x800-0x807)
assign slv_reg_wren = axi_wready && S_AXI_WVALID && axi_awready && S_AXI_AWVALID;

// Derive BRAM Port A address from AXI write address (word address = byte_addr >> 2)
assign bram_addr_a = axi_awaddr[ADDR_LSB + 10 : ADDR_LSB];

// BRAM write enable - only assert when writing to VRAM region AND write strobe is valid.
// Sprite-mode adds region addr[14]==1, which must NOT touch VRAM.
assign bram_we_a = slv_reg_wren && (axi_awaddr[14] == 1'b0) && (axi_awaddr[13] == 1'b0);

// BRAM chip enable, active during AXI writes to VRAM or AXI reads from VRAM
// This prevents leftover write strobes from corrupting BRAM
assign bram_en_a = (slv_reg_wren && axi_awaddr[14] == 1'b0 && axi_awaddr[13] == 1'b0)
                || (slv_reg_rden && axi_araddr[14] == 1'b0 && axi_araddr[13] == 1'b0);

// BRAM Port A - synchronous write with byte-enable (write strobe support)
always_ff @( posedge S_AXI_ACLK )
begin
  if (bram_we_a)
    begin
      for (byte_index = 0; byte_index <= 3; byte_index = byte_index + 1)
        begin
          // Only write bytes where write strobe is asserted
          if (S_AXI_WSTRB[byte_index] == 1)
            begin
              vram[bram_addr_a][(byte_index*8) +: 8] <= S_AXI_WDATA[(byte_index*8) +: 8];
            end
          end
       end
  else 
    begin
    end
end    

// BRAM Port A - synchronous read (1 cycle latency)
// Data becomes available 1 clock after address is presented
always_ff @( posedge S_AXI_ACLK )
begin
  if (bram_en_a)
    begin
      bram_rd_data_a <= vram[axi_araddr[ADDR_LSB + 10 : ADDR_LSB]];
    end
end

// Week 2: BRAM Port B - display-side read (pixel_clk domain, read-only)
always_ff @ ( posedge pixel_clk )
begin
  vram_rd_data <= vram[vram_rd_addr];
end

// Palette register writes (only when writing to palette region 0x800-0x807)
always_ff @( posedge S_AXI_ACLK )
begin
  if (S_AXI_ARESETN == 1'b0)
    begin
      for (int i = 0; i < 8; i++)
        begin
          palette_regs[i] <= 32'h0;
        end
    end else
      begin
        if (slv_reg_wren && axi_awaddr[14] == 1'b0 && axi_awaddr[13] == 1'b1)
          begin
            // Word address within palette region: axi_awaddr[ADDR_LSB +: 4] gives offset from 0x800
            // Palette registers are at word offsets 0x800-0x807, i.e., lower 3 bits of word addr
            if (axi_awaddr[ADDR_LSB + 10 : ADDR_LSB + 3] == 8'h00)
              begin
                // This is a palette register write (offsets 0-7 within the 0x800 region)
                for (byte_index = 0; byte_index <= 3; byte_index = byte_index + 1)
                  begin
                    if (S_AXI_WSTRB[byte_index] == 1)
                      palette_regs[axi_awaddr[ADDR_LSB + 2 : ADDR_LSB]][(byte_index*8) +: 8] <= S_AXI_WDATA[(byte_index*8) +: 8];
                  end
              end
          end
      end
end

// Sprite-state register writes (selected by axi_awaddr[14]==1).
// 32 32-bit words, byte-strobe writes.
always_ff @( posedge S_AXI_ACLK )
begin
  if (S_AXI_ARESETN == 1'b0)
    begin
      for (int i = 0; i < 32; i++)
        sprite_regs[i] <= 32'h0;
    end
  else
    begin
      if (slv_reg_wren && axi_awaddr[14] == 1'b1)
        begin
          for (byte_index = 0; byte_index <= 3; byte_index = byte_index + 1)
            begin
              if (S_AXI_WSTRB[byte_index] == 1)
                sprite_regs[axi_awaddr[ADDR_LSB + 4 : ADDR_LSB]][(byte_index*8) +: 8]
                    <= S_AXI_WDATA[(byte_index*8) +: 8];
            end
        end
    end
end

// Implement write response logic generation
// The write response and response valid signals are asserted by the slave
// when axi_wready, S_AXI_WVALID, axi_wready and S_AXI_WVALID are asserted.
// This marks the acceptance of address and indicates the status of
// write transaction.

always_ff @( posedge S_AXI_ACLK )
begin
  if ( S_AXI_ARESETN == 1'b0 )
    begin
      axi_bvalid  <= 0;
      axi_bresp   <= 2'b0;
    end
  else
    begin    
      if (axi_awready && S_AXI_AWVALID && ~axi_bvalid && axi_wready && S_AXI_WVALID)
        begin
          // indicates a valid write response is available
          axi_bvalid <= 1'b1;
          axi_bresp  <= 2'b0; // 'OKAY' response 
        end                   // work error responses in future
      else
        begin
          if (S_AXI_BREADY && axi_bvalid) 
            //check if bready is asserted while bvalid is high) 
            //(there is a possibility that bready is always asserted high)   
            begin
              axi_bvalid <= 1'b0; 
            end  
        end
    end
end   

// Implement axi_arready generation
// axi_arready is asserted for one S_AXI_ACLK clock cycle when
// S_AXI_ARVALID is asserted. axi_awready is 
// de-asserted when reset (active low) is asserted. 
// The read address is also latched when S_AXI_ARVALID is 
// asserted. axi_araddr is reset to zero on reset assertion.

always_ff @( posedge S_AXI_ACLK )
begin
  if ( S_AXI_ARESETN == 1'b0 )
    begin
      axi_arready <= 1'b0;
      axi_araddr  <= 32'b0;
    end 
  else
    begin    
      if (~axi_arready && S_AXI_ARVALID)
        begin
          // indicates that the slave has acceped the valid read address
          axi_arready <= 1'b1;
          // Read address latching
          axi_araddr  <= S_AXI_ARADDR;
        end
      else
        begin
          axi_arready <= 1'b0;
        end
    end 
end      

// AXI Read Data Channel with BRAM latency handling
// Track whether the read is from BRAM, and if so, delay RVALID assertion by 1 clock cycle using a pending flag.
logic read_from_bram; // flag: current read targets BRAM (needs latency delay)
logic bram_read_pending; // 1-cycle delay register for BRAM read valid

// Determine if read address targets BRAM or registers.
// VRAM (BRAM) lives at addr[14]==0, addr[13]==0 only.
assign read_from_bram = (axi_araddr[14] == 1'b0) && (axi_araddr[13] == 1'b0);

assign slv_reg_rden = axi_arready & S_AXI_ARVALID & ~axi_rvalid;

// BRAM read pending register, delays RVALID by 1 cycle for BRAM reads
always_ff @( posedge S_AXI_ACLK )
begin
  if ( S_AXI_ARESETN == 1'b0 )
    bram_read_pending <= 1'b0;
  else if (slv_reg_rden && read_from_bram)
    // BRAM read initiated, data will be ready next cycle
    bram_read_pending <= 1'b1;
  else
    bram_read_pending <= 1'b0;
end

// Implement axi_arvalid generation
// axi_rvalid is asserted for one S_AXI_ACLK clock cycle when both 
// S_AXI_ARVALID and axi_arready are asserted. The slave registers 
// data are available on the axi_rdata bus at this instance. The 
// assertion of axi_rvalid marks the validity of read data on the 
// bus and axi_rresp indicates the status of read transaction.axi_rvalid 
// is deasserted on reset (active low). axi_rresp and axi_rdata are 
// cleared to zero on reset (active low).  
always_ff @( posedge S_AXI_ACLK )
begin
  if ( S_AXI_ARESETN == 1'b0 )
    begin
      axi_rvalid <= 0;
      axi_rresp  <= 0;
    end 
  else
    begin    
      if (slv_reg_rden && !read_from_bram)
        begin
          // Register read: data available immediately
          axi_rvalid <= 1'b1;
          axi_rresp  <= 2'b0; // 'OKAY' response
        end   
      else if (bram_read_pending)
        begin
          // BRAM read: data available after 1-cycle latency
          axi_rvalid <= 1'b1;
          axi_rresp  <= 2'b0;
        end    
      else if (axi_rvalid && S_AXI_RREADY)
        begin
          axi_rvalid <= 1'b0;
        end      
    end
end    

// Read data multiplexer
// Routes read data from BRAM or registers based on address region
always_ff @ ( posedge S_AXI_ACLK )
begin
  if (S_AXI_ARESETN == 1'b0)
    begin
      axi_rdata <= 0;
    end else
      begin
        if (slv_reg_rden && !read_from_bram)
          begin
            // Register reads (available immediately).  Three sub-regions:
            //   addr[14]=1                -> sprite-state region
            //   addr[14]=0, addr[13]=1    -> palette + sync registers
            if (axi_araddr[14] == 1'b1)
              begin
                axi_rdata <= sprite_regs[axi_araddr[ADDR_LSB + 4 : ADDR_LSB]];
              end
            else
              begin
                case (axi_araddr[ADDR_LSB + 3 : ADDR_LSB])
                  // Sync registers at offsets 0x808-0x80A (lower 4 bits: 8, 9, A)
                  4'h8: axi_rdata <= frame_counter;
                  4'h9: axi_rdata <= current_draw_x;
                  4'hA: axi_rdata <= current_draw_y;
                  // Palette registers at offsets 0x800-0x807 (lower 4 bits: 0-7)
                  default: axi_rdata <= palette_regs[axi_araddr[ADDR_LSB + 2 : ADDR_LSB]];
                endcase
              end
          end else if (bram_read_pending)
            begin
              // BRAM reads: VRAM data (available 1 cycle after address)
              axi_rdata <= bram_rd_data_a;
            end
      end
end

// Add user logic here

// User logic ends

endmodule

